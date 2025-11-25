import QtQuick 2.12
import QtQuick.Controls 2.12

Item {
    id: root
    anchors.fill: parent
    visible: false

    // 扫描框大小
    property int scanSize: Math.min(width * 0.7, height * 0.6)
    property color neon: "#3a7afe"

    Rectangle {
        anchors.fill: parent
        color: "#000"
    }

    // ===== 摄像头画面 =====
    Image {
        id: cameraView
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: "image://qr/frame"

        Connections {
            target: qrScanner
            onFrameUpdated: {
                // 加随机数防缓存
                cameraView.source = "image://qr/frame?" + Math.random()
            }
        }
    }

    // ===== 遮罩 + 透明扫描区域 =====
    Canvas {
        id: mask
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            ctx.fillStyle = "rgba(0,0,0,0.55)";
            ctx.fillRect(0, 0, width, height);

            var s = scanSize;
            var x = (width - s) / 2;
            var y = (height - s) / 2 - 40;

            ctx.globalCompositeOperation = "destination-out";
            ctx.fillRect(x, y, s, s);
            ctx.globalCompositeOperation = "source-over";
        }
    }

    // ===== 四角边框 =====
    Repeater {
        model: 4
        Rectangle {
            width: 40
            height: 40
            color: "transparent"
            border.color: neon
            border.width: 4
            radius: 6

            anchors.left: (index % 2 === 0) ? parent.left : undefined
            anchors.right: (index % 2 === 1) ? parent.right : undefined
            anchors.top: (index < 2) ? parent.top : undefined
            anchors.bottom: (index >= 2) ? parent.bottom : undefined

            anchors.leftMargin: (index % 2 === 0) ? (mask.width - scanSize) / 2 : undefined
            anchors.rightMargin: (index % 2 === 1) ? (mask.width - scanSize) / 2 : undefined
            anchors.topMargin: (index < 2) ? (mask.height - scanSize) / 2 - 40 : undefined
            anchors.bottomMargin: (index >= 2) ? (mask.height - scanSize) / 2 + 40 : undefined
        }
    }

    // ===== 扫描结果 =====
    Text {
        id: resultLabel
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 40
        text: "请对准二维码"
        color: "white"
        font.pixelSize: 34
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    // ===== 返回按钮 =====
    Button {
        id: closeBtn
        width: 200
        height: 60
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40

        background: Rectangle {
            anchors.fill: parent
            radius: 10
            color: "#333333"
        }

        contentItem: Text {
            anchors.centerIn: parent
            text: "返回"
            color: "white"
            font.pixelSize: 26
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        onClicked: root.visible = false
    }

    // ===== 解码结果回调 =====
    Connections {
        target: qrScanner
        onQrDecoded: {
            resultLabel.text = text
        }
    }

    // ===== 控制开始/停止扫描 =====
    // 第一次显示时启动，再次显示时也会重新启动
    onVisibleChanged: {
        if (visible) {
            qrScanner.startScan()
            resultLabel.text = "请对准二维码"
        } else {
            qrScanner.stopScan()
        }
    }
}
