import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Item {
    id: root
    anchors.fill: parent
    visible: true
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
         // ★关键：吞掉所有鼠标/触摸事件，防止点到后面界面
        MouseArea {
            id: blocker                         // 拦截器
            anchors.fill: parent                // 覆盖整个遮罩
            acceptedButtons: Qt.AllButtons      // 接受所有鼠标按键（左/右/中）
            hoverEnabled: true                  // 允许 hover（不重要，但能更稳定）
            preventStealing: true               // 防止被 Flickable/SwipeView 抢事件（很关键）
            propagateComposedEvents: false      // 不向下传播组合事件（避免穿透）

            onPressed:  { mouse.accepted = true }  // 按下就吃掉
            onReleased: { mouse.accepted = true }  // 松开也吃掉
            onClicked:  { mouse.accepted = true }  // 点击吃掉（这里不做任何事）
            onWheel:    { wheel.accepted = true }  // 滚轮也吃掉（避免下面列表滚动）
        }
    }
    function incubStateText(active) {
        return active ? "孵育中" : "空闲"
    }

    function incubStateColor(active) {
        return active ? "#10b981" : "#9ca3af"
    }
    function isIncubDone(index) {
        switch (index) {
        case 0: return deviceService.status.incubPos1 && deviceService.status.incubRemain1 === 0
        case 1: return deviceService.status.incubPos2 && deviceService.status.incubRemain2 === 0
        case 2: return deviceService.status.incubPos3 && deviceService.status.incubRemain3 === 0
        case 3: return deviceService.status.incubPos4 && deviceService.status.incubRemain4 === 0
        case 4: return deviceService.status.incubPos5 && deviceService.status.incubRemain5 === 0
        case 5: return deviceService.status.incubPos6 && deviceService.status.incubRemain6 === 0
        }
        return false
    }

    function incubBorderColorByIndex(index) {
        if (isIncubDone(index))
            return "#ef4444"      // 🔴 孵育结束：红色

        switch (index) {
        case 0: return incubStateColor(deviceService.status.incubPos1)
        case 1: return incubStateColor(deviceService.status.incubPos2)
        case 2: return incubStateColor(deviceService.status.incubPos3)
        case 3: return incubStateColor(deviceService.status.incubPos4)
        case 4: return incubStateColor(deviceService.status.incubPos5)
        case 5: return incubStateColor(deviceService.status.incubPos6)
        }
    }
    // ===== 主卡片 =====
    Rectangle {
        width: 900
        height: 560
        radius: 12
        color: "#ffffff"
        anchors.centerIn: parent
        /* ===== 右上角实时温度 ===== */
        Text {
            id: realtimeTempText
            text: "实时温度："
                + deviceService.status.currentTemp.toFixed(1)
                + " ℃"

            font.pixelSize: 16
            font.bold: true
            color: "#111827"

            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 16
            anchors.rightMargin: 20
        }
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
                                color: "#f9fafb"
                                border.width: 2
                                border.color: incubBorderColorByIndex(index)

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 8

                                    // ===== 标题 =====
                                    Text {
                                        text: "孵育槽 " + (index + 1)
                                        font.pixelSize: 18
                                        font.bold: true
                                    }

                                    // ===== 状态 =====
                                    Text {
                                        text: {
                                            if (isIncubDone(index))
                                            return "孵育结束"
                                            switch (index) {
                                            case 0: return incubStateText(deviceService.status.incubPos1)
                                            case 1: return incubStateText(deviceService.status.incubPos2)
                                            case 2: return incubStateText(deviceService.status.incubPos3)
                                            case 3: return incubStateText(deviceService.status.incubPos4)
                                            case 4: return incubStateText(deviceService.status.incubPos5)
                                            case 5: return incubStateText(deviceService.status.incubPos6)
                                            }
                                        }
                                        color: isIncubDone(index)? "#ef4444": incubBorderColorByIndex(index)
                                        font.pixelSize: 16
                                        font.bold: isIncubDone(index)
                                    }

                                    // ===== 剩余时间 =====
                                    Text {
                                        text: {
                                            var active = false
                                            var remain = 0

                                            switch (index) {
                                            case 0:
                                                active = deviceService.status.incubPos1
                                                remain = deviceService.status.incubRemain1
                                                break
                                            case 1:
                                                active = deviceService.status.incubPos2
                                                remain = deviceService.status.incubRemain2
                                                break
                                            case 2:
                                                active = deviceService.status.incubPos3
                                                remain = deviceService.status.incubRemain3
                                                break
                                            case 3:
                                                active = deviceService.status.incubPos4
                                                remain = deviceService.status.incubRemain4
                                                break
                                            case 4:
                                                active = deviceService.status.incubPos5
                                                remain = deviceService.status.incubRemain5
                                                break
                                            case 5:
                                                active = deviceService.status.incubPos6
                                                remain = deviceService.status.incubRemain6
                                                break
                                            }

                                            return active
                                                ? ("剩余时间：" + (remain > 0 ? formatTime(remain) : "00:00"))
                                                : "剩余时间：--:--"
                                        }

                                        font.pixelSize: 16
                                        color: "#374151"
                                    }
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