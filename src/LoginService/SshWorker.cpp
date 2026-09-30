#include "SshWorker.hpp"

#include <QByteArray>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

#include <cstddef>

namespace {

QString quoteShellArgument(const QString &argument) {
    QString quoted = argument;
    quoted.replace("'", "'\\''");
    return "'" + quoted + "'";
}

bool getServerFingerprint(ssh_session session, QString &fingerprint) {
    ssh_key serverKey = nullptr;

    if (ssh_get_server_publickey(session, &serverKey) != SSH_OK) {
      return false;
    }

    unsigned char *hash = nullptr;
    size_t hashLength = 0;

    if(ssh_get_publickey_hash(serverKey, SSH_PUBLICKEY_HASH_SHA256, 
                              &hash, &hashLength) != SSH_OK) {
      
      ssh_key_free(serverKey);
      return false;
    }

    char *fingerprintText = 
      ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hashLength);

    ssh_clean_pubkey_hash(&hash);
    ssh_key_free(serverKey);

    if (fingerprintText == nullptr) {
      return false;
    }

    fingerprint = QString::fromLatin1(fingerprintText);
    ssh_string_free_char(fingerprintText);
    return true;
  }
}

SshWorker::SshWorker(QObject *parent) : QObject(parent) {}

SshWorker::~SshWorker() { 
    cleanup();
}

void SshWorker::login(const QString &host,
                      const QString &username, 
                      const QString &password, 
                      int port) {

     // Mögliche laufende oder wartende Session aufräumen
  cleanup();

  m_session = ssh_new();
  // Mögliche bestehende Verbindung sauber schließen
  if(m_session == nullptr) {
    failLogin("SSH session could not be created");
    return;
  }

  const QByteArray hostBytes = host.toUtf8();
  const QByteArray user = username.toUtf8();

  // Host, User und Port für die SSH-Verbindung setzen
  if (ssh_options_set(m_session, SSH_OPTIONS_HOST, hostBytes.constData()) != SSH_OK || 
      ssh_options_set(m_session, SSH_OPTIONS_USER, user.constData()) != SSH_OK || 
      ssh_options_set(m_session, SSH_OPTIONS_PORT, &port) != SSH_OK){
    failLogin("Could not set SSH options");
    return;
  }

  //  Versuchen eine Netzwerkverbindung zum Server herstellen
  if (ssh_connect(m_session) != SSH_OK) {
    failLogin("CONNECTION ERROR");
    return;
  }
  m_connected = true;

  // Überprüfen des Server-Keys BEVOR Zugangsdaten gesendet werden
  const auto hostStatus = ssh_session_is_known_server(m_session);

  if (hostStatus == SSH_KNOWN_HOSTS_UNKNOWN || 
      hostStatus == SSH_KNOWN_HOSTS_NOT_FOUND) {
    // Bei unbekanntem Key Fingerprint anzeigen und Besätigung verlangen
    QString fingerprint;

    if (!getServerFingerprint(m_session, fingerprint)) {
      failLogin("Could not read the server host-key fingerprint");
      return;
    }
    // Passwort speichern bis Dialog beantwortet wurde
    m_pendingPassword = password;
    m_waitingForHostKeyConfirmation = true;

    emit hostKeyConfirmationRequested(host, port, fingerprint);
    return;
  }

  if(hostStatus != SSH_KNOWN_HOSTS_OK) {
    failLogin("SSH host-key check failed");
    return;
  }

  m_pendingPassword = password;
  authenticatePendingLogin();     
}
// Zum Debuggen um aktuellen Fingerpint zu deleten: ssh-keygen -R [ip]

void SshWorker::confirmHostKey() {
  if(!m_waitingForHostKeyConfirmation ||
     m_session == nullptr ||
     !m_connected) {
      return;
    }

  // Den unbekannten Host-Key erst nach der Bestätigung speichern.
  if (ssh_session_update_known_hosts(m_session) != SSH_OK) {
    failLogin("Could not save the SSH host key");
    return;
    }

  authenticatePendingLogin();
}

void SshWorker::rejectHostKey() {
  if (!m_waitingForHostKeyConfirmation) {
    return;
  }

  failLogin("SSH host-key was not accepted");
}

bool SshWorker::authenticatePendingLogin() {
  if (m_session == nullptr || !m_connected) {
    failLogin("No active SSH connection for authentication");
    return false;
  }

  const QByteArray passwordBytes = m_pendingPassword.toUtf8();
  const int result = ssh_userauth_password(
      m_session, nullptr, passwordBytes.constData());

  // Passwort nicht länger als nötig im Worker behalten.
  m_pendingPassword.clear();
  m_waitingForHostKeyConfirmation = false;

  if (result != SSH_AUTH_SUCCESS) {
    failLogin("SSH authentication failed");
    return false;
  }

  m_loggedIn = true;
  qDebug() << "SSH connection successfully established";
  emit loginFinished(true);
  return true;
  }

void SshWorker::failLogin(const QString &message) {
  if (m_session != nullptr) {
    qWarning() << message << ssh_get_error(m_session);
  } else {
    qWarning() << message;
  }

  cleanup();
  emit loginFinished(false);
}

void SshWorker::cleanup() {
  m_pendingPassword.clear();
  m_waitingForHostKeyConfirmation = false;

  if (m_session != nullptr) {
    if (m_connected) {
      ssh_disconnect(m_session);
    }

    ssh_free(m_session);
    m_session = nullptr;
  }

  m_connected = false;
  m_loggedIn = false;
}

void SshWorker::logout() {
  cleanup();
  emit loggedOut();
}

void SshWorker::prepareCommand() {
    m_cancelRequested.store(false, std::memory_order_release);
}

void SshWorker::cancelCommand() {
    m_cancelRequested.store(true, std::memory_order_release);
}

void SshWorker::executeCommand(const QString &command) {
    
    // Überprüfung für eingeloggt und verbunden sein
    if (m_session == nullptr || !m_connected || !m_loggedIn) {
        emit commandFailed("No authenticated SSH connection");
        return;
    }

    // Sicherstellen, dass es der Command etwas enthält
    if (command.trimmed().isEmpty()) {
        emit commandFailed("Command must not be empty");
        return;
    }

    // Neuen libssh-Channel erstellen
    const QString shellCommand =
        "exec 2>&3; "
        "if [ -r \"$HOME/.common.alias\" ]; then "
        ". \"$HOME/.common.alias\" || exit; "
        "fi; "
        "shopt -s expand_aliases; "
        "eval " + quoteShellArgument(command);
    const QByteArray commandBytes =
        QString("bash -ic %1 3>&2 2>/dev/null")
            .arg(quoteShellArgument(shellCommand))
            .toUtf8();
    ssh_channel channel = ssh_channel_new(m_session);

    if(channel == nullptr) {
        emit commandFailed(
            QString("Could not create SSH Channe: %1")
            .arg(ssh_get_error(m_session)));
        return;
    }

    // Channel schließen
    const auto closeChannel = [&channel]() {
        if(ssh_channel_is_open(channel)) {
            ssh_channel_close(channel);
        }
        ssh_channel_free(channel);
    };

    // Bei geöffnetem Channel, Channel schließen
    if(ssh_channel_open_session(channel) != SSH_OK) {
        const QString error = QString ("Could not open SSH Channel: %1")
                                      .arg(ssh_get_error(m_session));
        closeChannel();
        emit commandFailed(error);
        return;
    }
    
    if (ssh_channel_request_exec(channel, commandBytes.constData()) != SSH_OK) {
        const QString error = QString ("Could not start command: %1")
                                      .arg(ssh_get_error(m_session));
    closeChannel();
    emit commandFailed(error);
    return;
    }

    constexpr int maxOutputBytes = 1024 * 1024;

    QByteArray stdoutData;
    QByteArray stderrData;
    char buffer[4096];

    QString readError;
    QString signalError;
    QElapsedTimer cancellationTimer;
    int cancellationStage = 0;
    bool cancellationTimedOut = false;

    while  (true) {

        if (m_cancelRequested.load(std::memory_order_acquire)) {
            if (cancellationStage == 0) {
                cancellationTimer.start();
                cancellationStage = 1;
                if (ssh_channel_request_send_signal(channel, "INT") != SSH_OK) {
                    signalError = QString("Could not send interrupt signal: %1")
                                      .arg(ssh_get_error(m_session));
                }
            } else if (cancellationTimer.elapsed() >= 1000 &&
                       cancellationStage == 1) {
                cancellationStage = 2;
                if (ssh_channel_request_send_signal(channel, "TERM") != SSH_OK) {
                    signalError = QString("Could not send terminate signal: %1")
                                      .arg(ssh_get_error(m_session));
                }
            } else if (cancellationTimer.elapsed() >= 2000 &&
                       cancellationStage == 2) {
                cancellationStage = 3;
                if (ssh_channel_request_send_signal(channel, "KILL") != SSH_OK) {
                    signalError = QString("Could not send kill signal: %1")
                                      .arg(ssh_get_error(m_session));
                }
            } else if (cancellationTimer.elapsed() >= 4000 &&
                       cancellationStage == 3) {
                cancellationTimedOut = true;
                break;
            }
        }

        bool receivedData = false;

        const int stdoutCount = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 0);
                                                            
        if(stdoutCount == SSH_ERROR) {
            if (cancellationStage != 0) {
                readError = QString("Error reading command output while cancelling: %1")
                                .arg(ssh_get_error(m_session));
                break;
            }
            readError = QString ("Error reading command output: %1")        
                                .arg(ssh_get_error(m_session));
        break;
        }

        if(stdoutCount > 0) {
            if(stdoutData.size() + stderrData.size() + stdoutCount > maxOutputBytes) {
                readError = "Command output exceeded the 1 MiB limit";
                break;
            }

            stdoutData.append(buffer, stdoutCount);
            receivedData = true;
        }

        const int stderrCount = ssh_channel_read_nonblocking(channel, buffer, sizeof(buffer), 1);

        if(stderrCount == SSH_ERROR) {
            if (cancellationStage != 0) {
                readError = QString("Error reading command error output while cancelling: %1")
                                .arg(ssh_get_error(m_session));
                break;
            }
            readError = QString("Error reading command error output: %1")
                                .arg(ssh_get_error(m_session));
        break;
        }

        if(stderrCount > 0) {
            if(stdoutData.size() + stderrData.size() + stderrCount > maxOutputBytes) {
                readError = "Command output exceeded the 1MiB limit";
                break;
            }

            stderrData.append(buffer, stderrCount);
            receivedData = true;
        }

        if(ssh_channel_is_eof(channel) && !receivedData) {
            break;
        }

        if(!receivedData) {
            QThread::msleep(10);
        }
    }

    if (cancellationStage != 0) {
        closeChannel();
        if (!readError.isEmpty()) {
            emit commandFailed(readError);
        } else if (cancellationTimedOut) {
            QString message =
                "The server did not stop the command after INT, TERM, and KILL";
            if (!signalError.isEmpty()) {
                message += QString(": %1").arg(signalError);
            }
            emit commandFailed(message);
        } else {
            emit commandCancelled();
        }
        return;
    }

    if(!readError.isEmpty()) {
        closeChannel();
        emit commandFailed(readError);
        return;
    }

    const int exitStatus = ssh_channel_get_exit_status(channel);
    closeChannel();

    emit commandCompleted(QString::fromUtf8(stdoutData),
                          QString::fromUtf8(stderrData),
                          exitStatus);
}

void SshWorker::executePresetCommand(const QString &commandId)
{
    QString command;

    if (commandId == "hostname") {
        command = "hostname";
    } else if (commandId == "whoami") {
        command = "whoami";
    } else if (commandId == "uptime") {
        command = "uptime";
    } else if (commandId == "disk-usage") {
        command = "df -h";
    } else {
        emit commandFailed("Unkown predefined command");
        return;
    }

    executeCommand(command);
}