#include "AppController.hpp"
#include "src/LoginService/LoginService.hpp"

#include <QDebug>
#include <qglobal.h>

AppController::AppController(QObject *parent) : QObject(parent), m_loginService(new LoginService(this)) {
  connect(m_loginService, &LoginService::hostKeyConfirmationRequested, this, &AppController::hostKeyConfirmationRequested);

  // Verbinden des AppControllers mit dem LoginService und auf entsprechende Seite wechseln
  connect(m_loginService, &LoginService::loginFinished, this, [this](bool success) {
    emit loggedInChanged();

    if(success) {
      openDashboard();
    } else {
      openLogin();
      emit loginFailed();
    }
  });
}

// Getter
bool AppController::isLoggedIn() const { return m_loginService->getLoggedIn(); }

QString AppController::getCurrentScreen() const { return m_currentScreen; }

void AppController::attemptLogin(const QString &ipAdress,
                                 const QString &username,
                                 const QString &password) {

  m_loginService->login(ipAdress, username, password);
                                 }
void AppController::confirmHostKey() {
  m_loginService->confirmHostKey();
}
void AppController::rejectHostKey(){
  m_loginService->rejectedHostKey();
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