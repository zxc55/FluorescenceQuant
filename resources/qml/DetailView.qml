import QtQuick 2.12
import QtQuick.Controls 2.12
import QtCharts 2.3

Item {
    id: root
    anchors.fill: parent

    property var record: ({})
    signal goBack()

    // ===== 批次绘制相关 =====
    property var adcList: []
    property int batchIndex: 0
    property int batchSize: 100

    // ===== 自动缩放 Y 轴 =====
    property real yMin: 0
    property real yMax: 1

    Component.onCompleted: Qt.callLater(tryLoad)
    onRecordChanged: Qt.callLater(tryLoad)

    // ===== 试图加载数据 =====
    function tryLoad() {
        if (!record || !record.sampleNo) return
        loadCurve()
    }

    // ===== 批量加载曲线 =====
    Timer {
        id: addBatchPoints
        interval: 1
        repeat: true
        onTriggered: {
            var end = Math.min(batchIndex + batchSize, adcList.length)
            for (var i=batchIndex; i<end; i++) {
                curve.append(i, adcList[i])
            }
            batchIndex = end

            if (batchIndex >= adcList.length) {
                console.log("🎉 曲线绘制完成:", adcList.length)
                stop()
                drawMaxMinPoints()
            }
        }
    }

    // ===== 加载曲线数据 =====
    function loadCurve() {
        adcList = mainViewModel.getAdcData(record.sampleNo)
        console.log("📊 数据点数:", adcList.length)

        curve.clear()
        maxPoint.clear()
        minPoint.clear()

        if (adcList.length === 0) return

        // 自动缩放 Y 轴
        yMin = Math.min.apply(null, adcList)
        yMax = Math.max.apply(null, adcList)

        axisY.min = yMin
        axisY.max = yMax

        batchIndex = 0
        addBatchPoints.start()
    }

    // ===== 最大值 / 最小值 点 =====
    function drawMaxMinPoints() {
        if (adcList.length === 0) return

        var maxVal = Math.max.apply(null, adcList)
        var minVal = Math.min.apply(null, adcList)

        var maxIndex = adcList.indexOf(maxVal)
        var minIndex = adcList.indexOf(minVal)

        maxPoint.append(maxIndex, maxVal)
        minPoint.append(minIndex, minVal)

        console.log("⭐ 最大值:", maxVal, "索引:", maxIndex)
        console.log("⭐ 最小值:", minVal, "索引:", minIndex)
    }

    // ===== 顶部栏 =====
    Rectangle {
        height: 60
        anchors.left: parent.left
        anchors.right: parent.right
        color: "#f0f2f5"

        Row {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 20

            Button {
                text: "← 返回"
                font.pixelSize: 22
                onClicked: root.goBack()
            }

            Label {
                text: "样品：" + record.sampleNo
                font.pixelSize: 24
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    // ===== 曲线图 =====
    ChartView {
        id: chart
        anchors {
            top: parent.top
            topMargin: 60
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        antialiasing: true
        legend.visible: true

        // ============= X 轴（自动）================
        ValueAxis {
            id: axisX
            min: 0
            max: adcList.length > 0 ? adcList.length : 500
            tickCount: 10
            titleText: "数据点"
        }

        // ============= Y 轴（自动缩放）============
        ValueAxis {
            id: axisY
            min: yMin
            max: yMax
            tickCount: 8
            titleText: "电压值"
        }

        // ============= 主曲线 =====================
        LineSeries {
            id: curve
            name: "ADC 曲线"
            color: "#3a7afe"
            axisX: axisX
            axisY: axisY
        }

        // ============= 最大值点 ===================
        ScatterSeries {
            id: maxPoint
            name: "最大值"
            markerSize: 12
            color: "red"
            borderColor: "darkred"
            axisX: axisX
            axisY: axisY
        }

        // ============= 最小值点 ===================
        ScatterSeries {
            id: minPoint
            name: "最小值"
            markerSize: 12
            color: "blue"
            borderColor: "darkblue"
            axisX: axisX
            axisY: axisY
        }

        // ============= 网格线 =====================
        backgroundColor: "#ffffff"
        plotAreaColor: "#ffffff"

    }
}
