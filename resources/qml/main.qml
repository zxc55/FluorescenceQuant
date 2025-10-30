import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtCharts 2.3
import Motor 1.0      // ✅ 电机控制 C++ 类

ApplicationWindow {
    visible: true
    width: 1024
    height: 600
    title: qsTr("荧光定量检测系统")

    property int maxPoints: 100
    property int xCount: 0
    property double latestValue: 0

    // ===== 电机控制对象 =====
    MotorController {
        id: motor
    }

    // ✅ 自动使能延迟定时器
    Timer {
        id: enableTimer
        interval: 1000
        repeat: false
        onTriggered: motor.enable()
    }

    // ✅ 程序启动自动启动电机线程并延时使能驱动
    Component.onCompleted: {
        console.log("🌟 系统启动：自动开启电机线程并使能驱动")
        motor.start()
        enableTimer.start()
    }

    // ===== 顶部 Tab 导航 =====
    TabBar {
        id: tabBar
        anchors.top: parent.top
        width: parent.width
        TabButton { text: "实时检测" }
        TabButton { text: "电机控制" }
    }

    // ===== 主内容区域 =====
    StackLayout {
        anchors.fill: parent
        anchors.topMargin: tabBar.height
        currentIndex: tabBar.currentIndex

        // ----------------------
        // 页面1：实时检测 + 电机联动
        // ----------------------
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                // ⚙️ 自动运动参数输入
                RowLayout {
                    spacing: 10
                    Layout.alignment: Qt.AlignHCenter
                    Label { text: "脉冲数:"; font.pixelSize: 20 }
                    TextField { id: autoPulse; text: "10000"; width: 120; font.pixelSize: 18 }
                    Label { text: "速度(RPM):"; font.pixelSize: 20 }
                    TextField { id: autoRpm; text: "200"; width: 120; font.pixelSize: 18 }
                }

                // ⚙️ 启动 / 停止采集 + 清空曲线
                RowLayout {
                    spacing: 20
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true

                    Button {
                        text: "启动采集"
                        font.pixelSize: 20
                        Layout.preferredWidth: 160
                        onClicked: {
                            mainViewModel.startReading()
                            motor.runPosition(0, 3,
                                              parseInt(autoRpm.text),
                                              parseInt(autoPulse.text))
                            logBox.text += "▶️ 启动采集 + 电机运行 "
                                         + autoPulse.text + " 脉冲\n"
                        }
                    }

                    Button {
                        text: "停止采集"
                        font.pixelSize: 20
                        Layout.preferredWidth: 160
                        onClicked: {
                            mainViewModel.stopReading()
                            motor.stopMotor()
                            logBox.text += "⏹️ 停止采集 + 电机立停\n"
                        }
                    }

                    Button {
                        text: "清空曲线"
                        font.pixelSize: 20
                        Layout.preferredWidth: 160
                        onClicked: {
                            dataSeries.clear()
                            xCount = 0
                            logBox.text += "🧹 已清空实时电压曲线\n"
                        }
                    }

                    Label {
                        text: "当前电压: " + latestValue.toFixed(3) + " V"
                        font.pixelSize: 24
                        color: "steelblue"
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                // 📈 实时曲线
                ChartView {
                    id: chart
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "通道0 实时电压曲线"
                    legend.visible: false
                    antialiasing: true
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
                        useOpenGL: true
                        axisX: axisX
                        axisY: axisY
                    }

                    Connections {
                        target: mainViewModel
                        onNewData: {
                            latestValue = value
                            xCount++
                            dataSeries.append(xCount, value)
                            if (dataSeries.count > maxPoints) {
                                dataSeries.removePoints(0, dataSeries.count - maxPoints)
                                axisX.min = xCount - maxPoints
                                axisX.max = xCount
                            }
                        }
                    }
                }
            }
        }

        // ----------------------
        // 页面2：电机控制界面
        // ----------------------
        Item {
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 20

                // -------------------------
                // 手动速度控制
                // -------------------------
                RowLayout {
                    spacing: 10
                    Layout.alignment: Qt.AlignHCenter

                    Label { text: "速度(RPM):"; font.pixelSize: 20 }
                    TextField { id: rpmField; text: "200"; width: 100; font.pixelSize: 18 }

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

                // -------------------------
                // 位置模式控制（定脉冲）
                // -------------------------
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
                            text: "正向走"
                            font.pixelSize: 20
                            Layout.preferredWidth: 120
                            onClicked: motor.runPosition(0, 3,
                                                         parseInt(posRpmField.text),
                                                         parseInt(pulseField.text))
                        }

                        Button {
                            text: "反向走"
                            font.pixelSize: 20
                            Layout.preferredWidth: 120
                            onClicked: motor.runPosition(1, 3,
                                                         parseInt(posRpmField.text),
                                                         parseInt(pulseField.text))
                        }
                    }
                }

                Button {
                    text: "关闭驱动"
                    font.pixelSize: 20
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 200
                    onClicked: motor.disable()
                }

                // 日志输出框
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
                    onLogMessage: function(msg) {
                        logBox.text += msg + "\n"
                    }
                }
            }
        }
    }
}
