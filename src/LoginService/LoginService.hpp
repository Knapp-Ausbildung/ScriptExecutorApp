#pragma once

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <libssh/libssh.h>
#include <QObject>
#include <qtmetamacros.h>
#include <QString>
#include <QStringList>

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
  void prepareCommandRequested();
  void cancelCommandRequested();

  void executeCommandRequested(const QString &command);
  void commandCompleted(const QString &standardOutput,
                        const QString &standardError,
                        int exitStatus);
  void commandCancelled();
  void commandFailed(const QString &message);
  void executePresetCommandRequested(const QString &commandId);
  void commandInputRequested(const QString &prompt, bool secret);
  void commandInputSubmitted(const QString &input);

  void remoteBranchesRequested(const QString &url);
  void remoteBranchesFailed(const QString &message);
  void fetchRemoteBranchRequested(const QString &url,
                                  const QString &branch);
  void pullRemoteBranchRequested(const QString &url,
                                 const QString &branch);
  void replaceRepositoryRequested(const QString &url,
                                  const QString &branch);
  void installedRepositoryOriginRequested();

public slots:
  void login(const QString &host, 
             const QString &username,
             const QString &password, 
             int port = 22);
  void confirmHostKey();
  void rejectedHostKey();
  void logout();
  void cancelCommand();
  void executeCommand(const QString &message);
  void executePresetCommand(const QString &commandId);
  void submitCommandInput(const QString &input);

  void loadRemoteBranches(const QString &url);
  void fetchRemoteBranch(const QString &url,
                         const QString &branch);
  void pullRemoteBranch(const QString &url,
                        const QString &branch);
  void replaceRepository(const QString &url,
                         const QString &branch);
  void loadInstalledRepositoryOrigin();
private:
  QThread *m_thread = nullptr;
  SshWorker *m_worker = nullptr;
  bool m_loggedIn = false; // Gerade logged in?
};