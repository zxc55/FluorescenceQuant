import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtCharts 2.3
import Motor 1.0

ApplicationWindow {
    visible: true
    width: 1024
    height: 600
    title: qsTr("荧光定量检测系统")

    // —— 性能相关参数 ——
    property int    maxPoints: 1200       // 建议 800~1500
    property int    xCount: 0
    property double latestValue: 0.0
    property bool   plottingEnabled: true
    property int    lastBatchSize: 0

    // 批缓存与节流刷新
    property var pendingBatch: []         // 暂存后台推来的点
    property int  flushIntervalMs: 100    // 100ms 刷一次 UI
    property int  drawSampleTarget: 10    // 每次最多画 ~10 个点（从批里均匀抽样）

    MotorController { id: motor }

    Timer {
        id: enableTimer
        interval: 1000
        repeat: false
        onTriggered: motor.enable()
    }
    Component.onCompleted: { motor.start(); enableTimer.start() }

    TabBar {
        id: tabBar
        anchors.top: parent.top
        width: parent.width
        TabButton { text: "实时检测" }
        TabButton { text: "电机控制" }
    }

    StackLayout {
        anchors.fill: parent
        anchors.topMargin: tabBar.height
        currentIndex: tabBar.currentIndex

        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                RowLayout {
                    spacing: 10
                    Layout.alignment: Qt.AlignHCenter
                    Label { text: "脉冲数:"; font.pixelSize: 20 }
                    TextField { id: autoPulse; text: "10000"; width: 120; font.pixelSize: 18 }
                    Label { text: "速度(RPM):"; font.pixelSize: 20 }
                    TextField { id: autoRpm; text: "200"; width: 120; font.pixelSize: 18 }
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
                        // 如果是软渲染或 linuxfb，建议设为 false；若有 GPU 可设 true
                        useOpenGL: false
                        axisX: axisX
                        axisY: axisY
                    }

                    // —— UI节流刷新定时器：100ms 批量绘制一次 ——
                    Timer {
                        id: flushTimer
                        interval: flushIntervalMs
                        repeat: true
                        running: true
                        onTriggered: {
                            if (!plottingEnabled) return
                            if (!pendingBatch || pendingBatch.length === 0) return

                            // 降采样：把 pendingBatch 均匀抽成 ~drawSampleTarget 个点
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
                            // 清空缓存
                            pendingBatch = []

                            // 控制滚动窗口
                            if (dataSeries.count > maxPoints) {
                                var toRemove = dataSeries.count - maxPoints
                                dataSeries.removePoints(0, toRemove)
                                axisX.min = Math.max(0, xCount - maxPoints)
                                axisX.max = xCount
                            }
                        }
                    }

                    // —— 连接到 ViewModel：单点 + 批量（批量只进缓存） ——
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
                            // 只入缓存，不直接画
                            // 注意：不要无限长积压，做个上限保护
                            if (!pendingBatch) pendingBatch = []
                            if (pendingBatch.length < 200) { // 大约最多缓存 200 点
                                for (var i = 0; i < values.length; ++i)
                                    pendingBatch.push(values[i])
                            } else {
                                // 缓存爆了就丢弃旧的，保最新（避免内存增长）
                                pendingBatch = values.slice(-drawSampleTarget)
                            }
                        }
                    }
                }
            }
        }

        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                RowLayout {
                    spacing: 10
                    Layout.alignment: Qt.AlignHCenter
                    Label { text: "速度(RPM):"; font.pixelSize: 20 }
                    TextField { id: rpmField; text: "200"; width: 100; font.pixelSize: 18 }
                    Button { text: "正转";  font.pixelSize: 20; Layout.preferredWidth: 120;
                        onClicked: motor.runSpeed(0, 3, parseInt(rpmField.text)) }
                    Button { text: "反转";  font.pixelSize: 20; Layout.preferredWidth: 120;
                        onClicked: motor.runSpeed(1, 3, parseInt(rpmField.text)) }
                    Button { text: "立停";  font.pixelSize: 20; Layout.preferredWidth: 120;
                        onClicked: motor.stopMotor() }
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
                        TextField { id: pulseField; text: "1000"; width: 120; font.pixelSize: 18 }
                        Label { text: "速度(RPM):"; font.pixelSize: 20 }
                        TextField { id: posRpmField; text: "200"; width: 120; font.pixelSize: 18 }
                        Button {
                            text: "正向走"; font.pixelSize: 20; Layout.preferredWidth: 120
                            onClicked: motor.runPosition(0, 3, parseInt(posRpmField.text), parseInt(pulseField.text))
                        }
                        Button {
                            text: "反向走"; font.pixelSize: 20; Layout.preferredWidth: 120
                            onClicked: motor.runPosition(1, 3, parseInt(posRpmField.text), parseInt(pulseField.text))
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#222"; radius: 6; border.color: "gray"
                    // 提醒：TextArea 高频拼接很耗时，如需频繁日志，可 500ms 合并一次写入
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
}
