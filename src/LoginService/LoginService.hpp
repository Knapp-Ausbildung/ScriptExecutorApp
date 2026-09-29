#pragma once

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <libssh/libssh.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qstring.h>

class LoginService : public QObject {
  Q_OBJECT

private:
  bool m_loggedIn = false; // Gerade logged in?
  ssh_session m_session = nullptr;

  bool authenticatePendingLogin();
  void failLogin(const QString &messsage);
  void cleanup();

  QString m_pendingPassword;
  bool m_connected = false;
  bool m_waitingForHostKeyConfirmation = false;

public:
  explicit LoginService(QObject *parent = nullptr);

  const bool getLoggedIn();

signals:
  void hostKeyConfirmationRequested(const QString &host, int port, 
                                  const QString &fingerprint);
  void loginFinished(bool success);

public slots:
  void login(const QString &ipAdress, 
             const QString &username,
             const QString &password, int port = 22);
  void confirmHostKey();
  void rejectedHostKey();
  void logout();
};