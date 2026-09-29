#pragma once

#include <QObject>
#include <QString>
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

    void executeCommand(const QString &command);
    void executePresetCommand(const QString &commandId);

signals:
    void hostKeyConfirmationRequested(const QString &host,
                                      int port,                              
                                      const QString &fingerprint);
    void loginFinished(bool success);
    void loggedOut();
    void commandCompleted(const QString &standardOutput,
                          const QString &standardError,
                          int exitStatus);
    void commandFailed(const QString &message);

private:
    bool authenticatePendingLogin();
    void failLogin(const QString &message);
    void cleanup();

    ssh_session m_session = nullptr;
    QString m_pendingPassword;
    bool m_loggedIn = false;
    bool m_connected = false;
    bool m_waitingForHostKeyConfirmation = false;
};