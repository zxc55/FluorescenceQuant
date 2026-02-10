import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Item {
    id: root
    width: 1024
    height: 600

    /* ========= 状态变量 ========= */
    property bool wifiEnabled: true
    property bool scanning: false
    property bool scanError: false
    property bool scanEmpty: false
    property string currentSsid: ""
    property string pendingSsid: ""
    property string pendingPassword: ""
    property int assumedKeyboardHeight: 260
    property bool keyboardShown: false
    property bool showConnectPanel: false
    signal requestConnect(string ssid)
    Timer {
        id: wifiEnableScanTimer
        interval: 10000    // 1.5 秒（经验值，rtl8188eu 很需要）
        repeat: false
        onTriggered: {
           // startScan()
        }
    }
    Timer {
    id: scanTimeoutTimer
    interval: 8000        // 8 秒超时
    repeat: false

    onTriggered: {
        if (scanning) {
            scanning = false
            scanError = true
            console.log("WiFi scan timeout")
        }
      }
    }
    /* ========= 信号强度组件 ========= */
    Component {
        id: wifiSignalComp
        Row {
            spacing: 2
            property int level: 0
            Repeater {
                model: 4
                Rectangle {
                    width: 4
                    height: 6 + index * 4
                    radius: 1
                    anchors.bottom: parent.bottom
                    color: index < level ? "#2563eb" : "#d1d5db"
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        /* ================= 顶部栏 ================= */
        Rectangle {
            height: 56
            Layout.fillWidth: true
            color: "#ffffff"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16

                Label {
                    text: "无线局域网"
                    font.pixelSize: 20
                    font.bold: true
                    Layout.fillWidth: true
                }
                Switch {
                    checked: wifiEnabled

                    onCheckedChanged: {
                        wifiEnabled = checked

                        if (checked) {

                            wifiController.turnOn()
                            
                            startScan()
                            //wifiController.statrUdcpc()
                            console.log("WiFi enabled")
                            // wifiEnableScanTimer.restart()
                        } else {
                            wifiController.turnOff()
                            console.log("WiFi disabled")
                            wifiEnableScanTimer.stop()
                            scanTimeoutTimer.stop()

                            scanning = false
                            scanError = false
                            scanEmpty = false
                            wifiModel.clear()
                        }
                    }
                }
            }
        }

        Rectangle {
            height: 1
            Layout.fillWidth: true
            color: "#e5e5e5"
        }

        /* ================= 扫描提示 / loading ================= */
        Rectangle {
            height: scanning || scanError || scanEmpty ? 44 : 0
            Layout.fillWidth: true
            visible: scanning || scanError || scanEmpty
            color: "#f9fafb"

            Row {
                anchors.centerIn: parent
                spacing: 8

                BusyIndicator {
                    running: scanning
                    visible: scanning
                    width: 20
                    height: 20
                }

                Label {
                    visible: scanning
                    text: "正在扫描 WiFi..."
                    color: "#6b7280"
                }

                Label {
                    visible: scanEmpty
                    text: "未发现可用的 WiFi 网络"
                    color: "#6b7280"
                }

                Row {
                    visible: scanError
                    spacing: 8

                    Label {
                        text: "WiFi 扫描失败"
                        color: "#dc2626"
                    }

                    Button {
                        text: "重试"
                        onClicked: {
                            startScan()
                        }
                    }
                }
            }
        }

        /* ================= WiFi 列表 ================= */
        ListView {
            id: wifiList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: wifiModel
            delegate: wifiDelegate
            visible: !scanError && !scanEmpty
        }
    }

    /* ================= 数据模型 ================= */
    ListModel {
        id: wifiModel
    }

    /* ================= 行代理 ================= */
    Component {
        id: wifiDelegate

        Rectangle {
            width: wifiList.width
            height: 52
            color: connected ? "#e0f2fe" : "white"

            RowLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Label {
                    text: ssid
                    font.pixelSize: 16
                    Layout.fillWidth: true
                    elide: Label.ElideRight
                }

                Label {
                    text: "✔"
                    visible: connected
                    color: "#2563eb"
                }

                Loader {
                    sourceComponent: wifiSignalComp
                    onLoaded: {
                        if (signal > 80) item.level = 4
                        else if (signal > 60) item.level = 3
                        else if (signal > 40) item.level = 2
                        else item.level = 1
                    }
                }

                Button {
                    text: "加入"
                    visible: !connected
                    onClicked: {
                        pendingSsid = ssid
                        wifiController.connect(ssid)
                       // showConnectPanel = true

                    }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                height: 1
                width: parent.width
                color: "#eeeeee"
            }
        }
    }

    /* ================= 密码对话框 ================= */
    // Dialog {
    //     id: wifiPwdDialog
    //         modal: true
    //         width: 480
    //         height: 300

    //         x: (root.width - width) / 2

    //         y: keyboardShown
    //         ? Math.max(20,
    //                     root.height - height - assumedKeyboardHeight - 20)
    //         : (root.height - height) / 2

    //         focus: true
    //         background: Rectangle {
    //             radius: 6
    //             color: "white"
    //             border.color: "#d1d5db"
    //         }

    //     ColumnLayout {
    //         anchors.fill: parent
    //         anchors.margins: 20
    //         spacing: 16

    //         /* ===== 标题 ===== */
    //         Label {
    //             text: "加入 WiFi"
    //             font.pixelSize: 18
    //             font.bold: true
    //             Layout.alignment: Qt.AlignHCenter
    //         }

    //         Rectangle {
    //             height: 1
    //             Layout.fillWidth: true
    //             color: "#e5e7eb"
    //         }

    //         /* ===== 网络名 ===== */
    //         ColumnLayout {
    //             Layout.fillWidth: true
    //             spacing: 6

    //             Label {
    //                 text: "网络："
    //                 color: "#6b7280"
    //                 font.pixelSize: 14
    //             }

    //             Label {
    //                 text: pendingSsid
    //                 font.pixelSize: 16
    //                 font.bold: true
    //                 elide: Label.ElideRight
    //             }
    //         }

    //         /* ===== 密码输入 ===== */
    //         ColumnLayout {
    //             Layout.fillWidth: true
    //             spacing: 6

    //             Label {
    //                 text: "密码"
    //                 color: "#6b7280"
    //                 font.pixelSize: 14
    //             }
    //         TextField {
    //             id: pwdField
    //             Layout.fillWidth: true
    //             height: 40
    //             echoMode: TextInput.Password
    //             placeholderText: "请输入 WiFi 密码"

    //             onActiveFocusChanged: {
    //                 keyboardShown = activeFocus
    //             }
    //         }
    //         }

    //         Item { Layout.fillHeight: true } // 占位，把按钮顶到底部

    //         /* ===== 按钮区 ===== */
    //         RowLayout {
    //             Layout.fillWidth: true
    //             spacing: 20

    //             Button {
    //                 text: "取消"
    //                 Layout.fillWidth: true
    //                 onClicked: {
    //                     pwdField.text = ""
    //                     wifiPwdDialog.close()
    //                 }
    //             }

    //             Button {
    //                 text: "确定"
    //                 Layout.fillWidth: true
    //                 onClicked: {
    //                     wifiController.connectWithPassword(
    //                         pendingSsid,
    //                         pwdField.text
    //                     )
    //                     pwdField.text = ""
    //                     wifiPwdDialog.close()
    //                 }
    //             }
    //         }
    //     }
    // }


    /* ================= C++ 信号 ================= */
    Connections {
        target: wifiController

        onScanFinished: {
            scanning = false
            scanError = false
            wifiModel.clear()

            currentSsid = wifiController.currentSSID()

            if (list.length === 0) {
                scanEmpty = true
                return
            }

            scanEmpty = false

            // 已连接置顶
            for (var i = 0; i < list.length; i++) {
                if (list[i].ssid === currentSsid) {
                    wifiModel.append({
                        ssid: list[i].ssid,
                        signal: list[i].signal,
                        connected: true
                    })
                }
            }
            for (var i = 0; i < list.length; i++) {
                if (list[i].ssid !== currentSsid) {
                    wifiModel.append({
                        ssid: list[i].ssid,
                        signal: list[i].signal,
                        connected: false
                    })
                }
            }
        }


        onConnected: function(ssid) {
            console.log("WiFi connected:", ssid)
            showConnectPanel = false
            startScan()
        }

        onConnectFailed_1: function(ssid, reason) {
            console.log("WiFi connect failed:", ssid, reason)

            if (ssid === pendingSsid) {
                // 无密码失败 → 弹出密码输入
                showConnectPanel = true
            }
        }
       
    }

    /* ================= 扫描函数 ================= */
    function startScan() {
        scanning = true
        scanError = false
        scanEmpty = false
        wifiController.scan()

        // 超时兜底（8 秒）
        Qt.callLater(function() {
            if (scanning) {
                scanning = false
                scanError = true
            }
        }, 8000)
    }

/* ================= 半透明遮罩 ================= */
Rectangle {
    anchors.fill: parent
    color: "#000000"
    opacity: 0.4
    visible: showConnectPanel
    z: 100

    MouseArea {
        anchors.fill: parent
        onClicked: {
            showConnectPanel = false
            keyboardShown = false
        }
    }
}
/* ================= WiFi 连接面板 ================= */
Rectangle {
    id: connectPanel
    width: 480
    height: 300
    radius: 8
    color: "white"
    visible: showConnectPanel
    z: 101

    x: (parent.width - width) / 2

    y: keyboardShown
       ? Math.max(20,
                  parent.height - height - assumedKeyboardHeight - 20)
       : (parent.height - height) / 2

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        /* ===== 标题 ===== */
        Label {
            text: "加入 WiFi"
            font.pixelSize: 18
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        Rectangle {
            height: 1
            Layout.fillWidth: true
            color: "#e5e7eb"
        }

        /* ===== 网络名 ===== */
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: "网络："
                color: "#6b7280"
                font.pixelSize: 14
            }

            Label {
                text: pendingSsid
                font.pixelSize: 16
                font.bold: true
                elide: Label.ElideRight
            }
        }

        /* ===== 密码输入 ===== */
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Label {
                text: "密码"
                color: "#6b7280"
                font.pixelSize: 14
            }

            TextField {
                id: pwdField
                Layout.fillWidth: true
                height: 40
                echoMode: TextInput.Password
                placeholderText: "请输入 WiFi 密码"

                onActiveFocusChanged: {
                    keyboardShown = activeFocus
                }
            }
        }

        Item { Layout.fillHeight: true }

        /* ===== 按钮区 ===== */
        RowLayout {
            Layout.fillWidth: true
            spacing: 16

            Button {
                text: "取消"
                Layout.fillWidth: true
                onClicked: {
                    pwdField.text = ""
                    showConnectPanel = false
                    keyboardShown = false
                }
            }

            Button {
                text: "确定"
                Layout.fillWidth: true
                onClicked: {
                    wifiController.connectWithPassword(
                        pendingSsid,
                        pwdField.text
                    )
                    pwdField.text = ""
                    showConnectPanel = false
                    keyboardShown = false
                }
            }
        }
    }
}

}
