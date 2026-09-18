#pragma once

#include <cstddef>
#include <qobject.h>
#include <qtmetamacros.h>
class LoginService : QObject {
  Q_OBJECT

private:
  bool m_loggedIn = false; // Gerade logged in?

public:
  explicit LoginService(QObject *parent = nullptr);

  const bool &getLoggedIn();

public slots:
  void login(const QString &ipAdress, const QString &password);
  void logout();
};