#include "AppController.hpp"
#include "src/LoginService/LoginService.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <qglobal.h>
#include <QDesktopServices>
#include <QUrl>
#include <QVariantMap>
#include <QVariantList>

namespace {
  QString removeAnsiSequence(QString text) {
    static const QRegularExpression ansiSequence(R"(\x1B(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1B\\)))");
    
    text.remove(ansiSequence);
    text.remove(QChar(0x18)); // Entfernt ESC-Zeichen
    return text;
  }

  QString repositoryTypeFromUrl(const QString &urlText) {
    const QString name = QUrl(urlText).path(QUrl::FullyDecoded).section('/', -1);

    for (const QString &type : {QStringLiteral("KiSoft-One"), 
                                QStringLiteral("SandBox"), 
                                QStringLiteral("SRC")}) {
      if (name == type || name.endsWith(QStringLiteral("-") + type)) {
        return type;
      }
    }
    return {};
  }
}

AppController::AppController(QObject *parent) 
  : QObject(parent), 
  m_loginService(new LoginService(this)) {
  connect(m_loginService, &LoginService::hostKeyConfirmationRequested, 
          this, &AppController::hostKeyConfirmationRequested);

  // Verbinden des AppControllers mit dem LoginService und auf entsprechende Seite wechseln
  connect(m_loginService, &LoginService::loginFinished, 
          this, [this](bool success) {
    emit loggedInChanged();

    if(success) {
      openDashboard();
      loadInstalledRepositoryInfo();
    } else {
      openLogin();
      emit loginFailed();
    }
  });

  connect(m_loginService, &LoginService::loggedOut, 
          this, [this] {
    emit loggedInChanged();
    openLogin();
  });


  connect(m_loginService, &LoginService::commandCompleted,
          this, [this] (const QString &standardOutput,
                        const QString &standardError,
                        int exitStatus) {
    if (m_checkingOracleBackupUser) {
      m_checkingOracleBackupUser = false;

      if (exitStatus != 0) {
        emit commandFailed(standardError.trimmed().isEmpty()
                           ? QStringLiteral("Oracle-Benutzerprüfung fehlgeschlagen")
                           : standardError.trimmed());
        return;
      }

      if (standardOutput.contains(QStringLiteral("BACKUPTOOL_USER_EXISTS"))) {

        emit commandOutputReceived(
    m_pendingOracleRestore
        ? QStringLiteral("[Debug] Benutzerprüfung erfolgreich; starte impdp.\n")
        : QStringLiteral("[Debug] Benutzerprüfung erfolgreich; starte expdp.\n"));
        m_loginService->runOracleDataPump(m_pendingOracleRestore);
        return;
      }

      if (standardOutput.contains(QStringLiteral("BACKUPTOOL_USER_MISSING"))) {
        m_creatingOracleBackupUser = true;
        m_loginService->createOracleBackupUser();
        return;
      }

    emit commandFailed(QStringLiteral("Oracle-Prüfung: Exit %1\nSTDOUT:\n%2\nSTDERR:\n%3")
                      .arg(exitStatus)
                      .arg(standardOutput.trimmed())
                      .arg(standardError.trimmed()));
    return;
}

if (m_creatingOracleBackupUser) {
  m_creatingOracleBackupUser = false;

  if (exitStatus != 0) {
    emit commandFailed(standardError.trimmed().isEmpty()
                       ? QStringLiteral("BACKUPTOOL_USER konnte nicht angelegt werden")
                       : standardError.trimmed());
    return;
  }

  if (!standardOutput.contains(QStringLiteral("BACKUPTOOL_USER_CREATED"))) {
    emit commandFailed(QStringLiteral("Die Benutzeranlage lieferte kein erwartetes Ergebnis"));
    return;
  }

  m_loginService->runOracleDataPump(m_pendingOracleRestore);
  return;
}

    if (m_loadingInstalledRepositoryOrigin) {
      m_loadingInstalledRepositoryOrigin = false;

      if (exitStatus != 0) {
        const QString message = standardError.trimmed().isEmpty()
                    ? QStringLiteral("Origin-URL konnte nicht gelesen werden")
                    : standardError.trimmed();
        emit remoteBranchesFailed(message);
        return;
      }

      const QString originUrl = standardOutput.trimmed();

      if (originUrl.isEmpty()) {
        emit remoteBranchesFailed("Das Repository hat keine Origin-URL");
        return;
      }

      QUrl origin(originUrl);
      origin.setUserInfo(QString());

      const QString cleanOrigin = origin.toString(QUrl::FullyEncoded);
      loadRemoteBranches(cleanOrigin);
      return;
    } 

    if (m_loadingInstalledRepositoryInfo) {
      m_loadingInstalledRepositoryInfo = false;

      if (exitStatus != 0) {
        emit commandFailed(standardError.trimmed().isEmpty() 
                           ? QStringLiteral("Installierte Repositories konnten nicht geladen werden")
                           : standardError.trimmed()); 
        return;              
      }

      const QString output = standardOutput.trimmed();

      if (output == QStringLiteral("__NO_INSTALLED_REPOSITORY__")) {
        m_installedRepositories.clear();
        m_activeRepositoryType.clear();
        m_installedRepository.clear();
        m_installedBranch.clear();
        m_installedRepositoryInfoLoaded = true;
        m_loginService->setActiveDirectory(QString());
        emit installedRepositoryChanged();
        m_repositoryTypeToActivateAfterReplace.clear();
        return;
    }

    QVariantList repositories;

    for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
      const QStringList fields = line.split('\t');

      if (fields.size() != 3) {
        m_loadingInstalledRepositoryInfo = false;
        emit commandFailed(QStringLiteral("Ungültiges Format der Repository Informationen"));
        return;
      }

      QVariantMap repository;
      QUrl origin(fields.at(1).trimmed());
      origin.setUserInfo(QString());

      repository.insert(QStringLiteral("type"), fields.at(0));
      repository.insert(QStringLiteral("origin"), origin.toString(QUrl::FullyEncoded));
      repository.insert(QStringLiteral("branch"), fields.at(2));
      repositories.append(repository);
    }

    m_installedRepositories = repositories;
    m_installedRepositoryInfoLoaded = true;

    if (!m_repositoryTypeToActivateAfterReplace.isEmpty()) {
      const QString type = m_repositoryTypeToActivateAfterReplace;
      m_repositoryTypeToActivateAfterReplace.clear();

      for (const QVariant &value : repositories) {
        const QVariantMap repository = value.toMap();

         if (repository.value(QStringLiteral("type")).toString() == type) {
          selectInstallRepository(type);
          return;
         }
      }
    }

    bool activeRepositoryStillInstalled = false;

    for (const QVariant &value : repositories) {
      const QVariantMap repository = value.toMap();

      if (repository.value(QStringLiteral("type")).toString() == m_activeRepositoryType) {
        activeRepositoryStillInstalled = true;
        break;
      }
    }

    if (repositories.size() == 1) {
      const QVariantMap repository = repositories.first().toMap();
      selectInstallRepository(repository.value(QStringLiteral("type")).toString());
    } else if (repositories.size() > 1 ) {
      if (activeRepositoryStillInstalled) {
        selectInstallRepository(m_activeRepositoryType);
      } else {
        m_activeRepositoryType.clear();
        m_installedRepository.clear();
        m_installedBranch.clear();
        m_loginService->setActiveDirectory(QString());
      }
    }

    emit installedRepositoryChanged();
    return;
  }
    if(!m_loadingRemoteBranches) {
      writeCommandLog(standardOutput, standardError, exitStatus);
      emit commandCompleted(standardOutput, standardError, exitStatus);
      if (m_refreshInstalledRepositoryAfterCommand) {
        if(!m_repositoryTypeToActivateAfterReplace.isEmpty() && 
           !standardOutput.contains(QStringLiteral("__REPOSITORY_INSTALL_SUCCEEDED__"))) {
          m_repositoryTypeToActivateAfterReplace.clear();
        }
        m_refreshInstalledRepositoryAfterCommand = false;
        loadInstalledRepositoryInfo();
      }
      return;
    }
    
    m_loadingRemoteBranches = false;

    if (exitStatus != 0) {
    const QString message = standardError.trimmed().isEmpty()
      ? QStringLiteral("Branches konnten nicht geladen werden")
      : standardError.trimmed();
    emit remoteBranchesFailed(message);
    return;
    }

    QStringList branches;
    const QString prefix = QStringLiteral("refs/heads/");

    for (const QString &line : standardOutput.split('\n', Qt::SkipEmptyParts)) {
      const qsizetype tabPosition = line.indexOf('\t');
      if (tabPosition < 0) {
        continue;
      }
      
      const QString ref = line.mid(tabPosition + 1).trimmed();
      if (ref.startsWith(prefix)) {
        branches.append(ref.mid(prefix.size()));
      }
    }

    branches.removeDuplicates();

    if (branches.isEmpty()) {
      emit remoteBranchesFailed("Das Remote-Repository enthält keine Branches");
      return;
    }

    m_availableBranches = branches;
    emit remoteBranchesLoaded(branches);
  });

  connect(m_loginService, &LoginService::commandFailed, 
          this, [this](const QString &message) {
      m_repositoryTypeToActivateAfterReplace.clear();

      m_checkingOracleBackupUser = false;
      m_creatingOracleBackupUser = false;

      if (m_loadingInstalledRepositoryInfo) {
        m_loadingInstalledRepositoryInfo = false;
        m_refreshInstalledRepositoryAfterCommand = false;
        emit commandFailed(message);
        return;
      }

      m_refreshInstalledRepositoryAfterCommand = false;

      if (m_loadingInstalledRepositoryOrigin) {
        m_loadingInstalledRepositoryOrigin = false;
        emit remoteBranchesFailed(message);
        return;
      }
      
      if (m_loadingRemoteBranches) {
        m_loadingRemoteBranches = false;
      emit remoteBranchesFailed(message);
      return;
    }

    emit commandFailed(message);
  });

  connect(m_loginService, &LoginService::commandCancelled,
          this, [this]() {

    m_checkingOracleBackupUser = false;
    m_creatingOracleBackupUser = false;
    
    m_repositoryTypeToActivateAfterReplace.clear();

    if (m_loadingInstalledRepositoryInfo) {
        m_loadingInstalledRepositoryInfo = false;
        m_refreshInstalledRepositoryAfterCommand = false;
        emit commandCancelled();
        return;
    }

    m_refreshInstalledRepositoryAfterCommand = false;

    if(m_loadingInstalledRepositoryOrigin) {
      m_loadingInstalledRepositoryOrigin = false;
      emit remoteBranchesFailed("Origin-Abfrage wurde abgebrochen");
      return;
    }
    
    if(m_loadingRemoteBranches) {
      m_loadingRemoteBranches = false;
      emit remoteBranchesFailed("Branch-Abfrage wurde abgebrochen");
      return;
    }

    emit commandCancelled();
  });

  connect(m_loginService, &LoginService::commandInputRequested,
          this, &AppController::commandInputRequested);
  
  connect(m_loginService, &LoginService::commandOutputReceived,
          this, [this](const QString &chunk) {
            if(m_loadingInstalledRepositoryInfo) {
              return;
            }
            
            emit commandOutputReceived(chunk);
  });

}
// Getter
bool AppController::isLoggedIn() const { 
  return m_loginService->getLoggedIn(); 
}

QString AppController::getCurrentScreen() const { 
  return m_currentScreen; 
}

QString AppController::installedRepository() const {
  return m_installedRepository;
}

QString AppController::installedBranch() const {
  return m_installedBranch;
}

bool AppController::installedRepositoryInfoLoaded() const {
  return m_installedRepositoryInfoLoaded;
}

QVariantList AppController::installedRepositories() const {
  return m_installedRepositories;
}

QString AppController::activeRepositoryType() const {
  return m_activeRepositoryType;

}
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
  if (!isKiSoftOneActive()) {
    emit commandFailed(QStringLiteral("Diese Aktion ist nur mit aktivem KiSoft-One Repository verfügbar"));
    return;
  }
  m_loginService->executeCommand(command);
}
// Für Preset-Buttons zum Ausführen von Befehlen
void AppController::executePresetCommand(const QString &commandId) {
  if (!isKiSoftOneActive()) {
    emit commandFailed(QStringLiteral("Diese Aktion ist nur mit aktivem KiSoft-One Repository verfügbar"));
    return;
  }
  m_loginService->executePresetCommand(commandId);
}

void AppController::cancelCommand() {
  m_loginService->cancelCommand();
}

void AppController::submitCommandInput(const QString &input) {
  m_loginService->submitCommandInput(input);
}

void AppController::writeCommandLog(const QString &standardOutput,
                                    const QString &standardError,
                                    int exitStatus) const {
  const QString logsPath = QDir::current().filePath("logs");

  QDir logsDir;

  if(!logsDir.mkpath(logsPath)) {
    qWarning() << "Log-Ordner konnte nicht erstellt werden: " << logsPath;
    return;
  }

  const bool needsArchive = exitStatus != 0;
  const QString archivePath = QDir(logsPath).filePath("archive");

  if (needsArchive && !logsDir.mkpath(archivePath)) {
    qWarning() << "Archiv-Ordner konnte nicht erstellt werden: " << archivePath;
    return;
  }

  const QString timestamp = QDateTime::currentDateTime().toString("dd-MM-yyyy-HH-mm-ss");
  const QString baseName = QString("%1_exit_%2").arg(timestamp).arg(exitStatus);

  const QString logFilePath = QDir(logsPath).filePath("latest.log");
  
  // If there are mutliple commands at the same time they are getting numbered

  QString archivedFilePath;

  if (needsArchive) {
    archivedFilePath = QDir(archivePath).filePath(baseName +".log");
  int number = 2;
  while (QFile::exists(archivedFilePath)) {
    archivedFilePath = QDir(archivePath).filePath(QString("%1_%2.log").arg(baseName).arg(number));
    ++number;
    }
  }

  QFile file(logFilePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Logdatei konnte nicht geschrieben werden: " << logFilePath << file.errorString();
    return;
  }
  
  const QString cleanStandardOutput = removeAnsiSequence(standardOutput);
  const QString cleanStandardError = removeAnsiSequence(standardError);

  QTextStream log(&file);
  log << "Zeit: " << timestamp << "\n";
  log << "Exit-Code: " << exitStatus << "\n\n";
  log << "Standardausgabe: \n" << cleanStandardOutput << "\n";
  log << "Fehlerausgabe: \n" << cleanStandardError << "\n";
  file.close();

  if (needsArchive && !QFile::copy(logFilePath, archivedFilePath)) {
    qWarning() << "Logdatei konnte nicht archiviert werden: " << archivedFilePath;
  }
}

void AppController::openLogsFolder() {
  const QString logsPath = QDir::current().absoluteFilePath("logs");

  if(!QDir().mkpath(logsPath)) {
    qWarning() << "Log-Ordner konnte nicht erstellt werden: " << logsPath;
    return;
  }

  if (!QDesktopServices::openUrl(QUrl::fromLocalFile(logsPath))) {
    qWarning() << "Log-Ordner konnte nicht geöffnet werden: " << logsPath;
  }
}

bool AppController::sendRepoLink(const QString &repoLink) {
  const QUrl url(repoLink.trimmed());

  if(!url.isValid() ||
     url.scheme() != QStringLiteral("https") ||
     url.host().isEmpty() ||
     !url.userInfo().isEmpty()) {
      emit commandFailed("Bitte einen gültigen Repository-Link einfügen!");
      return false;
     }

  m_selectedRepoUrl = url.toString(QUrl::FullyEncoded);
  return true;
  }

  void AppController::loadRemoteBranches(const QString &url) {
    const QUrl remote(url.trimmed());

    if (!remote.isValid() ||
        remote.scheme() != QStringLiteral("https") ||
        remote.host().isEmpty() ||
        !remote.userInfo().isEmpty()) {
          emit remoteBranchesFailed("Bitte einen gültigen Repository-Link verwenden");
          return;
        }

    if (m_loadingRemoteBranches) {
      emit remoteBranchesFailed("Branch-Abfrage läuft bereits");
      return;
    }

    m_selectedRepoUrl = remote.toString(QUrl::FullyEncoded);
    m_loadingRemoteBranches = true;
    m_loginService->loadRemoteBranches(m_selectedRepoUrl);

  }

  bool AppController::selectRemoteBranch(const QString &branch) {
    const QString selected = branch.trimmed();

    if (selected.isEmpty() || !m_availableBranches.contains(selected)) {
      return false;
    }

    m_selectedBranch = selected;
    return true;
  }

  void AppController::fetchSelectedBranch() {
    if (!isKiSoftOneActive()) {
    emit commandFailed(QStringLiteral("Diese Aktion ist nur mit aktivem KiSoft-One Repository verfügbar"));
    return;
  }

    if (!m_installedRepositoryInfoLoaded ||
        m_installedRepository.isEmpty() ||
        m_installedBranch.isEmpty()) {
          emit commandFailed("Bitte erst Repository und Branch auswählen");
          return;
        }
    
        m_loginService->fetchRemoteBranch(m_installedRepository, m_installedBranch);
  }

  void AppController::pullSelectedBranch() {
    if (!isKiSoftOneActive()) {
      emit commandFailed(QStringLiteral("Diese Aktion ist nur mit aktivem KiSoft-One Repository verfügbar"));
      return;
    }
    if (!m_installedRepositoryInfoLoaded || 
        m_installedRepository.isEmpty() ||
        m_installedBranch.isEmpty()) {
      emit commandFailed("Bitte erst Repository und Branch auswählen");
      return;
    }

    m_refreshInstalledRepositoryAfterCommand = true;
    m_loginService->pullRemoteBranch(m_installedRepository, m_installedBranch);
  }

  void AppController::replaceSelectedRepository() {
    if (m_selectedRepoUrl.isEmpty() || m_selectedBranch.isEmpty()) {
      emit commandFailed("Bitte erst Repository und Branch auswählen");
      return;
    }

    m_repositoryTypeToActivateAfterReplace = repositoryTypeFromUrl(m_selectedRepoUrl);

    if (m_repositoryTypeToActivateAfterReplace.isEmpty()) {
      emit commandFailed("Repository-Typ aus der URL konnte nicht ermittelt werden");
      return;
    }

    m_refreshInstalledRepositoryAfterCommand = true;
    m_loginService->replaceRepository(m_selectedRepoUrl, m_selectedBranch);
  }

  void AppController::loadInstalledRepositoryBranches() {
    if (!isKiSoftOneActive()) {
      emit commandFailed(QStringLiteral("Diese Aktion ist nur mit aktivem KiSoft-One Repository verfügbar"));
      return;
    }
    
    if(!m_installedRepositoryInfoLoaded || m_installedRepository.isEmpty()) {
      emit remoteBranchesFailed(QStringLiteral("Bitte erst Repository auswählen!"));
      return;
    }

    if (m_loadingRemoteBranches || m_loadingInstalledRepositoryOrigin) {
      emit remoteBranchesFailed("Branch-Abfrage läuft bereits");
      return;
    }

    loadRemoteBranches(m_installedRepository);
  }

  void AppController::loadInstalledRepositoryInfo() {
    if (m_loadingInstalledRepositoryInfo) {
      return;
    }

    m_installedRepositoryInfoLoaded = false;
    m_loadingInstalledRepositoryInfo = true;
    m_loginService->loadInstalledRepositoryInfo();
  }

void AppController::setActiveDirectory(const QString &type) {
  selectInstallRepository(type);
}

void AppController::selectInstallRepository(const QString &type) {
  for (const QVariant &value : m_installedRepositories) {
    const QVariantMap repository = value.toMap();

    if (repository.value(QStringLiteral("type")).toString() == type) {
      m_activeRepositoryType = type;
      m_installedRepository = repository.value(QStringLiteral("origin")).toString();
      m_installedBranch = repository.value(QStringLiteral("branch")).toString();

      m_loginService->setActiveDirectory(type);
      emit installedRepositoryChanged();
      return;
    }
  }

  emit commandFailed("Das ausgewählte Repository ist nicht installiert");
}

bool AppController::isKiSoftOneActive() const {
  return m_installedRepositoryInfoLoaded &&
         m_activeRepositoryType == QStringLiteral("KiSoft-One");
}

void AppController::createOracleDump() {
  if (!m_loginService->getLoggedIn()) {
    emit commandFailed(QStringLiteral("Keine SSH-Verbindung"));
    return;
  }
  
  m_checkingOracleBackupUser = true;
  m_pendingOracleRestore = false;
  m_loginService->checkOracleBackupUser();

}
void AppController::restoreOracleDump()  {
    if (!m_loginService->getLoggedIn()) {
      emit commandFailed(QStringLiteral("Keine SSH-Verbindung"));
      return;
    }

    m_checkingOracleBackupUser = true;
    m_pendingOracleRestore = true;
    m_loginService->checkOracleBackupUser();
}