#include "SshWorker.hpp"

#include <QByteArray>
#include <QDebug>

#include <cstddef>

namespace {

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