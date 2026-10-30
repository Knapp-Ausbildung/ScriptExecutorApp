#include "SshWorker.hpp"

#include <QByteArray>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QStandardPaths>


#include <cstddef>

namespace {

  // Set Repo Work Directory
  const QString kRemoteBuildRoot = QStringLiteral("/kisoft/build/");

  QString quoteShellArgument(const QString &argument) {
      QString quoted = argument;
      quoted.replace("'", "'\\''");
      return "'" + quoted + "'";
  }

  bool getKeyFingerprint(ssh_key key, QString &fingerprint) {
      if (key == nullptr) {
        return false;
      }
      unsigned char *hash = nullptr;
      size_t hashLength = 0;

      if(ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256,
                                &hash, &hashLength) != SSH_OK) {
        return false;
      }

      char *fingerprintText = 
        ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hashLength);

      ssh_clean_pubkey_hash(&hash);

      if (fingerprintText == nullptr) {
        return false;
      }

      fingerprint = QString::fromLatin1(fingerprintText);
      ssh_string_free_char(fingerprintText);
      return true;
    }

  bool getServerFingerprint(ssh_session session, QString &fingerprint) {
      ssh_key serverKey = nullptr;
      if (ssh_get_server_publickey(session, &serverKey) != SSH_OK) {
        return false;
      }

      const bool fingerprintRead = getKeyFingerprint(serverKey, fingerprint);
      ssh_key_free(serverKey);
      return fingerprintRead;
  }

  bool getStoredHostFingerprint(ssh_session session, QString &fingerprint) {
      ssh_knownhosts_entry *entry = nullptr;
      const auto status = ssh_session_get_known_hosts_entry(session, &entry);
      if (entry == nullptr) {
        return false;
      }

      const bool fingerprintRead = getKeyFingerprint(entry->publickey, fingerprint);
      ssh_knownhosts_entry_free(entry);
      return status != SSH_KNOWN_HOSTS_ERROR && fingerprintRead;
  }

  bool removeStoredHostKey(const QString &host, int port, QString &error) {
      const QString sshKeygenPath =
          QStandardPaths::findExecutable(QStringLiteral("ssh-keygen"));
      if (sshKeygenPath.isEmpty()) {
        error = QStringLiteral("ssh-keygen was not found; the old host key was not changed");
        return false;
      }

      const QString knownHostsName = port == 22
          ? host
          : QStringLiteral("[%1]:%2").arg(host).arg(port);

      QProcess process;
      process.start(sshKeygenPath, {QStringLiteral("-R"), knownHostsName});
      if (!process.waitForStarted(5000)) {
        error = QStringLiteral("Could not start ssh-keygen: %1").arg(process.errorString());
        return false;
      }
      if (!process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished();
        error = QStringLiteral("Timed out while replacing the stored SSH host key");
        return false;
      }
      if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString details = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        error = QStringLiteral("Could not remove the old SSH host key");
        if (!details.isEmpty()) {
          error += QStringLiteral(": ") + details;
        }
        return false;
      }

      return true;
  }

  QString repositoryPathForType(const QString &type)
  {
    static const QSet<QString> allowedTypes {
      QStringLiteral("SRC"),
      QStringLiteral("SandBox"),
      QStringLiteral("KiSoft-One")
    };

    if (!allowedTypes.contains(type)) {
      return {};
    }

    return QDir(kRemoteBuildRoot).filePath(type); 
  }

  bool repositoryTypeAndPrefixFromUrl(const QString &urlText,
                                      QString &type,
                                      QString &customerPrefix) {
    const QUrl url(urlText);
    const QString name = url.path(QUrl::FullyDecoded).section('/', -1);

    static const QStringList allowedTypes{
      QStringLiteral("KiSoft-One"),
      QStringLiteral("SandBox"),
      QStringLiteral("SRC")
    };

    for (const QString &candidate : allowedTypes) {
      if ( name == candidate) {
          type = candidate;
          customerPrefix.clear();
          return true;
      }

      const QString suffix = QStringLiteral("-") + candidate;
      if (name.endsWith(suffix)) {
        type = candidate;
        customerPrefix = name.left(name.size() - suffix.size());
        return !customerPrefix.isEmpty();
      }
    }

    return false;
  }
}

SshWorker::SshWorker(QObject *parent) : QObject(parent) {
  m_keepAliveTimer = new QTimer(this);
  m_keepAliveTimer->setInterval(10'000);

  connect(m_keepAliveTimer, &QTimer::timeout,
          this, &SshWorker::checkConnection);
}

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
  const bool hostKeyChanged = hostStatus == SSH_KNOWN_HOSTS_CHANGED ||
                              hostStatus == SSH_KNOWN_HOSTS_OTHER;

  if (hostStatus == SSH_KNOWN_HOSTS_UNKNOWN || 
      hostStatus == SSH_KNOWN_HOSTS_NOT_FOUND ||
      hostKeyChanged) {
    // Bei unbekanntem Key Fingerprint anzeigen und Besätigung verlangen
    QString fingerprint;
    QString previousFingerprint;

    if (!getServerFingerprint(m_session, fingerprint)) {
      failLogin("Could not read the server host-key fingerprint");
      return;
    }
    if (hostKeyChanged) {
      getStoredHostFingerprint(m_session, previousFingerprint);
    }

    // Passwort speichern bis Dialog beantwortet wurde
    m_pendingPassword = password;
    m_waitingForHostKeyConfirmation = true;
    m_pendingHostKeyChanged = hostKeyChanged;
    m_pendingHost = host;
    m_pendingPort = port;

    emit hostKeyConfirmationRequested(host, port, fingerprint,
                                      previousFingerprint, hostKeyChanged);
    return;
  }

  if (hostStatus != SSH_KNOWN_HOSTS_OK) {
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

  if (m_pendingHostKeyChanged) {
    QString error;
    if (!removeStoredHostKey(m_pendingHost, m_pendingPort, error)) {
      failLogin(error);
      return;
    }
  }

  // Den unbekannten Host-Key erst nach der Bestätigung speichern.
  if (ssh_session_update_known_hosts(m_session) != SSH_OK) {
    failLogin("Could not save the SSH host key");
    return;
    }

  if (ssh_session_is_known_server(m_session) != SSH_KNOWN_HOSTS_OK) {
    failLogin("SSH host-key was not updated in the known_hosts file");
    return;
  }

  m_pendingHostKeyChanged = false;
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
  m_pendingHostKeyChanged = false;
  m_pendingHost.clear();
  m_pendingPort = 22;

  if (result != SSH_AUTH_SUCCESS) {
    failLogin("SSH authentication failed");
    return false;
  }

  m_loggedIn = true;
  m_keepAliveFailures = 0;
  m_keepAliveTimer->start();
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

void SshWorker::checkConnection() {
  if(m_session == nullptr || !m_connected || !m_loggedIn) {
    return;
  }

  if (ssh_send_ignore(m_session, "keepalive") == SSH_OK) {
    m_keepAliveFailures = 0;
    return;
  }

  qWarning() << "SSH keepalive check failed:"
             << (m_session != nullptr ? ssh_get_error(m_session)
                                      : "SSH session is unavailable");
  ++m_keepAliveFailures;

  if (m_keepAliveFailures >= 2) {
    cleanup();
    emit loggedOut();
  }
}

bool SshWorker::validateGitRequest(const QString &urlText,
                                   QString &normalizedUrl,
                                   QString &error) const {
  if (m_session == nullptr || !m_connected || !m_loggedIn) {
    error = "No authenticated SSH connection";
    return false;
  }

  const QUrl remote(urlText.trimmed());

  if (!remote.isValid() ||
      remote.scheme() != QStringLiteral("https") ||
      remote.host().isEmpty() ||
      !remote.userInfo().isEmpty()) {
    error = "Invalid HTTPS repository URL";
    return false;
  }

  normalizedUrl = remote.toString(QUrl::FullyEncoded);
  return true;
}
                        
bool SshWorker::validateGitRequest(const QString &urlText,
                                   const QString &branchText,
                                   QString &normalizedUrl,
                                   QString &normalizedBranch,
                                   QString &error) const {

  if (!validateGitRequest(urlText, normalizedUrl, error)) {
    return false;
  }

  normalizedBranch = branchText.trimmed();

  if (normalizedBranch.isEmpty()) {
    error = "Branch must not be empty";
    return false;
  }

  return true;
}
                     
void SshWorker::cleanup() {
  if (m_keepAliveTimer != nullptr) {
    m_keepAliveTimer->stop();
  }

  m_keepAliveFailures = 0;

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

// Zum Ausführen von Commands mithilfe eines nicht interaktiven Terminals
void SshWorker::executeCommand(const QString &command) {
  executeCommandInternal(command, false);
}

// Zum Ausführen commands mit true | false option ob Pseudo-Terminal benutzt wird oder nicht
void SshWorker::executeCommandInternal(const QString &command,
                                       bool requestPty, bool continueOnOutputLimit) {
    
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
        const bool connectionLost = ssh_is_connected(m_session) == 0;
        closeChannel();
        emit commandFailed(error);

        if (connectionLost) {
            cleanup();
            emit loggedOut();
        }
        return;
    }

    if (requestPty && ssh_channel_request_pty(channel) != SSH_OK) {
      const QString error = 
        QString("Could not request terminal: %1")
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
    bool outputTruncated = false;

    char buffer[4096];

    QString readError;
    QString signalError;
    QElapsedTimer cancellationTimer;
    int cancellationStage = 0;
    bool cancellationTimedOut = false;

    QByteArray promptBuffer;
    QByteArray pendingLiveOutput;
    QElapsedTimer liveOutputTimer;
    liveOutputTimer.start();
    const auto flushLiveOutput = [&pendingLiveOutput, &liveOutputTimer, this]() {
        if (pendingLiveOutput.isEmpty()) {
            return;
        }

        emit commandOutputReceived(QString::fromUtf8(pendingLiveOutput));
        pendingLiveOutput.clear();
        liveOutputTimer.restart();
    };
    bool waitingForInput = false;

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
              if (!continueOnOutputLimit) {
                readError = "Command output exceeded the 1 MiB limit";
                break;
              }
              outputTruncated = true;
            } else {
            stdoutData.append(buffer, stdoutCount);
            }

            promptBuffer.append(buffer, stdoutCount);
            pendingLiveOutput.append(buffer, stdoutCount);
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
              if (!continueOnOutputLimit) {
                readError = "Command output exceeded the 1MiB limit";
                break;
              }
              outputTruncated = true;
            } else {
              stderrData.append(buffer, stderrCount);
            }

            promptBuffer.append(buffer, stderrCount);
            pendingLiveOutput.append(buffer, stderrCount);
            receivedData = true;
        }

        if (pendingLiveOutput.size() >= 16 * 1024 ||
            liveOutputTimer.elapsed() >= 33) {
            flushLiveOutput();
        }

        if (receivedData) {
        constexpr qsizetype maxPromptBytes = 256;

        if (promptBuffer.size() > maxPromptBytes) {
            promptBuffer.remove(0, promptBuffer.size() - maxPromptBytes);
        }
        
        QString promptText = QString::fromUtf8(promptBuffer);

        static const QRegularExpression ansiSgr(R"(\x1B\[[0-9;]*m)");
        promptText.remove(ansiSgr);

        static const QRegularExpression helperUsernamePrompt(R"re(please provide your git remote username[\s\S]*username:\s*$)re",
                                                             QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression httpsUsernamePrompt(R"re(username for 'https?://[^']+':\s*$)re",
                                                          QRegularExpression::CaseInsensitiveOption);        
        static const QRegularExpression gitPasswordPrompt(R"(password for 'https?://[^']+':\s*$)", 
                                                          QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression otherPasswordPrompt(R"((password|passphrase|passcode)[^\r\n]{0,80}[:?]\s*$)",
                                                            QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression confirmationPrompt(R"re(execute prepared updates\s*\(y/n\)\?:?\s*$)re",
                                                           QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression genericUsernamePrompt(R"(username:\s*$)",
                                                              QRegularExpression::CaseInsensitiveOption);
        
        const bool askForGitUsername = helperUsernamePrompt.match(promptText).hasMatch() ||
                                       httpsUsernamePrompt.match(promptText).hasMatch();
        const bool askForGitPassword = gitPasswordPrompt.match(promptText).hasMatch();
        const bool askForOtherPassword = !askForGitPassword && otherPasswordPrompt.match(promptText).hasMatch();
        const bool askForConfirmation = confirmationPrompt.match(promptText).hasMatch();
        const bool askForGenericUsername = genericUsernamePrompt.match(promptText).hasMatch();

        if(!waitingForInput && (askForGitUsername || askForGitPassword || 
                                askForOtherPassword || askForConfirmation || 
                                askForGenericUsername)) {
          flushLiveOutput();
          waitingForInput = true;
          emit commandInputRequested(promptText.trimmed(), !askForGitUsername);
        }
        }

        QByteArray response;
        {
          QMutexLocker locker(&m_inputMutex);
          if (m_inputPending) {
            response.swap(m_pendingInput);
            m_inputPending = false;
          }
        }

        if(!response.isEmpty()) {
          int offset = 0;

          while (offset < response.size()) {
            const int written = ssh_channel_write(channel, response.constData() + offset, response.size() - offset);

            if (written <= 0) {
              readError = QString ("Could not send command input: %1")
                                   .arg(ssh_get_error(m_session));

              break;
            }

            offset += written;
          }

          response.fill('\0');

          if (!readError.isEmpty()) {
            break;
          }

          waitingForInput = false;
          promptBuffer.clear();
        }

        if(ssh_channel_is_eof(channel) && !receivedData) {
            break;
        }

        if(!receivedData) {
            QThread::msleep(10);
        }
    }

    flushLiveOutput();

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

    if (outputTruncated) {
    stdoutData.append(
        "\n[Hinweis: Die lokal gespeicherte Befehlsausgabe ist auf 1 MiB begrenzt; "
        "weitere Ausgabe wurde nicht gespeichert.]\n");
    }

    emit commandCompleted(QString::fromUtf8(stdoutData),
                          QString::fromUtf8(stderrData),
                          exitStatus);
}

void SshWorker::executePresetCommand(const QString &commandId)
{
    if (commandId == "reinstallAll") {
      executePresetCommand(QStringLiteral("installRepository"));
      return;
    }

    QString command;
    
    // Set before the commands to true if you need a Pseudo-Terminal
    bool requestPty = false;

    if (commandId == "status") {
        
      requestPty = true;
      const QString qkingInvocation = 
                  "'/kisoft/user/KiSoft-One/wcs/lager/bin/qking'";

      command = "bash -lc " + quoteShellArgument(qkingInvocation);
    } else if (commandId == "restart") {

      requestPty = true;
      command = "sudo env SYSTEMD_COLORS=0 systemctl --no-pager restart kisoft-one.service";
    } else if (commandId == "stop") {

      requestPty = true;
      command = "sudo env SYSTEMD_COLORS=0 systemctl --no-pager stop kisoft-one.service";
    } else if (commandId == "rebuildDb") {

        command = "bash -lc 'cd \"$WCS_ROOT/../wmw\" && yes y | make reinstall'";
    } else if (commandId == "installRepository") {
        requestPty = true;
        const QString installScript =
            QStringLiteral("set -e\n"
                           "/kisoft/build/KiSoft-One/wcs/scripts/setupBuildTarget.sh\n"
            "export KX_SRC_ROOT=\"${KX_SRC_ROOT:-${WCS_ROOT%/wcs}}\"\n"
            ": \"${KX_SRC_ROOT:?KX_SRC_ROOT/WCS_ROOT is not set}\"\n"
                           ": \"${KLASSX_DB_CONNECT:?KLASSX_DB_CONNECT is not set; cannot determine installation state}\"\n"
                           "export KLASSX_HOME=\"$KX_SRC_ROOT\"\n"
                           "cd \"$KX_SRC_ROOT/test/tool\"\n"
                           "if printf 'exit\\n' | sqlplus -L \"$KLASSX_DB_CONNECT\" "
                           "2>&1 | grep -q 'Connected'; then\n"
                           "  echo 'Existing WMW database detected; running KXrelease force reinstall.'\n"
                           "  ./KXrelease.sh -f\n"
                           "else\n"
                           "  echo 'No WMW database detected; running first installation.'\n"
                           "  ./KXrelease.sh\n"
                           "fi");
        command = "sudo -v && bash -lc " + quoteShellArgument(installScript);

    } else if (commandId == "reinstallDb") {
        
        requestPty = true;
        command = "bash -lc 'cd \"$WCS_ROOT/../wmw\" && yes y | make uninstall && yes y | make install && "
                  "cd \"$WCS_ROOT/lager\" && updateParameterTables -b core_backup'";
    } else if (commandId == "reinstallKiSoft") {
       
        requestPty = true;
        command = "bash -lc 'cd \"$WCS_ROOT/\" && make release && "
                  "cd \"$WCS_ROOT/lager\" && updateParameterTables -b core_backup/'";
    }
    
    else {

        emit commandFailed("Unkown predefined command");
        return;
    }

    if (commandId == "reinstallKiSoft" ||
        commandId == "reinstallDb") {
      command = "sudo -v && " + command;
    }

    executeCommandInternal(command, requestPty);
}

void SshWorker::submitCommandInput(const QString &input) {
  QByteArray bytes = input.toUtf8();
  bytes.append('\n');

    {
      QMutexLocker locker(&m_inputMutex);
      m_pendingInput = bytes;
      m_inputPending = true;
    }

  bytes.fill('\0');
}

void SshWorker::loadRemoteBranches(const QString &urlText) {
  
  QString normalizedUrl;
  QString error;
  QUrl origin(urlText);
  origin.setUserInfo(QString());
  const QString cleanOrigin = origin.toString(QUrl::FullyEncoded);
  
  if (!validateGitRequest(cleanOrigin,  
                          normalizedUrl, error)) {
  
    emit commandFailed(error);
  return;
  }       
    const QString command = "git ls-remote --heads " + 
                            quoteShellArgument(normalizedUrl);
                          
    executeCommandInternal(command, true);
}

void SshWorker::fetchRemoteBranch(const QString &urlText,
                                  const QString &branchText) {
  QString normalizedUrl;
  QString normalizedBranch;
  QString error;
  
  if (!validateGitRequest(urlText, branchText, 
                          normalizedUrl, normalizedBranch, error)) {
  
    emit commandFailed(error);
  return;
  } 

  QString repoType;
  QString customerPrefix;

  if (!repositoryTypeAndPrefixFromUrl(normalizedUrl, repoType, customerPrefix)) {
    emit commandFailed("Repository name is not allowed");
    return;
  }

  if (m_activeRepositoryType.isEmpty()) {
    emit commandFailed("Bitte erst ein Repository auswählen");
    return;
  }

  if (repoType != m_activeRepositoryType) {
    emit commandFailed("Die Repository-URL stimmt nicht mit der aktiven Auswahl überein");
    return;
  }

  const QString targetPath = repositoryPathForType(m_activeRepositoryType);
  const QString refspec = QStringLiteral("refs/heads/") + normalizedBranch;

  const QString command = "git -C " + quoteShellArgument(targetPath) +
                          " fetch -- " + quoteShellArgument(normalizedUrl) 
                          + " " + quoteShellArgument(refspec);

  executeCommandInternal(command, true);
}

void SshWorker::pullRemoteBranch(const QString &urlText,
                                 const QString &branchText) {

  QString normalizedUrl;
  QString normalizedBranch;
  QString error;
  
  if (!validateGitRequest(urlText, branchText, 
                          normalizedUrl, normalizedBranch, error)) {
  
    emit commandFailed(error);
  return;
  }                    
  
  QString repoType;
  QString customerPrefix;

if (!repositoryTypeAndPrefixFromUrl(normalizedUrl, repoType, customerPrefix)) {
  emit commandFailed("Repository name is not allowed");
  return;
}
if (m_activeRepositoryType.isEmpty()) {
  emit commandFailed("Bitte erst ein Repository auswählen");
  return;
}

if (repoType != m_activeRepositoryType) {
  emit commandFailed("Die Repository-URL stimmt nicht mit der aktiven Auswahl überein");
  return;
}

const QString repoArg = quoteShellArgument(repositoryPathForType(m_activeRepositoryType));
const QString urlArg =
    quoteShellArgument(normalizedUrl);
const QString branchArg = quoteShellArgument(normalizedBranch);
const QString sourceRef = QStringLiteral("refs/heads/") + normalizedBranch;
const QString trackingRef =
    QStringLiteral("refs/remotes/script-executor/") + normalizedBranch;
const QString refspec = sourceRef + ":" + trackingRef;
const QString refspecArg = quoteShellArgument(refspec);
const QString localRefArg =
    quoteShellArgument(QStringLiteral("refs/heads/") + normalizedBranch);
const QString trackingRefArg = quoteShellArgument(trackingRef);

// Bei lokalen Änderungen -> Abbruch; Bei unterschiedlchen Historie -> Befehl schlägt fehl;
// Kurz vor Branch-Switch gibts ein erneutes fetch, um am aktuellen Stand zu sein
const QString command =
    "git -C " + repoArg +
    " rev-parse --is-inside-work-tree >/dev/null 2>&1 || "
    "{ echo 'Configured path is not a Git repository' >&2; exit 2; }; "
    "if [ -n \"$(git -C " + repoArg +
    " status --porcelain --untracked-files=all)\" ]; then "
    "echo 'Repository has local changes; commit, stash, or clean them first' >&2; "
    "exit 3; "
    "fi && "
    "git -C " + repoArg + " fetch -- " + urlArg + " " + refspecArg + " && "
    "if git -C " + repoArg + " show-ref --verify --quiet " + localRefArg + "; then "
    "git -C " + repoArg + " switch " + branchArg + " && "
    "git -C " + repoArg + " merge --ff-only " + trackingRefArg + "; "
    "else "
    "git -C " + repoArg + " switch --create " + branchArg + " " + trackingRefArg + "; "
    "fi";

  executeCommandInternal(command, true);
}

void SshWorker::replaceRepository(const QString &url, 
                                  const QString &branch) {
  QString normalizedUrl;
  QString normalizedBranch;
  QString error;
  
  if (!validateGitRequest(url, branch, 
                          normalizedUrl, normalizedBranch, error)) {
  
    emit commandFailed(error);
  return;
  }

  QString repoType;
  QString customerPrefix;

  if (!repositoryTypeAndPrefixFromUrl(normalizedUrl, repoType, customerPrefix)) {
    emit commandFailed("Repository name is not allowed");
    return;
  }

  // Zuerst in ein temporäres Nachbarsverzeichnis geklont, aktuelles Verzeichnis wird umbennant
  // Neues Verzeichnis wird erstellt und dort hineingecloned, wenn alles funktioniert hat
  // wird altes Verzeichnis gelöscht 
  const QString targetPath = repositoryPathForType(repoType);
  const QString targetArg = quoteShellArgument(targetPath);

  const QString encodedCustomerPrefix = QString::fromLatin1(QUrl::toPercentEncoding(customerPrefix));

  const QFileInfo targetInfo(targetPath);
  const QString stagingPath = targetInfo.dir().filePath("." + targetInfo.fileName() + "-stage.XXXXXX");

  const QString stagingPattern =
      quoteShellArgument(stagingPath);
  const QString branchArg = quoteShellArgument(normalizedBranch);
  const QString urlArg = quoteShellArgument(normalizedUrl);

  QString command =
      QStringLiteral("set -eu; target=") + targetArg +
      QStringLiteral("; "
                     "if [ -L \"$target\" ]; then "
                     "echo 'Target path must not be a symlink' >&2; exit 2; fi; "
                     "if [ -e \"$target\" ] && [ ! -d \"$target\" ]; then "
                     "echo 'Target path exists but is not a directory' >&2; exit 3; fi; "
                     "parent=${target%/*}; "
                     "mkdir -p -- \"$parent\" || "
                     "{ echo 'Could not create target parent directory' >&2; exit 11; }; "
                     "stage=$(mktemp -d ") + stagingPattern +
      QStringLiteral(") || exit 4; "
                     "if ! git clone --single-branch --branch ") + branchArg +
      QStringLiteral(" -- ") + urlArg +
      QStringLiteral(" \"$stage\"; then "
                     "rm -rf -- \"$stage\"; exit 5; fi; "
                     "if [ \"$(git -C \"$stage\" branch --show-current)\" != ") +
      branchArg +
      QStringLiteral(" ]; then "
                     "echo 'Cloned branch does not match the selection' >&2; "
                     "rm -rf -- \"$stage\"; exit 6; fi; "
                     "old=\"${target}.replace-old.$(date +%Y%m%d%H%M%S).$$\"; "
                     "had_old=0; "
                     "if [ -e \"$target\" ]; then "
                     "mv -- \"$target\" \"$old\" || "
                     "{ rm -rf -- \"$stage\"; exit 7; }; "
                     "had_old=1; fi; "
                     "if mv -- \"$stage\" \"$target\"; then "
                     "printf '%s\\n' '__REPOSITORY_INSTALL_SUCCEEDED__'; "
                     "if [ \"$had_old\" -eq 1 ]; then "
                     "mount_targets=$(findmnt -rn -o TARGET) || "
                     "{ echo 'Could not list mounted filesystems; old repository was not removed' >&2; exit 8; }; "
                     "if ! printf '%s\\n' \"$mount_targets\" | "
                     "awk -v root=\"$old\" 'index($0, root \"/\") == 1 "
                     "{ print length($0) \"\\t\" $0 }' | sort -rn | cut -f2- | "
                     "while IFS= read -r mountpoint; do "
                     "[ -n \"$mountpoint\" ] || continue; "
                     "echo \"Unmounting old repository mount: $mountpoint\"; "
                     "if ! sudo umount -- \"$mountpoint\"; then "
                     "echo \"Normal unmount failed; detaching busy mount: $mountpoint\" >&2; "
                     "sudo umount --lazy -- \"$mountpoint\" || exit 1; "
                     "fi; "
                     "done; then "
                     "echo \"New repository installed, but old repository mounts could not be detached: $old\" >&2; "
                     "exit 8; fi; "
                     "rm -rf -- \"$old\" || "
                     "{ echo \"New repository installed, but old directory remains: $old\" >&2; exit 8; }; "
                     "fi; "
                     "else "
                     "if [ \"$had_old\" -eq 1 ]; then "
                     "mv -- \"$old\" \"$target\" || "
                     "{ echo 'Replacement failed and old directory could not be restored' >&2; exit 9; }; "
                     "fi; "
                     "rm -rf -- \"$stage\"; "
                     "echo 'Could not install the new repository' >&2; exit 10; "
                     "fi");
  
    const QString cleanupCommand =
      "; new_prefix=" + quoteShellArgument(encodedCustomerPrefix) +
      "; new_type=" + quoteShellArgument(repoType) +
      "; "
      "repo_prefix() { "
      "origin=$(git -C \"$1\" remote get-url origin) || return 1; "
      "name=${origin##*/}; "
      "name=${name%.git}; "
      "suffix=\"-$2\"; "
      "case \"$name\" in "
      "*\"$suffix\") prefix=${name%\"$suffix\"} ;; "
      "*) return 1 ;; "
      "esac; "
      "[ -n \"$prefix\" ] || return 1; "
      "printf '%s' \"$prefix\"; "
      "}; "
      "mismatched=''; "
      "if [ -n \"$new_prefix\" ]; then "
      "for other_type in SRC SandBox KiSoft-One; do "
      "[ \"$other_type\" = \"$new_type\" ] && continue; "
      "other=\"$parent/$other_type\"; "
      "if [ ! -e \"$other\" ] && [ ! -L \"$other\" ]; then continue; fi; "
      "if [ -L \"$other\" ] || [ ! -d \"$other\" ] || "
      "! git -C \"$other\" rev-parse --is-inside-work-tree >/dev/null 2>&1; then "
      "echo 'Cannot safely classify an existing repository; cleanup aborted' >&2; "
      "exit 12; "
      "fi; "
      "old_prefix=$(repo_prefix \"$other\" \"$other_type\") || { "
      "echo 'Cannot determine an existing repository customer prefix; cleanup aborted' >&2; "
      "exit 13; "
      "}; "
      "if [ \"$old_prefix\" != \"$new_prefix\" ]; then "
      "mismatched=\"$mismatched $other_type\"; "
      "fi; "
      "done; "
      "for other_type in $mismatched; do "
      "other=\"$parent/$other_type\"; "
      "rm -rf -- \"$other\" || { "
      "echo 'Could not remove a repository with a different customer prefix' >&2; "
      "exit 14; "
      "}; "
      "done; "
      "echo 'Repository installed; mismatched repositories removed'; "
      "else "
      "echo 'Repository installed; cleanup skipped because URL has no customer prefix' >&2; "
      "fi";

  command += cleanupCommand;

  executeCommandInternal(command, true);
}

void SshWorker::loadInstalledRepositoryOrigin() {
  if (m_session == nullptr || !m_connected || !m_loggedIn) {
    emit commandFailed("No authenticated SSH connections");
    return;
  }

  if (m_activeRepositoryType.isEmpty()) {
    emit commandFailed("Bitte erst ein Repository auswählen");
    return;
  }

  const QString repoPath = quoteShellArgument(repositoryPathForType(m_activeRepositoryType));

  const QString command = "git -C " + repoPath + " remote get-url origin";

  executeCommandInternal(command, false);
}

void SshWorker::loadInstalledRepositoryInfo() {
  if (m_session == nullptr || !m_connected || !m_loggedIn) {

    emit commandFailed("No authenticated SSH connection");
    return;
  }

  const QString candidates =
      quoteShellArgument(QDir(kRemoteBuildRoot).filePath("SRC")) + " " +
      quoteShellArgument(QDir(kRemoteBuildRoot).filePath("SandBox")) + " " +
      quoteShellArgument(QDir(kRemoteBuildRoot).filePath("KiSoft-One"));

const QString command = "found=0; "
                        "for repo in " + candidates + "; do "
                        "if git -C \"$repo\" rev-parse --is-inside-work-tree >/dev/null 2>&1; then "
                        "type=${repo##*/}; "
                        "origin=$(git -C \"$repo\" remote get-url origin) || exit 2; "
                        "branch=$(git -C \"$repo\" branch --show-current) || exit 3; "
                        "if [ -z \"$branch\" ]; then branch='(detached HEAD)'; fi; "
                        "printf '%s\\t%s\\t%s\\n' \"$type\" \"$origin\" \"$branch\"; "
                        "found=1; "
                        "fi; "
                        "done; "
                        "if [ \"$found\" -eq 0 ]; then "
                        "printf '%s\\n' '__NO_INSTALLED_REPOSITORY__'; "
                        "fi";
  
  executeCommandInternal(command, false);
}

void SshWorker::setActiveDirectory(const QString &type) {

  if (type.isEmpty()) {
    m_activeRepositoryType.clear();
    return;
  }

  if (repositoryPathForType(type).isEmpty()) {
    emit commandFailed("Repository type is not allowed");
    return;
  }

  m_activeRepositoryType = type;
}

void SshWorker::runOracleDataPump(bool restore) {
  const QString schemas = QStringLiteral("LU,AFCONFIG,ARCHIVE,KLASSX,KXGUI,KXREPORT,USER_MANAGEMENT");

  QString dataPumpCommand;

  if (restore) {

    dataPumpCommand = QStringLiteral("impdp BACKUPTOOL_USER@kxxl "
                                     "PARFILE=\"$objects_parfile\"");
  } else {
    dataPumpCommand = QStringLiteral("expdp BACKUPTOOL_USER@kxxl "
                                     "DIRECTORY=DATA_PUMP_DIR "
                                     "DUMPFILE=kisoft_schemas.dmp "
                                     "SCHEMAS=%1 "
                                     "NOLOGFILE=YES "
                                     "REUSE_DUMPFILES=YES")
                                     .arg(schemas);
  }

  if (!restore) {
    const QString dumpScript = QStringLiteral(
        "set -o pipefail\n"
        "validate_backup_login() {\n"
        "  local check_sql status\n"
        "  check_sql=$(mktemp /tmp/scriptexecutor-logincheck.XXXXXX.sql) || return 1\n"
        "  chmod 600 \"$check_sql\" || {\n"
        "    rm -f -- \"$check_sql\"\n"
        "    return 1\n"
        "  }\n"
        "  cat > \"$check_sql\" << 'SQL'\n"
        "WHENEVER SQLERROR EXIT SQL.SQLCODE\n"
        "SELECT 1 FROM dual;\n"
        "EXIT SUCCESS\n"
        "SQL\n"
        "  sqlplus -L BACKUPTOOL_USER@kxxl @\"$check_sql\"\n"
        "  status=$?\n"
        "  rm -f -- \"$check_sql\"\n"
        "  return \"$status\"\n"
        "}\n"
        "if ! validate_backup_login; then\n"
        "  echo 'BACKUPTOOL_USER-Anmeldung fehlgeschlagen. Dump abgebrochen.' >&2\n"
        "  exit 93\n"
        "fi\n"
        "%1\n")
        .arg(dataPumpCommand);

    const QString encodedDumpScript = QString::fromLatin1(dumpScript.toUtf8().toBase64());

    const QString oracleCommand = QStringLiteral("bash -lc \"$(printf '%s' %1 | base64 -d)\"")
                                                .arg(quoteShellArgument(encodedDumpScript));

    const QString command = QStringLiteral("sudo -iu oracle bash -lc %1")
                                .arg(quoteShellArgument(oracleCommand));

    executeCommandInternal(command, true);
    return;
  }

  const QString grantSql = QStringLiteral("WHENEVER SQLERROR EXIT SQL.SQLCODE\n"
                                         "ALTER SESSION SET CONTAINER = KXXL;\n"
                                         "GRANT CREATE ANY TRIGGER TO BACKUPTOOL_USER;\n"
                                         "EXIT\n");

  const QString schemaDependencyGrantSql = QStringLiteral("WHENEVER SQLERROR EXIT SQL.SQLCODE\n"
                                                          "ALTER SESSION SET CONTAINER = KXXL;\n"
                                                          "GRANT SELECT ON SYS.V_$SESSION TO AFCONFIG;\n"
                                                          "GRANT EXECUTE ON SYS.DBMS_AQ TO KLASSX;\n"
                                                          "GRANT EXECUTE ON SYS.DBMS_AQADM TO KLASSX;\n"
                                                          "GRANT EXECUTE ON SYS.DBMS_LOCK TO KLASSX;\n"
                                                          "GRANT EXECUTE ON SYS.DBMS_CRYPTO TO USER_MANAGEMENT;\n"
                                                          "GRANT SELECT ON SYS.DBA_FREE_SPACE TO KLASSX;\n"
                                                          "GRANT SELECT ON SYS.DBA_TEMP_FILES TO KLASSX;\n"
                                                          "GRANT SELECT ON SYS.DBA_DATA_FILES TO KLASSX;\n"
                                                          "GRANT SELECT ON SYS.DBA_OBJECTS TO KLASSX;\n"
                                                          "GRANT SELECT ON SYS.V_$LOCK TO KLASSX;\n"
                                                          "GRANT SELECT ON SYS.V_$PROCESS TO KLASSX;\n"
                                                          "GRANT SELECT ON SYS.V_$SESSION TO KLASSX;\n"
                                                          "GRANT SELECT ON SYS.V_$SESSION_WAIT TO KLASSX;\n"
                                                          "GRANT SELECT ON SYS.V_$SQLAREA TO KLASSX;\n"
                                                          "GRANT SELECT ON SYS.V_$TEMP_SPACE_HEADER TO KLASSX;\n"
                                                          "GRANT SELECT ON SYS.V_$TEMP_EXTENT_POOL TO KLASSX;\n"
                                                          "EXIT\n");

  const QString revokeSql = QStringLiteral("WHENEVER SQLERROR EXIT SQL.SQLCODE\n"
                                          "ALTER SESSION SET CONTAINER = KXXL;\n"
                                          "DECLARE\n"
                                          "  privilege_count PLS_INTEGER;\n"
                                          "BEGIN\n"
                                          "  SELECT COUNT(*) INTO privilege_count\n"
                                          "  FROM dba_sys_privs\n"
                                          "  WHERE grantee = 'BACKUPTOOL_USER'\n"
                                          "    AND privilege = 'CREATE ANY TRIGGER';\n"
                                          "  IF privilege_count > 0 THEN\n"
                                          "    EXECUTE IMMEDIATE "
                                          "'REVOKE CREATE ANY TRIGGER FROM BACKUPTOOL_USER';\n"
                                          "  END IF;\n"
                                          "\n"
                                          "  SELECT COUNT(*) INTO privilege_count\n"
                                          "  FROM dba_sys_privs\n"
                                          "  WHERE grantee = 'BACKUPTOOL_USER'\n"
                                          "    AND privilege = 'ADMINISTER DATABASE TRIGGER';\n"
                                          "  IF privilege_count > 0 THEN\n"
                                          "    EXECUTE IMMEDIATE "
                                          "'REVOKE ADMINISTER DATABASE TRIGGER FROM BACKUPTOOL_USER';\n"
                                          "  END IF;\n"
                                          "\n"
                                          "  SELECT COUNT(*) INTO privilege_count\n"
                                          "  FROM dba_sys_privs\n"
                                          "  WHERE grantee = 'KLASSX'\n"
                                          "    AND privilege = 'ADMINISTER DATABASE TRIGGER';\n"
                                          "  IF privilege_count > 0 THEN\n"
                                          "    EXECUTE IMMEDIATE "
                                          "'REVOKE ADMINISTER DATABASE TRIGGER FROM KLASSX';\n"
                                          "  END IF;\n"
                                          "END;\n"
                                          "/\n"
                                          "EXIT\n");

const QString resetSql = QStringLiteral("WHENEVER SQLERROR EXIT SQL.SQLCODE\n"
                                        "ALTER SESSION SET CONTAINER = KXXL;\n"
                                        "DECLARE\n"
                                        "  dump_exists BOOLEAN;\n"
                                        "  dump_length NUMBER;\n"
                                        "  block_size BINARY_INTEGER;\n"
                                        "  session_count PLS_INTEGER;\n"
                                        "\n"
                                        "  PROCEDURE lock_user_if_exists(p_username VARCHAR2) IS\n"
                                        "    user_count PLS_INTEGER;\n"
                                        "  BEGIN\n"
                                        "    SELECT COUNT(*) INTO user_count\n"
                                        "    FROM dba_users WHERE username = p_username;\n"
                                        "    IF user_count > 0 THEN\n"
                                        "      EXECUTE IMMEDIATE 'ALTER USER ' ||\n"
                                        "        DBMS_ASSERT.SIMPLE_SQL_NAME(p_username) || ' ACCOUNT LOCK';\n"
                                        "    END IF;\n"
                                        "  END;\n"
                                        "\n"
                                        "  PROCEDURE drop_user_if_exists(p_username VARCHAR2) IS\n"
                                        "    retry_count PLS_INTEGER := 0;\n"
                                        "  BEGIN\n"
                                        "    LOOP\n"
                                        "      BEGIN\n"
                                        "        EXECUTE IMMEDIATE 'DROP USER ' ||\n"
                                        "          DBMS_ASSERT.SIMPLE_SQL_NAME(p_username) || ' CASCADE';\n"
                                        "        RETURN;\n"
                                        "      EXCEPTION\n"
                                        "        WHEN OTHERS THEN\n"
                                        "          IF SQLCODE = -1918 THEN\n"
                                        "            RETURN;\n"
                                        "          ELSIF SQLCODE = -1940 THEN\n"
                                        "            retry_count := retry_count + 1;\n"
                                        "            IF retry_count >= 30 THEN\n"
                                        "              RAISE_APPLICATION_ERROR(-20003,\n"
                                        "                'User ' || p_username || ' ist nach 30 Versuchen noch verbunden', TRUE);\n"
                                        "            END IF;\n"
                                        "            FOR s IN (SELECT sid, serial# FROM v$session\n"
                                        "                      WHERE username = p_username) LOOP\n"
                                        "              BEGIN\n"
                                        "                EXECUTE IMMEDIATE 'ALTER SYSTEM KILL SESSION ''' ||\n"
                                        "                  s.sid || ',' || s.serial# || ''' IMMEDIATE';\n"
                                        "              EXCEPTION\n"
                                        "                WHEN OTHERS THEN\n"
                                        "                  IF SQLCODE NOT IN (-31, -26) THEN RAISE; END IF;\n"
                                        "              END;\n"
                                        "            END LOOP;\n"
                                        "            DBMS_LOCK.SLEEP(1);\n"
                                        "          ELSE\n"
                                        "            RAISE;\n"
                                        "          END IF;\n"
                                        "      END;\n"
                                        "    END LOOP;\n"
                                        "  END;\n"
                                        "BEGIN\n"
                                        "  UTL_FILE.FGETATTR('DATA_PUMP_DIR', 'kisoft_schemas.dmp',\n"
                                        "                    dump_exists, dump_length, block_size);\n"
                                        "  IF NOT dump_exists OR dump_length = 0 THEN\n"
                                        "    RAISE_APPLICATION_ERROR(-20001,\n"
                                        "      'Dump fehlt oder ist leer; Restore abgebrochen');\n"
                                        "  END IF;\n"
                                        "\n"
                                        "  lock_user_if_exists('AFCONFIG');\n"
                                        "  lock_user_if_exists('ARCHIVE');\n"
                                        "  lock_user_if_exists('KLASSX');\n"
                                        "  lock_user_if_exists('KXGUI');\n"
                                        "  lock_user_if_exists('KXREPORT');\n"
                                        "  lock_user_if_exists('USER_MANAGEMENT');\n"
                                        "  lock_user_if_exists('LU');\n"
                                        "\n"
                                        "  FOR s IN (\n"
                                        "    SELECT sid, serial# FROM v$session\n"
                                        "    WHERE username IN ('AFCONFIG','ARCHIVE','KLASSX','KXGUI',\n"
                                        "                       'KXREPORT','USER_MANAGEMENT','LU')\n"
                                        "  ) LOOP\n"
                                        "    BEGIN\n"
                                        "      EXECUTE IMMEDIATE 'ALTER SYSTEM KILL SESSION ''' ||\n"
                                        "        s.sid || ',' || s.serial# || ''' IMMEDIATE';\n"
                                        "    EXCEPTION\n"
                                        "      WHEN OTHERS THEN\n"
                                        "        IF SQLCODE NOT IN (-31, -26) THEN RAISE; END IF;\n"
                                        "    END;\n"
                                        "  END LOOP;\n"
                                        "\n"
                                        "  FOR attempt IN 1..30 LOOP\n"
                                        "    SELECT COUNT(*) INTO session_count FROM v$session\n"
                                        "    WHERE username IN ('AFCONFIG','ARCHIVE','KLASSX','KXGUI',\n"
                                        "                       'KXREPORT','USER_MANAGEMENT','LU');\n"
                                        "    EXIT WHEN session_count = 0;\n"
                                        "    DBMS_LOCK.SLEEP(1);\n"
                                        "  END LOOP;\n"
                                        "\n"
                                        "  SELECT COUNT(*) INTO session_count FROM v$session\n"
                                        "  WHERE username IN ('AFCONFIG','ARCHIVE','KLASSX','KXGUI',\n"
                                        "                     'KXREPORT','USER_MANAGEMENT','LU');\n"
                                        "  IF session_count > 0 THEN\n"
                                        "    RAISE_APPLICATION_ERROR(-20002,\n"
                                        "      'Schema-Reset abgebrochen: Es sind noch Datenbanksitzungen aktiv');\n"
                                        "  END IF;\n"
                                        "\n"
                                        "  drop_user_if_exists('AFCONFIG');\n"
                                        "  drop_user_if_exists('ARCHIVE');\n"
                                        "  drop_user_if_exists('KLASSX');\n"
                                        "  drop_user_if_exists('KXGUI');\n"
                                        "  drop_user_if_exists('KXREPORT');\n"
                                        "  drop_user_if_exists('USER_MANAGEMENT');\n"
                                        "  drop_user_if_exists('LU');\n"
                                        "END;\n"
                                        "/\n"
                                        "EXIT\n");

const QString postImportSql = QStringLiteral("WHENEVER SQLERROR EXIT SQL.SQLCODE\n"
                                            "ALTER SESSION SET CONTAINER = KXXL;\n"
                                            "GRANT ADMINISTER DATABASE TRIGGER TO KLASSX;\n"
                                            "CREATE OR REPLACE EDITIONABLE TRIGGER \"KLASSX\".\"T_LOGON\"\n"
                                            "  AFTER LOGON ON DATABASE WHEN (USER IN "
                                            "('ARCHIVE','KLASSX','KXGUI','KXREPORT','LU','USER_MANAGEMENT')) BEGIN\n"
                                            "  EXECUTE IMMEDIATE q'|ALTER SESSION SET PLSQL_WARNINGS =\n"
                                            "      'ENABLE:ALL'\n"
                                            "      ,'DISABLE:05003'\n"
                                            "      ,'DISABLE:05004'\n"
                                            "      ,'DISABLE:05005'\n"
                                            "      ,'DISABLE:05018'\n"
                                            "      ,'DISABLE:05019'\n"
                                            "      ,'DISABLE:06009'\n"
                                            "      ,'DISABLE:06010'\n"
                                            "      ,'DISABLE:06020'\n"
                                            "      ,'DISABLE:07202'\n"
                                            "      ,'DISABLE:07203'\n"
                                            "      ,'DISABLE:07204'\n"
                                            "      ,'DISABLE:07206'\n"
                                            "      ,'DISABLE:07207'\n"
                                            "      |';\n"
                                            "  EXECUTE IMMEDIATE 'ALTER SESSION SET RECYCLEBIN = OFF';\n"
                                            "  $IF dbms_db_version.ver_le_12_1 $THEN\n"
                                            "    EXECUTE IMMEDIATE 'ALTER SESSION SET plscope_settings = ''IDENTIFIERS:ALL''';\n"
                                            "  $ELSE\n"
                                            "    EXECUTE IMMEDIATE 'ALTER SESSION SET plscope_settings = ''IDENTIFIERS:ALL, STATEMENTS:NONE''';\n"
                                            "  $END\n"
                                            "END t_logon;\n"
                                            "/\n"
                                            "ALTER TRIGGER \"KLASSX\".\"T_LOGON\" ENABLE;\n"
                                            "REVOKE ADMINISTER DATABASE TRIGGER FROM KLASSX;\n"
                                            "BEGIN\n"
                                            "  DBMS_UTILITY.COMPILE_SCHEMA('LU', FALSE);\n"
                                            "  DBMS_UTILITY.COMPILE_SCHEMA('AFCONFIG', FALSE);\n"
                                            "  DBMS_UTILITY.COMPILE_SCHEMA('ARCHIVE', FALSE);\n"
                                            "  DBMS_UTILITY.COMPILE_SCHEMA('KLASSX', FALSE);\n"
                                            "  DBMS_UTILITY.COMPILE_SCHEMA('KXGUI', FALSE);\n"
                                            "  DBMS_UTILITY.COMPILE_SCHEMA('KXREPORT', FALSE);\n"
                                            "  DBMS_UTILITY.COMPILE_SCHEMA('USER_MANAGEMENT', FALSE);\n"
                                            "END;\n"
                                            "/\n"
                                            "DECLARE\n"
                                            "  invalid_count PLS_INTEGER;\n"
                                            "BEGIN\n"
                                            "  SELECT COUNT(*) INTO invalid_count\n"
                                            "  FROM dba_objects\n"
                                            "  WHERE status = 'INVALID'\n"
                                            "    AND owner IN ('LU','AFCONFIG','ARCHIVE','KLASSX','KXGUI',\n"
                                            "                  'KXREPORT','USER_MANAGEMENT');\n"
                                            "  IF invalid_count > 0 THEN\n"
                                            "    RAISE_APPLICATION_ERROR(-20004,\n"
                                            "      invalid_count || ' ungültige Objekte nach dem Import');\n"
                                            "  END IF;\n"
                                            "END;\n"
                                            "/\n"
                                            "ALTER USER LU ACCOUNT UNLOCK;\n"
                                            "ALTER USER AFCONFIG ACCOUNT UNLOCK;\n"
                                            "ALTER USER ARCHIVE ACCOUNT UNLOCK;\n"
                                            "ALTER USER KLASSX ACCOUNT UNLOCK;\n"
                                            "ALTER USER KXGUI ACCOUNT UNLOCK;\n"
                                            "ALTER USER KXREPORT ACCOUNT UNLOCK;\n"
                                            "ALTER USER USER_MANAGEMENT ACCOUNT UNLOCK;\n"
                                            "EXIT\n");

const QString validateImportedSchemasSql = QStringLiteral("WHENEVER SQLERROR EXIT SQL.SQLCODE\n"
                                                           "ALTER SESSION SET CONTAINER = KXXL;\n"
                                                           "DECLARE\n"
                                                           "  missing_count PLS_INTEGER;\n"
                                                           "BEGIN\n"
                                                           "  SELECT COUNT(*) INTO missing_count\n"
                                                           "  FROM (\n"
                                                           "    SELECT 'LU' username FROM dual UNION ALL\n"
                                                           "    SELECT 'AFCONFIG' FROM dual UNION ALL\n"
                                                           "    SELECT 'ARCHIVE' FROM dual UNION ALL\n"
                                                           "    SELECT 'KLASSX' FROM dual UNION ALL\n"
                                                           "    SELECT 'KXGUI' FROM dual UNION ALL\n"
                                                           "    SELECT 'KXREPORT' FROM dual UNION ALL\n"
                                                           "    SELECT 'USER_MANAGEMENT' FROM dual\n"
                                                           "  ) expected\n"
                                                           "  WHERE NOT EXISTS (\n"
                                                           "    SELECT 1 FROM dba_users actual\n"
                                                           "    WHERE actual.username = expected.username\n"
                                                           "  );\n"
                                                           "  IF missing_count > 0 THEN\n"
                                                           "    RAISE_APPLICATION_ERROR(-20005,\n"
                                                           "      missing_count || ' Schemas fehlen nach dem Import');\n"
                                                           "  END IF;\n"
                                                           "END;\n"
                                                           "/\n"
                                                           "EXIT\n");

const QString schemaUserImportParameterFile = QStringLiteral("DIRECTORY=DATA_PUMP_DIR\n"
                                                             "DUMPFILE=kisoft_schemas.dmp\n"
                                                             "SCHEMAS=%1\n"
                                                             "CONTENT=METADATA_ONLY\n"
                                                             "INCLUDE=USER\n")
                                                             .arg(schemas);
                                                             
const QString schemaObjectsImportParameterFile = QStringLiteral("DIRECTORY=DATA_PUMP_DIR\n"
                                                                "DUMPFILE=kisoft_schemas.dmp\n"
                                                                "SCHEMAS=%1\n"
                                                                "EXCLUDE=USER\n"
                                                                "EXCLUDE=TRIGGER:\"='T_LOGON'\"\n")
                                                                .arg(schemas);

const QString encodedGrantSql = QString::fromLatin1(grantSql.toUtf8().toBase64());
const QString encodedSchemaDependencyGrantSql = QString::fromLatin1(schemaDependencyGrantSql.toUtf8().toBase64());
const QString encodedRevokeSql = QString::fromLatin1(revokeSql.toUtf8().toBase64());
const QString encodeResetSql = QString::fromLatin1(resetSql.toUtf8().toBase64());
const QString encodePostImportSql = QString::fromLatin1(postImportSql.toUtf8().toBase64());
const QString encodeValidateImportedSchemasSql = QString::fromLatin1(validateImportedSchemasSql.toUtf8().toBase64());
const QString encodeSchemaUserImportParameterFile = QString::fromLatin1(schemaUserImportParameterFile.toUtf8().toBase64());
const QString encodeSchemaObjectsImportParameterFile = QString::fromLatin1(schemaObjectsImportParameterFile.toUtf8().toBase64());

const QString restoreScriptTemplate = QStringLiteral("set -o pipefail\n"
                                            "users_parfile=\"\"\n"
                                            "objects_parfile=\"\"\n"
                                            "restore_log_stamp=\"$(date +%Y%m%d_%H%M%S)\"\n"
                                            "grant_trigger_privileges() {\n"
                                            "  printf '%s' %1 | base64 -d | sqlplus -s / as sysdba\n"
                                            "}\n"
                                            "revoke_trigger_privileges() {\n"
                                            "  printf '%s' %2 | base64 -d | sqlplus -s / as sysdba\n"
                                            "}\n"
                                            "reset_schemas() {\n"
                                            "  printf '%s' %3 | base64 -d | sqlplus -s / as sysdba\n"
                                            "}\n"
                                            "post_import_setup() {\n"
                                            " printf '%s' %4 | base64 -d | sqlplus -s / as sysdba\n"                                           
                                            "}\n"
                                            "validate_imported_schemas() {\n"
                                            "  printf '%s' %9 | base64 -d | sqlplus -s / as sysdba\n"
                                            "}\n"
                                            "grant_schema_dependencies() {\n"
                                            "  printf '%s' %8 | base64 -d | sqlplus -s / as sysdba\n"
                                            "}\n"
                                            "cleanup() {\n"
                                            "  original_status=$?\n"
                                            "  trap - EXIT\n"
                                            "  if ! revoke_trigger_privileges; then\n"
                                            "    echo 'Fehler: Temporäre Trigger-Rechte konnten nicht entzogen werden.' >&2\n"
                                            "    exit 90\n"
                                            "  fi\n"
                                            "  if { [ -n \"$users_parfile\" ] && ! rm -f -- \"$users_parfile\"; } || "
                                            "{ [ -n \"$objects_parfile\" ] && ! rm -f -- \"$objects_parfile\"; }; then\n"
                                            "    echo 'Fehler: Temporäre Data-Pump-Parameterdatei konnte nicht entfernt werden.' >&2\n"
                                            "    exit 89\n"
                                            "  fi\n"
                                            "  exit \"$original_status\"\n"
                                            "}\n"
                                            "validate_backup_login() {\n"
                                            "local check_sql status\n"
                                            "check_sql=$(mktemp /tmp/scriptexecutor-logincheck.XXXXXX.sql) || return 1\n"
                                            "chmod 600 \"$check_sql\" || {\n"
                                            "rm -f -- \"$check_sql\"\n"
                                            "return 1\n"
                                            "}\n"
                                            "cat > \"$check_sql\" << 'SQL'\n"
                                            "WHENEVER SQLERROR EXIT SQL.SQLCODE\n"
                                            "SELECT 1 FROM dual;\n"
                                            "EXIT SUCCESS\n"
                                            "SQL\n"
                                            "sqlplus -L BACKUPTOOL_USER@kxxl @\"$check_sql\"\n"
                                            "status=$?\n"
                                            "rm -f -- \"$check_sql\"\n"
                                            "return \"$status\"\n"
                                            "}\n"
                                            "trap cleanup EXIT\n"
                                            "trap 'exit 129' HUP\n"
                                            "trap 'exit 130' INT\n"
                                            "trap 'exit 143' TERM\n"
                                            "echo '[Restore] Erteile temporäre Datenbankrechte ...'\n"
                                            "grant_trigger_privileges || exit $?\n"
                                            "echo '[Restore] Prüfe BACKUPTOOL_USER ...'\n"
                                            "if ! validate_backup_login; then\n"
                                            " echo 'BACKUPTOOL_USER-Anmeldung fehlgeschlagen. Restore vor dem Schema-Reset abgebrochen. ' >&2\n"
                                            " exit 93\n"
                                            "fi\n"
                                            "echo '[Restore] Beende aktive Schema-Sitzungen und setze die Zielschemas zurück ...'\n"
                                            "reset_schemas || { echo 'Schema-Reset fehlgeschlagen; Import abgebrochen.' >&2; exit 1; }\n"
                                            "echo '[Restore] Erzeuge Data-Pump-Parameterdateien ...'\n"
                                            "users_parfile=$(mktemp /tmp/scriptexecutor-restore-users.XXXXXX.par) || exit 92\n"
                                            "objects_parfile=$(mktemp /tmp/scriptexecutor-restore-objects.XXXXXX.par) || exit 92\n"
                                            "printf '%s' %5 | base64 -d > \"$users_parfile\" || exit 92\n"
                                            "printf '%s' %6 | base64 -d > \"$objects_parfile\" || exit 92\n"
                                            "printf 'LOGFILE=scriptexecutor_restore_users_%s.log\\nLOGTIME=ALL\\nMETRICS=YES\\n' "
                                            "\"$restore_log_stamp\" >> \"$users_parfile\" || exit 92\n"
                                            "printf 'LOGFILE=scriptexecutor_restore_objects_%s.log\\nLOGTIME=ALL\\nMETRICS=YES\\n' "
                                            "\"$restore_log_stamp\" >> \"$objects_parfile\" || exit 92\n"
                                            "echo \"[Restore] Data-Pump-Logs liegen in DATA_PUMP_DIR: "
                                            "scriptexecutor_restore_users_${restore_log_stamp}.log und "
                                            "scriptexecutor_restore_objects_${restore_log_stamp}.log\"\n"
                                            "echo '[Restore] Importiere Schema-Benutzer ...'\n"
                                            "impdp BACKUPTOOL_USER@kxxl PARFILE=\"$users_parfile\"\n"
                                            "users_import_status=$?\n"
                                            "if [ \"$users_import_status\" -ne 0 ]; then exit \"$users_import_status\"; fi\n"
                                            "echo '[Restore] Prüfe importierte Schemas ...'\n"
                                            "if ! validate_imported_schemas; then\n"
                                            "  echo 'Schema-User-Import unvollständig; weitere Import-Schritte werden übersprungen.' >&2\n"
                                            "  exit 92\n"
                                            "fi\n"
                                            "echo '[Restore] Erteile Abhängigkeiten und importiere Datenbankobjekte ...'\n"
                                            "grant_schema_dependencies || exit $?\n"
                                            "%7\n"
                                            "import_status=$?\n"
                                            "if [ \"$import_status\" -ne 0 ]; then exit \"$import_status\"; fi\n"
                                            "echo '[Restore] Erstelle Trigger und kompiliere die Schemas ...'\n"
                                            "post_import_setup || exit 91\n"
                                            "echo '[Restore] Wiederherstellung abgeschlossen.'\n"
                                            "exit \"$import_status\"\n")
                                            .arg(quoteShellArgument(encodedGrantSql))
                                            .arg(quoteShellArgument(encodedRevokeSql))
                                            .arg(quoteShellArgument(encodeResetSql))
                                            .arg(quoteShellArgument(encodePostImportSql))
                                            .arg(quoteShellArgument(encodeSchemaUserImportParameterFile))
                                            .arg(quoteShellArgument(encodeSchemaObjectsImportParameterFile))
                                            .arg(dataPumpCommand)
                                            .arg(quoteShellArgument(encodedSchemaDependencyGrantSql))
                                            .arg(quoteShellArgument(encodeValidateImportedSchemasSql));

const QString restoreScript = restoreScriptTemplate;
    
const QString encodedRestoreScript = QString::fromLatin1(restoreScript.toUtf8().toBase64());

const QString oracleCommand = QStringLiteral("bash -lc \"$(printf '%s' %1 | base64 -d)\"")
                                            .arg(quoteShellArgument(encodedRestoreScript));
const QString command = QStringLiteral("sudo -iu oracle bash -lc %1")
                                      .arg(quoteShellArgument(oracleCommand));
  
  executeCommandInternal(command, true, true);
}

void SshWorker::createOracleBackupUser() {
  const QString sql = QStringLiteral("SET ECHO OFF\n"
                                    "SET VERIFY OFF\n"
                                    "SET FEEDBACK OFF\n"
                                    "SET HEADING OFF\n"
                                    "SET PAGESIZE 0\n"
                                    "WHENEVER SQLERROR EXIT SQL.SQLCODE\n"
                                    "ALTER SESSION SET CONTAINER = KXXL;\n"
                                    "ACCEPT NEW_PASSWORD CHAR PROMPT "
                                    "'New password for BACKUPTOOL_USER: ' HIDE\n"
                                    "ACCEPT CONFIRM_PASSWORD CHAR PROMPT "
                                    "'Confirm password for BACKUPTOOL_USER: ' HIDE\n"
                                    "DECLARE\n"
                                    "  user_count PLS_INTEGER;\n"
                                    "BEGIN\n"
                                    "  IF '&&NEW_PASSWORD' != '&&CONFIRM_PASSWORD' THEN\n"
                                    "    RAISE_APPLICATION_ERROR(-20001, 'Passwords do not match');\n"
                                    "  END IF;\n"
                                    "\n"
                                    "  SELECT COUNT(*) INTO user_count\n"
                                    "  FROM dba_users\n"
                                    "  WHERE username = 'BACKUPTOOL_USER';\n"
                                    "\n"
                                    "  IF user_count = 0 THEN\n"
                                    "    EXECUTE IMMEDIATE\n"
                                    "      'CREATE USER BACKUPTOOL_USER IDENTIFIED BY \"' ||\n"
                                    "      '&&NEW_PASSWORD' || '\"';\n"
                                    "    EXECUTE IMMEDIATE 'GRANT CONNECT, RESOURCE TO BACKUPTOOL_USER';\n"
                                    "    EXECUTE IMMEDIATE 'GRANT DATAPUMP_EXP_FULL_DATABASE TO BACKUPTOOL_USER';\n"
                                    "    EXECUTE IMMEDIATE 'GRANT DATAPUMP_IMP_FULL_DATABASE TO BACKUPTOOL_USER';\n"
                                    "    EXECUTE IMMEDIATE 'GRANT READ, WRITE ON DIRECTORY DATA_PUMP_DIR TO BACKUPTOOL_USER';\n"
                                    "    EXECUTE IMMEDIATE 'GRANT UNLIMITED TABLESPACE TO BACKUPTOOL_USER';\n"
                                    "  END IF;\n"
                                    "END;\n"
                                    "/\n"
                                    "SELECT 'BACKUPTOOL_USER_CREATED' FROM dual;\n"
                                    "UNDEFINE NEW_PASSWORD\n"
                                    "UNDEFINE CONFIRM_PASSWORD\n"
                                    "EXIT\n");
  
  const QString command = QStringLiteral("script=$(mktemp /tmp/scriptexecutor-user.XXXXXX.sql) || exit 1; "
                                        "trap 'rm -f -- \"$script\"' EXIT HUP INT TERM; "
                                        "chmod 644 \"$script\" || exit 1; "
                                        "cat > \"$script\" <<'__ORACLE_SQL__'\n") 
                                        + sql 
                                        + QStringLiteral(
                                                        "__ORACLE_SQL__\n"
                                                        "sudo -iu oracle bash -lc \"sqlplus -s / as sysdba @$script\"");

    executeCommandInternal(command, true);        
}

void SshWorker::checkOracleBackupUser() {
  const QString sql =
      QStringLiteral("WHENEVER SQLERROR EXIT SQL.SQLCODE\n"
                     "ALTER SESSION SET CONTAINER = KXXL;\n"
                     "SELECT CASE WHEN EXISTS (\n"
                     "  SELECT 1 FROM dba_users "
                     "WHERE username = 'BACKUPTOOL_USER'\n"
                     ") THEN 'BACKUPTOOL_USER_EXISTS'\n"
                     "  ELSE 'BACKUPTOOL_USER_MISSING'\n"
                     "END FROM dual;\n"
                     "EXIT\n");

  const QString encodedSql =
      QString::fromLatin1(sql.toUtf8().toBase64());

  const QString oracleCommand =
      QStringLiteral("printf '%s' %1 | base64 -d | "
                     "sqlplus -s / as sysdba")
          .arg(quoteShellArgument(encodedSql));

  const QString command =
      QStringLiteral("sudo -iu oracle bash -lc %1")
          .arg(quoteShellArgument(oracleCommand));

  executeCommandInternal(command, true);
}