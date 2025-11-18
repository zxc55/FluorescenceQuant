import QtQuick 2.12
import QtQuick.Controls 2.12
import QtCharts 2.3
import App 1.0   // CurveLoader 注册所在模块

Item {
    id: root
    anchors.fill: parent

    property var record: ({})
    property var adcList: []
    signal goBack()
   // ===== 自动缩放 Y 轴 =====
    property real yMin: 0
    property real yMax: 1
    // C++ 曲线加载器
    CurveLoader { id: loader }

    // ========================= 组件加载 =========================
    Component.onCompleted: {
        Qt.callLater(tryLoad)
    }
    onRecordChanged: {
        Qt.callLater(tryLoad)
    }

    function tryLoad() {
        if (!record || !record.sampleNo)
            return
        loadCurve()
    }

    // ========================= 加载曲线 =========================
    function loadCurve() {
        adcList = mainViewModel.getAdcData(record.sampleNo)
        yMin = Math.min.apply(null, adcList)
        yMax = Math.max.apply(null, adcList)

        axisY.min = yMin
        axisY.max = yMax
        feeder.buildAndReplace(adcList)
        root.visible = false
        root.visible = true
       // chart.update()
    }

    // ========================= 顶部栏 =========================
    Rectangle {
        height: 60
        width: parent.width
        anchors.top: parent.top
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

    // ========================= 曲线图区域 =========================
    ChartView {
        id: chart
        objectName: "chartView"   // C++ 可用，不依赖 qt_chart

        anchors {
            top: parent.top
            topMargin: 60
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

     //   antialiasing: true
      //  legend.visible: true

        // X 轴
        ValueAxis {
            id: axisX
            min: 0
            max: adcList.length > 0 ? adcList.length : 500
            tickCount: 10
            titleText: "数据点"
        }

        // Y 轴
        ValueAxis {
            id: axisY
            min:  yMin
            max: yMax
            tickCount: 8
            titleText: "电压值"
        }

        // QML 的曲线不绘制（占位，让坐标轴显示）
        LineSeries {
            id: dummyCurve
            name: "占位曲线"
            axisX: axisX
            axisY: axisY
            color:  '#1a5995'
            //fvisible: true
        }
        SeriesFeeder {
            id: feeder
            series: dummyCurve

            //  onChunkDone: console.log("CHUNK", index, "SIZE", size, "ELAPSED_MS", elapsedMs)
            // onFinished: {
            //     //console.log("FEEDER_FINISHED");
            //     root.loaded();
            // }
            //  onError: console.warn(message)
        }

        // Component.onCompleted: {
        //     var t0 = Date.now();
        //     console.log("QML_COMPONENT_COMPLETED_MS:", t0);
        //     if (CHUNK > 0) {
        //         feeder.buildAndAppendChunked(N, CHUNK);
        //     } else {
        //       //  feeder.buildAndReplace(N);
        //     }
        // }
        // 最大值点
        // ScatterSeries {
        //     id: maxPoint
        //     name: "最大值"
        //     markerSize: 10
        //     color: "red"
        //     borderColor: "darkred"
        //     axisX: axisX
        //     axisY: axisY
        // }

        // // 最小值点
        // ScatterSeries {
        //     id: minPoint
        //     name: "最小值"
        //     markerSize: 10
        //     color: "blue"
        //     borderColor: "darkblue"
        //     axisX: axisX
        //     axisY: axisY
        // }

        backgroundColor: "#ffffff"
        plotAreaColor: "#ffffff"
    }


    
}
