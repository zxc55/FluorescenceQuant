import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

Rectangle {
    id: pad
    width: parent ? parent.width : 1024
    height: 260
    color: "#202228"
    radius: 6
    border.color: "#3a3d44"
    visible: forcedVisible || (autoShow && target && target.activeFocus)
    z: 999

    // ===== 公共属性 / API =====
    property bool forcedVisible: false
    property bool autoShow: false
    property bool allowDecimal: true
    property bool allowNegative: true
    property var  target: null           // 绑定的 TextField/TextInput
    signal accepted(string text)

    function openFor(item) {
        target = item
        forcedVisible = true
    }
    function closePad() {
        forcedVisible = false
    }

    // ——— 文本操作工具 ———
    function getText() {
        return (target && target.text !== undefined) ? String(target.text) : ""
    }
    function setText(t) {
        if (target && target.text !== undefined) {
            target.text = t
        }
    }
    function appendDigit(ch) {
        var s = getText()
        setText(s + ch)
    }
    function appendDot() {
        if (!allowDecimal) return
        var s = getText()
        if (s.indexOf(".") === -1) setText(s.length ? (s + ".") : "0.")
    }
    function toggleMinus() {
        if (!allowNegative) return
        var s = getText()
        if (!s.length) { setText("-"); return }
        if (s[0] === "-") setText(s.slice(1))
        else setText("-" + s)
    }
    function backspace() {
        var s = getText()
        if (s.length > 0) setText(s.slice(0, -1))
    }
    function clearAll() {
        setText("")
    }
    function commit() {
        accepted(getText())
        closePad()
    }
    function cancel() {
        closePad()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // 显示条
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            radius: 4
            color: "#2b2f36"
            border.color: "#434b55"

            Text {
                id: display
                anchors.fill: parent
                anchors.margins: 10
                text: pad.getText()
                color: "white"
                font.pixelSize: 20
                elide: Text.ElideLeft
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
        }

        // 按键区
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 4
            rowSpacing: 8
            columnSpacing: 8

            // 第一排：1 2 3 退格
            Repeater {
                model: [
                    {t:"1"}, {t:"2"}, {t:"3"},
                    {t:"⌫", op:"back"}
                ]
                delegate: Button {
                    text: modelData.t
                    font.pixelSize: 20
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    onClicked: {
                        switch (modelData.op) {
                        case "back": pad.backspace(); break;
                        default: pad.appendDigit(modelData.t);
                        }
                    }
                }
            }

            // 第二排：4 5 6 清空
            Repeater {
                model: [
                    {t:"4"}, {t:"5"}, {t:"6"},
                    {t:"清空", op:"clr"}
                ]
                delegate: Button {
                    text: modelData.t
                    font.pixelSize: 20
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    onClicked: {
                        if (modelData.op === "clr") pad.clearAll()
                        else pad.appendDigit(modelData.t)
                    }
                }
            }

            // 第三排：7 8 9 ±
            Repeater {
                model: [
                    {t:"7"}, {t:"8"}, {t:"9"},
                    {t: allowNegative ? "±" : "—", op:"neg", en: allowNegative}
                ]
                delegate: Button {
                    text: modelData.t
                    enabled: modelData.en === undefined ? true : modelData.en
                    font.pixelSize: 20
                    Layout.fillWidth: true
                    Layout.preferredHeight: 46
                    onClicked: {
                        if (modelData.op === "neg") pad.toggleMinus()
                        else pad.appendDigit(modelData.t)
                    }
                }
            }

            // 第四排：0 . 取消 确认
            Button {
                text: "0"
                font.pixelSize: 20
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                onClicked: pad.appendDigit("0")
            }
            Button {
                text: "."
                enabled: allowDecimal
                font.pixelSize: 20
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                onClicked: pad.appendDot()
            }
            Button {
                text: "取消"
                font.pixelSize: 20
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                onClicked: pad.cancel()
            }
            Button {
                text: "确定"
                font.pixelSize: 20
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                onClicked: pad.commit()
            }
        }
    }
}
