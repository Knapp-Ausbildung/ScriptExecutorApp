#include "LoginService.hpp"
#include "SshWorker.hpp"

#include <QMetaObject>
#include <QThread>
#include <Qt>

#include <cstdlib>
#include <cstddef>
#include <string>
#include <libssh/libssh.h>

#include <qdebug.h>
#include <qglobal.h>
#include <qobject.h>
#include <qstring.h>
#include <qbytearray.h>

LoginService::LoginService(QObject *parent) 
    : QObject(parent), 
      m_thread(new QThread(this)), 
      m_worker(new SshWorker) { 
  // Worker und libssh-Aufrufe laufen in einem eigenen Thread
  m_worker->moveToThread(m_thread);

  connect(m_thread, &QThread::finished, 
          m_worker, &QObject::deleteLater);
  
  connect(this, &LoginService::loginRequested,
          m_worker, &SshWorker::login, Qt::QueuedConnection);  
  connect(this, &LoginService::confirmHostKeyRequested,
          m_worker, &SshWorker::confirmHostKey, Qt::QueuedConnection);  
  connect(this, &LoginService::rejectHostKeyRequested,
          m_worker, &SshWorker::rejectHostKey, Qt::QueuedConnection);  
  connect(this, &LoginService::logoutRequested,
          m_worker, &SshWorker::logout, Qt::QueuedConnection); 
  connect(this, &LoginService::executeCommandRequested,
          m_worker, &SshWorker::executeCommand, Qt::QueuedConnection);
  connect(this, &LoginService::executePresetCommandRequested,
          m_worker, &SshWorker::executePresetCommand, Qt::QueuedConnection);

  // Ergebnisse vom Worker zurück an den LoginService / UI-Thread
  connect(m_worker, &SshWorker::hostKeyConfirmationRequested,
          this, &LoginService::hostKeyConfirmationRequested);  
  connect(m_worker, &SshWorker::loginFinished,
          this, [this] (bool success) {
            m_loggedIn = success;
            emit loginFinished(success);
          });
  connect(m_worker, &SshWorker::loggedOut,
          this, [this] {
          m_loggedIn = false;
    emit loggedOut();
  });
  connect(m_worker, &SshWorker::commandCompleted,
          this, &LoginService::commandCompleted);  
  connect(m_worker, &SshWorker::commandFailed,
          this, &LoginService::commandFailed);


  m_thread->start();
}

bool LoginService::getLoggedIn() const {
   return m_loggedIn; 
}

void LoginService::login(const QString &host, const QString &username,
                         const QString &password, int port) {

  // Placeholder: free login
  //     if (!m_loggedIn) {
  //     m_loggedIn = false;
  //   }
  emit loginRequested(host, username, password, port);
                      
}

void LoginService::confirmHostKey() {
  emit confirmHostKeyRequested();
}

void LoginService::rejectedHostKey() {
  emit rejectHostKeyRequested();
}

void LoginService::logout() {
  emit logoutRequested();
}

void LoginService::executeCommand(const QString &command) {
  emit executeCommandRequested(command);
}
LoginService::~LoginService() {
  m_thread->quit();
  m_thread->wait();
}

void LoginService::executePresetCommand(const QString &commandId) {
  emit executePresetCommandRequested(commandId);
}