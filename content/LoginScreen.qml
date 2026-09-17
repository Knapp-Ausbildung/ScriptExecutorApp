import QtQuick 6.2
import QtQuick.Controls
import Backend


Item {

    width: 1200

    height: 700

    LoginData {
        id: loginData
    }

    Column {
        anchors.centerIn: parent
        spacing: 10

        TextField {
            id: ipAdressField
            placeholderText: "IP Adress"
        }

        TextField {
            id: passwordField
            placeholderText: "Password"
            echoMode: TextInput.Password
        }

        Button {
            text: "Login"

            onClicked: {
                loginData.saveCredentials(ipAdressField.text, passwordField.text)
            }
        }
    }
}
