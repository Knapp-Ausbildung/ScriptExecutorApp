import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    visible: true
    width: 600
    height: 400

    // Beispiel: Wenn eingeloggt -> Dashboard anzeigen
    // Wenn nicht eingeloggt -> Login-Formular anzeigen
    // über das loggedInChanged Signal weiß qml, dass sich der Status geändert hat und kann die UI entsprechend anpassen
    StackView {
        id: stack
        anchors.fill: parent

        initialItem: appController.loggedIn ? dashboardPage : loginPage
    }

    Component {
        id: loginPage

        Rectangle {
            color: "#f2f2f2"

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    text: "Login"
                    font.pixelSize: 24
                }

                TextField {
                    id: ipAddressField
                    placeholderText: "IP Adresse"
                    width: 220
                }

                TextField {
                    id: passwordField
                    placeholderText: "Passwort"
                    echoMode: TextInput.Password
                    width: 220
                }

                Button {
                    text: "Einloggen"
                    width: 220

                    onClicked: {
                        // Aufruf der C++-Methode
                        appController.attemptLogin(ipAddressField.text, passwordField.text)
                    }
                }
            }
        }
    }

    Component {
        id: dashboardPage

        Rectangle {
            color: "#e8f5e9"

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    text: "Dashboard"
                    font.pixelSize: 24
                }

                Text {
                    text: "Status: " + (appController.loggedIn ? "Eingeloggt" : "Ausgeloggt")
                }

                Button {
                    text: "Ausloggen"

                    onClicked: {
                        appController.attemptLogout()
                    }
                }
            }
        }
    }

    // Optional: Wenn sich der Screen ändert, kannst du auch hier reagieren
    Connections {
        target: appController
        function onLoggedInChanged() {
            // Beispiel: wenn sich login-State ändert, UI neu laden
            if (appController.loggedIn) {
                stack.replace(dashboardPage)
            } else {
                stack.replace(loginPage)
            }
        }
    }
}