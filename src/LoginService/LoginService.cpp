#include "LoginService.hpp"

LoginService::LoginService(QObject *parent) {}

const bool &LoginService::getLoggedIn() { return m_loggedIn; }

void LoginService::login(const QString &ipAdress, const QString &password) {
  // To Do: Server connection aufbauen probieren, wenn funktioniert dann
  // m_loggedin auf true setzen und loggedInChanged rufen damit die UI sich
  // ändert -> neue LoginData Instanz erstellen und befüllen

  // Placeholder: free login

  if (!m_loggedIn) {
    m_loggedIn = true;
  }
}

void LoginService::logout() {
  if (m_loggedIn) {
    m_loggedIn = false;
    
  }
}