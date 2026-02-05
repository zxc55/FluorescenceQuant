import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Item {
    id: root
    anchors.fill: parent

    property string otaStatus: "空闲"
    property bool busy: false
    property string wifiIp: ""
    Timer {
    interval: 1000
    running: true
    repeat: true
    onTriggered: {
        wifiIp = wifiController.currentIp()
     }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: parent.width * 0.75
        spacing: 20

        Label {
            text: "系统升级"
            font.pixelSize: 28
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#cccccc"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label { text: "当前版本：" }
            Label {
                text: otaManager.currentTitle + "  v" + otaManager.currentVersion
                color: "#2b6cb0"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label { text: "WiFi IP：" }
            Label {
                text: wifiIp === "" ? "未连接" : wifiIp
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 48
            radius: 6
            color: "#f7fafc"
            border.color: "#cbd5e0"

            Label {
                anchors.centerIn: parent
                text: otaStatus
            }
        }

        Button {
            text: "检查升级"
            Layout.fillWidth: true
            height: 48
            enabled: !busy

            onClicked: {
                otaStatus = "正在检查升级..."
                busy = true
                otaManager.checkUpdate()
            }
        }
    }

    Connections {
        target: otaManager

        onInfo: otaStatus = msg

        onError: {
            otaStatus = "错误：" + msg
            busy = false
        }

        onFinished: {
            otaStatus = success ? "升级流程完成" : "升级失败"
            busy = false
        }

        onVersionChanged: {
            otaStatus = "版本已更新"
        }
    }
}
