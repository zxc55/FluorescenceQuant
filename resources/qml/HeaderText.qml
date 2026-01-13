import QtQuick 2.12

Text {
    id: root

    // ===== 对外属性 =====
    property int fontSize: 20        // 表头字体大小（像素）
    property bool bold: true         // 是否加粗

    // ===== 默认样式 =====
    font.pixelSize: fontSize         // 字体大小
    font.bold: bold                  // 加粗
    color: "#333333"                 // 表头颜色
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight            // 超出省略
}
