import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtCharts 2.3
import Motor 1.0
// import QtQuick.Controls 2.15
// import QtQuick.Layouts 1.15

ApplicationWindow {
    visible: true
    width: 1024
    height: 600
    title: qsTr("荧光定量检测系统")

    // —— 性能相关参数 —— //
    property int    maxPoints: 1200
    property int    xCount: 0
    property double latestValue: 0.0
    property bool   plottingEnabled: true
    property int    lastBatchSize: 0

    // 批缓存与节流刷新
    property var pendingBatch: []
    property int  flushIntervalMs: 100
    property int  drawSampleTarget: 10

    // —— 设备控制 —— //
    MotorController { id: motor }

    Timer {
        id: enableTimer
        interval: 1000
        repeat: false
        onTriggered: motor.enable()
    }
    Component.onCompleted: {
        motor.start()
        enableTimer.start()
    }

    TabBar {
        id: tabBar
        anchors.top: parent.top
        width: parent.width
        TabButton { text: "实时检测" }
        TabButton { text: "电机控制" }
    }

    StackLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: tabBar.bottom
        anchors.bottom: parent.bottom
        // ✅ 键盘出现时给底部留出空间，避免遮挡
        anchors.bottomMargin: numpad.visible ? numpad.height : 0
        currentIndex: tabBar.currentIndex

        // ========== 页 1：实时检测 ==========
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                RowLayout {
                    spacing: 10
                    Layout.alignment: Qt.AlignHCenter

                    Label { text: "脉冲数:"; font.pixelSize: 20 }
                    TextField {
                        id: autoPulse
                        objectName: "脉冲数"
                        text: "10000"
                        width: 120
                        font.pixelSize: 18
                        onActiveFocusChanged: if (activeFocus) numpad.openFor(this)
                    }

                    Label { text: "速度(RPM):"; font.pixelSize: 20 }
                    TextField {
                        id: autoRpm
                        objectName: "速度(RPM)"
                        text: "200"
                        width: 120
                        font.pixelSize: 18
                        onActiveFocusChanged: if (activeFocus) numpad.openFor(this)
                    }

                    Button {
                        text: numpad.forcedVisible ? "隐藏键盘" : "显示键盘"
                        font.pixelSize: 18
                        onClicked: {
                            if (numpad.forcedVisible) numpad.closePad()
                            else numpad.openFor(autoPulse) // 默认给“脉冲数”弹出
                        }
                    }
                }

                RowLayout {
                    spacing: 16
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true

                    Button {
                        text: "启动采集"
                        font.pixelSize: 20
                        Layout.preferredWidth: 140
                        onClicked: {
                            mainViewModel.startReading()
                            motor.runPosition(0, 3, parseInt(autoRpm.text), parseInt(autoPulse.text))
                            logBox.text += "▶️ 启动采集 + 电机运行 " + autoPulse.text + " 脉冲\n"
                        }
                    }
                    Button {
                        text: "停止采集"
                        font.pixelSize: 20
                        Layout.preferredWidth: 140
                        onClicked: {
                            mainViewModel.stopReading()
                            motor.stopMotor()
                            logBox.text += "⏹️ 停止采集 + 电机立停\n"
                        }
                    }
                    Button {
                        text: plottingEnabled ? "暂停绘制" : "继续绘制"
                        font.pixelSize: 20
                        Layout.preferredWidth: 140
                        onClicked: {
                            plottingEnabled = !plottingEnabled
                            logBox.text += plottingEnabled ? "▶️ 继续绘制\n" : "⏸️ 暂停绘制\n"
                        }
                    }
                    Button {
                        text: "清空曲线"
                        font.pixelSize: 20
                        Layout.preferredWidth: 140
                        onClicked: {
                            dataSeries.clear()
                            xCount = 0
                            pendingBatch = []
                            logBox.text += "🧹 已清空实时电压曲线\n"
                        }
                    }

                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: "当前电压: " + latestValue.toFixed(3) + " V"
                            font.pixelSize: 22
                            color: "steelblue"
                        }
                        Label {
                            text: "最近批大小: " + lastBatchSize
                            font.pixelSize: 14
                            color: "#666"
                        }
                    }
                }

                ChartView {
                    id: chart
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "通道0 实时电压曲线（UI每 " + flushIntervalMs + "ms 刷新）"
                    legend.visible: false
                    antialiasing: false
                    animationOptions: ChartView.NoAnimation
                    backgroundColor: "white"

                    ValueAxis {
                        id: axisX
                        min: 0
                        max: maxPoints
                        titleText: "采样点"
                        labelsFont.pixelSize: 14
                    }
                    ValueAxis {
                        id: axisY
                        min: 0
                        max: 3.3
                        titleText: "电压 (V)"
                        labelsFont.pixelSize: 14
                    }

                    LineSeries {
                        id: dataSeries
                        name: "CH0"
                        color: "orange"
                        width: 2
                        useOpenGL: false   // 软件渲染平台建议 false
                        axisX: axisX
                        axisY: axisY
                    }

                    // —— UI节流刷新定时器：100ms 批量绘制一次 —— //
                    Timer {
                        id: flushTimer
                        interval: flushIntervalMs
                        repeat: true
                        running: true
                        onTriggered: {
                            if (!plottingEnabled) return
                            if (!pendingBatch || pendingBatch.length === 0) return

                            var src = pendingBatch
                            var want = Math.min(drawSampleTarget, src.length)
                            var step = src.length / want
                            var i = 0
                            while (i < want) {
                                var idx = Math.floor(i * step)
                                if (idx >= src.length) idx = src.length - 1
                                xCount++
                                dataSeries.append(xCount, src[idx])
                                i++
                            }
                            pendingBatch = []

                            if (dataSeries.count > maxPoints) {
                                var toRemove = dataSeries.count - maxPoints
                                dataSeries.removePoints(0, toRemove)
                                axisX.min = Math.max(0, xCount - maxPoints)
                                axisX.max = xCount
                            }
                        }
                    }

                    // —— 连接到 ViewModel：单点 + 批量（批量只进缓存） —— //
                    Connections {
                        target: mainViewModel

                        // 单点只更新数字显示
                        onNewData: {
                            latestValue = value
                        }

                        // 后台每 50ms 一批（~25点） → 进缓存，由 flushTimer 统一绘制
                        onNewDataBatch: function(values) {
                            if (!values || values.length === 0) return
                            lastBatchSize = values.length
                            latestValue = values[values.length - 1]
                            if (!pendingBatch) pendingBatch = []
                            if (pendingBatch.length < 200) { // 保护上限，避免积压
                                for (var i = 0; i < values.length; ++i)
                                    pendingBatch.push(values[i])
                            } else {
                                pendingBatch = values.slice(-drawSampleTarget)
                            }
                        }
                    }
                }
            }
        }

        // ========== 页 2：电机控制 ==========
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                RowLayout {
                    spacing: 10
                    Layout.alignment: Qt.AlignHCenter

                    Label { text: "速度(RPM):"; font.pixelSize: 20 }
                    TextField {
                        id: rpmField
                        objectName: "速度(RPM)"
                        text: "200"
                        width: 100
                        font.pixelSize: 18
                        onActiveFocusChanged: if (activeFocus) numpad.openFor(this)
                    }
                    Button {
                        text: "正转"
                        font.pixelSize: 20
                        Layout.preferredWidth: 120
                        onClicked: motor.runSpeed(0, 3, parseInt(rpmField.text))
                    }
                    Button {
                        text: "反转"
                        font.pixelSize: 20
                        Layout.preferredWidth: 120
                        onClicked: motor.runSpeed(1, 3, parseInt(rpmField.text))
                    }
                    Button {
                        text: "立停"
                        font.pixelSize: 20
                        Layout.preferredWidth: 120
                        onClicked: motor.stopMotor()
                    }
                }

                GroupBox {
                    title: "位置模式 (定脉冲运动)"
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true

                    RowLayout {
                        spacing: 10
                        Layout.alignment: Qt.AlignHCenter
                        Layout.fillWidth: true

                        Label { text: "脉冲数:"; font.pixelSize: 20 }
                        TextField {
                            id: pulseField
                            objectName: "脉冲数"
                            text: "1000"
                            width: 120
                            font.pixelSize: 18
                            onActiveFocusChanged: if (activeFocus) numpad.openFor(this)
                        }

                        Label { text: "速度(RPM):"; font.pixelSize: 20 }
                        TextField {
                            id: posRpmField
                            objectName: "速度(RPM)"
                            text: "200"
                            width: 120
                            font.pixelSize: 18
                            onActiveFocusChanged: if (activeFocus) numpad.openFor(this)
                        }

                        Button {
                            text: "正向走"
                            font.pixelSize: 20
                            Layout.preferredWidth: 120
                            onClicked: motor.runPosition(0, 3, parseInt(posRpmField.text), parseInt(pulseField.text))
                        }
                        Button {
                            text: "反向走"
                            font.pixelSize: 20
                            Layout.preferredWidth: 120
                            onClicked: motor.runPosition(1, 3, parseInt(posRpmField.text), parseInt(pulseField.text))
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#222"
                    radius: 6
                    border.color: "gray"

                    TextArea {
                        id: logBox
                        anchors.fill: parent
                        color: "lightgreen"
                        font.pixelSize: 16
                        wrapMode: TextEdit.Wrap
                        readOnly: true
                        text: ""
                    }
                }

                Connections {
                    target: motor
                    onLogMessage: function(msg) { logBox.text += msg + "\n" }
                }
            }
        }
    }

    // ===========================
    // 软键盘（由 qml/NumberPad.qml 提供类型）
    // ===========================
    NumberPad {
        id: numpad
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        autoShow: false           // 由我们在 TextField 聚焦时显式 openFor
        allowDecimal: true
        allowNegative: true
        z: 999

        // 可选：用户按“确认”后回调
        onAccepted: {
            // text 为键盘当前文本；target 为当前绑定的输入框
            // console.log("键盘确认:", text, " -> ", target ? target.objectName : "")
        }
    }
}
