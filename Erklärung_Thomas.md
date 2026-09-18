# ScriptExecutorApp – persönliche Erklärung und Projektverständnis

Diese Datei dient als verständliche Nachschlage- und Erklärungsseite für das aktuelle Projekt. Sie ist bewusst separat von der normalen README gehalten, damit die allgemeine Projektübersicht sauber bleibt und die detaillierte Projektlogik an einem eigenen Ort dokumentiert ist.

---

## 1. Was ist dieses Projekt?

Dieses Projekt ist eine Qt/QML-basierte Desktop-Anwendung, die für den Aufbau einer Verbindung zu entfernten Servern entwickelt wurde. Das eigentliche Ziel ist:

- einen entfernten Server per SSH zu erreichen,
- den Login-Status verwalten,
- später Skripte auf dem Server finden,
- diese Skripte prüfen und ggf. ausführbar machen,
- und sie schließlich ausführen oder für die Ausführung vorbereiten.

Das Projekt ist also derzeit noch ein Mix aus:

- Benutzeroberfläche mit QML,
- C++-Logik,
- SSH-Verbindung mit libssh,
- App-Controller für den Ablauf,
- Login-Service für die Authentifizierung.

---

## 2. Wie ist das Projekt aktuell aufgebaut?

Die Projektstruktur ist in etwa so:

```text
ScriptExecutor/
├── CMakeLists.txt
├── README.md
├── main.qml
├── ScriptExecutor.qmlproject
├── content/
│   ├── App.qml
│   └── fonts/
├── imports/
│   └── ScriptExecutor/
├── src/
│   ├── main.cpp
│   ├── app_environment.h
│   ├── import_qml_components_plugins.h
│   ├── import_qml_plugins.h
│   ├── App/
│   │   ├── AppController.hpp
│   │   └── AppController.cpp
│   ├── LoginService/
│   │   ├── LoginService.hpp
│   │   └── LoginService.cpp
│   └── LoginData/
│       ├── LoginData.hpp
│       └── LoginData.cpp
├── qmlcomponents/
├── qmlmodules/
├── asset_imports/
├── build/
└── qtquickcontrols2.conf
```

Diese Aufteilung ist wichtig:

- `src/` enthält die C++-Logik
- `content/` enthält die QML-Oberfläche
- `main.qml` / `App.qml` ist die sichtbare UI
- `AppController` verwaltet den Lauf der App
- `LoginService` verwaltet die Verbindung
- `LoginData` ist eher ein reines Datenmodell

---

## 3. Welche Dateien gibt es und was machen sie?

### 3.1 CMakeLists.txt

Diese Datei ist das Herzstück des Builds.

Sie sagt dem Compiler:

- welches Projekt gebaut werden soll,
- welche Bibliotheken benötigt werden,
- welche C++-Dateien kompiliert werden sollen,
- welche Qt-Komponenten eingebunden werden,
- und welche QML-Resourcen dazukommen.

Im Projekt ist das wichtig, weil Qt und libssh hier zusammenlaufen.

Wichtige Einträge sind:

```cmake
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Qml Quick)
find_package(LibSSH REQUIRED)
```

Das bedeutet:

- Qt 6 wird verwendet,
- Qml und Quick für UI,
- libssh für SSH-Verbindungen.

Weiter unten werden die Dateien hinzugefügt:

```cmake
qt_add_executable(ScriptExecutorApp
    src/main.cpp
    src/App/AppController.cpp
    src/App/AppController.hpp
    src/LoginService/LoginService.hpp
    src/LoginService/LoginService.cpp
)
```

Das heißt: Der Build kennt jetzt die wichtigsten Teile der App.

---

### 3.2 src/main.cpp

Das ist der Startpunkt der Anwendung.

Diese Datei macht im Grunde vier Dinge:

1. Qt-Umgebung vorbereiten
2. eine QGuiApplication starten
3. QML-Engine initialisieren
4. den AppController an QML binden

Wichtiger Teil:

```cpp
AppController appController;
engine.rootContext()->setContextProperty("appController", &appController);
```

Das ist der entscheidende Schritt:

- QML kann auf das C++-Objekt `appController` zugreifen
- QML kann Funktionen aufrufen, z. B. `attemptLogin(...)`
- QML kann Eigenschaften lesen, z. B. `loggedIn`

Ohne diese Zeile funktioniert die Verbindung zwischen QML und C++ nicht.

---

### 3.3 src/App/AppController.hpp

Das ist die zentralste Klasse für den Programmfluss.

Sie verwaltet den Zustand der App. Der Controller weiß:

- ob der User eingeloggt ist,
- welcher Screen gerade aktiv ist,
- was nach Login/Logout passiert,
- welche Signale an QML gesendet werden.

Wichtige Eigenschaften:

```cpp
Q_PROPERTY(QString currentScreen READ getCurrentScreen NOTIFY currentScreenChanged)
Q_PROPERTY(bool loggedIn READ isLoggedIn NOTIFY loggedInChanged)
```

Das bedeutet:

- QML kann `appController.loggedIn` lesen
- QML kann `appController.currentScreen` lesen
- wenn sich der Wert ändert, werden Signale ausgelöst

Das ist wichtig für die UI, weil dann die Oberfläche automatically aktualisiert werden kann.

---

### 3.4 src/App/AppController.cpp

Das ist die Logik hinter dem Controller.

Die wichtigsten Funktionen:

#### `attemptLogin(...)`

Diese Funktion wird aus QML aufgerufen. Beispiel:

```qml
appController.attemptLogin(ipAddressField.text, userNameField.text, passwordField.text)
```

Dann macht sie Folgendes:

1. LoginService aufrufen
2. Verbindung herstellen
3. prüfen, ob Login erfolgreich war
4. falls ja: Screen auf Dashboard setzen
5. falls nein: auf Login zurücksetzen

Das ist der eigentliche Ablauf der App.

#### `attemptLogout()`

Wenn der Nutzer sich ausloggt, wird der LoginService beendet und es wird zurück zum Login-Screen gewechselt.

#### `openDashboard()`

Falls der Nutzer eingeloggt ist, wird der Dashboard-Bildschirm geöffnet.

#### `openLogin()`

Falls nicht eingeloggt ist, springt man zurück zum Login-Bildschirm.

---

### 3.5 src/LoginService/LoginService.hpp

Das ist die SSH-Authentifizierungslogik.

Es verwaltet:

- den Login-Status `m_loggedIn`
- die SSH-Session `m_session`
- Verbindungslogik
- Abmelde-Logik

Das ist die Klasse, die wirklich mit dem Server spricht.

Wichtige Methoden:

```cpp
void login(const QString &ipAdress, const QString &username,
           const QString &password, int port = 22);
void logout();
const bool getLoggedIn();
```

Diese Klasse stellt also den Kommunikationsdienst für den Server dar.

---

### 3.6 src/LoginService/LoginService.cpp

Das ist der eigentliche SSH-Code.

Der Ablauf ist derzeit:

1. Eine neue SSH-Session wird erstellt
2. Host, Benutzer und Port werden gesetzt
3. `ssh_connect(...)` versucht die Verbindung
4. Host-Key wird überprüft
5. Passwort-Authentifizierung wird versucht
6. wenn erfolgreich: `m_loggedIn = true`

Beispiel:

```cpp
m_session = ssh_new();
ssh_options_set(m_session, SSH_OPTIONS_HOST, ipAdress.toStdString().c_str());
ssh_options_set(m_session, SSH_OPTIONS_USER, username.toStdString().c_str());
ssh_options_set(m_session, SSH_OPTIONS_PORT, &port);
```

Danach versucht die Anwendung, den Server wirklich zu kontaktieren.

Wenn die Verbindung fehlschlägt, gibt `ssh_error_exit(...)` den Fehler aus und setzt `m_loggedIn = false`.

Das ist gut und wichtig, weil man auf diese Weise die Verbindung sauber verhandeln kann.

---

### 3.7 src/LoginData/LoginData.hpp und LoginData.cpp

Diese Dateien sind ein einfacher Datencontainer für Login-Daten.

Sie enthalten z. B.:

- IP-Adresse
- Passwort
- andere Login-Informationen

Methode:

```cpp
void saveCredentials(const QString &ipadress, const QString &password);
```

Diese Klasse ist nicht der eigentliche Server-Logikteil, sondern eher ein Objekt, das Login-Werte zwischen speichern und weiterverarbeiten kann.

Das ist sauber, aber momentan eher als Basis für spätere Erweiterung gedacht.

---

### 3.8 content/App.qml

Das ist die Haupt-UI-Datei.

Sie enthält:

- das Fenster
- einen Login-Bereich
- einen Dashboard-Bereich
- den Wechsel zwischen diesen Bereichen
- den Button-Click, der den AppController aufruft

Wichtiger Teil:

```qml
onClicked: {
    appController.attemptLogin(ipAddressField.text, userNameField.text, passwordField.text)
}
```

Das bedeutet:

- Wenn der Nutzer auf Login klickt,
- dann wird C++ aufgerufen,
- dann versucht der Controller eine Verbindung aufzubauen.

Es gibt auch eine `Connections`-Sektion:

```qml
Connections {
    target: appController
    function onLoggedInChanged() {
        if (appController.loggedIn) {
            stack.replace(dashboardPage)
        } else {
            stack.replace(loginPage)
        }
    }
}
```

Das ist wichtig, weil QML auf Statusänderungen reagieren kann und sich dann automatisch das richtige Screen-Element anzeigt.

---

### 3.9 main.qml

Diese Datei scheint als allgemeine QML-Hauptdatei genutzt zu werden, oft als Einstiegspunkt für die UI. In diesem Projekt wird aber hauptsächlich die `content/App.qml` verwendet, die in der App als Hauptoberfläche eingebunden wird.

Die Datei ist also Teil der Qt/QML-Initialisierung, aber die eigentliche Logik liegt hier im aktuellen Stand eher in `content/App.qml`.

---

### 3.10 app_environment.h

Diese Datei setzt verschiedene Qt-Umgebungsvariablen.

Sie sorgt dafür, dass die Anwendung sauber mit DPI, QML und UI-Settings läuft.

Beispiele:

```cpp
qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", "1");
qputenv("QT_QUICK_CONTROLS_CONF", ":/qtquickcontrols2.conf");
```

Das ist keine Business-Logik, sondern eine Umgebungseinstellung für die Qt-Anwendung.

---

### 3.11 import_qml_plugins.h / import_qml_components_plugins.h

Diese Dateien sind generiert und dienen dazu, QML-Plugins zu importieren.

Sie sind wichtig, damit QML-Komponenten aus dem Projekt richtig registriert und verwendet werden können.

Das sind eher technische Hilfsdateien, keine eigentliche App-Logik.

---

## 4. Wie läuft die App im aktuellen Zustand?

Der aktuelle Ablauf ist grob so:

1. `main.cpp` startet die Anwendung.
2. `AppController` wird erzeugt.
3. `appController` wird an QML übergeben.
4. Benutzer gibt IP, Benutzername und Passwort ein.
5. QML ruft `attemptLogin(...)` auf.
6. `AppController` ruft `LoginService::login(...)` auf.
7. `LoginService` baut mit libssh eine SSH-Verbindung auf.
8. Wenn der Login erfolgreich war, wird `loggedIn = true`.
9. QML bekommt das Signal `loggedInChanged`.
10. QML zeigt den Dashboard-Bildschirm.

Wenn die Verbindung fehlschlägt:

- bleibt das `loggedIn` auf false,
- und die App bleibt im Login-Bildschirm.

---

## 5. Was funktioniert bereits?

Aktuell ist bereits vorhanden:

- Qt/QML-Anwendung startet
- Login-UI ist vorhanden
- Benutzer kann IP, Benutzer und Passwort eingeben
- Verbindung zum SSH-Server kann versucht werden
- AppController verwaltet den Zustandswechsel
- Login-Status wird in C++ verwaltet
- QML reagiert auf Zustandsänderungen

Das ist ein guter Grundbaustein.

---

## 6. Was fehlt noch für das echte Ziel?

Das eigentliche Endziel ist nicht nur Login, sondern:

- Script-Pfade prüfen,
- Server-Pfade überprüfen,
- vorhandene Skripte finden,
- diese als ausführbar markieren,
- und sie ausführen.

Das fehlt derzeit noch.

Der Projektstand ist aktuell eher:

- Login und Serververbindung ist da,
- aber echte Script-Erkennung und Ausführung ist noch nicht implementiert.

---

## 7. Nächstes großes Ziel

Das größte nächste Ziel ist:

"Script-Pfade hinzufügen und prüfen, ob der verbundene Server unter diesen Pfaden Skripte hat. Wenn ja, diese Skripte als ausführbar markieren und für die Ausführung vorbereiten."

Das bedeutet konkret:

### Schritt 1: Pfade definieren
Zum Beispiel:

```cpp
QStringList scriptPaths = {
    "/opt/scripts",
    "/var/custom/scripts",
    "/home/user/scripts"
};
```

### Schritt 2: Auf dem Server prüfen
Mit SSH-Abfragen prüfen, ob diese Ordner existieren und Dateien enthalten.

### Schritt 3: Skripte erkennen
Dateien filtern, die wirklich Skripte sind.

### Schritt 4: ausführbar machen
Mit `chmod +x` die Dateien markieren.

### Schritt 5: Ausführung vorbereiten
Notfall, Ergebnisanzeige, Exit-Code, Fehlerbehandlung.

---

## 8. Gute Struktur für die Zukunft

Wenn das Projekt weiter wächst, wäre eine saubere Struktur so:

- `AppController`: App-Ablauf und Zustände
- `LoginService`: SSH-Login und Verbindung
- `ScriptService`: Skript-Pfade, Suche, Rechte, Ausführung
- `ScriptEntry` / `ScriptModel`: Einzelne Script-Dateien mit Namen, Pfad, Status
- `QML`: UI für Login, Script-Liste und Ausführung

Das wäre eine sehr logische und wartbare Aufteilung.

---

## 9. Warum ist das aktuell sinnvoll?

Diese Anwendung ist ein guter Kandidat für saubere Trennung:

- UI darf nichts über Serverlogik wissen
- LoginService darf nichts über die Darstellung wissen
- AppController koordiniert die Gesamtsituation
- QML zeigt nur den aktuellen Zustand an

So bleibt die App leichter verständlich und erweiterbar.

---

## 10. Kurzfassung

Wenn ich das Projekt in einem Satz zusammenfassen müsste:

> ScriptExecutorApp ist aktuell eine Qt/QML-App, die einen SSH-Login zu einem Remote-Server verwaltet und als Grundlage für zukünftige Script-Erkennung und Script-Ausführung dienen soll.

Das ist die aktuelle Lage.

---

## 11. Wie geht es jetzt weiter?

Die nächste Entwicklung sollte in dieser Reihenfolge passieren:

1. Script-Pfade konfigurieren
2. per SSH prüfen, ob diese Pfade existieren
3. Dateien darin finden
4. prüfen, ob sie ausführbar sind
5. bei Bedarf `chmod +x` setzen
6. Script-Liste in der UI anzeigen
7. Script-Ausführung vorbereiten und überwachen

Das ist das eigentliche nächste Ziel des Projekts.

---

## 12. Fazit

Das Projekt hat bereits eine gute Basis:

- Qt/QML-Anwendung läuft
- Login-Mechanik ist vorhanden
- SSH-Verbindung kann hergestellt werden
- App-Controller organisiert den Ablauf

Der nächste Schritt ist jetzt klar und konkret:

- Script-Pfade einbauen,
- Remote-Verzeichnis prüfen,
- Skripte erkennen,
- sie ausführbar machen,
- und damit die App von "Login-Anwendung" zu "Script-Executor" weiterentwickeln.

Diese Datei soll als persönliche, verständliche Erklärung dienen und zusammen mit dem Code als Referenz für die zukünftige Weiterentwicklung genutzt werden.
