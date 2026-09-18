#include "AppController.hpp"
#include "src/LoginService/LoginService.hpp"

#include <QDebug>

AppController::AppController(QObject *parent) : QObject(parent) {
  m_loginService = new LoginService();
}

// Getter
bool AppController::isLoggedIn() const { return m_loginService->getLoggedIn(); }
QString AppController::getCurrentScreen() const { return m_currentScreen; }

void AppController::attemptLogin(const QString &ipAdress,
                                 const QString &password) {

  m_loginService->login(ipAdress, password);

  emit loggedInChanged();

  if (isLoggedIn()) {
    openDashboard();
  } else
    openLogin();
}

void AppController::attemptLogout() {
  m_loginService->logout();

  emit loggedInChanged();

  if (isLoggedIn()) {
    openDashboard();
  } else
    openLogin();
}

void AppController::openLogin() {
  if (!isLoggedIn()) {
    m_currentScreen = "login";
    emit currentScreenChanged();
  }
}

void AppController::openDashboard() {
  // Nur wenn eingeloggt ist, darf man zum Dashboard.
  if (isLoggedIn()) {
    m_currentScreen = "dashboard";
    emit currentScreenChanged();
  }
}