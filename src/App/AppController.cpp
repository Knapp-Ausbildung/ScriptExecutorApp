#include "AppController.hpp"
#include "src/LoginService/LoginService.hpp"

#include <QDebug>
#include <qglobal.h>

AppController::AppController(QObject *parent) 
  : QObject(parent), 
  m_loginService(new LoginService(this)) {
  connect(m_loginService, 
          &LoginService::hostKeyConfirmationRequested, 
          this, 
          &AppController::hostKeyConfirmationRequested);

  // Verbinden des AppControllers mit dem LoginService und auf entsprechende Seite wechseln
  connect(m_loginService, 
          &LoginService::loginFinished, 
          this, 
          [this](bool success) {
    emit loggedInChanged();

    if(success) {
      openDashboard();
    } else {
      openLogin();
      emit loginFailed();
    }
  });

  connect(m_loginService, 
          &LoginService::loggedOut, 
          this, 
          [this] {
    emit loggedInChanged();
    openLogin();
  });

  connect(m_loginService, &LoginService::commandCompleted,
          this, &AppController::commandCompleted);
          
  connect(m_loginService, &LoginService::commandFailed, 
          this, &AppController::commandFailed);
  connect(m_loginService, &LoginService::commandCancelled,
          this, &AppController::commandCancelled);
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

void AppController::executeCommand(const QString &command) {
  m_loginService->executeCommand(command);
}
// Für Preset-Buttons zum Ausführen von Befehlen
void AppController::executePresetCommand(const QString &commandId) {
  m_loginService->executePresetCommand(commandId);
}

void AppController::cancelCommand() {
  m_loginService->cancelCommand();
}