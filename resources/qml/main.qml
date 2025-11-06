import QtQuick 2.12                       // 引入 QtQuick 基础模块
import QtQuick.Controls 2.12              // 引入控件模块（Button/Label 等）
import QtQuick.Layouts 1.12               // 引入布局模块（RowLayout/ColumnLayout）
import Motor 1.0                          // 引入你注册的 MotorController 模块
import QtQuick.VirtualKeyboard 2.0        // 引入虚拟键盘模块
import QtQuick.VirtualKeyboard.Settings 2.0 // 引入虚拟键盘设置模块（singleton）

ApplicationWindow {                       // 应用主窗口
    id: win                                // 对象 id，便于引用
    visible: true                          // 启动后可见
    width: 1024                            // 窗口宽度
    height: 600                            // 窗口高度
    color: "#f9fafb"                       // 背景色（浅灰）

    // ===== 主题色 / 尺寸 =====
    readonly property color  brand:      "#3a7afe"    // 主题主色（蓝）
    readonly property color  textMain:   "#1f2937"    // 主文字颜色
    readonly property color  textSub:    "#6b7280"    // 次文字颜色
    readonly property color  line:       "#e5e7eb"    // 分割线颜色
    readonly property int    radiusL:    12           // 大圆角
    readonly property int    radiusS:    8            // 小圆角

    // 背景淡渐变圆
    Rectangle {                             // 背景图层容器
        anchors.fill: parent                // 填充整个窗口
        color: "transparent"                // 透明
        z: -1                               // 置于最底层
        Rectangle {                         // 右上角的淡色圆形渐变
            width: 340; height: 340         // 宽高
            radius: 170                     // 圆形
            anchors.right: parent.right     // 右对齐
            anchors.top: parent.top         // 顶部对齐
            anchors.margins: 20             // 距边距
            gradient: Gradient {            // 渐变定义
                GradientStop { position: 0.0; color: "#113a7afe" }  // 中心偏蓝
                GradientStop { position: 1.0; color: "#00113a7a" }  // 外圈更淡
            }
            opacity: 0.08                   // 透明度
        }
    }

    // 启动设置
    Component.onCompleted: {                // 组件加载完成时执行
        VirtualKeyboardSettings.activeLocales = ["en_US", "zh_CN"] // 启用的键盘语言
        VirtualKeyboardSettings.locale = "zh_CN"                   // 默认中文键盘
        enableTimer.start()                 // 启动延时计时器（示例用）
        try {                               // 尝试读取卡在位初值
            if (keysObj && keysObj.inserted !== undefined) {         // 如果 C++ 注入的 keys 存在
                cardInserted = keysObj.inserted                      // 设置界面状态
                if (cardInserted) {                                  // 如果已插卡
                    overlayText = "正在检测中…"                       // 弹层文案
                    overlayBusy = true                               // 显示 BusyIndicator
                    overlayVisible = true                            // 显示遮罩
                }
            }
        } catch(e) {}                        // 忽略异常，保证界面可用
    }

    // 键盘高度
    readonly property int kbHeight: Math.round(              // 只读属性：当前虚拟键盘高度
        Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0 // 可见则取高度，否则 0
    )

    // 卡在位 / 弹层状态
    property var  keysObj: (typeof keys !== 'undefined') ? keys : null // C++ 注入的卡检测对象（可能为空）
    property bool cardInserted: false          // 当前是否插卡
    property bool   overlayVisible: false      // 弹层是否可见
    property string overlayText: ""            // 弹层文本
    property bool   overlayBusy: false         // 弹层是否显示忙碌圈
    property int    currentPage: 0             // 当前页索引（0~3，左侧第 5 个为动作按钮）

    // 定时器
    Timer { id: overlayCloseTimer; interval: 1200; repeat: false; onTriggered: overlayVisible = false } // 弹层自动关闭
    Timer { id: enableTimer; interval: 800; repeat: false }                                            // 示例定时器（预留）

    // 去抖
    Timer {                                    // 卡在位信号去抖计时器
        id: cardDebounce                       // 计时器 id
        interval: 120                          // 去抖时间 120ms
        repeat: false                          // 单次触发
        property string pending: ""            // 待处理状态："in" 插卡 / "out" 拔卡
        onTriggered: {                         // 到时后应用状态
            if (pending === "in") {            // 插卡
                if (!cardInserted) {           // 仅在状态变化时处理
                    cardInserted = true        // 标记为已插卡
                    overlayText = "正在检测中…" // 弹层文案
                    overlayBusy = true         // 显示忙碌圈
                    overlayVisible = true      // 显示弹层
                    overlayCloseTimer.stop()   // 停止自动关闭
                }
            } else if (pending === "out") {    // 拔卡
                if (cardInserted) {            // 状态变化才处理
                    cardInserted = false       // 标记为未插卡
                    overlayText = "检测结束"   // 文案
                    overlayBusy = false        // 关闭忙碌圈
                    if (!overlayVisible) overlayVisible = true // 若未显示则显示
                    overlayCloseTimer.restart() // 启动自动关闭
                }
            }
            pending = ""                       // 清空待处理
        }
    }
    Connections {                               // 监听 C++ keysObj 的信号
        target: keysObj ? keysObj : null        // 只有 keysObj 存在才绑定
        onCardInserted:  { cardDebounce.pending = "in";  cardDebounce.restart() } // 收到插卡→去抖
        onCardRemoved:   { cardDebounce.pending = "out"; cardDebounce.restart() } // 收到拔卡→去抖
    }

    // 虚拟键盘
    InputPanel {                                // 虚拟键盘面板
        id: panel                               // id
        z: 100                                  // 提高层级覆盖内容
        anchors.left: parent.left               // 左右贴边
        anchors.right: parent.right
        anchors.bottom: parent.bottom           // 底部贴边
        visible: Qt.inputMethod.visible         // 仅在键盘可见时显示
    }

    // ===== 顶部栏 =====
    Rectangle {                                 // 顶部条
    id: topBar                                  // id
    height: 40                                  // 高度
    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top // 横向铺满顶部
    color: "#ffffff"; border.color: line; border.width: 1         // 白底+边线

    // 当前时间（每秒更新）
    property date now: new Date()               // 顶部栏时间属性，初始为当前
    Timer {                                     // 每秒更新时间
         interval: 1000                         // 1 秒
         running: true                          // 启动即运行
         repeat: true                           // 循环触发
         onTriggered: topBar.now = new Date()   // 刷新时间
    }

    RowLayout {                                  // 顶部内容行布局
        anchors.fill: parent; anchors.margins: 12; spacing: 12     // 填充+内边距+间距

        Rectangle { width: 10; height: 10; radius: 5; color: brand; Layout.alignment: Qt.AlignVCenter } // 左侧蓝点
        Label { text: "青岛普瑞邦生物工程有限公司"; font.pixelSize: 20; color: textMain; Layout.alignment: Qt.AlignVCenter } // 标题文字
        Item { Layout.fillWidth: true }         // 弹性空白，推右侧元素靠右

        Label {                                  // 右侧时间显示
            id: timeLabel                        // id
            text: Qt.formatDateTime(topBar.now, "yyyy-MM-dd  HH:mm:ss") // 格式化日期时间
            color: textSub                       // 次文字色
            Layout.alignment: Qt.AlignVCenter | Qt.AlignRight // 右对齐垂直居中
        }
    }
}

    // ================= 主体：左侧导航 + 右侧内容 =================
    RowLayout {                                  // 主体横向布局：左侧菜单 + 右侧内容
        anchors {                                // 锚定到顶部栏下方到底部
            left: parent.left; right: parent.right
            top: topBar.bottom; bottom: parent.bottom
            bottomMargin: kbHeight               // 预留键盘高度，避免遮挡
        }
        spacing: 2                               // 左右区域之间无间距

// ---------- 左侧菜单栏（等高、上图标下文字，第5个为动作） ----------
Rectangle {                                      // 左侧栏容器
    id: sideBar                                  // id
    Layout.preferredWidth: 180                   // 固定期望宽度（窄一点）
    Layout.fillHeight: true                      // 竖向填满
    color: "#e8edf5"                             // 浅灰底色
    border.color: "#cfd6e2"; border.width: 1     // 边框

    // 互斥分组（仅前四个按钮）
    ButtonGroup { id: navGroup; exclusive: true } // 分组，确保前四个按钮互斥选中

    // 高度计算：上下边距 + 4 个间距，剩余等分为 5 份
    readonly property int padV : 12              // 上下内边距
    readonly property int gap  : 10              // 相邻按钮间距
    readonly property real tileH : (height - 2*padV - 4*gap) / 5 // 等分后的单个按钮高度

    // 可复用的页面按钮（前四个）
    Component {                                  // 定义一个按钮组件
        id: pageButtonComp                       // 组件 id
        Button {                                 // 按钮控件
            id: control                          // 按钮自身 id（供内部引用）
            // 参数：label、idx、iconSource
            property alias label: txt.text       // 把内部文字暴露为 label 属性
            property int  idx: 0                 // 按钮对应的页面索引
            property url  iconSource: ""         // 图标路径
            checkable: true                      // 允许选中态
            ButtonGroup.group: navGroup          // 加入互斥分组
            checked: currentPage === idx         // 是否选中取决于当前页
            onClicked: currentPage = idx         // 被点击时切换当前页
            width: parent ? parent.width : 180   // 宽度填满父项
            height: sideBar.tileH                // 高度等于计算后的 1/5
            padding: 0                           // 去除默认 padding

            // ★ 直接在这里写背景，用 control.checked
            background: Rectangle {              // 自定义背景
                anchors.fill: parent             // 填满按钮
                radius: 10                       // 圆角
                gradient: Gradient {             // 渐变：选中为蓝，不选为灰
                    GradientStop { position: 0.0; color: control.checked ? "#4c86ff" : "#f5f6fa" }
                    GradientStop { position: 1.0; color: control.checked ? "#2f6ff5" : "#e7ebf3" }
                }
                border.color: control.checked ? "#2b5fd8" : "#cfd6e2" // 边框颜色
                border.width: 1                 // 边框宽度
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    height: 6; radius: 10; color: control.checked ? "#33ffffff" : "#22ffffff" } // 顶部高光
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: 8; radius: 10; color: control.checked ? "#1a000000" : "#14000000" } // 底部阴影
            }

            // 上图标，下文字
            contentItem: Item {                  // 自定义内容区域
                anchors.fill: parent             // 填满按钮
                Column {                         // 垂直排列：图标在上，文字在下
                    anchors.fill: parent         // 填满父项
                    anchors.margins: 10          // 内边距
                    spacing: 6                   // 图标与文字之间间距
                    Image {                      // 按钮图标
                        anchors.horizontalCenter: parent.horizontalCenter // 水平居中
                        source: control.iconSource // 图标资源路径
                        width:  parent.width  * 0.55 // 宽度按比例缩放
                        height: parent.height * 0.55 // 高度按比例缩放（约占按钮 55%）
                        fillMode: Image.PreserveAspectFit // 保持比例
                        smooth: true             // 平滑缩放
                        antialiasing: true       // 抗锯齿
                        visible: control.iconSource !== "" // 有路径才显示
                    }
                    Text {                       // 按钮文字
                        id: txt                   // 文字 id（与 label 绑定）
                        text: ""                  // 初值留空，通过 label 赋值
                        font.pixelSize: 18       // 字号
                        font.bold: true          // 加粗
                        color: control.checked ? "white" : "#374151" // 选中白字，未选深灰
                        horizontalAlignment: Text.AlignHCenter       // 文本居中
                        anchors.horizontalCenter: parent.horizontalCenter // 位置居中
                    }
                }
            }
        }
    }

    // 第 5 个：动作按钮（不切页，只打印日志）
    Component {                                  // 定义动作按钮组件
        id: actionButtonComp                     // 组件 id
        Button {                                 // 按钮控件
            id: control                          // id
            property alias label: txt.text       // 暴露文字为 label
            property url  iconSource: ""         // 图标路径
            checkable: false                     // 不参与选中切换
            width: parent ? parent.width : 180   // 宽度填满
            height: sideBar.tileH                // 高度 1/5
            padding: 0                           // 去 padding
            onClicked: {                         // 点击仅执行动作
                console.log("开始检测")          // 打印日志（可改为调用 C++ 接口）
                // 如需联动 C++：if (typeof mainViewModel !== 'undefined' && mainViewModel.startReading) mainViewModel.startReading()
            }
            background: Rectangle {              // 始终蓝色背景（强调动作）
                anchors.fill: parent; radius: 10 // 填满+圆角
                gradient: Gradient {             // 蓝色渐变
                    GradientStop { position: 0.0; color: "#4c86ff" }
                    GradientStop { position: 1.0; color: "#2f6ff5" }
                }
                border.color: "#2b5fd8"; border.width: 1 // 边框
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    height: 6; radius: 10; color: "#33ffffff" } // 顶部高光
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: 8; radius: 10; color: "#1a000000" } // 底部阴影
            }
            contentItem: Item {                  // 内容：图标+文字
                anchors.fill: parent             // 填满
                Column {                         // 垂直排布
                    anchors.fill: parent         // 填满
                    anchors.margins: 10          // 内边距
                    spacing: 6                   // 间距
                    Image {                      // 图标
                        anchors.horizontalCenter: parent.horizontalCenter // 居中
                        source: control.iconSource // 图标路径
                        width:  parent.width  * 0.55 // 比例宽
                        height: parent.height * 0.55 // 比例高
                        fillMode: Image.PreserveAspectFit // 等比缩放
                        smooth: true             // 平滑
                        antialiasing: true       // 抗锯齿
                        visible: control.iconSource !== "" // 有图才显示
                    }
                    Text {                       // 文字
                        id: txt                   // id
                        text: "开始检测"          // 文案
                        font.pixelSize: 18       // 字号
                        font.bold: true          // 粗体
                        color: "white"           // 白色文字
                        horizontalAlignment: Text.AlignHCenter // 文本居中
                        anchors.horizontalCenter: parent.horizontalCenter // 位置居中
                    }
                }
            }
        }
    }

    // 左栏布局：等高竖满（5 个 Loader）
    Column {                                     // 左栏按钮容器（竖向分布）
        anchors {                                // 边距与约束
            left: parent.left; right: parent.right
            top: parent.top; bottom: parent.bottom
            leftMargin: 12; rightMargin: 12
            topMargin: sideBar.padV; bottomMargin: sideBar.padV
        }
        spacing: sideBar.gap                     // 按钮之间的间距

        Loader {                                 // 第 1 个按钮：样品检测
            width: parent.width; height: sideBar.tileH // 固定宽高
            sourceComponent: pageButtonComp      // 使用页面按钮组件
            onLoaded: {                          // 加载后设置参数
                item.label="样品检测"; item.idx=0
                item.iconSource="qrc:/resources/icons/test-tube-line.png" // 图标路径（按你的 qrc 配置）
            }
        }
        Loader {                                 // 第 2 个按钮：项目管理
            width: parent.width; height: sideBar.tileH
            sourceComponent: pageButtonComp
            onLoaded: {
                item.label="项目管理"; item.idx=1
                item.iconSource="qrc:/resources/icons/PM.png"
            }
        }
        Loader {                                 // 第 3 个按钮：历史记录
            width: parent.width; height: sideBar.tileH
            sourceComponent: pageButtonComp
            onLoaded: {
                item.label="历史记录"; item.idx=2
                item.iconSource="qrc:/resources/icons/history.png"
            }
        }
        Loader {                                 // 第 4 个按钮：系统设置
            width: parent.width; height: sideBar.tileH
            sourceComponent: pageButtonComp
            onLoaded: {
                item.label="系统设置"; item.idx=3
                item.iconSource="qrc:/resources/icons/setting.png"
            }
        }
        Loader {                                 // 第 5 个按钮：开始检测（动作）
            width: parent.width; height: sideBar.tileH
            sourceComponent: actionButtonComp
            onLoaded: {
                item.iconSource="qrc:/resources/icons/start.png"  // 可选图标
            }
        }
    }
}


        // ---------- 右侧内容区（卡片容器） ----------
        Rectangle {                              // 右侧外层容器
            id: rightPane                         // id
            Layout.fillWidth: true                // 横向填满剩余空间
            Layout.fillHeight: true               // 竖向填满
            color: "transparent"                  // 透明背景

            // 卡片背景
            Rectangle {                           // 白色卡片容器
                id: card                          // id
                anchors.fill: parent; anchors.margins: 16 // 填满并四周留白
                radius: radiusL; color: "#ffffff" // 圆角+白底
                border.color: line; border.width: 1 // 边框

                // 底部柔影
                Rectangle {                       // 简易“阴影”条
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: 10; radius: radiusL   // 高度与圆角
                    gradient: Gradient {          // 渐变由透明到淡黑
                        GradientStop { position: 0.0; color: "#00ffffff" }
                        GradientStop { position: 1.0; color: "#11000000" }
                    }
                    z: -1                         // 置底
                }

                // 内部内容
                StackLayout {                     // 右侧堆叠页面（根据 currentPage 切换）
                    id: stack                     // id
                    anchors.fill: parent          // 填满卡片
                    anchors.margins: 18           // 内边距
                    currentIndex: currentPage     // 当前显示的页面

                    // ===== 0 样品检测 =====
                    Item {                        // 页面容器
                        ColumnLayout {            // 竖向布局
                            anchors.fill: parent; spacing: 12 // 填满+间距
                            Label { text: "样品检测"; font.pixelSize: 24; color: textMain } // 标题
                            Rectangle {           // 内容占位
                                Layout.fillWidth: true; Layout.fillHeight: true // 填满剩余
                                radius: radiusS; color: "#fafafa"; border.color: line // 卡片样式
                                Label { anchors.centerIn: parent; text: "这里放样品检测界面内容"; color: textSub } // 文本占位
                            }
                        }
                    }

                    // ===== 1 项目管理 =====
                    Item {
                        ColumnLayout {
                            anchors.fill: parent; spacing: 12
                            Label { text: "项目管理"; font.pixelSize: 24; color: textMain }
                            Rectangle {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                radius: radiusS; color: "#fafafa"; border.color: line
                                Label { anchors.centerIn: parent; text: "这里放项目管理内容"; color: textSub }
                            }
                        }
                    }

                    // ===== 2 历史记录 =====
                    Item {
                        ColumnLayout {
                            anchors.fill: parent; spacing: 12
                            Label { text: "历史记录"; font.pixelSize: 24; color: textMain }
                            Rectangle {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                radius: radiusS; color: "#fafafa"; border.color: line
                                Label { anchors.centerIn: parent; text: "这里放历史记录内容"; color: textSub }
                            }
                        }
                    }

                    // ===== 3 系统设置 =====
                    Item {
                        ColumnLayout {
                            anchors.fill: parent; spacing: 12
                            Label { text: "系统设置"; font.pixelSize: 24; color: textMain }
                            Rectangle {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                radius: radiusS; color: "#fafafa"; border.color: line
                                Label { anchors.centerIn: parent; text: "这里放系统自检内容"; color: textSub }
                            }
                        }
                    }

                    // ===== 4 开始检查（不会切到这页；保留占位）=====
                    Item {
                        ColumnLayout {
                            anchors.fill: parent; spacing: 16
                            Label { text: "开始检查（动作按钮不切页）"; font.pixelSize: 24; color: textSub }
                            Rectangle {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                radius: radiusS; color: "#fafafa"; border.color: line
                                Label { anchors.centerIn: parent; text: "点击左侧“开始检测”只会打印日志"; color: textSub }
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== 弹层 =====
    Rectangle {                                  // 全屏半透明遮罩
        id: overlay                              // id
        anchors.fill: parent                     // 填满窗口
        visible: overlayVisible                  // 控制可见性
        color: "#80000000"                       // 半透明黑
        z: 1000                                  // 置顶
        MouseArea { anchors.fill: parent; enabled: overlayVisible } // 避免穿透点击
        Rectangle {                               // 居中白色弹窗
            width: 440; height: 230; radius: radiusL // 尺寸+圆角
            anchors.centerIn: parent             // 置中
            color: "#ffffff"; border.color: line; border.width: 1 // 样式
            Column {                             // 弹窗内容垂直布局
                anchors.centerIn: parent; spacing: 16  // 居中+间距
                BusyIndicator { running: overlayBusy; visible: true; width: 48; height: 48 } // 忙碌圈
                Label { text: overlayText; font.pixelSize: 24; color: textMain } // 文案
                Label {                                 // “即将关闭…” 提示
                    visible: !overlayBusy && overlayText === "检测结束" // 仅在结束时显示
                    text: "即将关闭…"; color: textSub; font.pixelSize: 14 // 次文字
                    horizontalAlignment: Text.AlignHCenter               // 文本居中
                }
            }
        }
    }
}
