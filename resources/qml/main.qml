import QtQuick 2.12
import QtQuick.Controls 2.12
import QtCharts 2.3
import QtQuick.Layouts 1.12

ApplicationWindow {
    visible: true
    width: 1024
    height: 600
    title: qsTr("荧光定量检测 - 通道0 实时曲线")

    property int maxPoints: 100     // 显示的最大点数
    property int xCount: 0          // 当前采样编号
    property double latestValue: 0  // 当前电压值

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10   // ✅ Qt 5.12 兼容写法
        spacing: 10

        // ===== 顶部按钮区 =====
        RowLayout {
            spacing: 20
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true

            Button {
                text: "启动采集"
                font.pixelSize: 20
                Layout.preferredWidth: 160
                onClicked:mainViewModel.startReading();
            }

            Button {
                text: "停止采集"
                font.pixelSize: 20
                Layout.preferredWidth: 160
                onClicked:mainViewModel.stopReading();   
            }

            Label {
                text: "当前电压: " + latestValue.toFixed(3) + " V"
                font.pixelSize: 24
                color: "steelblue"
                Layout.alignment: Qt.AlignVCenter
            }
        }

        // ===== 图表显示区 =====
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
