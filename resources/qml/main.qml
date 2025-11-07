// qml/main.qml
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.VirtualKeyboard 2.0
import QtQuick.VirtualKeyboard.Settings 2.0
import App 1.0
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
    ProjectsViewModel {
    id: projectsVm
}
    // ===== 主题 / 常量 =====
    readonly property color  brand:      "#3a7afe"
    readonly property color  textMain:   "#1f2937"
    readonly property color  textSub:    "#6b7280"
    readonly property color  line:       "#e5e7eb"
    readonly property int    radiusL:    12
    readonly property int    radiusS:    8

    // 可能由 main.cpp 注入的对象（安全判空使用）
    property var keysObj: (typeof keys !== "undefined") ? keys : null

    // 卡状态 & 弹层
    property bool   cardInserted: false
    property bool   overlayVisible: false
    property string overlayText: ""
    property bool   overlayBusy: false

    // 当前页面（0..3）
    property int currentPage: 0

    // 虚拟键盘高度
    readonly property int kbHeight: Math.round(Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0)

    // ==== 初始化 ====
    Component.onCompleted: {
        VirtualKeyboardSettings.activeLocales = ["en_US", "zh_CN"]
        VirtualKeyboardSettings.locale = "zh_CN"
        console.log("projectsVm.count (onCompleted) =", (typeof projectsVm !== "undefined") ? projectsVm.count : "N/A")
        try {
            if (keysObj && keysObj.inserted !== undefined) {
                cardInserted = keysObj.inserted
                if (cardInserted) {
                    overlayText = "正在检测中…"
                    overlayBusy = true
                    overlayVisible = true
                }
            }
        } catch(e) {}
    }

    // 键盘面板
    InputPanel {
        id: panel
        z: 100
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: Qt.inputMethod.visible
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

    // 主体布局：左侧导航 + 右侧内容
    RowLayout {
        anchors {
            left: parent.left; right: parent.right
            top: topBar.bottom; bottom: parent.bottom
            bottomMargin: kbHeight
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
                    onClicked: console.log("开始检测")
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

                    // 0 样品检测（占位）
                    Item {
                        ColumnLayout {
                            anchors.fill: parent; spacing: 12
                            Label { text: "样品检测"; font.pixelSize: 24; color: textMain }
                            Rectangle {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                radius: radiusS; color: "#fafafa"; border.color: line
                                Label { anchors.centerIn: parent; text: "这里放样品检测界面内容"; color: textSub }
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
                id: flick
                anchors.top: header.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                clip: true
                boundsBehavior: Flickable.DragAndOvershootBounds
                interactive: true

                // ✅ 内容高度至少比 Flickable 高 1 像素，这样即使少数据也能拖动
                contentHeight: Math.max(contentCol.height, flick.height + 1)

                Column {
                    id: contentCol
                    width: flick.width

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

                    // 2 历史记录（占位）
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

                    // 3 系统设置（占位）
                    Item {
                        ColumnLayout {
                            anchors.fill: parent; spacing: 12
                            Label { text: "系统设置"; font.pixelSize: 24; color: textMain }
                            Rectangle {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                radius: radiusS; color: "#fafafa"; border.color: line
                                Label { anchors.centerIn: parent; text: "这里放系统设置内容"; color: textSub }
                            }
                        }
                    }
                }
            }
        }
    }

    // ===== 弹层 =====
    Rectangle {
        id: overlay
        anchors.fill: parent
        visible: overlayVisible
        color: "#80000000"
        z: 1000
        MouseArea { anchors.fill: parent; enabled: overlayVisible }
        Rectangle {
            width: 440; height: 230; radius: radiusL
            anchors.centerIn: parent
            color: "#ffffff"; border.color: line; border.width: 1
            Column {
                anchors.centerIn: parent; spacing: 16
                BusyIndicator { running: overlayBusy; visible: true; width: 48; height: 48 }
                Label { text: overlayText; font.pixelSize: 24; color: textMain }
                Label {
                    visible: !overlayBusy && overlayText === "检测结束"
                    text: "即将关闭…"; color: textSub; font.pixelSize: 14
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
