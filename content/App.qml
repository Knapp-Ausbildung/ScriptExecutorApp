import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {

    visible: true
    width: 1200
    height: 800
    component AnimatedCommandButton: Item {
        id: control

        property string text: ""
        property string accessibleName: text
        property bool running: false
        property var clickHandler: null

        implicitWidth: innerButton.implicitWidth
        implicitHeight: innerButton.implicitHeight

        Button {
            id: innerButton
            anchors.fill: parent
            text: control.text
            Accessible.name: control.accessibleName

            onClicked: {
                if (control.clickHandler) {
                    control.clickHandler()
                
                }
            }
        }

        Rectangle {
            id: pulseBorder
                        
            property color pulseColor: "#22c55e"

            parent: innerButton.background
            anchors.fill: parent
            z: 1
            enabled: false
            color: "transparent"
            border.width: 3
            border.color: control.running ? pulseColor : "transparent"
            radius: height / 2

            SequentialAnimation on pulseColor {
                running: control.running
                loops: Animation.Infinite

                ColorAnimation {
                    from: "#22c55e"
                    to: "#bbf7d0"
                    duration: 1800
                }

                ColorAnimation {
                    from: "#bbf7d0"
                    to: "#22c55e"
                    duration: 1800
                }
            }
        }
    }

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
            id: loginRoot
            color: "#f2f2f2"

            function performLogin() {
                appController.attemptLogin(ipAddressField.text, userNameField.text, passwordField.text)
            }

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
                    onAccepted: userNameField.forceActiveFocus()
                }

                TextField {
                    id: userNameField
                    placeholderText: "Username"
                    Accessible.name: "Username"
                    width: 220
                    onAccepted: passwordField.forceActiveFocus()
                }
                TextField {
                    id: passwordField
                    placeholderText: "Passwort"
                    Accessible.name: "Passwort"
                    echoMode: TextInput.Password
                    width: 220
                    onAccepted: loginRoot.performLogin()
                }

                Button {
                    text: "Einloggen"
                    Accessible.name: "Einloggen"
                    width: 220

                    onClicked: {
                        // Aufruf der C++-Methode
                        loginRoot.performLogin()
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

            readonly property bool kiSoftOneActive:
                appController.installedRepositoryInfoLoaded &&
                appController.activeRepositoryType === "KiSoft-One"

            property string activeCommandButton: ""
            
            property string outputHtml: ""
            property bool outputScrollRestorePending: false
            property bool outputWasAtBottom: true
            property real outputScrollPosition: 0

            property string gitUsernameCache: ""
            property string gitPatCache: ""

            property string oracleSudoPasswordCache: ""
            property string oracleBackupPasswordCache: ""

            function escapeHtml(text) {
                return String(text)
                            .replace(/&/g, "&amp;")
                            .replace(/</g, "&lt;")
                            .replace(/>/g, "&gt;")
                            .replace(/\r\n?/g, "<br/>")
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
                    const escaped = escapeHtml(segment);
                    return color
                        ? "<span style=\"color:" + color + "\">" + escaped + "</span>"
                        : escaped
                }
                    text = String(text).replace(/\u001b\[[0-?]*[ -/]*[@-~]/g,
                    function (sequence) {
                    return sequence.endsWith("m") ? sequence : ""
                    })

                    const sgr = /\u001b\[([0-9;]*)m/g;
                      let result = "";
                      let currentColor = "";
                      let lastIndex = 0;
                      let match;
                while ((match = sgr.exec(String(text))) !== null) {
                    result += segmentToHtml(text.slice(lastIndex, match.index), currentColor)

                    const codes = match[1] === ""
                        ? [0]
                        : match[1].split(";").map(function(code) { return Number(code) });

                    for (let i = 0; i < codes.length; i++)
                    {
                        const code = codes[i];
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
                if (!outputScrollRestorePending) {
                    const flickable = commandOutputScroll.contentItem;
                    const maxY = Math.max(0, flickable.contentHeight - flickable.height);
                    outputWasAtBottom = maxY - flickable.contentY <= 2
                    outputScrollPosition = flickable.contentY
                    outputScrollRestorePending = true
                }

                const formattedText = ansiToHtml(text);
                outputHtml += formattedText

                if (outputHtml.length > 300000) {
                    outputHtml = "<br><span>[Earlier output omitted to limit memory]</span><br>"
                                 + formattedText
                }

                commandOutput.text = outputHtml
            }

            function clearOutput() {
                outputHtml = ""
                outputScrollRestorePending = false
                outputWasAtBottom = true
                outputScrollPosition = 0
                commandOutput.text = ""
            }

            function submitCommand() {
                const command = commandField.text.trim();

                if (command.length === 0 || commandRunning)
                    return
                
                    commandRunning = true
                    commandError.text = ""
                    dashboardRoot.appendOutput("$ " + command + "\n")
                    appController.executeCommand(command)
            }

            function runPreset(commandId, displayCommand, buttonId) {
                if (commandRunning)
                return

                activeCommandButton = buttonId
                commandRunning = true
                commandError.text = ""
                dashboardRoot.appendOutput("$ " + displayCommand + "\n")
                appController.executePresetCommand(commandId)
            }

            function runPresetWithSudoPassword(commandId, displayCommand, buttonId) {
                if (commandRunning)
                    return

                clearOraclePasswordCache()
                reinstallSudoCredentialsDialog.commandId = commandId
                reinstallSudoCredentialsDialog.displayCommand = displayCommand
                reinstallSudoCredentialsDialog.buttonId = buttonId
                reinstallSudoCredentialsDialog.open()
            }

            function repositoryDisplayName(remoteUrl) {
                const parts = String(remoteUrl).split("/_git/");
                const lastPart = parts[parts.length - 1].split(/[?#]/)[0];
                return decodeURIComponent(lastPart.replace(/\/+$/, "").replace(/\.git$/i, ""))
            }

            function clearOraclePasswordCache() {
                oracleSudoPasswordCache = ""
                oracleBackupPasswordCache = ""
                oracleSudoPasswordField.clear()
                oracleBackupPasswordField.clear()
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

                Text {
                    visible: !appController.installedRepositoryInfoLoaded
                    text: "Repository wird geladen ..."
                }

                Text {
                    visible: appController.installedRepositoryInfoLoaded &&
                             appController.installedRepository.length === 0
                    text: "Noch kein Repository geladen - bitte erst das Setup durchführen"
                }

                Text {
                    visible: appController.installedRepositoryInfoLoaded &&
                             appController.installedRepository.length > 0
                    text: "Aktuell geladenes Repository: " + dashboardRoot.repositoryDisplayName(appController.installedRepository)
                }
                
                Text {
                    visible: appController.installedRepositoryInfoLoaded &&
                             appController.installedRepository.length > 0 
                    text: "Aktueller Branch: " + appController.installedBranch
                }
                
                Row {
                    spacing: 10

                    Button {
                        text: "Setup Repository"
                        enabled: !dashboardRoot.commandRunning
                        Accessible.name: "Import Branch"
                        onClicked: selectRepo.open()
                    }

                    Button {
                        text: "Fetch current branch"
                        enabled: !dashboardRoot.commandRunning &&
                                 appController.installedRepositoryInfoLoaded &&
                                 appController.installedRepository.length > 0 &&
                                 appController.installedBranch.length > 0 &&
                                 dashboardRoot.kiSoftOneActive

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
                        enabled: !dashboardRoot.commandRunning &&
                                 appController.installedRepositoryInfoLoaded &&
                                 appController.installedRepository.length > 0 &&
                                 appController.installedBranch.length > 0 &&
                                 dashboardRoot.kiSoftOneActive
                                 
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
                        enabled: !dashboardRoot.commandRunning &&
                                 appController.installedRepositoryInfoLoaded &&
                                 appController.installedRepository.length > 0 &&
                                 appController.installedBranch.length > 0 &&
                                 dashboardRoot.kiSoftOneActive

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

                    Button {
                        visible: appController.installedRepositories.length > 1
                        enabled: !dashboardRoot.commandRunning
                        text: "Switch Repository"
                        Accessible.name: text
                        
                        onClicked: repositorySwitchDialog.open()
                    }

                }

                // TextField {
                //     id: commandField
                //     placeholderText: "Hier Command eingeben"
                //     Accessible.name: "Hier Command eingeben"
                //     width: 400
                //     enabled: !dashboardRoot.commandRunning &&
                //              dashboardRoot.kiSoftOneActive

                //     onAccepted: dashboardRoot.submitCommand()
                //  }
                Row {
                    spacing: 10
                    Layout.fillWidth: true
                // Button {
                //     text: dashboardRoot.commandRunning ? "Wird ausgeführt ..." : "Befehl ausführen"
                //     enabled: !dashboardRoot.commandRunning &&
                //              dashboardRoot.kiSoftOneActive
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

                AnimatedCommandButton {
                    text: "Status"
                    Accessible.name: "Status"
                    running: dashboardRoot.activeCommandButton === "status"
                    enabled: !dashboardRoot.commandRunning &&
                             dashboardRoot.kiSoftOneActive
                    clickHandler: function () {
                        dashboardRoot.runPreset("status", "qking", "status")
                    }
                }

                AnimatedCommandButton {
                    text: "Restart"
                    Accessible.name: "Restart"
                    running: dashboardRoot.activeCommandButton === "restart"
                    enabled: !dashboardRoot.commandRunning &&
                             dashboardRoot.kiSoftOneActive
                    clickHandler: function() {
                        dashboardRoot.runPreset("restart", "restart", "restart")
                    }    
                }

                AnimatedCommandButton {
                    text: "Stop"
                    Accessible.name: "Stop"
                    running: dashboardRoot.activeCommandButton === "stop"
                    enabled: !dashboardRoot.commandRunning &&
                             dashboardRoot.kiSoftOneActive
                    clickHandler: function() {
                        dashboardRoot.runPreset("stop", "stop", "stop")
                    }
                }
                }

            Row {
                spacing: 10

                AnimatedCommandButton {
                    text: "Rebuild Database"
                    Accessible.name: "Rebuild Database"
                    running: dashboardRoot.activeCommandButton === "rebuildDb"
                    enabled: !dashboardRoot.commandRunning &&
                             dashboardRoot.kiSoftOneActive
                    clickHandler: function() {
                        dashboardRoot.runPreset("rebuildDb", "Datenbank wird rebuilded ...", "rebuildDb")
                    }
                }
                AnimatedCommandButton {
                    text: "Dump current DB"
                    Accessible.name: text
                    running: dashboardRoot.activeCommandButton === "oracleDump"

                    enabled: !dashboardRoot.commandRunning
                    clickHandler: function() {
                    oracleCredentialsDialog.restoreMode = false;
                    oracleCredentialsDialog.open()
                    }
                }

                AnimatedCommandButton {
                    text: "Load current dumped DB"
                    Accessible.name: text
                    running: dashboardRoot.activeCommandButton === "oracleRestore"
                    enabled: !dashboardRoot.commandRunning
                    clickHandler: function() {
                        if (dashboardRoot.commandRunning) {
                            return
                        }

                    restoreConfirmation.open()
                    }                 
                }

                AnimatedCommandButton {
                    text: "Reinstall Database"
                    Accessible.name: "Reinstall Database"
                    running: dashboardRoot.activeCommandButton === "reinstallDb"
                    enabled: !dashboardRoot.commandRunning &&
                             dashboardRoot.kiSoftOneActive
                    clickHandler: function() {
                        dashboardRoot.runPresetWithSudoPassword(
                            "reinstallDb", "Datenbank wird reinstalliert ...", "reinstallDb")
                    }
                }

                AnimatedCommandButton {
                    text: "Reinstall KiSoft"
                    Accessible.name: "Rebuild Database"
                    running: dashboardRoot.activeCommandButton === "reinstallKiSoft"
                    enabled: !dashboardRoot.commandRunning &&
                             dashboardRoot.kiSoftOneActive
                    clickHandler: function() {
                        dashboardRoot.runPresetWithSudoPassword(
                            "reinstallKiSoft", "KiSoft wird reinstalliert ...", "reinstallKiSoft")
                    }
                }   
            
                AnimatedCommandButton {
                    text: "Reinstall All"
                    Accessible.name: "Reinstall all"
                    running: dashboardRoot.activeCommandButton === "reinstallAll"
                    enabled: !dashboardRoot.commandRunning &&
                             dashboardRoot.kiSoftOneActive
                    clickHandler: function () {
                        dashboardRoot.runPresetWithSudoPassword(
                            "reinstallAll", "Alles wird reinstalliert", "reinstallAll")
                    }
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
                        anchors.bottomMargin: 18
                    }
                    ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                

                TextArea {
                    id: commandOutput
                    width: commandOutputScroll.availableWidth
                    readOnly: true
                    wrapMode: TextArea.NoWrap
                    textFormat: TextEdit.RichText
                    font.family: "monospace"

                    onContentHeightChanged: {
                        if (!dashboardRoot.outputScrollRestorePending) {
                            return
                        }

                        Qt.callLater(function() {
                            if (!dashboardRoot.outputScrollRestorePending) {
                                return
                            }

                            const flickable = commandOutputScroll.contentItem;
                            const maxY = Math.max(0, flickable.contentHeight - flickable.height);
                            flickable.contentY = dashboardRoot.outputWasAtBottom
                                    ? maxY
                                    : Math.min(dashboardRoot.outputScrollPosition, maxY)
                            dashboardRoot.outputScrollRestorePending = false
                        })
                    }
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

                    if (exitStatus !== 0) {
                        const details = standardError.trim()
                        commandError.text = details.length > 0
                                            ? details
                                            : "Befehl beendet mit Exit-Code " + exitStatus
                    } else {
                        commandError.text = ""
                    }

                    dashboardRoot.activeCommandButton= ""
                    dashboardRoot.clearOraclePasswordCache()
                }

                function onCommandFailed(message) {
                    dashboardRoot.commandRunning = false
                    commandError.text = message
                    dashboardRoot.appendOutput("\n[Fehler] " + message + "\n")
                    dashboardRoot.activeCommandButton= ""
                    dashboardRoot.clearOraclePasswordCache()
                }

                function onCommandCancelled() {
                    dashboardRoot.commandRunning = false
                    commandError.text = ""
                    dashboardRoot.appendOutput("\n[Befehl abgebrochen]\n")
                    dashboardRoot.activeCommandButton= ""
                    dashboardRoot.clearOraclePasswordCache()
                }

                function onCommandInputRequested(prompt, secret) {
                    const lowerPrompt = prompt.toLowerCase();
                    const oracleCommand = dashboardRoot.activeCommandButton === "oracleDump" ||
                                          dashboardRoot.activeCommandButton === "oracleRestore"
                    const reinstallCommand = dashboardRoot.activeCommandButton === "reinstallAll" ||
                                             dashboardRoot.activeCommandButton === "reinstallKiSoft" ||
                                             dashboardRoot.activeCommandButton === "reinstallDb"
                    const askForUsername = lowerPrompt.includes("please provide your git remote username") ||
                                           lowerPrompt.includes("username for 'https://");

                    const askForPat =  lowerPrompt.includes("password for 'https://");
                    const askForUpdateParameterTables = lowerPrompt.includes("execute prepared updates") &&
                                                       lowerPrompt.includes("(y/n)?");
                    
                    if(askForUsername) {
                        if (dashboardRoot.gitUsernameCache.length > 0) {
                            appController.submitCommandInput(dashboardRoot.gitUsernameCache)
                        } else {
                        gitCredentialsDialog.prompt = prompt
                        gitCredentialsDialog.requestedField = "username"
                        gitCredentialsDialog.open()
                       }
                       return 
                    }

                    if (askForPat) {
                        if (dashboardRoot.gitPatCache.length > 0) {
                            appController.submitCommandInput(dashboardRoot.gitPatCache)
                        } else {
                    
                            gitCredentialsDialog.prompt = prompt
                            gitCredentialsDialog.requestedField = "pat"
                            gitCredentialsDialog.open()
                        }
                        return
                    }

                    if (askForUpdateParameterTables) {
                        appController.submitCommandInput("y");
                        return;
                    }

                    if ((oracleCommand || reinstallCommand) && lowerPrompt.includes("password")) {
                        const isSudoPrompt = lowerPrompt.includes("sudo") &&
                                             lowerPrompt.includes("for klassx")
                        const password = isSudoPrompt
                                         ? dashboardRoot.oracleSudoPasswordCache
                                         : dashboardRoot.oracleBackupPasswordCache
                        if (password.length > 0) {
                            appController.submitCommandInput(password)
                            return
                            
                        }
                    }

                    if (oracleCommand && lowerPrompt.trim().endsWith("username:")) {
                        dashboardRoot.appendOutput("\n[Fehler] Oracle-Anmeldung fehlgeschlagen; BACKUPTOOL_USER fragt erneut nach dem Benutzernamen.")
                        appController.cancelCommand()
                        return
                    }
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
                    onAccepted: commandInputDialog.accept()
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

        Dialog {
            id: restoreConfirmation
            anchors.centerIn: Overlay.overlay
            modal: true
            title: "Datenbank wiederherstellen?"
            standardButtons: Dialog.Yes | Dialog.No

        Text {
            text: "Der Restore löscht die Schemas LU, AFCONFIG, ARCHIVE, KLASSX, KXGUI, KXREPORT und USER_MANAGEMENT vollständig und importiert sie neu. Dabei werden vorhandene Daten überschrieben. Fortfahren?"
            wrapMode: Text.Wrap
        }

        onAccepted: {
            if (dashboardRoot.commandRunning)
            return

            oracleCredentialsDialog.restoreMode = true
            oracleCredentialsDialog.open()
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
            dashboardRoot.activeCommandButton= ""
        }
        
        function onCommandOutputReceived(chunk) { 
            dashboardRoot.appendOutput(chunk)
        }
    }  
    Dialog {
        id: gitCredentialsDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "Git-Zugangdaten"
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string prompt: ""
        property string requestedField: ""
        property string errorMessage: ""
        
        contentItem: Column {
            spacing: 8

            Text {
                text: gitCredentialsDialog.prompt
                wrapMode: Text.Wrap
            }
            TextField {
                id: gitUsernameField
                width: 300
                placeholderText: "Git-Username"
                onAccepted: gitPatField.forceActiveFocus()
            }

            TextField {
                id: gitPatField
                width: 300
                placeholderText: "Personal Access Token"
                echoMode: TextInput.Password
                onAccepted: gitCredentialsDialog.accept()
            }

            Text {
                text: gitCredentialsDialog.errorMessage
                color: "red"
                visible: text.length > 0
            }
        }
                onAccepted: {
                const username = gitUsernameField.text.trim();
                const pat = gitPatField.text;

                if (username.length === 0 || pat.length === 0) {
                    errorMessage = "Bitte Benutzername und PAT eingeben"
                    Qt.callLater(function() {gitCredentialsDialog.open() });
                    return
                }

                dashboardRoot.gitUsernameCache = username
                dashboardRoot.gitPatCache = pat
                
                const answer = requestedField === "username"
                               ? username 
                               : pat

                appController.submitCommandInput(answer)

                gitUsernameField.clear()
                gitPatField.clear()
                errorMessage = ""
            }

            onRejected: {
                gitUsernameField.clear()
                gitPatField.clear()
                dashboardRoot.gitUsernameCache = ""
                dashboardRoot.gitPatCache = ""
                appController.cancelCommand()
            }
        }
        
    Dialog {
        id: oracleCredentialsDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: restoreMode ? "Oracle-Zugangsdaten für Restore"
                           : "Oracle-Zugangsdaten für Dump"
        standardButtons: Dialog.Ok | Dialog.Cancel

        property bool restoreMode: false

        contentItem: ColumnLayout {
            TextField {
                id: oracleSudoPasswordField
                Layout.fillWidth: true
                placeholderText: "sudo-Passwort für klassx"
                echoMode: TextInput.Password
            }

            TextField {
                id: oracleBackupPasswordField
                Layout.fillWidth: true
                placeholderText: "Passwort für BACKUPTOOL_USER"
                echoMode: TextInput.Password
            }
        }

        onAccepted: {
            dashboardRoot.oracleSudoPasswordCache = oracleSudoPasswordField.text
            dashboardRoot.oracleBackupPasswordCache = oracleBackupPasswordField.text
            oracleSudoPasswordField.clear()
            oracleBackupPasswordField.clear()

            dashboardRoot.commandRunning = true
            dashboardRoot.activeCommandButton = restoreMode
                                                ? "oracleRestore"
                                                : "oracleDump"
            commandError.text = ""

            if (restoreMode) {
                appController.restoreOracleDump()
            } else {
                appController.createOracleDump()
            }
        }

        onRejected: {
            dashboardRoot.clearOraclePasswordCache()
        }
    }

    Dialog {
        id: reinstallSudoCredentialsDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "sudo-Passwort für Reinstall"
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string commandId: ""
        property string displayCommand: ""
        property string buttonId: ""

        contentItem: ColumnLayout {
    TextField {
        id: reinstallSudoPasswordField
        Layout.fillWidth: true
        placeholderText: "sudo-Passwort für klassx"
        echoMode: TextInput.Password
    }
        }

        onAccepted: {
    dashboardRoot.oracleSudoPasswordCache = reinstallSudoPasswordField.text
    reinstallSudoPasswordField.clear()
    dashboardRoot.runPreset(commandId, displayCommand, buttonId)
        }

        onRejected: {
    reinstallSudoPasswordField.clear()
    dashboardRoot.clearOraclePasswordCache()
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

        function loadBranches() {
            console.log("Repository bestätigt: ", repoURL.text);
            selectRepo.errorMessage = ""

            if (appController.sendRepoLink(repoURL.text)) {
                selectRepo.errorMessage = ""
                selectRepo.loadingBranches = true
                appController.loadRemoteBranches(repoURL.text)
            } else {
                selectRepo.errorMessage = "Bitte einen gültigen Repository-Link einfügen!"
            }
        }

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
                onAccepted: selectRepo.loadBranches()
            }
            Button {
                enabled: !selectRepo.loadingBranches
                text: selectRepo.loadingBranches 
                      ? "Branches werden geladen ..." 
                      : "Branches laden"

                onClicked: { selectRepo.loadBranches()
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
        id: repositorySwitchDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "Repository wählen"
        standardButtons: Dialog.Cancel

        contentItem: Column {
            spacing: 8

            Repeater {
                model: appController.installedRepositories

                delegate: Button {
                    required property var modelData;
                    text: modelData.type

                    onClicked: {
                        appController.setActiveDirectory(modelData.type)
                        repositorySwitchDialog.close()
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