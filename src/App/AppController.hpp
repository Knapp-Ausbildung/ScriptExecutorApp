#pragma once

#include "src/LoginService/LoginService.hpp"
#include <QObject>
#include <QString>
#include <qglobal.h>
#include <qtmetamacros.h>

// AppController ist eine QObject-basierte Klasse.
// Warum? Weil wir sie später als Kontextproperty an QML übergeben wollen.
// QML kann dann direkt auf Eigenschaften und Slots zugreifen.
class AppController : public QObject {
  Q_OBJECT

  // Q_PROPERTY macht Eigenschaften für QML verfügbar.
  // Beispiel: appController.loggedIn
  // QML kann diese Eigenschaft beobachten und automatisch neu zeichnen.

  // Format:
  // Q_PROPERTY(attribut READ getter-methodenname NOTIFY signal)
  Q_PROPERTY(
      QString currentScreen READ getCurrentScreen NOTIFY currentScreenChanged)

  // Wir verbinden praktisch QML über diese property mit dem LoginService
  Q_PROPERTY(bool loggedIn READ isLoggedIn NOTIFY loggedInChanged)

public:
  // parent ist optional, aber in Qt üblich.
  explicit AppController(QObject *parent = nullptr);

  // Getter-Funktionen für QML.
  // READ currentScreen
  QString getCurrentScreen() const;

  bool isLoggedIn() const;

public slots: // public slots sind für QML aufrufbar.

  void attemptLogin(const QString &ipAdress, const QString &password);
  void attemptLogout();

  // Wechsel auf Login
  void openLogin();

  // Wechsel auf Dashboard
  void openDashboard();

signals:
  // Diese Signale werden emittiert, wenn sich der Zustand ändert.
  // QML kann darauf reagieren und UI neu rendern.
  void currentScreenChanged();

  void loggedInChanged();

private:
  LoginService *m_loginService;

  // Aktueller Screen / Zustand der App.
  // Beispiele:
  // "login" -> Login-Bildschirm
  // "dashboard" -> Hauptbildschirm
  QString m_currentScreen = "login";
};