import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Item {
    id: root
    anchors.fill: parent
    visible: false

    // ===== 常量 =====
    property int slotCount: 6

    // ===== 孵育槽数据（后续由 C++ 驱动）=====
    // temp: 当前温度
    // remain: 剩余秒
    // state: 状态值
    property var slots: [
        { temp: 36.5, remain: 1800, state: 2 },
        { temp: 37.0, remain: 1200, state: 2 },
        { temp: 25.0, remain: 0,    state: 0 },
        { temp: 25.0, remain: 0,    state: 0 },
        { temp: 25.0, remain: 0,    state: 0 },
        { temp: 25.0, remain: 0,    state: 0 }
    ]

    // ===== 半透明遮罩 =====
    Rectangle {
        anchors.fill: parent
        color: "#80000000"
    }

    // ===== 主卡片 =====
    Rectangle {
        width: 900
        height: 560
        radius: 12
        color: "#ffffff"
        anchors.centerIn: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            // ---------- 标题 ----------
            Text {
                text: "孵育槽管理"
                font.pixelSize: 26
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            // ---------- 孵育槽网格 ----------
            GridLayout {
                columns: 3
                rowSpacing: 16
                columnSpacing: 16
                Layout.fillWidth: true
                Layout.fillHeight: true

                Repeater {
                    model: slotCount

                    Rectangle {
                        width: 260
                        height: 160
                        radius: 10
                        border.width: 2
                        border.color: stateColor(slots[index].state)
                        color: "#f9fafb"

                        Column {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 6

                            Text {
                                text: "孵育槽 " + (index + 1)
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Text {
                                text: "温度: " + slots[index].temp.toFixed(1) + " ℃"
                                font.pixelSize: 16
                            }

                            Text {
                                text: "剩余: " + formatTime(slots[index].remain)
                                font.pixelSize: 16
                            }

                            Text {
                                text: "状态: " + stateText(slots[index].state)
                                font.pixelSize: 16
                                color: stateColor(slots[index].state)
                            }
                        }
                    }
                }
            }

            // ---------- 返回 ----------
            Button {
                text: "返回"
                width: 200
                height: 48
                Layout.alignment: Qt.AlignHCenter
                onClicked: root.visible = false
            }
        }
    }

    // ===== 工具函数 =====
    function formatTime(sec) {
        if (sec <= 0) return "--:--"
        var m = Math.floor(sec / 60)
        var s = sec % 60
        return (m < 10 ? "0" + m : m) + ":" + (s < 10 ? "0" + s : s)
    }

    function stateText(s) {
        switch (s) {
        case 0: return "空闲"
        case 1: return "预热中"
        case 2: return "孵育中"
        case 3: return "已完成"
        case 4: return "异常"
        default: return "未知"
        }
    }

    function stateColor(s) {
        switch (s) {
        case 0: return "#9ca3af"
        case 1: return "#f59e0b"
        case 2: return "#10b981"
        case 3: return "#3b82f6"
        case 4: return "#ef4444"
        default: return "#6b7280"
        }
    }

    // ===== 页面生命周期 =====
    onVisibleChanged: {
        if (visible) {
            console.log("进入孵育槽管理界面")
        }
    }
}
