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
    property var selectedHistoryItem: {}   // 当前选中的历史记录
    onCurrentPageChanged: console.log("📌 切换页面 currentPage =", currentPage)
    property var uvadcList: []
    // ===== 主题 / 常量 =====
    readonly property color  brand:      "#3a7afe"
    readonly property color  textMain:   "#1f2937"
    readonly property color  textSub:    "#6b7280"
    readonly property color  line:       "#e5e7eb"
    readonly property int    radiusL:    12
    readonly property int    radiusS:    8
    property int motor_state: deviceService.status.motorState
    
    // 可能由 main.cpp 注入的对象（安全判空使用）
    // property var keysObj: (typeof keys !== "undefined") ? keys : null
    property bool cardInserted: keys ? keys.inserted : false
    // 卡状态 & 弹层
   
    property bool   overlayVisible: false
    property string overlayText: ""
    property bool   overlayBusy: false

    // 当前页面（0..3）
    property int currentPage: 0

    property string lastSampleNo: ""     // 上一次样品编号
    property bool   sampleNoInited: false

    // 虚拟键盘高度
    readonly property int kbHeight: Math.round(Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0)
    

    property bool testRunning: false     // 防止重复检测
    property bool motorMoving: false     // 电机运行标志
    property var originCheckTimer: Timer // 定时器对象引用
    
    Component.onCompleted: {
        VirtualKeyboardSettings.activeLocales = ["en_US", "zh_CN"]
        VirtualKeyboardSettings.locale = "zh_CN"
    }

    Connections {
        target: userVm

        onLoggedInChanged: {
            if (userVm.loggedIn) {
                console.log("登录成功 role =", userVm.roleName)
                loginLayer.visible = false      // 隐藏登录界面
            } else {
                console.log("登录失败")
            }
        }
    }

    // =====================================================
    // 登录遮罩层
    // =====================================================
    Rectangle {
        id: loginLayer
        anchors.fill: parent
        color: "#AA000000"                                // 半透明黑色遮罩
        z: 999                                            // 始终覆盖最前面
        visible: true                                     // 程序启动时显示登录界面

        property real kbHeight: 0                         // 当前键盘高度（用于判断遮挡）
        property real liftY: 0                            // 弹窗需要上移的距离（像素）

        function updateKbHeight() {                       // 更新键盘高度（兼容不同平台）
            var h1 = panel.implicitHeight                 // InputPanel 自己的隐式高度（通常可靠）
            var h2 = Qt.inputMethod.keyboardRectangle.height // 输入法上报的键盘高度（某些平台可能为 0）
            kbHeight = Qt.inputMethod.visible ? Math.max(h1, h2) : 0 // 键盘显示就取最大值，否则为 0
        }

        function adjustLoginPanel() {                     // 调整登录框位置，避免被键盘遮挡
            updateKbHeight()                              // 先刷新键盘高度

            if (!Qt.inputMethod.visible) {                // 键盘没显示
                liftY = 0                                 // 复位：不抬起
                return                                    // 结束
            }

            var margin = 12                               // 预留边距（让输入框离键盘上沿有点空隙）
            var kbTop = loginLayer.height - kbHeight      // 键盘上沿 Y（相对 loginLayer）

            // 取密码输入框“底部”在 loginLayer 里的坐标
            var p = passwordField.mapToItem(loginLayer, 0, passwordField.height) // 映射底部点
            var fieldBottomY = p.y                        // 输入框底部 Y

            var needLift = fieldBottomY + margin - kbTop  // 需要上移多少才能露出来（>0 表示被挡住）
            if (needLift < 0) needLift = 0                // 没挡住就不抬

            // 计算最大可上移（避免弹窗顶到屏幕外）
            var baseTop = (loginLayer.height - panel_login.height) / 2 // 居中时弹窗顶部
            var maxLift = baseTop - 20                    // 顶部至少留 20px
            if (maxLift < 0) maxLift = 0                  // 防止负数

            if (needLift > maxLift) needLift = maxLift    // 限幅：别抬过头
            liftY = needLift                              // 应用上移量
        }

        // 防止所有点击穿透背景
        MouseArea {
            anchors.fill: parent
            onClicked: {}                                 // 什么都不做 → 阻断事件
        }

        // 登录界面
        Rectangle {
            id: panel_login
            width: 380
            height: 260
            radius: 20
            color: "white"

            anchors.centerIn: parent                      // 仍然居中
            anchors.verticalCenterOffset: -loginLayer.liftY // ★ 关键：用 liftY 向上抬

            Behavior on anchors.verticalCenterOffset {    // ★ 动画：看起来就是“上弹”
                NumberAnimation { duration: 180 }         // 动画时长
            }

            Column {
                anchors.centerIn: parent
                spacing: 18

                Text {
                    text: "用户登录"
                    font.pixelSize: 26
                    font.bold: true
                    color: "#333"
                }

                ComboBox {
                    id: usernameField
                    width: 260
                    model: ["admin", "eng", "op"]
                    currentIndex: 0
                }

                TextField {
                    id: passwordField
                    width: 260
                    echoMode: TextInput.Password
                    placeholderText: "密码"

                    onActiveFocusChanged: {               // ★ 点进密码框时，确保不被遮
                        if (activeFocus) {                // 获得焦点
                            Qt.callLater(loginLayer.adjustLoginPanel) // 下一帧再算（避免布局未更新）
                        }
                    }
                }

                Button {
                    width: 260
                    text: "登录"

                    onClicked: {
                        var user = usernameField.model[usernameField.currentIndex]
                        userVm.login(user, passwordField.text)
                    }
                }
            }
        }

        Connections {                                     // ★ 监听键盘显隐/尺寸变化
            target: Qt.inputMethod
            onVisibleChanged: {                           // 键盘弹出/收起
                Qt.callLater(loginLayer.adjustLoginPanel) // 触发调整
            }
            onKeyboardRectangleChanged: {                 // 键盘高度变化（有的平台会触发）
                Qt.callLater(loginLayer.adjustLoginPanel) // 触发调整
            }
        }

        Component.onCompleted: {                          // ★ 启动时同步一次
            Qt.callLater(loginLayer.adjustLoginPanel)     // 防止初始状态异常
        }
    }

    // 键盘面板
    InputPanel {
        id: panel
        z: 9999
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: Qt.inputMethod.visible

        onVisibleChanged: {                               // ★ 键盘显隐变化时也触发调整
            Qt.callLater(loginLayer.adjustLoginPanel)     // 让登录框跟着动
        }
        onImplicitHeightChanged: {                        // ★ 键盘高度变化时触发（更稳）
            Qt.callLater(loginLayer.adjustLoginPanel)     // 让登录框跟着动
        }
    }

    //顶部栏
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
        Timer {
            interval: 1000
            running: true
            repeat: true
            onTriggered: topBar.now = new Date()
        }

        // 👍 正确：RowLayout 外层用 anchors，RowLayout 内不再写 anchors
        RowLayout {
            anchors.fill: parent         // ✔ 外层是普通 Rectangle，可以 anchors
            anchors.margins: 12          // ✔ 合法
            spacing: 12

            Rectangle {
                width: 10
                height: 10
                radius: 5
                color: brand
                Layout.alignment: Qt.AlignVCenter
            }

            Label {
                text: "青岛普瑞邦生物工程有限公司"
                font.pixelSize: 20
                color: textMain
                Layout.alignment: Qt.AlignVCenter
            }

          //  Item { Layout.fillWidth: true }
            Item { Layout.fillWidth: true  // 占位以保持左右结构正常

                        // 居中文本显示用户
                        Text {
                            anchors.centerIn: parent
                            color: textMain
                            font.pixelSize: 18
                            text: userVm.loggedIn
                                ? ("当前用户：" + userVm.username)
                                : "未登录"
                              }
                    }
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
Timer {
    id: waitMotorReadyTimer
    interval: 50        // 50ms 轮询一次
    repeat: true
    running: false

    onTriggered: {
        if (motor_state === 8) {   // ✅ 电机就绪
            stop()
            doStartTest()    // 真正执行检测流程
        }
    }
}
function startTest() {
    console.log("▶ 请求开始检测")
    console.log("当前 motor_state =", motor_state)

    if (motor_state !== 8) {
        // 可选 UI 提示
        overlayText = "电机准备中，请稍候..."
        overlayBusy = true
        overlayVisible = true

        waitMotorReadyTimer.start()
        return
    }
    doStartTest()
}
Timer {
    id: waitMotorStopTimer
    interval: 20        // 20ms 轮询一次
    repeat: true
    running: false

    onTriggered: {
        var state = deviceService.status.motorState
        if (state === 5) { 
            stop()
            console.log("✅ 电机已停止，开始检测流程")

            doStartTestInternal()
        }
    }
}

function doStartTest() {
    // === 启动 ADS1115 连续采集 ===]
      var curNo = tfSampleId.text
        // ① 如果和上一次一样 → 重新生成
    if (curNo === lastSampleNo) {
        console.log("⚠️ 样品编号重复，重新生成")

        var newNo = mainViewModel.generateSampleNo()
        tfSampleId.text = newNo
        curNo = newNo
    }
    mainViewModel.setCurrentSample(tfSampleId.text)
    mainViewModel.startReading()
    console.log("🧪[" + nowStr() + "] 启动连续采集")
    console.log("▶ 请求开始检测，等待电机停止")
    deviceService.motorStart_2()
    waitMotorStopTimer.start()
}
function doStartTestInternal()
{
    console.log("✅[" + nowStr() + "] 电机停止 → 停止采集")

    // === 停止采集 ===
    mainViewModel.stopReading()
    console.log("⏹[" + nowStr() + "] 停止采集")

    // === 回原点 ===
    uvadcList = mainViewModel.getAdcData(tfSampleId.text)
    var res = mainViewModel.calcTC(uvadcList,projectPage.selectedId)          // 调用 C++ 函数

    var curNo = tfSampleId.text
        // ① 如果和上一次一样 → 重新生成
    // if (curNo === lastSampleNo) {
    //     console.log("⚠️ 样品编号重复，重新生成")

    //     var newNo = mainViewModel.generateSampleNo()
    //     tfSampleId.text = newNo
    //     curNo = newNo
    // }
    // ✅ 取你要落库的三个值：C_net / T_net / ratioTC
    var C_net  = Number(res.C_net   || 0)
    var T_net  = Number(res.T_net   || 0)
    var ratio  = Number(res.ratioTC || 0)
    // ② 记录为“已使用样品号”
    lastSampleNo = curNo

    console.log("🧪 本次使用样品编号 =", curNo)
    // === 读取界面输入信息 ===
    var sampleNo = tfSampleId.text          // 样品编号
    var projectId = projectPage.selectedId       
    var projectName = qrMethodConfigVm.getProjectNameById(projectId)   // ★ 获取项目名称
    var source   = tfSampleSource.text      // 样品来源
    var name     = tfSampleName.text        // 样品名称
    var batch    = qrMethodConfigVm.getBatchCodeById(projectPage.selectedId) // 批次编码
    var curve    = standardCurveBox.currentText  // 标准曲线
    var conc     = Number(res.concentration || 0)                        // 检测浓度
    var ref      = parseFloat(refValueField.text || 0)  // 参考值
    var result   = res.resultStr || ""                  // 检测结果
    var time     = Qt.formatDateTime(new Date(), "yyyy-MM-dd HH:mm:ss") // 时间
    var unit     = tfLab.text               // 检测单位
    var person   = tfOperator.text          // 检测人
    var dilution = dilutionBox.currentText  // 稀释倍数
    console.log("结果：", result)
    // === 组装记录对象 ===
    var record = {
                "projectId": projectId,
                "projectName": projectName,   // ★ 写入数据库
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
                "dilutionInfo": dilution,
                "C": C_net,
                "T": T_net,
                "ratio": ratio
                }
            var uploadRecord = {
                // ===== root =====
                "assayId": 1,                // 固定
                "name": "1",                 // 固定或 sampleNo

                // ===== dataRows 内的一行 =====
                "company": "p",              // 固定
                "sample": sampleNo,          // 样品编号
                "T_Value": T_net,                // 
                "C_Value": C_net,                // 
                "T/C": res.ratioTC || 0,     // ★ 来自 calcTC
                "concentration": res.concentration || 0, // ★
                "result": res.resultStr || "",            // ★ 阳性 / 阴性
                "date": Qt.formatDate(new Date(), "yyyy-MM-dd"),
                "project": projectName,      // 方法名
                "serial": batch,             // 批次编码
                "CurveFormula": "1",         // 默认
                "DilutionFactor": Number(dilution)
            }
        
        labkeyService.uploadRun(uploadRecord)
        console.log("[DEBUG] 即将写入数据库:", JSON.stringify(record))

        // === 写入数据库 ===
        var ok = projectsVm.insertProjectInfo(record)
        console.log(ok ? "[DB] 插入成功 ✅" : "[DB] 插入失败 ❌")
        var concStr = (isFinite(conc) ? conc.toFixed(3) : "0.00")
        // === 界面提示完成 ===
       overlayText = ok                                                   // 根据数据库写入结果决定提示文本
        ? ("检测完成，数据已保存 \n浓度：" + concStr)              // 成功：提示 + 浓度
        : ("检测完成，但数据库写入失败 \n浓度：" + concStr)        // 失败：提示 + 浓度
        overlayBusy = false
        overlayVisible = true
        testRunning = false
        historyVm.refresh()
        if(settingsVm.autoPrint)
        {
            console.log( " 启动打印 ✅" )
            printerCtrl.printRecord(record)
        }
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
                            if (!deviceService.status.cardHome) {
                                overlayText = "请检查检测卡位置"
                                overlayBusy = false
                                overlayVisible = true
                                return
                            }
                            if(projectPage.selectedId === -1){
                                overlayText = "请选择项目"
                                overlayBusy = false
                                overlayVisible = true
                                return
                            }                           
                            console.log("检测插卡后 → 检查电机原点状态")
                            overlayText = "电机运行中..."
                            overlayBusy = true
                            overlayVisible = true
                            deviceService.motorStart()
                            console.log("---------检测中---------")
                            startTest()                        
                              
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
                    // ===== 0 样品检测 ===== 
                    Item {
                        id: sampleTestPage
                        Layout.fillWidth: true

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

                                Item {
                                    width: parent.width
                                    height: parent.height
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

                                            Label {
                                                text: "项目名称"
                                                width: 200
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                                font.bold: true
                                                padding: 5   // ⭐微调，让中文完全居中
                                            }

                                            Label {
                                                text: "浓度(μg/kg)"
                                                width: 200
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                                font.bold: true
                                                padding: 5
                                            }

                                            Label {
                                                text: "结论"
                                                width: 200
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                                font.bold: true
                                                padding: 5
                                            }

                                            Label {
                                                text: "参考值(μg/kg)"
                                                width: 200
                                                horizontalAlignment: Text.AlignLeft
                                                verticalAlignment: Text.AlignVCenter
                                                font.bold: true
                                                padding: 5
                                            }
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

                                        Item {
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
                                    text: "样品信息"
                                    width: (parent.width - 40) / 3
                                    onClicked: 
                                    {
                                    //  if (settingsVm.autoIdGen) {
                                    //     tfSampleId.text = mainViewModel.generateSampleNo()
                                    //     }
                                     sampleInfoPopup.visible = true
                                    }
                                }
                                Button {
                                    text: "详细信息"
                                    width: (parent.width - 40) / 3
                                }
                                Button {
                                    text: "孵育设置"
                                    width: (parent.width - 40) / 3
                                     onClicked: incubationPage.visible = true
                                }
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
                                            Label { text: qrMethodConfigVm.getProjectNameById(projectPage.selectedId); width: 150; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                                            Label { text: qrMethodConfigVm.getBatchCodeById(projectPage.selectedId); width: 200; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                                            Label { text: Qt.formatDateTime(new Date(), "yyyy-MM-dd HH:mm"); width: 300; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; anchors.verticalCenter: parent.verticalCenter }
                                        }
                                    }
                                }

                                // ===== 第四块：选择框（带标题） =====
                                Row {
                                    id: paramSelectRow
                                    width: parent.width
                                    height: 100
                                    spacing: 40
                                    anchors.topMargin: 8
                                    anchors.top: singleRowTable.bottom

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
                    }
                    // ===== 1 项目管理（带滑动表格） =====
                    Item {
                        id: projectPage
                        property int selectedId: -1 // 当前选中行
                        onSelectedIdChanged: {
                        console.log("[QrMethodConfig] selectedId =", selectedId,
                            "temperature =", qrMethodConfigVm.getTemperatureById(selectedId),
                            "timeSec =",     qrMethodConfigVm.getTimeSecById(selectedId))
                            
                        deviceService.setTargetTemperature(qrMethodConfigVm.getTemperatureById(selectedId))
                        deviceService.setIncubationTime(qrMethodConfigVm.getTimeSecById(selectedId))
                        }
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12

                            // === 标题栏 ===
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                Label {
                                    text: "项目管理"
                                    font.pixelSize: 24
                                    font.bold: true
                                    color: textMain
                                }
                                Item { Layout.fillWidth: true }
                                Button { text: "刷新"; onClicked: qrMethodConfigVm.refresh() }
                                Button {
                                            text: "扫描二维码"
                                            onClicked: scanPage.visible = true                                     
                                           }
                                    Button {
                                        text: "删除"

                                        enabled: projectPage.selectedId > 0 && userVm.roleName !== "operator"

                                        onClicked: {
                                            if (userVm.roleName === "operator") {
                                                console.log("operator 禁止删除 → return")
                                                return;
                                            }

                                            if (projectPage.selectedId > 0) {
                                                console.log("执行删除 → ID =", projectPage.selectedId)
                                                qrMethodConfigVm.deleteById(projectPage.selectedId)
                                                projectPage.selectedId = -1
                                                qrMethodConfigVm.refresh()
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
                                    contentHeight: Math.max(contentCol.height, flickProject.height + 1)

                                    Column {
                                        id: contentCol
                                        width: flickProject.width

                                        // === 数据行 ===
                                        Repeater {
                                            id: dataRepeater
                                            model: qrMethodConfigVm

                                            delegate: Rectangle {
                                                width: parent.width
                                                height: 44
                                                color: (projectPage.selectedId === rid)
                                                    ? "#dbeafe"
                                                    : (index % 2 === 0 ? "#ffffff" : "#f9fafb")
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

                                                    // === 每列文字 ===
                                                    Rectangle {
                                                        width: projArea.colId
                                                        height: parent.height
                                                        color: "transparent"
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
                                                        width: projArea.colName
                                                        height: parent.height
                                                        color: "transparent"
                                                        Label {
                                                            anchors.centerIn: parent
                                                            text: projectName
                                                            color: textMain
                                                            font.bold: true
                                                            elide: Label.ElideRight
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
                                                            text: batchCode
                                                            color: textMain
                                                            font.bold: true
                                                            elide: Label.ElideRight
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

                                        // === 空白补齐行 ===
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
                                                        width: 24
                                                        height: 24
                                                        radius: 4
                                                        border.color: "#d1d5db"
                                                        border.width: 1
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
                         Layout.fillWidth: true
                         Layout.fillHeight: true

                        // 列宽 & 行高
                        property int rowHeight: 44
                        property int w_pname: 140
                        property int w_sel: 44
                        property int w_id: 80
                        property int w_pid: 0
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
                                                w_unit + w_person + w_dilution+ w_pname
                        function selectAll() {
                            var arr = []
                            for (var i = 0; i < historyVm.count; ++i) {
                                var row = historyVm.getRow(i)
                                arr.push(row.id)
                            }
                            selectedIds = arr
                        }

                        function unselectAll() {
                            selectedIds = []
                        }
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

                                Item { Layout.fillWidth: true }  // 左右分隔

                                // --- 左侧一组操作按钮 ---
                                RowLayout {
                                    spacing: 10
                                    Button { text: "刷新"; onClicked: historyVm.refresh() }

                                    Button {
                                        text: "全选"
                                        onClicked: {
                                        historyPage.selectAll()
                                        }
                                    }

                                     Button {
                                            text: "反选"
                                            onClicked: {
                                            historyPage.unselectAll()
                                            }
                                        }
                                    Button {
                                        text: "删除选中"
                                        enabled: historyPage.selectedIds.length > 0 && userVm.roleName !== "operator"
                                        onClicked: historyPage.deleteSelected()
                                    }
                                    Button {
                                        text: "导出 CSV"
                                        onClicked: {
                                            // let name = "history_" + new Date().toLocaleString().replace(/[ :\/]/g, "_") + ".csv"
                                            // let filePath = "/mnt/SDCARD/export/" + name
                                            // historyVm.exportCsv(filePath)
                                            // console.log("[CSV] 导出:", filePath)
                                        }
                                    }
                                    // ✅ 新增：详细信息按钮
                                    Button {
                                        text: "详细信息"
                                        enabled: historyPage.selectedIds.length === 1
                                        onClicked: {
                                            if (historyPage.selectedIds.length === 1) {
                                                let id = historyPage.selectedIds[0]
                                                selectedHistoryItem = historyVm.getById(id)
                                                currentPage = 4  // 跳到详细信息页////
                                            } else {
                                                console.log("⚠️ 请选择一条记录查看详细信息")
                                            }
                                        }
                                    }
                                    Button {
                                        text: "打印"
                                        enabled: historyPage.selectedIds.length === 1    // ★ 只能选中 1 条时启用

                                        onClicked: {
                                            if (historyPage.selectedIds.length === 1) {

                                                let id = historyPage.selectedIds[0]
                                                let rec = historyVm.getById(id)      // ⭐ 已经包含全部信息
                                                printerCtrl.printRecord(rec)       // ⭐ 直接打印，无需 projectId 查询 DB
                                                console.log("🖨️ 打印记录：项目编号 =", rec.projectId)                                     
                                            } else {
                                                console.log("⚠️ 请选择一条记录进行打印")
                                            }
                                        }
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
                                        }
                                        Rectangle { width: historyPage.w_id; 
                                              height: parent.height; color: "transparent"; 
                                              Text { 
                                                anchors.centerIn: parent;
                                                text: "ID";       
                                                font.bold: true 
                                                font.pixelSize: 14
                
                                                } 
                                        }
                                        Rectangle { width: 0;                      height: parent.height; color: "transparent"; visible: false }
                                        Rectangle { width: historyPage.w_pname;    height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "项目名称"; font.bold: true }}
                                        Rectangle { width: historyPage.w_no;       height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "样品编号"; font.bold: true } }
                                        Rectangle { width: historyPage.w_src;      height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "样品来源"; font.bold: true } }
                                        Rectangle { width: historyPage.w_name;     height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "样品名称"; font.bold: true } }
                                        Rectangle { width: historyPage.w_curve;    height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "标准曲线"; font.bold: true } }
                                        Rectangle { width: historyPage.w_batch;    height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "批次";     font.bold: true } }
                                        Rectangle { width: historyPage.w_conc;     height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "浓度";     font.bold: true } }
                                        Rectangle { width: historyPage.w_ref;      height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "参考";     font.bold: true } }
                                        Rectangle { width: historyPage.w_res;      height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "结果";     font.bold: true } }
                                        Rectangle { width: historyPage.w_time;     height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "检测时间"; font.bold: true } }
                                        Rectangle { width: historyPage.w_unit;     height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "单位";     font.bold: true } }
                                        Rectangle { width: historyPage.w_person;   height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "检测人";   font.bold: true } }
                                        Rectangle { width: historyPage.w_dilution; height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: "稀释倍数"; font.bold: true } }
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
                                                Rectangle { width: historyPage.w_id;       height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: modelId } }
                                               // Rectangle { width: historyPage.w_pid;      height: parent.height; color: "transparent"; Text { anchors.centerIn: parent; text: projectId } }
                                                Rectangle { width: historyPage.w_pid;      height:parent.height;  visible: false}
                                                Rectangle { width: historyPage.w_pname;    height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: projectName }}
                                                Rectangle { width: historyPage.w_no;       height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: sampleNo } }
                                                Rectangle { width: historyPage.w_src;      height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: sampleSource } }
                                                Rectangle { width: historyPage.w_name;     height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: sampleName } }
                                                Rectangle { width: historyPage.w_curve;    height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: standardCurve } }
                                                Rectangle { width: historyPage.w_batch;    height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: batchCode } }
                                                Rectangle { width: historyPage.w_conc;     height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: Number(detectedConc).toFixed(2) } }
                                                Rectangle { width: historyPage.w_ref;      height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: Number(referenceValue).toFixed(2) } }
                                                Rectangle {
                                                    width: historyPage.w_res; height: parent.height; color: "transparent"
                                                    HeaderText { anchors.centerIn: parent; text: result; color: result === "合格" ? "green" : "red" }
                                                }
                                                Rectangle { width: historyPage.w_time;     height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: detectedTime } }
                                                Rectangle { width: historyPage.w_unit;     height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: detectedUnit } }
                                                Rectangle { width: historyPage.w_person;   height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: detectedPerson } }
                                                Rectangle { width: historyPage.w_dilution; height: parent.height; color: "transparent"; HeaderText { anchors.centerIn: parent; text: dilutionInfo } }
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
                    // ===== 3 系统设置页 =====
                    Item {
                        id: systemPage
                        property int sysIndex: 0       // 当前子页面
                        Layout.fillWidth: true
                        
                        Column {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 16
                            // ===== 顶部按钮栏 =====
                            Row {
                                width: parent.width
                                height: 50
                                spacing: 12

                                // 通用按钮组件
                                Component {
                                    id: sysBtnComp
                                    Rectangle {
                                        id: btn
                                        width: 140
                                        height: 44
                                        radius: 10
                                        border.width: 1
                                        border.color: checked ? "#2b5fd8" : "#cfd6e2"
                                        color: checked ? "#4c86ff" : "#f5f6fa"

                                        property alias text: lbl.text
                                        property int idx: 0
                                        property bool checked: systemPage.sysIndex === idx

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: systemPage.sysIndex = btn.idx
                                        }

                                        Text {
                                            id: lbl
                                            anchors.centerIn: parent
                                            font.pixelSize: 18
                                            font.bold: true
                                            color: btn.checked ? "white" : "#374151"
                                        }
                                    }
                                }

                                Loader { sourceComponent: sysBtnComp; onLoaded: { item.text = "功能设置";  item.idx = 0 } }
                                Loader { sourceComponent: sysBtnComp; onLoaded: { item.text = "无线局域网";item.idx = 1 } }
                                Loader { sourceComponent: sysBtnComp; onLoaded: { item.text = "厂家信息";  item.idx = 2 } }
                                Loader { sourceComponent: sysBtnComp; onLoaded: { item.text = "关于仪器";  item.idx = 3 } }
                                Loader { sourceComponent: sysBtnComp; onLoaded: { item.text = "恢复出厂";  item.idx = 4 } }
                            }
                            Rectangle {// 系统设置右侧大白框
                                id: sysContent                      // 内容区域 id
                                width: parent.width                 // 宽度跟随外层
                                height: parent.height - 60          // 高度略小一点，留给上面的标题栏
                                radius: 12                          // 圆角
                                color: "#ffffff"                    // 白色背景
                                border.color: "#d1d5db"             // 边框颜色
                                border.width: 1                     // 边框宽度

                                StackLayout {                       // 多页切换容器
                                    id: sysStack                    // id
                                    anchors.fill: parent            // 填满 sysContent
                                    currentIndex: systemPage.sysIndex  // 根据左侧菜单切换页

                                    // 0️⃣ 功能设置页 ———— 干净风格（无边框，无蓝色背景）
                                    Item {
                                        id: funcPage
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true

                                        Flickable {
                                            id: funcFlick
                                            anchors.fill: parent
                                            contentWidth: width
                                            contentHeight: contentColumn.height
                                            clip: true
                                            flickableDirection: Flickable.VerticalFlick
                                            boundsBehavior: Flickable.StopAtBounds

                                            // 自动滚动条
                                            ScrollBar.vertical: ScrollBar {
                                                policy: ScrollBar.AlwaysOn
                                            }

                                            Column {
                                                id: contentColumn
                                                width: funcFlick.width
                                                spacing: 20
                                                anchors.top: parent.top
                                                anchors.margins: 20

                                                // 顶部分割线
                                                Rectangle {
                                                    width: parent.width
                                                    height: 1
                                                    color: "#e5e7eb"
                                                }

                                                // =========== 行组件 ===========
                                                // 每一行结构： Row (Label + Switch)
                                                Component {
                                                    id: rowItemComp
                                                    Row {
                                                        width: contentColumn.width - 20
                                                        height: 50
                                                        spacing: 20

                                                        Label {
                                                            id: rowLabel
                                                            width: parent.width - 120
                                                            text: "未命名"
                                                            font.pixelSize: 20
                                                            color: "#1f2937"
                                                            leftPadding: 40 
                                                            verticalAlignment: Text.AlignVCenter
                                                        }

                                                        Switch {
                                                            id: rowSwitch
                                                            width: 70
                                                            height: 38

                                                            indicator: Rectangle {
                                                                implicitWidth: 70
                                                                implicitHeight: 38
                                                                radius: 19
                                                                color: rowSwitch.checked ? "#4c86ff" : "#d1d5db"
                                                                border.color: rowSwitch.checked ? "#3a6ae8" : "#c0c0c0"

                                                                Rectangle {
                                                                    width: 32
                                                                    height: 32
                                                                    radius: 16
                                                                    y: 3
                                                                    x: rowSwitch.checked ? (70 - 35) : 3
                                                                    color: "white"
                                                                    border.color: "#a1a1aa"
                                                                    Behavior on x { NumberAnimation { duration: 120 } }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }

                                                // =========================
                                                // ① 启动自动打印
                                                // =========================
                                                Loader {
                                                    sourceComponent: rowItemComp
                                                    onLoaded: {
                                                        let row = item;
                                                        let lbl = row.children[0];
                                                        let sw = row.children[1];

                                                        lbl.text = "启动自动打印";
                                                        sw.checked = Qt.binding(() => settingsVm.autoPrint);                                                
                                                        sw.onToggled.connect(function () {
                                                            settingsVm.autoPrint = sw.checked;
                                                            settingsVm.save();
                                                        });
                                                    }
                                                }

                                                // =========================
                                                // ② ID 号自动生成
                                                // =========================
                                                Loader {
                                                    sourceComponent: rowItemComp
                                                    onLoaded: {
                                                        let row = item;
                                                        let lbl = row.children[0];
                                                        let sw = row.children[1];

                                                        lbl.text = "ID号自动生成";
                                                        sw.checked = Qt.binding(() => settingsVm.autoIdGen);

                                                        sw.onToggled.connect(function () {
                                                            settingsVm.autoIdGen = sw.checked;
                                                            settingsVm.save();
                                                        });
                                                    }
                                                }

                                                // =========================
                                                // ③ 自动上传服务器
                                                // =========================
                                                Loader {
                                                    sourceComponent: rowItemComp
                                                    onLoaded: {
                                                        let row = item;
                                                        let lbl = row.children[0];
                                                        let sw = row.children[1];

                                                        lbl.text = "启动数据自动上传服务器";
                                                        sw.checked = Qt.binding(() => settingsVm.autoUpload);
                                                        sw.enabled = Qt.binding(() => userVm.roleName === "admin")
                                                        sw.onToggled.connect(function () {
                                                            settingsVm.autoUpload = sw.checked;
                                                            settingsVm.save();
                                                        });
                                                    }
                                                }

                                                // =========================
                                                // ④ 微动开关
                                                // =========================
                                                Loader {
                                                    sourceComponent: rowItemComp
                                                    onLoaded: {
                                                        let row = item;
                                                        let lbl = row.children[0];
                                                        let sw = row.children[1];

                                                        lbl.text = "启用微动开关";
                                                        sw.checked = Qt.binding(() => settingsVm.microSwitch);

                                                        sw.onToggled.connect(function () {
                                                            settingsVm.microSwitch = sw.checked;
                                                            settingsVm.save();
                                                        });
                                                    }
                                                }

                                                // =========================
                                                // ⑤ 厂家名称打印
                                                // =========================
                                                Loader {
                                                    sourceComponent: rowItemComp
                                                    onLoaded: {
                                                        let row = item;
                                                        let lbl = row.children[0];
                                                        let sw = row.children[1];

                                                        lbl.text = "厂家名称打印";
                                                        sw.checked = Qt.binding(() => settingsVm.manufacturerPrint);
                                                        sw.enabled = Qt.binding(() => userVm.roleName !== "op")
                                                        sw.onToggled.connect(function () {
                                                            settingsVm.manufacturerPrint = sw.checked;
                                                            settingsVm.save();
                                                        });
                                                    }
                                                }

                                                // =========================
                                                // ⑥ 打印样品来源
                                                // =========================
                                                Loader {
                                                    sourceComponent: rowItemComp
                                                    onLoaded: {
                                                        let row = item;
                                                        let lbl = row.children[0];
                                                        let sw = row.children[1];

                                                        lbl.text = "打印样品来源";
                                                        sw.checked = Qt.binding(() => settingsVm.printSampleSource);
                                                        sw.enabled = Qt.binding(() => userVm.roleName !== "op")
                                                        sw.onToggled.connect(function () {
                                                            settingsVm.printSampleSource = sw.checked;
                                                            settingsVm.save();
                                                        });
                                                    }
                                                }

                                                // =========================
                                                // ⑦ 打印参考值
                                                // =========================
                                                Loader {
                                                    sourceComponent: rowItemComp
                                                    onLoaded: {
                                                        let row = item;
                                                        let lbl = row.children[0];
                                                        let sw = row.children[1];

                                                        lbl.text = "打印参考值";
                                                        sw.checked = Qt.binding(() => settingsVm.printReferenceValue);
                                                        sw.enabled = Qt.binding(() => userVm.roleName !== "op")
                                                        sw.onToggled.connect(function () {
                                                            settingsVm.printReferenceValue = sw.checked;
                                                            settingsVm.save();
                                                        });
                                                    }
                                                }

                                                // =========================
                                                // ⑧ 打印检测人员
                                                // =========================
                                                Loader {
                                                    sourceComponent: rowItemComp
                                                    onLoaded: {
                                                        let row = item;
                                                        let lbl = row.children[0];
                                                        let sw = row.children[1];
                                                        sw.enabled = Qt.binding(() => userVm.roleName !== "op")
                                                        lbl.text = "打印检测人员";
                                                        sw.checked = Qt.binding(() => settingsVm.printDetectedPerson);

                                                        sw.onToggled.connect(function () {
                                                            settingsVm.printDetectedPerson = sw.checked;
                                                            settingsVm.save();
                                                        });
                                                    }
                                                }

                                                // =========================
                                                // ⑨ 打印稀释倍数
                                                // =========================
                                                Loader {
                                                    sourceComponent: rowItemComp
                                                    onLoaded: {
                                                        let row = item;
                                                        let lbl = row.children[0];
                                                        let sw = row.children[1];

                                                        lbl.text = "打印稀释倍数";
                                                        sw.checked = Qt.binding(() => settingsVm.printDilutionInfo);
                                                        sw.enabled = Qt.binding(() => userVm.roleName !== "op")
                                                        sw.onToggled.connect(function () {
                                                            settingsVm.printDilutionInfo = sw.checked;
                                                            settingsVm.save();
                                                        });
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    // // 1️⃣ 工具
                                    // Item {
                                    //     Label {
                                    //         text: "工具页（待填）"
                                    //         anchors.centerIn: parent
                                    //         font.pixelSize: 22
                                    //         color: "#6b7280"
                                    //           }
                                    // }
                                      WifiPage  {
                                                    id: wifiPage
                                                }
                                    // 2️⃣ 厂家信息
                                    Item {
                                        id: manufacturerInfoPage
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        anchors.margins: 10

                                        ColumnLayout {
                                            id: manuCol
                                            anchors.fill: parent
                                            spacing: 10

                                            // ===== 标题 =====
                                            Label {
                                                text: "厂家信息"
                                                font.pixelSize: 26
                                                font.bold: true
                                                color: "#1f2937"
                                                Layout.alignment: Qt.AlignHCenter
                                            }

                                            // ===== 整个信息大框 =====
                                            Rectangle {
                                                id: infoBox
                                                radius: 12
                                                color: "#ffffff"
                                                border.color: "#ffffff"
                                                border.width: 1
                                                Layout.fillWidth: true
                                                Layout.fillHeight: true
                                                Layout.alignment: Qt.AlignHCenter

                                                Column {
                                                    id: infoContent
                                                    width: parent.width * 0.9
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    spacing: 14
                                                    anchors.margins: 30


                                                    // ===== 公司信息 =====
                                                    Label { text: "公司名称：青岛普瑞邦生物工程有限公司"; font.pixelSize: 20; color: "#111827" }
                                                    Label { text: "公司地址：山东省青岛市高新区广博路17号 MAX商务红湾21号楼101室"; font.pixelSize: 20; color: "#111827"; wrapMode: Text.Wrap }
                                                    Label { text: "公司网址：http://www.pribolab.cn/"; font.pixelSize: 20; color: "#111827" }
                                                    Label { text: "公司邮箱：info@pribolab.cn"; font.pixelSize: 20; color: "#111827" }
                                                    Label { text: "客服热线：400-688-5349"; font.pixelSize: 20; color: "#111827" }

                                                    // ===== 客户提示文本 =====
                                                    Text {
                                                        id: customerText
                                                        text: 
                                                            "尊敬的客户朋友，您好！\n\n" +
                                                            "十分感谢您选择我们的产品，希望您能有一个愉快的使用体验！\n\n" +
                                                            "如在使用过程中有任何问题、建议和意见，请随时联系我们，我们将在最短的\n\n" +
                                                            "时间内给您圆满的答复。再次感谢您对我们的支持！"
                                                        font.pixelSize: 20
                                                        font.bold: true
                                                        color: '#090a0b'
                                                        lineHeight: 0.8
                                                        lineHeightMode: Text.ProportionalHeight
                                                        wrapMode: Text.Wrap
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    // 3️⃣ 关于仪器
                                OtaPage {
                                            id: otaPage
                                        }
                                    // 4️⃣ 恢复出厂
                                    Item {
                                        Label {
                                            text: ""
                                            anchors.centerIn: parent
                                            font.pixelSize: 22
                                            color: "red"
                                            font.bold: true
                                        }
                                    }
                                }
                            }
                     }
                    }
                 }
             }
                    Item {
                                id: detailPage
                                anchors {
                                    left: rightPane.left
                                    right: rightPane.right
                                    top: rightPane.top
                                    bottom: rightPane.bottom
                                }
                                visible: currentPage === 4////
                                z: 999

                                Loader {
                                    id: detailLoader
                                    anchors.fill: parent
                                    source: "qrc:/qml/DetailView.qml"
                                    active: currentPage === 4////

                                    onLoaded: {
                                        item.record = selectedHistoryItem
                                        item.goBack.connect(() => currentPage = 2)
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
                                font.pixelSize: 18
                                placeholderText: "请输入样品编号"
                                Layout.fillWidth: true                      
                             //   text : "0000000000"//mainViewModel.generateSampleNo()              
                                
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
    CameraScannerPage {
        id: scanPage
        visible: false
        anchors.fill: parent
    }
    IncubationManagerPage  {
    id: incubationPage
    visible: false
    anchors.fill: parent
    }

}
 