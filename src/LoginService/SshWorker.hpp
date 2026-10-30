#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QMutex>
#include <QByteArray>

#include <atomic>
#include <libssh/libssh.h>

class SshWorker : public QObject {
    Q_OBJECT

public:
    explicit SshWorker(QObject *parent = nullptr);
    ~SshWorker() override;

public slots:
    void login(const QString &host,
               const QString &username,
               const QString &password,
               int port);

    void confirmHostKey();
    void rejectHostKey();
    void logout();
    

    void prepareCommand();
    void cancelCommand();
    void executeCommand(const QString &command);
    void executePresetCommand(const QString &commandId);
    void submitCommandInput(const QString &input);

    void loadRemoteBranches(const QString &url);
    void fetchRemoteBranch(const QString &url,
                           const QString &branch);
    void pullRemoteBranch(const QString &url,
                          const QString &branch);
    void replaceRepository(const QString &url,
                           const QString &branch);
    void setActiveDirectory(const QString &type);

    void loadInstalledRepositoryOrigin();
    void loadInstalledRepositoryInfo();

    void runOracleDataPump(bool restore);
    void checkOracleBackupUser();
    void createOracleBackupUser();

signals:
    void hostKeyConfirmationRequested(const QString &host,
                                      int port,
                                      const QString &fingerprint,
                                      const QString &previousFingerprint,
                                      bool keyChanged);
    void loginFinished(bool success);
    void loggedOut();
    void commandCompleted(const QString &standardOutput,
                          const QString &standardError,
                          int exitStatus);
    void commandCancelled();
    void commandFailed(const QString &message);
    void commandInputRequested(const QString &prompt, bool secret);
    void commandOutputReceived(const QString &chunk);

private:
    bool authenticatePendingLogin();
    void failLogin(const QString &message);
    void cleanup();
    void checkConnection();
    
    bool validateGitRequest(const QString &urlText,
                            QString &normalizedUrl,
                            QString &error) const;    
                            
    bool validateGitRequest(const QString &urlText,
                            const QString &branchText,
                            QString &normalizedUrl,
                            QString &normalizedBranch,
                            QString &error) const;

    void executeCommandInternal(const QString &command, bool requestPty, bool continueOnOutputLimit = false);

    QTimer *m_keepAliveTimer = nullptr;
    int m_keepAliveFailures = 0;

    ssh_session m_session = nullptr;
    QString m_pendingPassword;
    QString m_pendingHost;
    int m_pendingPort = 22;
    bool m_loggedIn = false;
    bool m_connected = false;
    bool m_waitingForHostKeyConfirmation = false;
    bool m_pendingHostKeyChanged = false;
    std::atomic_bool m_cancelRequested = false;

    QMutex m_inputMutex;
    QByteArray m_pendingInput;
    bool m_inputPending = false;

    QString m_activeRepositoryType;
};