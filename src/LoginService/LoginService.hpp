#pragma once

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <libssh/libssh.h>
#include <qobject.h>
#include <qtmetamacros.h>
#include <qstring.h>

class QThread;
class SshWorker;

class LoginService : public QObject {
  Q_OBJECT

public:
  explicit LoginService(QObject *parent = nullptr);
  ~LoginService() override;

  bool getLoggedIn() const;

signals:
  void hostKeyConfirmationRequested(const QString &host, int port, 
                                  const QString &fingerprint);
  void loginFinished(bool success);
  void loggedOut();

  void loginRequested(const QString &host, const QString &username,
                         const QString &password, int port);
  
  void confirmHostKeyRequested();
  void rejectHostKeyRequested();
  void logoutRequested();

  void executeCommandRequested(const QString &command);
  void commandCompleted(const QString &standardOutput,
                        const QString &standardError,
                        int exitStatus);
  void commandFailed(const QString &message);
  void executePresetCommandRequested(const QString &commandId);

public slots:
  void login(const QString &host, 
             const QString &username,
             const QString &password, 
             int port = 22);
  void confirmHostKey();
  void rejectedHostKey();
  void logout();
  void executeCommand(const QString &message);
  void executePresetCommand(const QString &commandId);
private:
  QThread *m_thread = nullptr;
  SshWorker *m_worker = nullptr;
  bool m_loggedIn = false; // Gerade logged in?
};