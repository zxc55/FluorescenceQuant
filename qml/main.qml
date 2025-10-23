import QtQuick 2.15
import QtQuick.Window 2.15

Window {
    id: window
    width: 1024
    height: 768
    visible: true
    title: qsTr("荧光定量分析")

    Rectangle {
        anchors.fill: parent
        color: "#f0f0f0"

        Text {
            anchors.centerIn: parent
            text: qsTr("欢迎使用荧光定量分析系统")
            font.pointSize: 24
            color: "#333333"
        }
    }
}