import QtQuick
import Nightstand.Core

// "Pair with your phone": code + QR for the setup page the clock serves.
Item {
    id: root

    property string stepLabel
    signal manual

    readonly property string spacedCode: Pairing.code.slice(0, 3) + " " + Pairing.code.slice(3)

    Column {
        x: Theme.margin + 8
        width: 440
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -30
        spacing: 8

        Text {
            visible: root.stepLabel !== ""
            text: root.stepLabel
            color: Theme.dim
            font.pixelSize: Theme.textSmall
            font.weight: Font.DemiBold
            font.letterSpacing: 2
        }
        Text {
            text: qsTr("Pair with your phone")
            color: "white"
            font.pixelSize: Theme.textHeading
            font.weight: Font.DemiBold
            font.letterSpacing: -0.5
        }
        Text {
            width: parent.width
            text: Pairing.busy ? Pairing.busyMessage
                  : Pairing.displayUrl === "" ? qsTr("Waiting for a network connection…")
                  : qsTr("Scan the code, or open <b>%1</b> on a phone on the same Wi-Fi.").arg(Pairing.displayUrl)
            textFormat: Text.StyledText
            color: Theme.dim
            font.pixelSize: Theme.textBody
            lineHeight: 1.15
            wrapMode: Text.WordWrap
        }
        Item { width: 1; height: 12 }
        Text {
            text: qsTr("CODE")
            color: Theme.dim
            font.pixelSize: Theme.textSmall
            font.weight: Font.DemiBold
            font.letterSpacing: 2
        }
        Text {
            text: root.spacedCode
            color: "white"
            font.pixelSize: 64
            font.weight: Font.DemiBold
            font.letterSpacing: 4
            font.features: { "tnum": 1 }
        }
        Text {
            visible: Pairing.error !== ""
            width: parent.width
            text: Pairing.error
            color: Theme.error
            font.pixelSize: Theme.textSmall
            wrapMode: Text.WordWrap
        }
    }

    QrCode {
        id: qr
        x: parent.width - Theme.margin - width
        y: Theme.topBar - 8
        width: 236
        height: 236
        text: Pairing.url
        opacity: Pairing.busy ? 0.3 : 1
    }

    PillButton {
        anchors.right: parent.right
        anchors.rightMargin: Theme.margin
        y: parent.height - Theme.margin - height
        text: qsTr("Type it in here")
        onClicked: root.manual()
    }
}
