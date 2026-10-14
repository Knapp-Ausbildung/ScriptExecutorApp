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

namespace {
  QString removeAnsiSequence(QString text) {
    static const QRegularExpression ansiSequence(R"(\x1B(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1B\\)))");
    
    text.remove(ansiSequence);
    text.remove(QChar(0x18)); // Entfernt ESC-Zeichen
    return text;
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

      loadRemoteBranches(originUrl);
      return;
    } 

    if (exitStatus == 0 && standardOutput.trimmed() == QStringLiteral("__NO_INSTALLED_REPOSITORY__")) {
      m_installedRepository.clear();
      m_installedBranch.clear();
      m_installedRepositoryInfoLoaded = true;
      emit installedRepositoryChanged();
      return;
    }

    if (m_loadingInstalledRepositoryInfo) {
      m_loadingInstalledRepositoryInfo = false;

      if (exitStatus == 0 &&
          standardOutput.trimmed() ==
              QStringLiteral("__NO_INSTALLED_REPOSITORY__")) {
                m_installedRepository.clear();
                m_installedBranch.clear();
                m_installedRepositoryInfoLoaded = true;
                emit installedRepositoryChanged();
                return;
      }

      const int separator = standardOutput.indexOf('\t');
      if (exitStatus != 0 || separator < 0) {
        m_installedRepository.clear();
        m_installedBranch.clear();
        emit installedRepositoryChanged();
        emit commandFailed(standardError.trimmed().isEmpty()
                           ? QStringLiteral("Installiertes Repository konnte nicht gelesen werden")
                           : standardError.trimmed());
        return;
      }

      m_installedRepository = standardOutput.left(separator).trimmed();
      m_installedBranch = standardOutput.mid(separator + 1).trimmed();
      m_installedRepositoryInfoLoaded = true;
      emit installedRepositoryChanged();
      return;
    }

    if(!m_loadingRemoteBranches) {
      writeCommandLog(standardOutput, standardError, exitStatus);
      emit commandCompleted(standardOutput, standardError, exitStatus);
      if (m_refreshInstalledRepositoryAfterCommand) {
        m_refreshInstalledRepositoryAfterCommand = false;
        if (exitStatus == 0) {
          loadInstalledRepositoryInfo();
        }
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
    if (!m_installedRepositoryInfoLoaded ||
        m_installedRepository.isEmpty() ||
        m_installedBranch.isEmpty()) {
          emit commandFailed("Bitte erst Repository und Branch auswählen");
          return;
        }
    
        m_loginService->fetchRemoteBranch(m_installedRepository, m_installedBranch);
  }

  void AppController::pullSelectedBranch() {
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

    m_refreshInstalledRepositoryAfterCommand = true;
    m_loginService->replaceRepository(m_selectedRepoUrl, m_selectedBranch);
  }

  void AppController::loadInstalledRepositoryBranches() {

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
