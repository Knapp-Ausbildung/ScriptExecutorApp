import QtQuick 2.15
import QtQuick.Controls 2.15

ApplicationWindow {
    visible: true
    width: 1200
    height: 800

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
                    id: userNameField
                    placeholderText: "Username"
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
                        appController.attemptLogin(ipAddressField.text, userNameField.text, passwordField.text)
                    }
                }
            }
        }
    }

    Component {
        id: dashboardPage

        Rectangle {
            id: dashboardRoot
            color: "#e8f5e9"

             property bool commandRunning: false

            function submitCommand() {
                const command = commandField.text.trim()

                if (command.length === 0 || commandRunning)
                    return
                
                    commandRunning = true
                    commandError.text = ""
                    commandOutput.text += "$ " + command + "\n"
                    appController.executeCommand(command)
            }

            Column {
                anchors.centerIn: parent
                spacing: 20

                Text {
                    text: "Dashboard"
                    font.pixelSize: 24
                }

                Text {
                    text: dashboardRoot.commandRunning ? "Befehl läuft ... " : "Bereit"
                }
                TextField {
                    id: commandField
                    placeholderText: "Hier Command eingeben"
                    width: 400
                    enabled: !dashboardRoot.commandRunning

                    onAccepted: dashboardRoot.submitCommand()
                }

                Button {
                    text: dashboardRoot.commandRunning ? "Wird ausgeführt ..." : "Befehl ausführen"
                    enabled: !dashboardRoot.commandRunning
                    onClicked: dashboardRoot.submitCommand()
                }

                Text {
                    id: commandError
                    color: "red"
                    wrapMode: Text.Wrap
                    width: 500
                }

                ScrollView {
                    id: commandOutputScroll
                    width: 600
                    height: 240
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                

                TextArea {
                    id: commandOutput
                    width: commandOutputScroll.availableWidth
                    readOnly: true
                    wrapMode: TextArea.Wrap
                    }
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
            Connections {
                target: appController

                function onCommandCompleted(standardOutput, standardError, exitStatus) {
                    dashboardRoot.commandRunning = false
                    commandOutput.text += standardOutput + standardError

                    if (exitStatus !== 0) {
                        commandError.text = "Befehl beendet mit Exit-Code " + exitStatus
                    } else {
                        commandError.text = ""
                    }
                }

                function onCommandFailed(message) {
                    dashboardRoot.commandRunning = false
                    commandError.text = message
                    commandOutput.text += "\n[Fehler] " + message + "\n"
                }
            }
        }
    }

    Popup {
        id: loginFailedPopup

        anchors.centerIn: parent

        width: 300
        height: 120

        modal: true

        Column {
            anchors.centerIn: parent
            spacing: 10

            Text {
                text: "Login Failed!"
            }

            Button {
                text: "OK"
                onClicked: loginFailedPopup.close()
            }
        }
    }

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

        function onLoginFailed() {
            loginFailedPopup.open()
        }
    }

    Dialog {
        id: hostKeyDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: qsTr("SSH-Host-Key bestätigen")
        standardButtons: Dialog.Yes | Dialog.No

        property string fingerprint: ""

        contentItem: Text {
            text: qsTr("Prüfe den Fingerprint über einen vertrauenswürdigen Weg:\n\n%1")
                       .arg(hostKeyDialog.fingerprint)
            wrapMode: Text.Wrap
        }

        onAccepted: appController.confirmHostKey()
        onRejected: appController.rejectHostKey()
    }

    Connections {
        target: appController

        function onHostKeyConfirmationRequested(host, port, fingerprint) {
            hostKeyDialog.title = 
                qsTr("Host-Key für %1:%2 bestätigen").arg(host).arg(port)
            hostKeyDialog.fingerprint = fingerprint
            hostKeyDialog.open()
        }
    }


}