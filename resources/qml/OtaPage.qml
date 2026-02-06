import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Item {
    id: root
    anchors.fill: parent

    // ===== 状态属性 =====
    property bool showUpgradeConfirm: false
    property bool upgrading: false
    property bool busy: false

    property string newVersion: ""
    property string newTitle: ""

    property string otaStatus: "空闲"
    property string wifiIp: ""

    // ===== 定时刷新 WiFi IP =====
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: {
            wifiIp = wifiController.currentIp()
        }
    }

    // ================= 主界面 =================
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
         //   enabled: !busy && !upgrading

            onClicked: {
                if (wifiController.isWifiConnected())
                {
                otaStatus = "正在检查升级..."
                busy = true
                otaManager.checkUpdate()
                }
                else
                {
                otaStatus = "请先连接 WiFi"
                }
            }
        }
    }

    // ================= 升级确认 / 升级中弹窗 =================
    Rectangle {
        id: upgradeMask
        anchors.fill: parent
        visible: showUpgradeConfirm || upgrading
        color: "#80000000"
        z: 999

        // 吃掉所有触摸，防止点穿
        MouseArea {
            anchors.fill: parent
        }

        Rectangle {
            width: parent.width * 0.65
            height: 220
            radius: 10
            color: "#ffffff"
            anchors.centerIn: parent

            Column {
                anchors.centerIn: parent
                spacing: 18

                Label {
                    text: upgrading ? "正在升级" : "发现新版本"
                    font.pixelSize: 22
                    font.bold: true
                }

                Label {
                    text: upgrading
                          ? "升级中，请等待\n请勿断电"
                          : (newTitle === "" ? "v" + newVersion
                                             : newTitle + "  v" + newVersion)
                    horizontalAlignment: Text.AlignHCenter
                    color: upgrading ? "#d9534f" : "#333333"
                    font.pixelSize: 16
                }

                // ===== 按钮区（仅确认阶段显示）=====
                Row {
                    spacing: 30
                    visible: !upgrading
                    anchors.horizontalCenter: parent.horizontalCenter

                    Button {
                        text: "升级"
                        width: 100
                        height: 40

                        onClicked: {
                            // 先切 UI 状态
                            showUpgradeConfirm = false
                            upgrading = true
                            busy = true
                            text = "升级中，请勿断电"
                            otaManager.requestStartUpdate()
                         //   otaManager.startUpdateAsync()           
                     //   otaManager.startUpdate()                
                        }
                    }

                    Button {
                        text: "取消"
                        width: 100
                        height: 40

                        onClicked: {
                            showUpgradeConfirm = false
                            busy = false
                            otaStatus = "已取消升级"
                        }
                    }
                }
            }
        }
    }

    // ================= OTA 信号处理 =================
    Connections {
        target: otaManager

        onInfo: {
            otaStatus = msg
        }

        onError: {
            otaStatus = "错误：" + msg
            busy = false
            upgrading = false
            showUpgradeConfirm = false
        }

        onFinished: {
            otaStatus = success ? "升级完成，正在重启..." : "升级失败"
            busy = false
            upgrading = false
            showUpgradeConfirm = false
        }

        onVersionChanged: {
            otaStatus = "版本已更新"
        }

        onUpdateAvailable: function(version) {
            newVersion = version
            // 如果你没有 remoteTitle，可以不赋值
            // newTitle = otaManager.remoteTitle
           // newTitle = ""
            showUpgradeConfirm = true
            busy = false
            upgrading = false
            otaStatus = "发现新版本,升级后请勿断电"
        }

        onNoUpdate: {
            otaStatus = "当前已是最新版本"
           // busy = false
        }
    }
}
