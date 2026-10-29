import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {

    visible: true
    width: 1200
    height: 800
    minimumWidth: 960
    minimumHeight: 640
    Material.theme: Material.Dark
    Material.accent: "#35c7e4"
    Material.primary: "#102636"
    palette.window: "#0b1520"
    palette.windowText: "#e3edf5"
    palette.base: "#08121b"
    palette.text: "#e3edf5"
    palette.button: "#15394d"
    palette.buttonText: "#e4edf5"
    palette.highlight: "#0d5a70"
    palette.highlightedText: "#ffffff"

    component FlatButton: Button {
        id: flatButton

        property string iconText: ""
        property int labelPixelSize: 13
        property bool destructive: false

        leftPadding: 8
        rightPadding: 8
        topPadding: 0
        bottomPadding: 0
        implicitWidth: buttonContent.implicitWidth + leftPadding + rightPadding
        implicitHeight: 40

        contentItem: Item {
            implicitWidth: buttonContent.implicitWidth
            implicitHeight: buttonContent.implicitHeight

            RowLayout {
                id: buttonContent
                anchors.fill: parent
                spacing: flatButton.iconText.length > 0 ? 6 : 0

                Text {
                    id: buttonIcon
                    Layout.alignment: Qt.AlignVCenter
                    visible: flatButton.iconText.length > 0
                    text: flatButton.iconText
                    color: flatButton.enabled ? "#49c9e2" : "#82909d"
                    font.family: flatButton.font.family
                    font.pixelSize: Math.min(22, Math.max(14, flatButton.height * 0.42))
                }

                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignVCenter
                    text: flatButton.text
                    font.family: flatButton.font.family
                    font.pixelSize: flatButton.labelPixelSize
                    color: flatButton.enabled ? "#e4edf5" : "#82909d"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.NoWrap
                }

                Item {
                    Layout.preferredWidth: flatButton.iconText.length > 0
                                           ? buttonIcon.implicitWidth : 0
                }
            }
        }

        background: Rectangle {
            radius: 7
            color: !flatButton.enabled ? "#253542"
                  : flatButton.destructive
                    ? (flatButton.down ? "#991b1b"
                       : flatButton.hovered ? "#dc2626" : "#b91c1c")
                  : flatButton.down ? "#0d5a70"
                  : flatButton.hovered ? "#1b4c61"
                  : "#15394d"
            border.color: flatButton.focus ? "#34bfd4"
                          : flatButton.destructive ? "#7f1d1d" : "#29485b"
            border.width: 1
        }
    }

    component ThemedTextField: TextField {
        id: themedTextField

        implicitHeight: 40
        color: "#e3edf5"
        placeholderTextColor: "#8197a8"
        selectionColor: "#1b6d84"
        selectedTextColor: "#ffffff"
        font.pixelSize: 14

        background: Rectangle {
            implicitHeight: 40
            color: "#08121b"
            radius: 6
            border.color: themedTextField.activeFocus ? "#35c7e4" : "#294354"
        }
    }

    component ThemedLabel: Label {
        color: "#e3edf5"
        font.pixelSize: 14
        wrapMode: Label.Wrap
    }

    component ThemedDialog: Dialog {
        implicitWidth: Math.max(implicitContentWidth, 420)
        property Item enterFocusTarget: null

        onOpened: {
            if (enterFocusTarget) {
                enterFocusTarget.forceActiveFocus()
                return
            }

            const acceptButton = standardButton(Dialog.Ok) || standardButton(Dialog.Yes)
            if (acceptButton)
                acceptButton.forceActiveFocus()
        }

        Material.theme: Material.Dark
        Material.background: "#101f2c"
        Material.foreground: "#e3edf5"
        Material.primary: "#102636"
        Material.accent: "#35c7e4"

        background: Rectangle {
            color: "#101f2c"
            radius: 10
            border.color: "#294354"
        }
    }

    component RepositoryBranchIcon: Item {
        implicitWidth: 34
        implicitHeight: 34

        Canvas {
            anchors.fill: parent

            onPaint: {
                const context = getContext("2d")
                const unit = Math.min(width, height) / 34
                const branchX = 8 * unit
                const branchY = 22 * unit
                const branchEndX = 24 * unit
                const branchEndY = 8 * unit

                context.clearRect(0, 0, width, height)
                context.strokeStyle = "#35c7e4"
                context.fillStyle = "#101f2c"
                context.lineWidth = 2.5 * unit
                context.lineCap = "round"
                context.lineJoin = "round"

                context.beginPath()
                context.moveTo(branchX, 5 * unit)
                context.lineTo(branchX, 29 * unit)
                context.stroke()

                context.beginPath()
                context.moveTo(branchX, branchY)
                context.bezierCurveTo(
                    branchX, 13 * unit, branchEndX, 21 * unit, branchEndX, branchEndY)
                context.stroke()

                const drawNode = function(x, y) {
                    context.beginPath()
                    context.arc(x, y, 4 * unit, 0, Math.PI * 2)
                    context.fill()
                    context.stroke()
                }

                drawNode(branchX, 5 * unit)
                drawNode(branchX, 29 * unit)
                drawNode(branchEndX, branchEndY)
            }
        }
    }

    component ActionCard: Rectangle {
        id: actionCard

        property string title
        property string iconText
        property string description
        default property alias cardContent: cardLayout.data

        color: "#101f2c"
        radius: 9
        border.color: "#1d3445"
        implicitHeight: cardLayout.implicitHeight + 20

        ColumnLayout {
            id: cardLayout
            anchors.fill: parent
            anchors.margins: 9
            spacing: 6

            RowLayout {
                spacing: 8

                Text {
                    text: actionCard.iconText
                    color: "#35c7e4"
                    font.pixelSize: 20
                }

                Text {
                    text: actionCard.title
                    color: "#e3edf5"
                    font.pixelSize: 16
                    font.bold: true
                }
            }

            Text {
                Layout.fillWidth: true
                text: actionCard.description
                color: "#9bb0c0"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }
    }

    component AnimatedCommandButton: Item {
        id: control

        property string text: ""
        property string accessibleName: text
        property string iconText: ""
        property bool running: false
        property var clickHandler: null

        implicitWidth: innerButton.implicitWidth
        implicitHeight: innerButton.implicitHeight

        FlatButton {
            id: innerButton
            anchors.fill: parent
            text: control.text
            iconText: control.iconText
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
            radius: 7

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
            color: "#0b1520"

            function performLogin() {
                appController.attemptLogin(ipAddressField.text, userNameField.text, passwordField.text)
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 80, 320)
                implicitHeight: loginForm.implicitHeight + 48
                color: "#101f2c"
                radius: 12
                border.color: "#1d3445"

                ColumnLayout {
                    id: loginForm
                    anchors.fill: parent
                    anchors.margins: 24
                    spacing: 12

                    Text {
                        text: "ScriptExecutorApp"
                        color: "#e8f0f7"
                        Accessible.name: text
                        font.pixelSize: 24
                        font.bold: true
                    }

                    Text {
                        text: "Am Server anmelden"
                        color: "#9bb0c0"
                        font.pixelSize: 13
                    }

                    ThemedTextField {
                        id: ipAddressField
                        Layout.fillWidth: true
                        placeholderText: "IP-Adresse"
                        Accessible.name: placeholderText
                        onAccepted: userNameField.forceActiveFocus()
                    }

                    ThemedTextField {
                        id: userNameField
                        Layout.fillWidth: true
                        placeholderText: "Benutzername"
                        Accessible.name: placeholderText
                        onAccepted: passwordField.forceActiveFocus()
                    }

                    ThemedTextField {
                        id: passwordField
                        Layout.fillWidth: true
                        placeholderText: "Passwort"
                        Accessible.name: placeholderText
                        echoMode: TextInput.Password
                        onAccepted: loginRoot.performLogin()
                    }

                    FlatButton {
                        Layout.fillWidth: true
                        text: "Einloggen"
                        Accessible.name: text
                        iconText: "→"
                        onClicked: loginRoot.performLogin()
                    }
                }
            }
        }
    }

    Component {
        id: dashboardPage

        Rectangle {
            id: dashboardRoot
            color: "#0b1520"

            property bool commandRunning: false
            property int liveOutputChunkCount: 0

            onCommandRunningChanged: {
                if (commandRunning)
                    liveOutputChunkCount = 0
            }

            readonly property bool kiSoftOneActive:
                appController.installedRepositoryInfoLoaded &&
                appController.activeRepositoryType === "KiSoft-One"

            property string activeCommandButton: ""
            
            property string gitUsernameCache: ""
            property string gitPatCache: ""

            property string oracleSudoPasswordCache: ""
            property string oracleBackupPasswordCache: ""
            property string outputHtml: ""
            property string outputPlainText: ""
            property string outputSearchTerm: ""
            property string outputAnsiColor: ""
            property string outputTextColor: "#d4e0ea"
            property string pendingAnsiSequence: ""
            property var pendingOutputChunks: []
            property var outputSegments: []
            property real preservedOutputScrollY: 0
            property bool outputScrollToEnd: true
            property bool outputScrollNeedsRestore: false

            function escapeOutputHtml(text) {
                return String(text)
                        .replace(/&/g, "&amp;")
                        .replace(/</g, "&lt;")
                        .replace(/>/g, "&gt;")
                        .replace(/\r\n?/g, "<br/>")
                        .replace(/\n/g, "<br/>")
                        .replace(/ /g, "&nbsp;")
            }

            function formatOutputChunk(text) {
                const colors = {
                    30: "#aeb9c4", 31: "#cc0000", 32: "#00aa00", 33: "#aa7700",
                    34: "#0000cc", 35: "#aa00aa", 36: "#008888", 37: "#cccccc",
                    90: "#8a9aaa", 91: "#ff5555", 92: "#55ff55", 93: "#ffff55",
                    94: "#5555ff", 95: "#ff55ff", 96: "#55ffff", 97: "#ffffff"
                }
                const terminalColors = [
                    "#aeb9c4", "#800000", "#008000", "#808000",
                    "#000080", "#800080", "#008080", "#c0c0c0",
                    "#8a9aaa", "#ff0000", "#00ff00", "#ffff00",
                    "#0000ff", "#ff00ff", "#00ffff", "#ffffff"
                ]

                function toHex(value) {
                    const hex = Math.max(0, Math.min(255, Math.floor(value))).toString(16)
                    return hex.length < 2 ? "0" + hex : hex
                }

                function rgbColor(red, green, blue) {
                    return "#" + toHex(red) + toHex(green) + toHex(blue)
                }

                function color256(index) {
                    if (index < 16)
                        return terminalColors[index]

                    if (index < 232) {
                        const cube = index - 16
                        const levels = [0, 95, 135, 175, 215, 255]
                        return rgbColor(levels[Math.floor(cube / 36)],
                                        levels[Math.floor((cube % 36) / 6)],
                                        levels[cube % 6])
                    }

                    const gray = 8 + (index - 232) * 10
                    return rgbColor(gray, gray, gray)
                }

                const input = pendingAnsiSequence + String(text)
                pendingAnsiSequence = ""
                let color = outputAnsiColor
                let segment = ""
                let html = ""
                let plain = ""

                function flushSegment() {
                    if (segment.length === 0)
                        return

                    const escaped = escapeOutputHtml(segment)
                    html += "<span style=\"color:" +
                            (color || dashboardRoot.outputTextColor) + "\">" +
                            escaped + "</span>"
                    segment = ""
                }

                for (let i = 0; i < input.length;) {
                    if (input.charCodeAt(i) !== 0x1b) {
                        segment += input[i]
                        plain += input[i]
                        i += 1
                        continue
                    }

                    if (i + 1 >= input.length) {
                        pendingAnsiSequence = input.slice(i)
                        break
                    }

                    if (input[i + 1] !== "[") {
                        i += 1
                        continue
                    }

                    let end = i + 2
                    while (end < input.length) {
                        const code = input.charCodeAt(end)
                        if (code >= 0x40 && code <= 0x7e)
                            break
                        end += 1
                    }

                    if (end >= input.length) {
                        pendingAnsiSequence = input.slice(i)
                        break
                    }

                    flushSegment()
                    if (input[end] === "m") {
                        const values = input.slice(i + 2, end)
                        const codes = values.length === 0
                                ? [0]
                                : values.split(";").map(function(value) {
                                    return Number(value)
                                })
                        for (let j = 0; j < codes.length; j++) {
                            if (codes[j] === 0 || codes[j] === 39) {
                                color = ""
                            } else if (codes[j] === 38 && j + 1 < codes.length) {
                                if (codes[j + 1] === 5 && j + 2 < codes.length) {
                                    color = color256(codes[j + 2])
                                    j += 2
                                } else if (codes[j + 1] === 2 && j + 4 < codes.length) {
                                    color = rgbColor(codes[j + 2], codes[j + 3], codes[j + 4])
                                    j += 4
                                }
                            } else if (colors[codes[j]] !== undefined) {
                                color = colors[codes[j]]
                            }
                        }
                    }
                    i = end + 1
                }

                flushSegment()
                outputAnsiColor = color
                return { html: html, plain: plain }
            }

            function appendOutput(text, isLiveChunk) {
                if (isLiveChunk)
                    liveOutputChunkCount += 1

                pendingOutputChunks.push(String(text))
                if (pendingOutputChunks.length >= 64)
                    flushOutputBuffer()
                else if (!outputFlushTimer.running)
                    outputFlushTimer.start()
            }

            function flushOutputBuffer() {
                outputFlushTimer.stop()
                if (pendingOutputChunks.length === 0)
                    return

                const outputFlickable = commandOutputScroll.contentItem
                const previousScrollY = outputFlickable.contentY
                const wasAtBottom = previousScrollY >= Math.max(
                            0, outputFlickable.contentHeight - outputFlickable.height - 2)
                const outputIsFiltered = outputSearchTerm.trim().length > 0
                const shouldStayAtBottom = outputIsFiltered
                        ? outputScrollToEnd : wasAtBottom
                const pendingOutput = pendingOutputChunks.join("")
                pendingOutputChunks = []
                const formattedChunks = []
                let appendedHtml = ""
                let appendedPlainText = ""
                for (let offset = 0; offset < pendingOutput.length;) {
                    let end = Math.min(offset + 16000, pendingOutput.length)
                    if (end < pendingOutput.length) {
                        const lastCodeUnit = pendingOutput.charCodeAt(end - 1)
                        if (lastCodeUnit >= 0xd800 && lastCodeUnit <= 0xdbff)
                            end -= 1
                    }

                    const formattedChunk = formatOutputChunk(pendingOutput.slice(offset, end))
                    if (formattedChunk.html.length > 0 || formattedChunk.plain.length > 0) {
                        formattedChunks.push(formattedChunk)
                        appendedHtml += formattedChunk.html
                        appendedPlainText += formattedChunk.plain
                    }
                    offset = end
                }

                outputSegments = outputSegments.concat(formattedChunks)
                outputHtml += appendedHtml
                outputPlainText += appendedPlainText
                const outputTruncated = outputPlainText.length > 150000
                if (outputTruncated) {
                    while (outputPlainText.length > 125000 && outputSegments.length > 0) {
                        const removedSegment = outputSegments.shift()
                        outputPlainText = outputPlainText.slice(removedSegment.plain.length)
                    }
                    outputHtml = outputSegments.map(function(segment) {
                        return segment.html
                    }).join("")
                }

                if (outputIsFiltered) {
                    refreshDisplayedOutput(false)
                } else if (outputTruncated) {
                    commandOutput.text = outputHtml
                } else if (appendedHtml.length > 0) {
                    commandOutput.insert(commandOutput.length, appendedHtml)
                }

                preservedOutputScrollY = previousScrollY
                outputScrollToEnd = shouldStayAtBottom
                outputScrollNeedsRestore = outputTruncated ||
                        (outputIsFiltered && !shouldStayAtBottom)
                if (outputScrollToEnd || outputScrollNeedsRestore)
                    outputScrollTimer.restart()
            }

            function finishOutput() {
                flushOutputBuffer()
                outputScrollToEnd = !outputVerticalScrollBar.pressed
                if (outputScrollToEnd)
                    outputScrollTimer.restart()
            }

            function syncOutputScrollBars() {
                const flickable = commandOutputScroll.contentItem
                const verticalContentHeight = Math.max(flickable.contentHeight, flickable.height)
                const horizontalContentWidth = Math.max(flickable.contentWidth, flickable.width)
                outputVerticalScrollBar.size = verticalContentHeight > 0
                        ? flickable.height / verticalContentHeight : 1
                outputVerticalScrollBar.position = Math.max(
                            0, Math.min(1 - outputVerticalScrollBar.size,
                                        flickable.visibleArea.yPosition))
                outputHorizontalScrollBar.size = horizontalContentWidth > 0
                        ? flickable.width / horizontalContentWidth : 1
                outputHorizontalScrollBar.position = Math.max(
                            0, Math.min(1 - outputHorizontalScrollBar.size,
                                        flickable.visibleArea.xPosition))
            }

            function outputIsAtBottom() {
                const flickable = commandOutputScroll.contentItem
                return flickable.contentY >= Math.max(
                            0, flickable.contentHeight - flickable.height - 2)
            }

            function clearOutput() {
                outputFlushTimer.stop()
                outputScrollTimer.stop()
                pendingOutputChunks = []
                outputSegments = []
                outputHtml = ""
                outputPlainText = ""
                outputAnsiColor = ""
                pendingAnsiSequence = ""
                preservedOutputScrollY = 0
                outputScrollToEnd = true
                outputScrollNeedsRestore = false
                commandOutput.text = "<span></span>"
                commandOutputScroll.contentItem.contentX = 0
                commandOutputScroll.contentItem.contentY = 0
                syncOutputScrollBars()
            }

            function refreshDisplayedOutput(resetScroll) {
                const searchTerm = outputSearchTerm.trim().toLowerCase()
                if (searchTerm.length === 0) {
                    commandOutput.text = outputHtml
                } else {
                    const matchingLines = outputPlainText.split(/\r?\n/).filter(function(line) {
                        return line.toLowerCase().includes(searchTerm)
                    })
                    commandOutput.text = "<span style=\"color:" + outputTextColor + "\">" +
                            escapeOutputHtml(matchingLines.join("\n")) + "</span>"
                }

                if (resetScroll === false)
                    return

                outputScrollToEnd = true
                outputScrollNeedsRestore = false
                commandOutputScroll.contentItem.contentX = 0
                outputScrollTimer.restart()
            }

            Timer {
                id: outputFlushTimer
                interval: 50
                repeat: false
                onTriggered: dashboardRoot.flushOutputBuffer()
            }

            Timer {
                id: outputScrollTimer
                interval: 16
                repeat: false
                onTriggered: {
                    const outputFlickable = commandOutputScroll.contentItem
                    if (outputVerticalScrollBar.pressed)
                        return

                    if (dashboardRoot.outputScrollToEnd) {
                        outputFlickable.contentY = Math.max(
                                    0, outputFlickable.contentHeight - outputFlickable.height)
                    } else if (dashboardRoot.outputScrollNeedsRestore) {
                        outputFlickable.contentY = Math.min(
                                    dashboardRoot.preservedOutputScrollY,
                                    Math.max(0, outputFlickable.contentHeight -
                                             outputFlickable.height))
                    }
                    dashboardRoot.outputScrollNeedsRestore = false
                    dashboardRoot.syncOutputScrollBars()
                }
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

            function requestBranchSwitch() {
                if (commandRunning)
                    return

                commandRunning = true
                activeCommandButton = "branchSwitch"
                commandError.text = ""
                selectBranchChoice.model = []
                selectBranchChoice.currentIndex = -1
                appController.loadInstalledRepositoryBranches()
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
                anchors.margins: 0
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    color: "#102636"
                    radius: 0

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 12
                        spacing: 12

                        ColumnLayout {
                            spacing: 1

                            Text {
                                text: "ScriptExecutorApp"
                                color: "#e8f0f7"
                                font.pixelSize: 19
                                font.bold: true
                            }

                            Text {
                                text: "Verwalten. Automatisieren. Entlasten."
                                color: "#9bb0c0"
                                font.pixelSize: 11
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            Layout.preferredWidth: 9
                            Layout.preferredHeight: 9
                            radius: 5
                            color: appController.loggedIn ? "#22c55e" : "#ef4444"
                        }

                        Text {
                            text: appController.loggedIn ? "Eingeloggt" : "Ausgeloggt"
                            color: "#d7e2eb"
                        }

                        FlatButton {
                            text: "Ausloggen"
                            Accessible.name: text
                            onClicked: appController.attemptLogout()
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.preferredHeight: appController.installedRepositories.length > 1
                                            ? 166 : 120
                    color: "#101f2c"
                    radius: 9
                    border.color: "#1d3445"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 14

                        RepositoryBranchIcon {
                            Layout.preferredWidth: 34
                            Layout.preferredHeight: 34
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: "AKTUELLES REPOSITORY"
                                color: "#9bb0c0"
                                font.pixelSize: 10
                                font.letterSpacing: 0.7
                            }

                            Text {
                                Layout.fillWidth: true
                                text: !appController.installedRepositoryInfoLoaded
                                      ? "Repository wird geladen ..."
                                      : appController.installedRepository.length > 0
                                        ? dashboardRoot.repositoryDisplayName(appController.installedRepository)
                                        : "Noch kein Repository geladen"
                                color: "#e8f0f7"
                                font.pixelSize: 17
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Text {
                                visible: appController.installedRepository.length > 0
                                text: "Aktueller Branch: " + appController.installedBranch
                                color: "#a9bac8"
                                font.pixelSize: 11
                            }
                        }

                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.fillHeight: true
                            color: "#294354"
                        }

                        ColumnLayout {
                            Layout.preferredWidth: 320
                            Layout.minimumWidth: 320
                            Layout.maximumWidth: 320
                            Layout.fillHeight: true
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Rectangle {
                                    Layout.preferredWidth: 34
                                    Layout.preferredHeight: 34
                                    radius: 17
                                    color: appController.installedRepository.length > 0
                                           ? "#16a05a" : "#b87920"

                                    Text {
                                        anchors.centerIn: parent
                                        text: appController.installedRepository.length > 0 ? "✓" : "!"
                                        color: "#ffffff"
                                        font.pixelSize: 19
                                        font.bold: true
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Text {
                                        text: appController.installedRepository.length > 0
                                              ? "Repository bereit" : "Repository nicht geladen"
                                        color: "#e3edf5"
                                        font.bold: true
                                    }

                                    Text {
                                        text: appController.installedRepositoryInfoLoaded
                                              ? (appController.installedRepository.length > 0
                                                 ? "KiSoft-One Repository aktiv"
                                                 : "Bitte Repository einrichten")
                                              : "Lade Repository-Status ..."
                                        color: "#9bb0c0"
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                FlatButton {
                                    Layout.fillWidth: true
                                    text: "Branch wechseln"
                                    iconText: "⇄"
                                    labelPixelSize: 12
                                    Accessible.name: text
                                    enabled: !dashboardRoot.commandRunning &&
                                             appController.installedRepository.length > 0 &&
                                             dashboardRoot.kiSoftOneActive
                                    onClicked: dashboardRoot.requestBranchSwitch()
                                }

                                FlatButton {
                                    Layout.fillWidth: true
                                    text: "Repository öffnen"
                                    iconText: "↗"
                                    labelPixelSize: 12
                                    Accessible.name: text
                                    visible: appController.installedRepository.length > 0
                                    onClicked: appController.openInstalledRepository()
                                }
                            }

                            FlatButton {
                                Layout.fillWidth: true
                                text: "Repository wählen"
                                iconText: "▤"
                                Accessible.name: text
                                visible: appController.installedRepositories.length > 1
                                enabled: !dashboardRoot.commandRunning
                                onClicked: repositorySwitchDialog.open()
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.bottomMargin: 2
                    spacing: 12

                    ScrollView {
                        id: actionPanel
                        Layout.preferredWidth: 450
                        Layout.minimumWidth: 400
                        Layout.maximumWidth: 500
                        Layout.fillHeight: true
                        clip: true
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded

                        ColumnLayout {
                            width: actionPanel.availableWidth
                            height: Math.max(actionPanel.availableHeight, implicitHeight)
                            spacing: 8

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                implicitHeight: repositoryActionsContent.implicitHeight + 20
                                color: "#101f2c"
                                radius: 9
                                border.color: "#1d3445"

                                ColumnLayout {
                                    id: repositoryActionsContent
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    spacing: 6

                                    RowLayout {
                                        spacing: 8

                                        RepositoryBranchIcon {
                                            Layout.preferredWidth: 24
                                            Layout.preferredHeight: 24
                                        }

                                        Text {
                                            text: "Repository Aktionen"
                                            color: "#e3edf5"
                                            font.pixelSize: 16
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        text: "Verwalte das Repository und Branches."
                                        color: "#9bb0c0"
                                        font.pixelSize: 11
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        spacing: -3

                                        RowLayout {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.margins: 0

                                            FlatButton {
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                Layout.margins: 0
                                                text: "Setup Repository"
                                                iconText: "▤"
                                                enabled: !dashboardRoot.commandRunning
                                                Accessible.name: "Import Branch"
                                                onClicked: selectRepo.open()
                                            }
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.margins: 0
                                            spacing: 8

                                            FlatButton {
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                Layout.minimumWidth: 0
                                                Layout.preferredWidth: 1
                                                Layout.margins: 0
                                                text: "Fetch Branch"
                                                iconText: "☁"
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

                                            FlatButton {
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                Layout.minimumWidth: 0
                                                Layout.preferredWidth: 1
                                                Layout.margins: 0
                                                text: "Pull Branch"
                                                iconText: "↓"
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
                                                    appController.pullSelectedBranch()
                                                }
                                            }
                                        }

                                    }
                                }
                            }
                  
                            ActionCard {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                title: "Datenbank Aktionen"
                                iconText: "▤"
                                description: "Sichere, lade und rebuild deine Datenbank."

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: -3

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.margins: 0
                                        spacing: 8

                                        AnimatedCommandButton {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.minimumWidth: 0
                                            Layout.preferredWidth: 1
                                            Layout.margins: 0
                                            text: "Dump DB"
                                            iconText: "▤"
                                            Accessible.name: text
                                            running: dashboardRoot.activeCommandButton === "oracleDump"
                                            enabled: !dashboardRoot.commandRunning
                                            clickHandler: function() {
                                                oracleCredentialsDialog.restoreMode = false
                                                oracleCredentialsDialog.open()
                                            }
                                        }

                                        AnimatedCommandButton {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.minimumWidth: 0
                                            Layout.preferredWidth: 1
                                            Layout.margins: 0
                                            text: "Load dumped DB"
                                            iconText: "↑"
                                            Accessible.name: text
                                            running: dashboardRoot.activeCommandButton === "oracleRestore"
                                            enabled: !dashboardRoot.commandRunning
                                            clickHandler: function() {
                                                if (!dashboardRoot.commandRunning)
                                                    restoreConfirmation.open()
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.margins: 0
                                        spacing: 8

                                        AnimatedCommandButton {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.margins: 0
                                            text: "Rebuild Database"
                                            iconText: "↻"
                                            Accessible.name: text
                                            running: dashboardRoot.activeCommandButton === "rebuildDb"
                                            enabled: !dashboardRoot.commandRunning &&
                                                     dashboardRoot.kiSoftOneActive
                                            clickHandler: function() {
                                                dashboardRoot.runPreset("rebuildDb",
                                                    "Datenbank wird rebuilded ...", "rebuildDb")
                                            }
                                        }
                                    }
                                }
                            }

                            ActionCard {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                title: "Installation"
                                iconText: "◇"
                                description: "Installiere oder repariere Komponenten."

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    spacing: -3

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.margins: 0
                                        spacing: 8

                                        AnimatedCommandButton {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.minimumWidth: 0
                                            Layout.preferredWidth: 1
                                            Layout.margins: 0
                                            text: "Reinstall Database"
                                            iconText: "▤"
                                            Accessible.name: text
                                            running: dashboardRoot.activeCommandButton === "reinstallDb"
                                            enabled: !dashboardRoot.commandRunning &&
                                                     dashboardRoot.kiSoftOneActive
                                            clickHandler: function() {
                                                dashboardRoot.runPresetWithSudoPassword(
                                                    "reinstallDb",
                                                    "Datenbank wird reinstalliert ...",
                                                    "reinstallDb")
                                            }
                                        }

                                        AnimatedCommandButton {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.minimumWidth: 0
                                            Layout.preferredWidth: 1
                                            Layout.margins: 0
                                            text: "Reinstall KiSoft"
                                            iconText: "◇"
                                            Accessible.name: text
                                            running: dashboardRoot.activeCommandButton === "reinstallKiSoft"
                                            enabled: !dashboardRoot.commandRunning &&
                                                     dashboardRoot.kiSoftOneActive
                                            clickHandler: function() {
                                                dashboardRoot.runPresetWithSudoPassword(
                                                    "reinstallKiSoft",
                                                    "KiSoft wird reinstalliert ...",
                                                    "reinstallKiSoft")
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        Layout.margins: 0
                                        spacing: 8

                                        AnimatedCommandButton {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.margins: 0
                                            text: "Reinstall All"
                                            iconText: "▶"
                                            Accessible.name: text
                                            running: dashboardRoot.activeCommandButton === "reinstallAll"
                                            enabled: !dashboardRoot.commandRunning &&
                                                     dashboardRoot.kiSoftOneActive
                                            clickHandler: function() {
                                                dashboardRoot.runPresetWithSudoPassword(
                                                    "reinstallAll", "Alles wird reinstalliert",
                                                    "reinstallAll")
                                            }
                                        }
                                    }
                                }
                            }

                            ActionCard {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                title: "Service Steuerung"
                                iconText: "⚙"
                                description: "Steuere und überwache laufende Dienste."

                                GridLayout {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    columns: 3
                                    columnSpacing: 8
                                    rowSpacing: columnSpacing

                                    AnimatedCommandButton {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        text: "Status"
                                        iconText: "i"
                                        Accessible.name: text
                                        running: dashboardRoot.activeCommandButton === "status"
                                        enabled: !dashboardRoot.commandRunning &&
                                                 dashboardRoot.kiSoftOneActive
                                        clickHandler: function() {
                                            dashboardRoot.runPreset("status", "qking", "status")
                                        }
                                    }

                                    AnimatedCommandButton {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        text: "Restart"
                                        iconText: "↻"
                                        Accessible.name: text
                                        running: dashboardRoot.activeCommandButton === "restart"
                                        enabled: !dashboardRoot.commandRunning &&
                                                 dashboardRoot.kiSoftOneActive
                                        clickHandler: function() {
                                            dashboardRoot.runPreset("restart", "restart", "restart")
                                        }
                                    }

                                    AnimatedCommandButton {
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        text: "Stop"
                                        iconText: "■"
                                        Accessible.name: text
                                        running: dashboardRoot.activeCommandButton === "stop"
                                        enabled: !dashboardRoot.commandRunning &&
                                                 dashboardRoot.kiSoftOneActive
                                        clickHandler: function() {
                                            dashboardRoot.runPreset("stop", "stop", "stop")
                                        }
                                    }
                                }
                            }

                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: 420
                        color: "#101f2c"
                        radius: 9
                        border.color: "#1d3445"
                        border.width: 1

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 9

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Text {
                                    text: "▤"
                                    color: "#35c7e4"
                                    font.pixelSize: 24
                                }

                                ColumnLayout {
                                    spacing: 2

                                    Text {
                                        text: "Live Konsole"
                                        color: "#e3edf5"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }

                                    Text {
                                        text: "Echtzeit-Ausgabe der Skript-Befehle"
                                        color: "#9bb0c0"
                                        font.pixelSize: 11
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Item {
                                    Layout.fillWidth: true
                                }

                                FlatButton {
                                    text: "Befehl abbrechen"
                                    Accessible.name: text
                                    destructive: true
                                    visible: dashboardRoot.commandRunning
                                    onClicked: appController.cancelCommand()
                                }

                                FlatButton {
                                    text: "Ausgabe leeren"
                                    Accessible.name: text
                                    onClicked: dashboardRoot.clearOutput()
                                }

                                FlatButton {
                                    text: "Logs öffnen"
                                    Accessible.name: text
                                    onClicked: appController.openLogsFolder()
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                ThemedTextField {
                                    id: outputSearchField
                                    Layout.fillWidth: true
                                    placeholderText: "In Logs suchen ..."
                                    placeholderTextColor: "#76899a"
                                    Accessible.name: placeholderText
                                    color: "#e3edf5"
                                    selectByMouse: true
                                    onTextChanged: {
                                        dashboardRoot.outputSearchTerm = text
                                        dashboardRoot.refreshDisplayedOutput()
                                    }
                                    background: Rectangle {
                                        color: "#142536"
                                        radius: 6
                                        border.color: outputSearchField.activeFocus
                                                       ? "#35c7e4" : "#1d3445"
                                    }
                                }

                                FlatButton {
                                    text: "Suche leeren"
                                    Accessible.name: text
                                    enabled: outputSearchField.text.length > 0
                                    onClicked: outputSearchField.clear()
                                }
                            }

                            Text {
                                id: commandError
                                Layout.fillWidth: true
                                visible: text.length > 0
                                color: "#ff7777"
                                wrapMode: Text.Wrap
                            }

                            Rectangle {
                                id: commandOutputViewport
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.minimumHeight: 150
                                color: "#08121b"
                                radius: 6
                                border.color: "#1d3445"

                                ScrollView {
                                    id: commandOutputScroll
                                    anchors.left: parent.left
                                    anchors.leftMargin: 8
                                    anchors.top: parent.top
                                    anchors.topMargin: 8
                                    anchors.right: outputVerticalScrollBar.left
                                    anchors.rightMargin: 4
                                    anchors.bottom: outputHorizontalScrollBar.top
                                    anchors.bottomMargin: 4
                                    clip: true
                                    LayoutMirroring.enabled: false
                                    LayoutMirroring.childrenInherit: true
                                    contentWidth: commandOutput.width
                                    contentHeight: commandOutput.height

                                    ScrollBar.vertical: ScrollBar {
                                        policy: ScrollBar.AlwaysOff
                                    }

                                    ScrollBar.horizontal: ScrollBar {
                                        policy: ScrollBar.AlwaysOff
                                    }

                                    TextArea {
                                        id: commandOutput
                                        width: Math.max(commandOutputScroll.availableWidth, contentWidth)
                                        height: Math.max(commandOutputScroll.availableHeight, contentHeight)
                                        textFormat: TextEdit.RichText
                                        text: "<span></span>"
                                        font.family: "monospace"
                                        color: dashboardRoot.outputTextColor
                                        readOnly: true
                                        selectByMouse: true
                                        wrapMode: TextEdit.NoWrap
                                        background: null
                                    }
                                }

                                ScrollBar {
                                    id: outputVerticalScrollBar
                                    parent: commandOutputViewport
                                    anchors.top: commandOutputScroll.top
                                    anchors.right: commandOutputViewport.right
                                    anchors.rightMargin: 5
                                    anchors.bottom: outputHorizontalScrollBar.top
                                    width: 12
                                    orientation: Qt.Vertical
                                    policy: ScrollBar.AlwaysOn
                                    onPositionChanged: {
                                        if (pressed) {
                                            const flickable = commandOutputScroll.contentItem
                                            dashboardRoot.outputScrollToEnd = false
                                            dashboardRoot.outputScrollNeedsRestore = false
                                            flickable.contentY = Math.min(
                                                        Math.max(0, flickable.contentHeight -
                                                                 flickable.height),
                                                        position * flickable.contentHeight)
                                        }
                                    }
                                    onPressedChanged: {
                                        if (pressed) {
                                            dashboardRoot.outputScrollToEnd = false
                                            dashboardRoot.outputScrollNeedsRestore = false
                                        } else {
                                            dashboardRoot.outputScrollToEnd =
                                                    dashboardRoot.outputIsAtBottom()
                                            if (dashboardRoot.outputScrollNeedsRestore)
                                                outputScrollTimer.restart()
                                        }
                                    }
                                }

                                ScrollBar {
                                    id: outputHorizontalScrollBar
                                    parent: commandOutputViewport
                                    anchors.left: commandOutputScroll.left
                                    anchors.right: outputVerticalScrollBar.left
                                    anchors.bottom: commandOutputViewport.bottom
                                    height: 12
                                    orientation: Qt.Horizontal
                                    policy: ScrollBar.AlwaysOn
                                    visible: size < 1
                                    onPositionChanged: {
                                        if (pressed) {
                                            const flickable = commandOutputScroll.contentItem
                                            flickable.contentX = Math.min(
                                                        Math.max(0, flickable.contentWidth -
                                                                 flickable.width),
                                                        position * flickable.contentWidth)
                                        }
                                    }
                                }

                                Connections {
                                    target: commandOutputScroll.contentItem
                                    function onContentXChanged() {
                                        dashboardRoot.syncOutputScrollBars()
                                    }
                                    function onContentYChanged() {
                                        dashboardRoot.syncOutputScrollBars()
                                    }
                                    function onMovementStarted() {
                                        dashboardRoot.outputScrollToEnd = false
                                    }
                                    function onMovementEnded() {
                                        dashboardRoot.outputScrollToEnd =
                                                dashboardRoot.outputIsAtBottom()
                                    }
                                    function onContentWidthChanged() {
                                        dashboardRoot.syncOutputScrollBars()
                                    }
                                    function onContentHeightChanged() {
                                        dashboardRoot.syncOutputScrollBars()
                                        if (dashboardRoot.outputScrollToEnd ||
                                                dashboardRoot.outputScrollNeedsRestore)
                                            outputScrollTimer.restart()
                                    }
                                    function onWidthChanged() {
                                        dashboardRoot.syncOutputScrollBars()
                                    }
                                    function onHeightChanged() {
                                        dashboardRoot.syncOutputScrollBars()
                                    }
                                }
                            }

                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                    Layout.bottomMargin: 8
                    Layout.preferredHeight: 34
                    color: "#101f2c"
                    radius: 7
                    border.color: "#1d3445"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12

                        Text {
                            id: footerDateTime
                            property date currentTime: new Date()
                            text: Qt.formatDateTime(currentTime, "dd.MM.yyyy HH:mm") +
                                  "  ·  v" + Qt.application.version
                            color: "#9bb0c0"
                        }

                        Timer {
                            interval: 60000
                            repeat: true
                            running: true
                            onTriggered: footerDateTime.currentTime = new Date()
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Text {
                            text: dashboardRoot.commandRunning ? "Befehl läuft ..." : "Bereit"
                            color: dashboardRoot.commandRunning ? "#f5c46a" : "#9bb0c0"
                        }
                    }
                }
            }
            Connections {
                target: appController

                function onCommandCompleted(standardOutput, standardError, exitStatus) {
                    if (dashboardRoot.liveOutputChunkCount === 0 &&
                            standardOutput.length > 0) {
                        dashboardRoot.appendOutput(standardOutput)
                    }

                    dashboardRoot.finishOutput()
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
                    commandError.text = message
                    dashboardRoot.appendOutput("\n[Fehler] " + message + "\n")
                    dashboardRoot.finishOutput()
                    dashboardRoot.commandRunning = false
                    dashboardRoot.activeCommandButton= ""
                    dashboardRoot.clearOraclePasswordCache()
                }

                function onCommandCancelled() {
                    commandError.text = ""
                    dashboardRoot.appendOutput("\n[Befehl abgebrochen]\n")
                    dashboardRoot.finishOutput()
                    dashboardRoot.commandRunning = false
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
                    const askForUpdateParameterTables =
                            /execute prepared updates\s*\(y\/n\)\?:?\s*$/i.test(prompt.trim())
                    
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
            ThemedDialog {
                id: commandInputDialog
                anchors.centerIn: Overlay.overlay
                modal: true
                title: secret ? "Passwort eingeben" : "Eingabe benötigt"
                standardButtons: Dialog.Ok | Dialog.Cancel
                enterFocusTarget: commandInputField

                property string prompt: ""
                property bool secret: false

                contentItem: Column {
                    spacing: 8
                }

                ThemedLabel {
                    text: commandInputDialog.prompt
                }

                ThemedTextField {
                    id: commandInputField
                    placeholderText: commandInputDialog.secret
                                    ? "Passwort eingeben" : "Eingabe hier ..."
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

        ThemedDialog {
            id: restoreConfirmation
            anchors.centerIn: Overlay.overlay
            modal: true
            title: "Datenbank wiederherstellen?"
            standardButtons: Dialog.Yes | Dialog.No

        ThemedLabel {
            text: "Der Restore löscht die Schemas LU, AFCONFIG, ARCHIVE, KLASSX, KXGUI, KXREPORT und USER_MANAGEMENT vollständig und importiert sie neu. Dabei werden vorhandene Daten überschrieben. Fortfahren?"
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
            dashboardRoot.appendOutput(chunk, true)
        }
    }  
    ThemedDialog {
        id: gitCredentialsDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "Git-Zugangdaten"
        standardButtons: Dialog.Ok | Dialog.Cancel
        enterFocusTarget: gitUsernameField

        property string prompt: ""
        property string requestedField: ""
        property string errorMessage: ""
        
        contentItem: Column {
            spacing: 8

            ThemedLabel {
                text: gitCredentialsDialog.prompt
            }
            ThemedTextField {
                id: gitUsernameField
                width: 300
                placeholderText: "Git-Username"
                onAccepted: gitPatField.forceActiveFocus()
            }

            ThemedTextField {
                id: gitPatField
                width: 300
                placeholderText: "Personal Access Token"
                echoMode: TextInput.Password
                onAccepted: gitCredentialsDialog.accept()
            }

            ThemedLabel {
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
        
    ThemedDialog {
        id: oracleCredentialsDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: restoreMode ? "Oracle-Zugangsdaten für Restore"
                           : "Oracle-Zugangsdaten für Dump"
        standardButtons: Dialog.Ok | Dialog.Cancel
        enterFocusTarget: oracleSudoPasswordField

        property bool restoreMode: false

        contentItem: ColumnLayout {
            ThemedTextField {
                id: oracleSudoPasswordField
                Layout.fillWidth: true
                placeholderText: "sudo-Passwort für klassx"
                echoMode: TextInput.Password
                onAccepted: oracleBackupPasswordField.forceActiveFocus()
            }

            ThemedTextField {
                id: oracleBackupPasswordField
                Layout.fillWidth: true
                placeholderText: "Passwort für BACKUPTOOL_USER"
                echoMode: TextInput.Password
                onAccepted: oracleCredentialsDialog.accept()
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

    ThemedDialog {
        id: reinstallSudoCredentialsDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "sudo-Passwort für Reinstall"
        standardButtons: Dialog.Ok | Dialog.Cancel
        enterFocusTarget: reinstallSudoPasswordField

        property string commandId: ""
        property string displayCommand: ""
        property string buttonId: ""

        contentItem: ColumnLayout {
            ThemedTextField {
                id: reinstallSudoPasswordField
                Layout.fillWidth: true
                placeholderText: "sudo-Passwort für klassx"
                echoMode: TextInput.Password
                onAccepted: reinstallSudoCredentialsDialog.accept()
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
        onOpened: loginFailedOkButton.forceActiveFocus()
        background: Rectangle {
            color: "#101f2c"
            radius: 10
            border.color: "#294354"
        }

        Column {
            anchors.centerIn: parent
            spacing: 10

            ThemedLabel {
                text: "Login Failed!"
                Accessible.name: "Login failed"
            }

            FlatButton {
                id: loginFailedOkButton
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

    ThemedDialog {
        id: hostKeyDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: qsTr("SSH-Host-Key bestätigen")
        standardButtons: Dialog.Yes | Dialog.No

        property string fingerprint: ""

        contentItem: ThemedLabel {
            text: qsTr("Prüfe den Fingerprint über einen vertrauenswürdigen Weg:\n\n%1")
                       .arg(hostKeyDialog.fingerprint)
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

    ThemedDialog {
        id: selectRepo
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "Repository festlegen"

        property string repo: ""
        property string errorMessage: ""
        property bool loadingBranches: false
        enterFocusTarget: repoURL

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

            ThemedLabel {
                text: "Repository Link"
            }

            Row {
                spacing: 8
            ThemedTextField {
                width: 400
                id: repoURL
                placeholderText: "Link hier einfügen!"
                onAccepted: selectRepo.loadBranches()
            }
            FlatButton {
                implicitWidth: 160
                enabled: !selectRepo.loadingBranches
                text: selectRepo.loadingBranches 
                      ? "Branches werden geladen ..." 
                      : "Branches laden"

                onClicked: { selectRepo.loadBranches()
                }
            }
        
        }
            ThemedLabel {
                text: selectRepo.errorMessage
                color: "red"
                visible: text.length > 0
            }
        }
    }
    
    ThemedDialog {
        id: selectBranchDialog
        title: "Bitte Branch auswählen"
        anchors.centerIn: Overlay.overlay
        modal: true
        enterFocusTarget: selectBranchChoice

        function confirmSelectedBranch() {
            if (selectBranchChoice.currentIndex < 0)
                return

            if (appController.selectRemoteBranch(selectBranchChoice.currentText)) {
                selectBranchDialog.close()
                replaceRepositoryDialog.open()
            }
        }

        contentItem: Column {
            spacing: 8

            ComboBox {
                width: 300
                id: selectBranchChoice
                model: []
                currentIndex: -1
                onAccepted: selectBranchDialog.confirmSelectedBranch()
            }

            FlatButton {
                id: confirmBranch
                width: 300
                enabled: selectBranchChoice.currentIndex >= 0
                text: "Branch auswählen"
                Accessible.name: "Branch auswählen"

                onClicked: selectBranchDialog.confirmSelectedBranch()
            }

      }
    }

    ThemedDialog {
        id: repositorySwitchDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "Repository wählen"
        standardButtons: Dialog.Cancel
        enterFocusTarget: repositoryChoices.count > 0
                          ? repositoryChoices.itemAt(0) : null

        contentItem: Column {
            spacing: 8

            Repeater {
                id: repositoryChoices
                model: appController.installedRepositories

                delegate: FlatButton {
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

    ThemedDialog {
        id: replaceRepositoryDialog
        anchors.centerIn: Overlay.overlay
        modal: true
        title: "Repository vollständig ersetzen?"
        standardButtons: Dialog.Yes | Dialog.No

        contentItem: ThemedLabel {
            text: "Ziel: /kisoft/user/testing\n\n" + 
                  "ACHTUNG! Es werden ALLE bereits existierenden Dateien und lokalen Änderungen gelöscht \n" +
                  "und anschließend durch den ausgewählten Remote-Branch ersetzt! \n" + 
                  "Dies kann nicht wieder hergestellt werden! \n\n Fortfahren?"
        }

        onAccepted: appController.replaceSelectedRepository()
    }
}