#include "LoginService.hpp"
#include <cstdlib>
#include <libssh/libssh.h>
#include <qdebug.h>
#include <qglobal.h>
#include <qobject.h>
#include <string>

// Hilfsfunktion für Fehlerausgabe
void LoginService::ssh_error_exit(ssh_session session, const std::string &msg) {

  qDebug() << QString::fromStdString(msg) << ": " << ssh_get_error(session);
  m_loggedIn = false;
  if (session)
    ssh_free(session);
}

LoginService::LoginService(QObject *parent) : QObject(parent) {}

const bool LoginService::getLoggedIn() { return m_loggedIn; }

void LoginService::login(const QString &ipAdress, const QString &username,
                         const QString &password, int port) {

  // Placeholder: free login
  //     if (!m_loggedIn) {
  //     m_loggedIn = false;
  //   }

  // session erstellen
  m_session = ssh_new();

  if (!m_session) {
    qDebug() << "ERROR: SSH Session could not be initiated!";
    return;
  }

  // Attribute setzen
  ssh_options_set(m_session, SSH_OPTIONS_HOST, ipAdress.toStdString().c_str());
  ssh_options_set(m_session, SSH_OPTIONS_USER, username.toStdString().c_str());
  ssh_options_set(m_session, SSH_OPTIONS_PORT, &port);

  if (ssh_connect(m_session) != SSH_OK) {
    ssh_error_exit(m_session, "CONNECTION ERROR");
    return;
  }

  //   if (ssh_session_is_known_server(m_session) != SSH_KNOWN_HOSTS_OK) {
  //     std::cerr << "Wrning: Host-Key not known oder changed!\n";
  //     return;
  //   }

  if (ssh_userauth_password(m_session, nullptr,
                            password.toStdString().c_str()) !=
      SSH_AUTH_SUCCESS) {
    ssh_error_exit(m_session, "AUTHENTICATION ERROR");
    return;
  }

  qDebug() << "Connection successfully established!";
  m_loggedIn = true;
}

void LoginService::logout() {

  if (m_session) {
    ssh_disconnect(m_session);
    ssh_free(m_session);
    m_session = nullptr;
  }
  m_loggedIn = false;
}
