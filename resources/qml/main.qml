// qml/main.qml
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.VirtualKeyboard 2.0
import QtQuick.VirtualKeyboard.Settings 2.0
import App 1.0
import Motor 1.0
ApplicationWindow {
    id: win
    visible: true
    width: 1024
    height: 600
    color: "#f9fafb"
    title: "FluorescenceQuant"
    font.family: "Microsoft YaHei"   // 字体（根据系统可用字体调整）
    font.bold: true                  // 全部加粗
    font.pixelSize: 20               // 默认字号
    MotorController { id: motor }
    // ===== 主题 / 常量 =====
    readonly property color  brand:      "#3a7afe"
    readonly property color  textMain:   "#1f2937"
    readonly property color  textSub:    "#6b7280"
    readonly property color  line:       "#e5e7eb"
    readonly property int    radiusL:    12
    readonly property int    radiusS:    8

    // 可能由 main.cpp 注入的对象（安全判空使用）
    // property var keysObj: (typeof keys !== "undefined") ? keys : null
    property bool cardInserted: keys ? keys.inserted : false
    // 卡状态 & 弹层
   
    property bool   overlayVisible: false
    property string overlayText: ""
    property bool   overlayBusy: false

    // 当前页面（0..3）
    property int currentPage: 0

    // 虚拟键盘高度
    readonly property int kbHeight: Math.round(Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0)


    property bool testRunning: false     // 防止重复检测
    property bool motorMoving: false     // 电机运行标志
    property var originCheckTimer: Timer // 定时器对象引用
    // ==== 初始化 ====
    Component.onCompleted: {
        VirtualKeyboardSettings.activeLocales = ["en_US", "zh_CN"]
        VirtualKeyboardSettings.locale = "zh_CN"
        console.log("projectsVm.count (onCompleted) =", (typeof projectsVm !== "undefined") ? projectsVm.count : "N/A")
        motor.start()
        motor.back();
    }

    // 键盘面板
    InputPanel {
        id: panel
        z: 9999
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: Qt.inputMethod.visible
        parent: win
    }

    // 顶部栏
    Rectangle {
        id: topBar
        height: 40
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: "#ffffff"
        border.color: line
        border.width: 1

        property date now: new Date()
        Timer { interval: 1000; running: true; repeat: true; onTriggered: topBar.now = new Date() }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 12
            Rectangle { width: 10; height: 10; radius: 5; color: brand; Layout.alignment: Qt.AlignVCenter }
            Label { text: "青岛普瑞邦生物工程有限公司"; font.pixelSize: 20; color: textMain; Layout.alignment: Qt.AlignVCenter }
            Item { Layout.fillWidth: true }
            Label {
                text: Qt.formatDateTime(topBar.now, "yyyy-MM-dd  HH:mm:ss")
                color: textSub
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
            }
        }
    }

    // 背景装饰
    Rectangle {
        anchors.fill: parent; color: "transparent"; z: -1
        Rectangle {
            width: 340; height: 340; radius: 170
            anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 20
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#113a7afe" }
                GradientStop { position: 1.0; color: "#00113a7a" }
            }
            opacity: 0.08
        }
    }
    Connections {
    target: keys
    onInsertedChanged: function(on) {
         cardInserted = !!on
         console.log("插卡状态:", on)
    }
    }

function nowStr() {
    var d = new Date()
    var ms = ("00" + d.getMilliseconds()).slice(-3)
    return Qt.formatTime(d, "hh:mm:ss") + "." + ms
}

function startTest() {
    if (testRunning) {
        console.log("⚠️[" + nowStr() + "] 已在检测中，忽略重复触发")
        return
    }
    testRunning = true

    console.log("🧪[" + nowStr() + "] 回原点完成 → 延时 2 秒后启动检测")
    overlayText = "正在检测中…"
    overlayBusy = true
    overlayVisible = true

    // === 延时 2 秒后启动电机与采集 ===
    var delayTimer = Qt.createQmlObject('import QtQuick 2.0; Timer { interval:2000; repeat:false }', win)
    console.log("⏳[" + nowStr() + "] 启动延时 2000ms → 即将启动采集与电机")

    delayTimer.triggered.connect(function() {
        console.log("⏱[" + nowStr() + "] 延时结束 → 启动采集与电机")

        // === 启动 ADS1115 连续采集 ===
        mainViewModel.setCurrentSample(tfSampleId.text)
        mainViewModel.startReading()
        console.log("🧪[" + nowStr() + "] 启动连续采集")

        // === 启动电机运行 ===
        motor.runPosition(1, 0, 100, 45000)
        console.log("🚀[" + nowStr() + "] 电机开始运行")

        // === 检测电机状态直到停止 ===
        var motorCheck = Qt.createQmlObject('import QtQuick 2.0; Timer { interval:500; repeat:true }', win)
        motorCheck.triggered.connect(function() {
            var status = motor.readRegister(0xF1)
            console.log("📖[" + nowStr() + "] 电机状态 0xF1 =", status)

            if (status === 1) { // ✅ 停止状态
                motorCheck.stop()
                console.log("✅[" + nowStr() + "] 电机停止 → 停止采集")

                // === 停止采集 ===
                mainViewModel.stopReading()
                console.log("⏹[" + nowStr() + "] 停止采集")

                // === 回原点 ===
                motor.back()
                console.log("🔙[" + nowStr() + "] 回原点")

                // === 读取界面输入信息 ===
                var sampleNo = tfSampleId.text          // 样品编号
                var source   = tfSampleSource.text      // 样品来源
                var name     = tfSampleName.text        // 样品名称
                var batch    = projectsVm.getBatchById(projectPage.selectedId) // 批次编码
                var curve    = standardCurveBox.currentText  // 标准曲线
                var conc     = 0                        // 检测浓度（暂时为 0）
                var ref      = parseFloat(refValueField.text || 0)  // 参考值
                var result   = "未测"                   // 检测结果
                var time     = Qt.formatDateTime(new Date(), "yyyy-MM-dd HH:mm:ss") // 时间
                var unit     = tfLab.text               // 检测单位
                var person   = tfOperator.text          // 检测人
                var dilution = dilutionBox.currentText  // 稀释倍数

                // === 组装记录对象 ===
                var record = {
                    "projectId": projectPage.selectedId,
                    "sampleNo": sampleNo,
                    "sampleSource": source,
                    "sampleName": name,
                    "standardCurve": curve,
                    "batchCode": batch,
                    "detectedConc": conc,
                    "referenceValue": ref,
                    "result": result,
                    "detectedTime": time,
                    "detectedUnit": unit,
                    "detectedPerson": person,
                    "dilutionInfo": dilution
                }

                console.log("[DEBUG] 即将写入数据库:", JSON.stringify(record))

                // === 写入数据库 ===
                var ok = projectsVm.insertProjectInfo(record)
                console.log(ok ? "[DB] 插入成功 ✅" : "[DB] 插入失败 ❌")

                // === 界面提示完成 ===
                overlayText = ok ? "检测完成，数据已保存 ✅" : "检测完成，但数据库写入失败 ❌"
                overlayBusy = false
                overlayVisible = true
                testRunning = false
            }
        })
        motorCheck.start()
    })
    delayTimer.start()
}






 

    // 主体布局：左侧导航 + 右侧内容
    RowLayout {
        anchors {
            left: parent.left; right: parent.right
            top: topBar.bottom; bottom: parent.bottom
           // bottomMargin: kbHeight
        }
        spacing: 0

        // ===== 左侧菜单栏（保持不变）=====
        Rectangle {
            id: sideBar
            Layout.preferredWidth: 180
            Layout.fillHeight: true
            color: "#e8edf5"
            border.color: "#cfd6e2"; border.width: 1

            ButtonGroup { id: navGroup; exclusive: true }
            readonly property int padV : 12
            readonly property int gap  : 10
            readonly property real tileH : (height - 2*padV - 4*gap) / 5

            Component {
                id: pageButtonComp
                Button {
                    id: control
                    property alias label: txt.text
                    property int  idx: 0
                    property url  iconSource: ""
                    checkable: true
                    ButtonGroup.group: navGroup
                    checked: currentPage === idx
                    onClicked: currentPage = idx
                    width: parent ? parent.width : 180
                    height: sideBar.tileH
                    padding: 0
                    background: Rectangle {
                        anchors.fill: parent
                        radius: 10
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: control.checked ? "#4c86ff" : "#f5f6fa" }
                            GradientStop { position: 1.0; color: control.checked ? "#2f6ff5" : "#e7ebf3" }
                        }
                        border.color: control.checked ? "#2b5fd8" : "#cfd6e2"
                        border.width: 1
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                            height: 6; radius: 10; color: control.checked ? "#33ffffff" : "#22ffffff" }
                        Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                            height: 8; radius: 10; color: control.checked ? "#1a000000" : "#14000000" }
                    }
                    contentItem: Item {
                        anchors.fill: parent
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6
                            Image {
                                anchors.horizontalCenter: parent.horizontalCenter
                                source: control.iconSource
                                width:  parent.width  * 0.55
                                height: parent.height * 0.55
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                                visible: control.iconSource !== ""
                            }
                            Text {
                                id: txt
                                text: ""
                                font.pixelSize: 18
                                font.bold: true
                                color: control.checked ? "white" : "#374151"
                                horizontalAlignment: Text.AlignHCenter
                                anchors.horizontalCenter: parent.horizontalCenter
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            Component {
                id: actionButtonComp
                Button {
                    id: control
                    property alias label: txt.text
                    property url  iconSource: ""
                    checkable: false
                    width: parent ? parent.width : 180
                    height: sideBar.tileH
                    padding: 0
                    onClicked:{ 
                   if (!cardInserted) {
                            overlayText = "请检查检测卡位置"
                            overlayBusy = false
                            overlayVisible = true
                            return
                        }

                        console.log("检测插卡后 → 检查电机原点状态")
                        var val = motor.readRegister(0x34)
                        console.log("寄存器 0x34 值:", val)

                        if (val === 0) {
                            overlayText = "电机未在原点，正在回原点..."
                            overlayBusy = true
                            overlayVisible = true
                            motor.back()
                            originCheckTimer.start()
                            return
                        } else if (val > 0) {
                            console.log("✅ 电机在原点，先前进一段再回原点")
                            overlayText = "电机前进中..."
                            overlayBusy = true
                            overlayVisible = true
                            motor.runPosition(1, 0, 100, 10000)

                            var forwardCheck = Qt.createQmlObject('import QtQuick 2.0; Timer { interval:500; repeat:true; }', win)
                            forwardCheck.triggered.connect(function() {
                                var status = motor.readRegister(0xF1)
                                console.log("⚙️ 电机状态 0xF1 =", status)
                                if (status === 1) {
                                    forwardCheck.stop()
                                    console.log("✅ 前进完成，开始回原点")
                                    overlayText = "返回原点中..."
                                    motor.back()

                                    var backTimer = Qt.createQmlObject('import QtQuick 2.0; Timer { interval:500; repeat:true; }', win)
                                    backTimer.triggered.connect(function() {
                                        var val2 = motor.readRegister(0x34)
                                        console.log("📖 寄存器 0x34 =", val2)
                                        if (val2 === 1) {
                                            backTimer.stop()
                                            console.log("✅ 已回原点，准备开始检测")
                                            overlayText = "准备检测中..."
                                            overlayBusy = true
                                            overlayVisible = true
                                            startTest()   // ✅ 只在这里启动一次
                                        }
                                    })
                                    backTimer.start()
                                }
                            })
                            forwardCheck.start()
                        } else {
                            overlayText = "读取电机状态失败，请检查通信"
                            overlayBusy = false
                            overlayVisible = true
                        }         
                    }
                background: Rectangle {
                            anchors.fill: parent; radius: 10
                            gradient: Gradient {
                            GradientStop { position: 0.0; color: "#4c86ff" }
                            GradientStop { position: 1.0; color: "#2f6ff5" }
                           }
                            border.color: "#2b5fd8"; border.width: 1
                            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                height: 6; radius: 10; color: "#33ffffff" }
                            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                                height: 8; radius: 10; color: "#1a000000" }
                    }
                    contentItem: Item {
                        anchors.fill: parent
                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 6
                            Image {
                                anchors.horizontalCenter: parent.horizontalCenter
                                source: control.iconSource
                                width:  parent.width  * 0.55
                                height: parent.height * 0.55
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                antialiasing: true
                                visible: control.iconSource !== ""
                            }
                            Text {
                                id: txt
                                text: "开始检测"
                                font.pixelSize: 18
                                font.bold: true
                                color: "white"
                                horizontalAlignment: Text.AlignHCenter
                                anchors.horizontalCenter: parent.horizontalCenter
                            }
                        }
                    }
                }
            }

            Column {
                anchors {
                    left: parent.left; right: parent.right
                    top: parent.top; bottom: parent.bottom
                    leftMargin: 12; rightMargin: 12
                    topMargin: sideBar.padV; bottomMargin: sideBar.padV
                }
                spacing: sideBar.gap

                Loader { width: parent.width; height: sideBar.tileH; sourceComponent: pageButtonComp; onLoaded: { item.label="样品检测"; item.idx=0; item.iconSource="qrc:/resources/icons/test-tube-line.png" } }
                Loader { width: parent.width; height: sideBar.tileH; sourceComponent: pageButtonComp; onLoaded: { item.label="项目管理"; item.idx=1; item.iconSource="qrc:/resources/icons/PM.png" } }
                Loader { width: parent.width; height: sideBar.tileH; sourceComponent: pageButtonComp; onLoaded: { item.label="历史记录"; item.idx=2; item.iconSource="qrc:/resources/icons/history.png" } }
                Loader { width: parent.width; height: sideBar.tileH; sourceComponent: pageButtonComp; onLoaded: { item.label="系统设置"; item.idx=3; item.iconSource="qrc:/resources/icons/setting.png" } }
                Loader { width: parent.width; height: sideBar.tileH; sourceComponent: actionButtonComp; onLoaded: { item.iconSource="qrc:/resources/icons/start.png" } }
            }
        }

        // ===== 右侧内容 =====
        Rectangle {
            id: rightPane
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            Rectangle {
                id: card
                anchors.fill: parent; anchors.margins: 16
                radius: radiusL; color: "#ffffff"
                border.color: line; border.width: 1

                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: 10; radius: radiusL
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#00ffffff" }
                        GradientStop { position: 1.0; color: "#11000000" }
                    }
                    z: -1
                }

                StackLayout {
                    id: stack
                    anchors.fill: parent
                    anchors.margins: 18
                    currentIndex: currentPage





// ==========================
// 样品检测界面右侧
// ==========================
Item {
    id: sampleTestPage
    anchors.fill: parent
    Column {
        anchors.fill: parent
        spacing: 10

        // ===== 第一块：项目检测结果表格 =====
        Rectangle {
            id: resultTable
            width: parent.width
            height: 44 * 5
            radius: 8
            color: "#ffffff"
            border.color: "#cfd6e2"
            border.width: 1
            clip: true

            Column {
                anchors.fill: parent

                // === 表头 ===
                Rectangle {
                    id: resultHeader
                    width: parent.width
                    height: 44
                    color: "#e6e8ec"
                    border.color: "#c0c0c0"
                    Row {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        Label { text: "项目名称"; width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter; font.bold: true }
                        Label { text: "浓度(μg/kg)"; width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter; font.bold: true }
                        Label { text: "结论"; width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter; font.bold: true }
                        Label { text: "参考值(μg/kg)"; width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter; font.bold: true }
                    }
                }

                // === 内容区 ===
                Flickable {
                    id: resultFlick
                    anchors.top: resultHeader.bottom
                    width: parent.width
                    height: 44 * 4
                    contentHeight: resultColumn.height
                    clip: true

                    Column {
                        id: resultColumn
                        width: parent.width

                        // ✅ 这里以后可以替换成 C++ 模型，暂时固定3行演示
                        Repeater {
                            model: 3
                            delegate: Rectangle {
                                width: parent.width
                                height: 44
                                color: index % 2 === 0 ? "#ffffff" : "#f9fafb"
                                border.color: "#e5e7eb"
                                border.width: 1
                                Row {
                                    anchors.fill: parent
                                    spacing: 8
                                    Label { text: ""; width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                                    Label { text: ""; width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                                    Label { text: ""; width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                                    Label { text: ""; width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ===== 第二块：按钮 =====
        Row {
            width: parent.width
            height: 50
            spacing: 20
            Button {
                text: "样品信息";
                width: (parent.width - 40) / 3 
                onClicked: sampleInfoPopup.visible = true
            }
            Button { text: "详细信息"; width: (parent.width - 40) / 3 }
            Button { text: "打印"; width: (parent.width - 40) / 3 }
        }
        Rectangle { height: 6; color: "transparent" } 
        // ===== 第三块：样品信息表格（带表头） =====
        Rectangle {
            id: singleRowTable
            width: parent.width
            height: 44 * 2
            radius: 8
            color: "#ffffff"
            border.color: "#cfd6e2"
            border.width: 1
            clip: false

            Column {
                anchors.fill: parent
                spacing: 0
                // === 表头 ===
                Rectangle {
                    id: sampleHeader
                    width: parent.width
                    height: 44
                    color: "#e6e8ec"
                    border.color: "#c0c0c0"
                    Row {
                        anchors.fill: parent
                        spacing: 8
                        Label { text: "样品编号"; width: 150; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter; font.bold: true }
                        Label { text: "项目名称"; width: 150; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter; font.bold: true }
                        Label { text: "批次编码"; width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter; font.bold: true }
                        Label { text: "测试时间"; width: 300; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter; font.bold: true }
                    }
                }

                // === 内容行 ===
                Rectangle {
                    width: parent.width
                    height: 44
                    color: "#ffffff"
                    border.color: "#e5e7eb"
                    border.width: 1
                    Row {
                        anchors.fill: parent
                        spacing: 8
                        Label { text: ""; width: 150; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                        Label { text: projectsVm.getNameById(projectPage.selectedId); width: 150; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                        Label { text: projectsVm.getBatchById(projectPage.selectedId); width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                        Label { text: Qt.formatDateTime(new Date(), "yyyy-MM-dd HH:mm"); width: 300; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                    }
                }
            }
        }

            // ===== 第四块：选择框（带标题） =====
            Row {
                id: paramSelectRow
                width: parent.width
                height: 100                // ✅ 高度稍微增加，容纳标题文字
                spacing: 40
                anchors.topMargin: 8

                // === 标准曲线选择 ===
                Column {
                    spacing: 6
                    width: 220
                    Label {
                        text: "标准曲线选择"
                        font.pixelSize: 18
                        color: textMain
                        horizontalAlignment: Text.AlignHCenter
                    }
                    ComboBox {
                        id: standardCurveBox
                        width: parent.width
                        model: ["粮食谷物", "加工副产物", "配合饲料"]
                        currentIndex: 0
                        font.pixelSize: 18
                    }
                }

                // === 超曲线范围稀释 ===
                Column {
                    spacing: 6
                    width: 220
                    Label {
                        text: "超曲线范围稀释"
                        font.pixelSize: 18
                        color: textMain
                        horizontalAlignment: Text.AlignHCenter
                    }
                    ComboBox {
                        id: dilutionBox
                        width: parent.width
                        model: ["1", "5"]
                        currentIndex: 0
                        font.pixelSize: 18
                    }
                }

                // === 参考值选择 ===
                Column {
                    spacing: 6
                    width: 220
                    Label {
                        text: "参考值选择"
                        font.pixelSize: 18
                        color: textMain
                        horizontalAlignment: Text.AlignHCenter
                    }
                    TextField {
                        id: refValueField
                        width: parent.width
                        placeholderText: "参考值(μg/kg)"
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        font.pixelSize: 18
                    }
                }
            }

    }
}



                    // 1 项目管理（使用 ListView + header，消除表头与首行之间空白）
// ===== 1 项目管理（带滑动表格） =====
Item {
    id: projectPage
    property int selectedId: 1   // 当前选中行

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // === 标题栏 ===
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Label { text: "项目管理"; font.pixelSize: 24; font.bold: true; color: textMain }
            Item { Layout.fillWidth: true }
            Button { text: "刷新"; onClicked: projectsVm.refresh() }
            Button {
                text: "删除"
                enabled: projectPage.selectedId > 0
                onClicked: {
                    if (projectPage.selectedId > 0) {
                        projectsVm.deleteById(projectPage.selectedId)
                        projectPage.selectedId = -1
                        projectsVm.refresh()
                    }
                }
            }
        }

        // === 外层矩形容器 ===
        Rectangle {
            id: projArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#ffffff"
            border.color: "#d1d5db"
            border.width: 1
            clip: true

            // === 列宽定义 ===
            readonly property int colId: 60
            readonly property int colName: 220
            readonly property int colBatch: 180
            readonly property int colUpdate: 220

// === 固定表头 ===
Rectangle {
    id: header
    width: parent.width
    height: 42
    color: "#f3f4f6"
    border.color: "#d1d5db"
    border.width: 1

    Row {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Rectangle {
            width: projArea.colId
            height: parent.height
            color: "transparent"
            Label {
                anchors.centerIn: parent
                text: "序号"
                color: textMain
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Rectangle {
            width: projArea.colName
            height: parent.height
            color: "transparent"
            Label {
                anchors.centerIn: parent
                text: "项目名称"
                color: textMain
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Rectangle {
            width: projArea.colBatch
            height: parent.height
            color: "transparent"
            Label {
                anchors.centerIn: parent
                text: "批次编码"
                color: textMain
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }

        Rectangle {
            width: projArea.colUpdate
            height: parent.height
            color: "transparent"
            Label {
                anchors.centerIn: parent
                text: "更新时间"
                color: textMain
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}


            // === 内容滚动区 ===
            Flickable {
                id: flickProject    
                anchors.top: header.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                clip: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                interactive: true

                // ✅ 内容高度至少比 Flickable 高 1 像素，这样即使少数据也能拖动
                contentHeight: Math.max(contentCol.height, flickProject.height + 1)

                Column {
                    id: contentCol
                    width: flickProject.width

// === 数据行 ===
Repeater {
    id: dataRepeater
    model: projectsVm

    delegate: Rectangle {
        width: parent.width
        height: 44
        color: (projectPage.selectedId === rid)
                ? "#dbeafe"                        // 选中行浅蓝色
                : (index % 2 === 0 ? "#ffffff" : "#f9fafb") // 交替行底色
        border.color: "#e5e7eb"
        border.width: 1

        MouseArea {
            anchors.fill: parent
            onClicked: projectPage.selectedId = rid
            hoverEnabled: true
            onEntered: parent.color = (projectPage.selectedId === rid) ? "#dbeafe" : "#eef2ff"
            onExited: parent.color = (projectPage.selectedId === rid)
                    ? "#dbeafe"
                    : (index % 2 === 0 ? "#ffffff" : "#f9fafb")
        }

        Row {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            // === 左侧选择框 ===
            Rectangle {
                width: 24
                height: 24
                radius: 4
                border.color: (projectPage.selectedId === rid) ? "#3b82f6" : "#9ca3af"
                border.width: 1
                color: (projectPage.selectedId === rid) ? "#3b82f6" : "transparent"
                anchors.verticalCenter: parent.verticalCenter
                Text {
                    visible: projectPage.selectedId === rid
                    text: "✔"
                    color: "white"
                    anchors.centerIn: parent
                    font.pixelSize: 18
                }
            }

            // === 每列文字都居中 ===
            Rectangle {
                width: projArea.colId; height: parent.height; color: "transparent"
                Label {
                    anchors.centerIn: parent
                    text: rid
                    color: textMain
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Rectangle {
                width: projArea.colName; height: parent.height; color: "transparent"
                Label {
                    anchors.centerIn: parent
                    text: name
                    color: textMain
                    font.bold: true
                    elide: Label.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Rectangle {
                width: projArea.colBatch; height: parent.height; color: "transparent"
                Label {
                    anchors.centerIn: parent
                    text: batch
                    color: textMain
                    font.bold: true
                    elide: Label.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Rectangle {
                width: projArea.colUpdate; height: parent.height; color: "transparent"
                Label {
                    anchors.centerIn: parent
                    text: updatedAt
                    color: textSub
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }
}


                    // === 空白补齐行（带横线）===
                    Repeater {
                        model: Math.max(0, 8 - dataRepeater.count)
                        delegate: Rectangle {
                            width: parent.width
                            height: 44
                            color: (index % 2 === 0 ? "#ffffff" : "#f9fafb")
                            border.color: "#e5e7eb"
                            border.width: 1

                            Row {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8
                                Rectangle {
                                    width: 24; height: 24; radius: 4
                                    border.color: "#d1d5db"; border.width: 1
                                    color: "transparent"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Rectangle { width: projArea.colId; height: parent.height; color: "transparent" }
                                Rectangle { width: projArea.colName; height: parent.height; color: "transparent" }
                                Rectangle { width: projArea.colBatch; height: parent.height; color: "transparent" }
                                Rectangle { width: projArea.colUpdate; height: parent.height; color: "transparent" }
                            }
                        }
                    }
                }

                // === 滚动条 ===
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AlwaysOn
                    interactive: true
                }
            }
        }
    }
}

// ===== 2 历史记录页（带选中删除）=====
Item {
    id: historyPage
    anchors.fill: parent

    // 列宽 & 行高
    property int rowHeight: 44
    property int w_sel: 44
    property int w_id: 80
    property int w_pid: 80
    property int w_no: 120
    property int w_src: 120
    property int w_name: 140
    property int w_curve: 120
    property int w_batch: 100
    property int w_conc: 100
    property int w_ref: 100
    property int w_res: 100
    property int w_time: 160
    property int w_unit: 100
    property int w_person: 120
    property int w_dilution: 120
    property int totalWidth: w_sel + w_id + w_pid + w_no + w_src + w_name + w_curve +
                             w_batch + w_conc + w_ref + w_res + w_time +
                             w_unit + w_person + w_dilution

    // 选中集合
    property var selectedIds: []

    function isSelected(recId) {
        return selectedIds.indexOf(recId) !== -1
    }
    function setSelected(recId, on) {
        var arr = selectedIds.slice(0)
        var pos = arr.indexOf(recId)
        if (on && pos === -1) arr.push(recId)
        if (!on && pos !== -1) arr.splice(pos, 1)
        selectedIds = arr
    }
    function toggleSelected(recId) { setSelected(recId, !isSelected(recId)) }
    function selectAllOnPage(on) {
        // 遍历当前可见的 model
        for (var i = 0; i < listView.count; ++i) {
            var it = listView.itemAtIndex(i)
            if (it && it.modelId !== undefined)
                setSelected(it.modelId, on)
        }
    }
    function deleteSelected() {
        if (selectedIds.length === 0) return
        for (var i = 0; i < selectedIds.length; ++i) {
            if (historyVm && historyVm.deleteById)
                historyVm.deleteById(selectedIds[i])
        }
        selectedIds = []
        if (historyVm && historyVm.refresh) historyVm.refresh()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        // === 顶部栏 ===
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Label { text: "历史记录"; font.pixelSize: 24; font.bold: true; color: "#111827" }
            Item { Layout.fillWidth: true }
            Button { text: "刷新"; onClicked: historyVm.refresh() }
            Button {
                text: "删除选中"
                enabled: historyPage.selectedIds.length > 0
                onClicked: historyPage.deleteSelected()
            }
            Button {
                text: "导出CSV"
                onClicked: {
                    let name = "history_" + new Date().toLocaleString().replace(/[ :\/]/g, "_") + ".csv"
                    let filePath = "/mnt/SDCARD/export/" + name
                    historyVm.exportCsv(filePath)
                    console.log("[CSV] 导出:", filePath)
                }
            }
        }

        // === 表格主体 ===
        Rectangle {
            id: his_table
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 8
            color: "#ffffff"
            border.color: "#d1d5db"
            border.width: 1
            clip: true

            // === 表头（固定） ===
            Rectangle {
                id: headerBar
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: historyPage.rowHeight
                color: "#f3f4f6"
                border.color: "#d1d5db"
                border.width: 1
                clip: true

                Row {
                    id: headerRow
                    x: -bodyFlick.contentX              // 跟随内容横向滚动
                    width: historyPage.totalWidth
                    height: parent.height
                    spacing: 0

                    // 选择列（全选）
                    Rectangle {
                        width: historyPage.w_sel; height: parent.height; color: "transparent"
                        CheckBox {
                            id: cbSelectAll
                            anchors.centerIn: parent
                            tristate: false
                            checked: (historyPage.selectedIds.length > 0
                                      && historyPage.selectedIds.length === listView.count
                                      && listView.count > 0)
                            onClicked: historyPage.selectAllOnPage(checked)
                        }
                    }
                    Rectangle { width: historyPage.w_id;       height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "ID";       font.bold: true } }
                    Rectangle { width: historyPage.w_pid;      height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "项目ID";   font.bold: true } }
                    Rectangle { width: historyPage.w_no;       height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "样品编号"; font.bold: true } }
                    Rectangle { width: historyPage.w_src;      height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "样品来源"; font.bold: true } }
                    Rectangle { width: historyPage.w_name;     height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "样品名称"; font.bold: true } }
                    Rectangle { width: historyPage.w_curve;    height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "标准曲线"; font.bold: true } }
                    Rectangle { width: historyPage.w_batch;    height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "批次";     font.bold: true } }
                    Rectangle { width: historyPage.w_conc;     height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "浓度";     font.bold: true } }
                    Rectangle { width: historyPage.w_ref;      height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "参考";     font.bold: true } }
                    Rectangle { width: historyPage.w_res;      height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "结果";     font.bold: true } }
                    Rectangle { width: historyPage.w_time;     height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "检测时间"; font.bold: true } }
                    Rectangle { width: historyPage.w_unit;     height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "单位";     font.bold: true } }
                    Rectangle { width: historyPage.w_person;   height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "检测人";   font.bold: true } }
                    Rectangle { width: historyPage.w_dilution; height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: "稀释倍数"; font.bold: true } }
                }
            }

            // === 内容区 ===
            Flickable {
                id: bodyFlick
                anchors.top: headerBar.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                clip: true

                contentWidth: historyPage.totalWidth
                contentHeight: listView.contentHeight

                ListView {
                    id: listView
                    x: 0
                    y: 0
                    width: historyPage.totalWidth
                    height: bodyFlick.height
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: 0
                    model: (typeof historyVm !== "undefined" && historyVm) ? historyVm : 0

                    delegate: Rectangle {
                        // 把 model 中的 id 单独存到属性，避免和 QML 的 id 关键字混淆
                        property var modelId: id

                        width: historyPage.totalWidth
                        height: historyPage.rowHeight
                        color: historyPage.isSelected(modelId) ? "#dbeafe" :
                               (index % 2 === 0 ? "#ffffff" : "#f9fafb")
                        border.color: "#e5e7eb"
                        border.width: 1

                        Row {
                            width: parent.width
                            height: parent.height
                            spacing: 0

                            // 选择列
                            Rectangle {
                                width: historyPage.w_sel; height: parent.height; color: "transparent"
                                CheckBox {
                                    anchors.centerIn: parent
                                    checked: historyPage.isSelected(modelId)
                                    onClicked: historyPage.toggleSelected(modelId)
                                }
                            }

                            // 其余列
                            Rectangle { width: historyPage.w_id;       height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: modelId } }
                            Rectangle { width: historyPage.w_pid;      height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: projectId } }
                            Rectangle { width: historyPage.w_no;       height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: sampleNo } }
                            Rectangle { width: historyPage.w_src;      height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: sampleSource } }
                            Rectangle { width: historyPage.w_name;     height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: sampleName } }
                            Rectangle { width: historyPage.w_curve;    height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: standardCurve } }
                            Rectangle { width: historyPage.w_batch;    height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: batchCode } }
                            Rectangle { width: historyPage.w_conc;     height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: Number(detectedConc).toFixed(2) } }
                            Rectangle { width: historyPage.w_ref;      height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: Number(referenceValue).toFixed(2) } }
                            Rectangle {
                                width: historyPage.w_res; height: parent.height; color: "transparent"
                                Text { anchors.centerIn: parent; text: result; color: result === "合格" ? "green" : "red" }
                            }
                            Rectangle { width: historyPage.w_time;     height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: detectedTime } }
                            Rectangle { width: historyPage.w_unit;     height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: detectedUnit } }
                            Rectangle { width: historyPage.w_person;   height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: detectedPerson } }
                            Rectangle { width: historyPage.w_dilution; height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: dilutionInfo } }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: historyPage.toggleSelected(modelId)
                        }
                    }

                    // 无数据占位
                    Rectangle {
                        anchors.fill: parent
                        visible: listView.count === 0
                        color: "transparent"
                        Text { anchors.centerIn: parent; text: "暂无数据"; color: "#909399" }
                    }
                }

                // 滚动条
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
            }
        }
    }

    Component.onCompleted: {
        if (typeof historyVm !== "undefined" && historyVm && historyVm.refresh)
            historyVm.refresh()
    }
}
                }
            }
        }
    }
// ===== 开始检查弹层（覆盖全屏，卡片可上下微调）=====
Rectangle {
    id: overlayPopup2
    anchors.fill: parent             // 一定要覆盖整个窗口
    visible: overlayVisible          // 仍然用你的这三个变量
    color: "#CC000000"
    z: 10000                         // 保证最上层

    // 点击背景关闭（忙碌时禁用）
    MouseArea {
        anchors.fill: parent
        enabled: overlayVisible
        onClicked: { if (!overlayBusy) overlayVisible = false }
    }

    // —— 想上下挪一点，就改这个偏移量（负数上移，正数下移）——
    readonly property int centerYOffset: -30

    // 中间卡片
    Rectangle {
        id: overlayPopup2Card
        width: Math.min(parent.width - 160, 520)
        radius: 16
        color: "#ffffff"
        border.color: "#e5e7eb"; border.width: 1

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: overlayPopup2.centerYOffset
    }

    // 内容布局
    Column {
        id: overlayPopup2Content
        width: overlayPopup2Card.width - 48
        anchors.horizontalCenter: overlayPopup2Card.horizontalCenter
        anchors.verticalCenter: overlayPopup2Card.verticalCenter
        spacing: 14

        BusyIndicator {
            running: overlayBusy
            visible: overlayBusy
            width: 44; height: 44
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Label {
            text: overlayText
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: 24
            color: '#3a7afe'     
            font.bold: true      // 加粗
        }

        Label {
            visible: !overlayBusy
            text: "请点击下方“确认”继续"
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: 14
            color: textSub
        }

        Button {
            id: overlayPopup2OkBtn
            visible: !overlayBusy
            text: "确 认"
            width: 200
            height: 44
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: 18
            onClicked: overlayVisible = false

            contentItem: Text {
                text: overlayPopup2OkBtn.text
                font: overlayPopup2OkBtn.font
                color: "white"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                implicitWidth: 200
                implicitHeight: 44
                radius: 10
                border.color: "#2b5fd8"
                border.width: 1
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#4c86ff" }
                    GradientStop { position: 1.0; color: "#2f6ff5" }
                }
            }
        }
    }
}


// ===== 样品信息弹窗（优化排版版） =====
Rectangle {
    id: sampleInfoPopup
    anchors.fill: parent
    visible: false
    color: "#80000000"          // 半透明黑背景
    z: 9998                     // 稍低于键盘（键盘 z=9999）
    focus: true

    // === 弹窗主体 ===
    Rectangle {
        id: popupBox
        width: 620
        height: 460
        radius: 12
        color: "#ffffff"
        border.color: "#cfd6e2"
        anchors.centerIn: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 16

            // === 顶部标题栏 ===
            Rectangle {
                id: titleBar
                Layout.fillWidth: true
                height: 50
                radius: 8
                color: "#3a7afe"

                Label {
                    text: "样品信息"
                    anchors.centerIn: parent
                    color: "white"
                    font.pixelSize: 22
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Rectangle { height: 1; color: "#e5e7eb"; Layout.fillWidth: true }

            // === 表单区 ===
            GridLayout {
                id: formGrid
                columns: 2
                columnSpacing: 25
                rowSpacing: 18
                Layout.fillWidth: true
                Layout.topMargin: 10

                // 自动生成样品编号
                property string sampleId: {
                    let date = new Date()
                    let yyyy = date.getFullYear()
                    let mm = ("0" + (date.getMonth() + 1)).slice(-2)
                    let dd = ("0" + date.getDate()).slice(-2)
                    let seq = ("000" + Math.floor(Math.random() * 9999)).slice(-4)
                    return yyyy + mm + dd + seq
                }

                // 左列 Label 右对齐，右列 TextField 填满
                Label {
                    text: "样品编号："
                    font.pixelSize: 18
                    color: textMain
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                }
                TextField {
                    id: tfSampleId
                    text: formGrid.sampleId
                    font.pixelSize: 18
                    placeholderText: "请输入样品编号"
                    Layout.fillWidth: true
                }

                Label {
                    text: "样品名称："
                    font.pixelSize: 18
                    color: textMain
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                }
                TextField {
                    id: tfSampleName
                    font.pixelSize: 18
                    placeholderText: "请输入样品名称"
                    Layout.fillWidth: true
                }

                Label {
                    text: "样品来源："
                    font.pixelSize: 18
                    color: textMain
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                }
                TextField {
                    id: tfSampleSource
                    font.pixelSize: 18
                    placeholderText: "请输入样品来源"
                    Layout.fillWidth: true
                }

                Label {
                    text: "检测单位："
                    font.pixelSize: 18
                    color: textMain
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                }
                TextField {
                    id: tfLab
                    font.pixelSize: 18
                    placeholderText: "请输入检测单位"
                    Layout.fillWidth: true
                }

                Label {
                    text: "检测人员："
                    font.pixelSize: 18
                    color: textMain
                    horizontalAlignment: Text.AlignRight
                    Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                }
                TextField {
                    id: tfOperator
                    font.pixelSize: 18
                    placeholderText: "请输入检测人员"
                    Layout.fillWidth: true
                }
            }

            Rectangle { height: 1; color: "#e5e7eb"; Layout.fillWidth: true }

            // === 按钮区 ===
            RowLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 0       // ✅ 代替 topPadding
                spacing: 20

                Button {
                    text: "清空"
                    width: 120
                    height: 44
                    onClicked: {
                        tfSampleId.text = formGrid.sampleId
                        tfSampleName.text = ""
                        tfSampleSource.text = ""
                        tfLab.text = ""
                        tfOperator.text = ""
                    }
                }

                Button {
                    text: "完成"
                    width: 120
                    height: 44
                    onClicked: {
                        console.log("完成：", tfSampleId.text, tfSampleName.text,
                                    tfSampleSource.text, tfLab.text, tfOperator.text)
                        sampleInfoPopup.visible = false
                    }
                }

                Button {
                    text: "取消"
                    width: 120
                    height: 44
                    onClicked: sampleInfoPopup.visible = false
                }
            }

        }
    }

    // === 背景点击关闭逻辑 ===
    MouseArea {
        anchors.fill: parent
        propagateComposedEvents: false
        z: -1
        onClicked: {
            var local = mapToItem(popupBox, mouse.x, mouse.y)
            if (local.x < 0 || local.y < 0 || local.x > popupBox.width || local.y > popupBox.height) {
                sampleInfoPopup.visible = false
            }
        }
    }

    // === 内层阻止冒泡，但允许内部控件响应 ===
    MouseArea {
        anchors.fill: popupBox
        propagateComposedEvents: true
        acceptedButtons: Qt.NoButton
    }
}

// === 电机原点轮询定时器 ===
Timer {
    id: originCheckTimer
    interval: 500
    repeat: true
    running: false

    onTriggered: {
        var val = motor.readRegister(0x34)   // ✅ 改为 0x34
        console.log("轮询原点状态: 0x34 =", val)

        if (val === 1) {
            console.log("✅ 电机回原点完成 → 延时 2 秒启动检测")
            originCheckTimer.stop()
            var t = Qt.createQmlObject('import QtQuick 2.0; Timer { interval:2000; repeat:false; }', win)
            t.triggered.connect(function() {
                console.log("⏱[" + nowStr() + "] 延时 2000ms 结束 → 启动检测")
                startTest()
                t.destroy()
            })
            console.log("⏳[" + nowStr() + "] 启动延时 2000ms")
            t.start()
        } else if (val < 0) {
            overlayText = "读取电机状态失败，请检查通信"
            overlayBusy = false
            originCheckTimer.stop()
        }
    }
}


}
 