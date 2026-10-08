import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

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
                    Accessible.name: "Login"
                    font.pixelSize: 24
                }

                TextField {
                    id: ipAddressField
                    placeholderText: "IP Adresse"
                    Accessible.name: "IP Adresse"
                    width: 220
                }

                TextField {
                    id: userNameField
                    placeholderText: "Username"
                    Accessible.name: "Username"
                    width: 220
                }
                TextField {
                    id: passwordField
                    placeholderText: "Passwort"
                    Accessible.name: "Passwort"
                    echoMode: TextInput.Password
                    width: 220
                }


                Button {
                    text: "Einloggen"
                    Accessible.name: "Einloggen"
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
            
            property string outputHtml: ""

            function escapeHtml(text) {
                return String(text)
                            .replace(/&/g, "&amp;")
                            .replace(/</g, "&lt;")
                            .replace(/>/g, "&gt;")
                            .replace(/\r/g, "")
                            .replace(/\n/g, "<br/>")
                            .replace(/ /g, "&nbsp;")
            }

            // Parser für Rückgabe des Terminals
            function ansiToHtml(text) {
                const colors = { 
                    30: "#000000", 31: "#cc0000", 32: "#00aa00", 33: "#aa7700",
                    34: "#0000cc", 35: "#aa00aa", 36: "#008888", 37: "#cccccc",
                    90: "#555555", 91: "#ff5555", 92: "#55ff55", 93: "#ffff55",
                    94: "#5555ff", 95: "#ff55ff", 96: "#55ffff", 97: "#ffffff"
                }

                function segmentToHtml(segment, color) {
                    const escaped = escapeHtml(segment)
                    return color
                        ? "<span style=\"color:" + color + "\">" + escaped + "</span>"
                        : escaped
                }

                const sgr = new RegExp("\\u001b\\[([0-9;]*)m", "g") 
                      let result = ""
                      let currentColor = ""
                      let lastIndex = 0
                      let match
                while ((match = sgr.exec(String(text))) !== null) {
                    result += segmentToHtml(text.slice(lastIndex, match.index), currentColor)

                    const codes = match[1] === ""
                        ? [0]
                        : match[1].split(";").map(function(code) { return Number(code) })

                    for (let i = 0; i < codes.length; i++)
                    {
                        const code = codes[i]
                        if (code === 0 || code === 39)
                            currentColor = ""
                        else if (colors[code] !== undefined)
                            currentColor = colors[code]
                    }
                    lastIndex = sgr.lastIndex
                }

                result += segmentToHtml(text.slice(lastIndex), currentColor)
                return result
            }

            function appendOutput(text) {
                outputHtml += ansiToHtml(text)
                commandOutput.text = outputHtml
            }

            function clearOutput() {
                outputHtml = ""
                commandOutput.text = ""
            }

            function submitCommand() {
                const command = commandField.text.trim()

                if (command.length === 0 || commandRunning)
                    return
                
                    commandRunning = true
                    commandError.text = ""
                    dashboardRoot.appendOutput("$ " + command + "\n")
                    appController.executeCommand(command)
            }

            function runPreset(commandId, displayCommand) {
                if (commandRunning)
                return

                commandRunning = true
                commandError.text = ""
                dashboardRoot.appendOutput("$ " + displayCommand + "\n")
                appController.executePresetCommand(commandId)
            }
            ColumnLayout {
                anchors.fill: parent
                anchors.top: parent.top
                anchors.margins: 20
                spacing: 12

                Text {
                    text: "Dashboard"
                    Accessible.name: "Dashboard"
                    font.pixelSize: 24
                }

                Text {
                    text: dashboardRoot.commandRunning ? "Befehl läuft ... " : "Bereit"
                }
                
                Row {
                    spacing: 10

                    Button {
                        text: "Setup Repository"
                        Accessible.name: "Import Branch"
                        onClicked: selectRepo.open()
                    }

                    Button {
                        text: "Fetch current branch"
                        enabled: !dashboardRoot.commandRunning

                        onClicked: {
                            if (dashboardRoot.commandRunning)
                                return

                            dashboardRoot.commandRunning = true
                            commandError.text = ""
                            appController.fetchSelectedBranch()
                        }
                    }

                    Button {
                        text: "Pull current branch"
                        enabled: !dashboardRoot.commandRunning

                        onClicked: {
                            if(dashboardRoot.commandRunning) {
                                return
                            }

                            dashboardRoot.commandRunning = true
                            commandError.text = ""
                            appController.pullSelectedBranch()
                        }
                    }

                    Button {
                        text: "Get new Branch"
                        enabled: !dashboardRoot.commandRunning

                        onClicked: {
                            if (dashboardRoot.commandRunning) {
                                return
                            }

                            dashboardRoot.commandRunning = true
                            commandError.text = ""
                            selectBranchChoice.model = []
                            selectBranchChoice.currentIndex = -1
                            appController.loadInstalledRepositoryBranches()
                        }

                    }

                }

                // TextField {
                //     id: commandField
                //     placeholderText: "Hier Command eingeben"
                //     Accessible.name: "Hier Command eingeben"
                //     width: 400
                //     enabled: !dashboardRoot.commandRunning

                //     onAccepted: dashboardRoot.submitCommand()
                // }
                Row {
                    spacing: 10
                    Layout.fillWidth: true
                //     Button {
                //     text: dashboardRoot.commandRunning ? "Wird ausgeführt ..." : "Befehl ausführen"
                //     enabled: !dashboardRoot.commandRunning
                //     onClicked: dashboardRoot.submitCommand()
                // }
                Button {
                    text:"Befehl abbrechen"
                    Accessible.name: "Befehl abbrechen"
                    visible: dashboardRoot.commandRunning
                    onClicked: appController.cancelCommand()
                    }
                  }
                
            Row {
                    spacing: 10

                Button {
                    text: "Status"
                    Accessible.name: "Status"
                    enabled: !dashboardRoot.commandRunning
                    onClicked: dashboardRoot.runPreset("status", "qking")
                }

                Button {
                    text: "Restart"
                    Accessible.name: "Restart"
                    enabled: !dashboardRoot.commandRunning
                    onClicked: dashboardRoot.runPreset("restart", "restart")
                }

                Button {
                    text: "Stop"
                    Accessible.name: "Stop"
                    enabled: !dashboardRoot.commandRunning
                    onClicked: dashboardRoot.runPreset("stop", "stop")
                }
                }

            Row {
                spacing: 10

                Button {
                    text: "Rebuild Database"
                    Accessible.name: "Rebuild Database"
                    enabled: !dashboardRoot.commandRunning
                    onClicked: dashboardRoot.runPreset("rebuildDb", "make reinstall Database")
                }

                Button {
                    text: "Reinstall Database"
                    Accessible.name: "Reinstall Database"
                    enabled: !dashboardRoot.commandRunning
                    onClicked: dashboardRoot.runPreset("reinstallDb", "make install Database")
                }

                Button {
                    text: "Reinstall KiSoft"
                    Accessible.name: "Rebuild Database"
                    enabled: !dashboardRoot.commandRunning
                    onClicked: dashboardRoot.runPreset("reinstallKiSoft", "make reinstall Database")
                }   
            
                Button {
                    text: "Reinstall All"
                    Accessible.name: "Reinstall all"
                    enabled: !dashboardRoot.commandRunning
                    onClicked: dashboardRoot.runPreset("reinstallAll", "make reinstall all")
                }
            }

                Text {
                    id: commandError
                    color: "red"
                    wrapMode: Text.Wrap
                    width: 500
                }

                ScrollView {
                    id: commandOutputScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: 150
                    Layout.preferredHeight: 300
                    clip: true
                    ScrollBar.vertical: ScrollBar {
                        policy: ScrollBar.AsNeeded
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.right: parent.right
                        anchors.topMargin: 10
                    }
                    ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                

                TextArea {
                    id: commandOutput
                    width: commandOutputScroll.availableWidth
                    readOnly: true
                    wrapMode: TextArea.NoWrap
                    textFormat: TextEdit.RichText
                    font.family: "monospace"

                    onTextChanged: Qt.callLater(function() {
                        const bar = commandOutputScroll.ScrollBar.vertical
                        bar.position = Math.max(0, 1 - bar.size)
                        })
                    }
                }
                    
                Item {
                    Layout.fillWidth: true
                    height: Math.max(statusLabel.implicitHeight,
                                    clearOutputButton.implicitHeight)

                    Text {
                        id: statusLabel
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Status: " + (appController.loggedIn ? "Eingeloggt" : "Ausgeloggt")
                    }
                    Row {
                        id: logActionsRow
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10

                    Button {
                        text: "Logs öffnen"
                        Accessible.name: "Logs öffnen"
                        onClicked: appController.openLogsFolder();
                    }
                    Button {
                        id: clearOutputButton
                        text: "Ausgabe leeren"
                        Accessible.name: "Ausgabe leeren"
                        onClicked: dashboardRoot.clearOutput()
                    }
                    }
                }

                Button {
                    text: "Ausloggen"
                    Accessible.name: "Ausloggen"

                    onClicked: {
                        appController.attemptLogout()
                    }
                }
            }
            Connections {
                target: appController

                function onCommandCompleted(standardOutput, standardError, exitStatus) {
                    dashboardRoot.commandRunning = false
                    appendOutput(standardOutput + standardError)

                    if (exitStatus !== 0) {
                        commandError.text = "Befehl beendet mit Exit-Code " + exitStatus
                    } else {
                        commandError.text = ""
                    }
                }

                function onCommandFailed(message) {
                    dashboardRoot.commandRunning = false
                    commandError.text = message
                    dashboardRoot.appendOutput("\n[Fehler] " + message + "\n")
                }

                function onCommandCancelled() {
                    dashboardRoot.commandRunning = false
                    commandError.text = ""
                    dashboardRoot.appendOutput("\n[Befehl abgebrochen]\n")
                }

                function onCommandInputRequested(prompt, secret) {
                    commandInputDialog.prompt = prompt
                    commandInputDialog.secret = secret
                    commandInputDialog.open()
                }

                function onInstalledRepositoryPullStarted() {
                    dashboardRoot.commandRunning = true
                    commandError.text = ""
                }
            }
            Dialog {
                id: commandInputDialog
                anchors.centerIn: Overlay.overlay
                modal: true
                title: secret ? "Passwort eingeben" : "Eingabe benötigt"
                standardButtons: Dialog.Ok | Dialog.Cancel

                property string prompt: ""
                property bool secret: false

                contentItem: Column {
                    spacing: 8
                }

                Text {
                    text: commandInputDialog.prompt
                    wrapMode: Text.Wrap
                }

                TextField {
                    id: commandInputField
                    echoMode: commandInputDialog.secret 
                              ? TextInput.Password 
                              : TextInput.Normal
                }
            

            onAccepted: {
                appController.submitCommandInput(commandInputField.text)
                commandInputField.clear()
            }

            onRejected: {
                commandInputField.clear()
                appController.cancelCommand()
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
                Accessible.name: "Login failed"
            }

            Button {
                text: "OK"
                Accessible.name: "OK"
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
        Accessible.name: "SSH-Host-Key bestätigen"
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

    Dialog {
        id: selectRepo
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "Repository festlegen"
        Accessible.name: "Repository festlegen"

        property string repo: ""
        property string errorMessage: ""
        property bool loadingBranches: false

        contentItem: Column {
            spacing: 8

            Text {
                text: "Repository Link"
            }

            Row {
                spacing: 8
            TextField {
                width: 400
                id: repoURL
                placeholderText: "Link hier einfügen!"
            }
            Button {
                enabled: !selectRepo.loadingBranches
                text: selectRepo.loadingBranches 
                      ? "Branches werden geladen ..." 
                      : "Branches laden"

                onClicked: {
                    console.log("Repository bestätigt: ", repoURL.text)
                    selectRepo.errorMessage = ""

                    if (appController.sendRepoLink(repoURL.text)) {
                        selectRepo.errorMessage = ""
                        selectRepo.loadingBranches = true
                        appController.loadRemoteBranches(repoURL.text)
                    } else {
                        selectRepo.errorMessage = "Bitte einen gültigen Repository-Link einfügen!"
                    }
                }
            }
        
        }
            Text {
                text: selectRepo.errorMessage
                color: "red"
                visible: text.length > 0
                wrapMode: Text.Wrap
            }
        }
    }

    Connections {
        target: appController

        function onRemoteBranchesLoaded(branches) {
            dashboardRoot.commandRunning = false
            selectRepo.loadingBranches = false;
            selectBranchChoice.model = branches

            if (selectRepo.visible) {
                selectRepo.close()
            }

            selectBranchDialog.open()
        }

        function onRemoteBranchesFailed(message) {
            dashboardRoot.commandRunning = false

            if (selectRepo.visible) {
                selectRepo.loadingBranches = false
                selectRepo.errorMessage = message
            } else {
            commandError.text = message
            }
        }   
    }
    
    Dialog {
        id: selectBranchDialog
        title: "Bitte Branch auswählen"
        Accessible.name: "Branch auswählen"
        anchors.centerIn: Overlay.overlay
        modal: true

        contentItem: Column {
            spacing: 8

        ComboBox {
            width: 300
            id: selectBranchChoice
            model: []
            currentIndex: -1
        }

        Button {
            id: confirmBranch
            enabled: selectBranchChoice.currentIndex >= 0
            text: "Branch auswählen"
            Accessible.name: "Branch auswählen"

            property string selectedBranch: ""

            onClicked: {
                if (appController.selectRemoteBranch(selectBranchChoice.currentText)) {
                    selectBranchDialog.close()
                    replaceRepositoryDialog.open()
                }
            }
        }
      }
    }

    Dialog {
        id: replaceRepositoryDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "Repository vollständig ersetzen?"
        standardButtons: Dialog.Yes | Dialog.No

        contentItem: Text {
            text: "Ziel: /kisoft/user/testing\n\n" + 
                  "ACHTUNG! Es werden ALLE bereits existierenden Dateien und lokalen Änderungen gelöscht \n" +
                  "und anschließend durch den ausgewählten Remote-Branch ersetzt! \n" + 
                  "Dies kann nicht wieder hergestellt werden! \n\n Fortfahren?"
            wrapMode: Text.wrap
        }

        onAccepted: appController.replaceSelectedRepository()
    }
}