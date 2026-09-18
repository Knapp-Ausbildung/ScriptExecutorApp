#pragma once

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <libssh/libssh.h>
#include <qobject.h>
#include <qtmetamacros.h>

class LoginService : QObject {
  Q_OBJECT

private:
  bool m_loggedIn = false; // Gerade logged in?
  ssh_session m_session;

  void ssh_error_exit(ssh_session session, const std::string &msg); // Helper

public:
  explicit LoginService(QObject *parent = nullptr);

  const bool getLoggedIn();

public slots:
  void login(const QString &ipAdress, const QString &username,
             const QString &password, int port = 22);
  void logout();
};