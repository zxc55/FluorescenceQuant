import QtQuick 2.12
import QtQuick.Controls 2.12

Item {
    id: root
    anchors.fill: parent
    visible: false
    property int scanSize: Math.min(width * 0.7, height * 0.6)
    property color neon: "#3a7afe"
    property bool _closing: false                                // 防止重复关闭
    property string lastQrText: ""
    Rectangle {
        anchors.fill: parent
        color: "#000"
    }
function parseQrText(text) {
    var result = {
        ok: false,
        id: "",
        bn: "",
        error: ""
    }

    if (!text || text.length === 0) {
        result.error = "二维码内容为空"
        return result
    }

    var parts = text.split(";")
    for (var i = 0; i < parts.length; ++i) {
        if (parts[i].length === 0)
            continue

        var kv = parts[i].split(":")
        if (kv.length !== 2)
            continue

        var key = kv[0].trim()
        var val = kv[1].trim()

        if (key === "id") {
            result.id = val      // ⭐ 字符串，直接用
        } else if (key === "bn") {
            result.bn = val
        }
    }

    if (!result.id) {
        result.error = "二维码 id 字段缺失"
        return result
    }

    if (!result.bn) {
        result.error = "二维码 bn 字段缺失"
        return result
    }

    result.ok = true
    return result
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
    //     onClicked: {
    //     qrScanner.stopScan()      // 关闭前先停扫描，避免继续回调
    //     root.visible = false      // 隐藏页面 = 返回
    // }
    }

    // ===== 解码结果回调 =====
    Connections {
        target: qrScanner
        onQrDecoded: {
            if (_closing)                                         // 如果已经在关闭流程中
                return                                            // 直接忽略重复回调
            _closing = true                                       // 标记关闭中，防重复触发
            resultLabel.text = text
            root.lastQrText = text
            dbWorker.postLookupQrMethodConfig(text)
            //root.visible = false 
        }
    }
    Connections {
    target: dbWorker
    onQrMethodConfigLookedUp: function(result) {
        console.log("ok=", result.ok, "pid=", result.projectId, "bn=", result.batchCode, "err=", result.error)
              if (result.ok) {
            // ✅ 本地命中，正常流程
            console.log("Local config hit")
            _closing = true
            root.visible = false 
            return
        }
         var parsed = parseQrText(root.lastQrText)
        if (!parsed.ok) {
           console.log("QR parse failed:", parsed.error)
            return
        }
        labkeyService.fetchMethodLibrary(parsed.id, parsed.bn)
    }
}
    Connections {
        target: labkeyService

        onMethodLibraryReady: {
            console.log("LabKey method config ready")
            // 1. 基本校验
            if (!data || !data.rows || data.rows.length === 0) {
                console.log("MethodLibrary rows empty")
                return
            }

            // 2. 取第一条记录
            var row = data.rows[0]

            // 3. 基础字段（按你定的语义）
            var projectId   = row.type          // type -> projectId（字符串）
            var batchCode   = row.serial        // serial -> batchCode
            var methodName  = row.methodName
            var methodData  = row.methodPara
            var updatedAt   = row.Created

            // 4. 解析 incubationPara（JSON 字符串）
            var incubation = {}
            try {
                incubation = JSON.parse(row.incubationPara)
            } catch (e) {
                console.log("parse incubationPara failed:", e)
                return
            }

            var temperature = incubation.temperature
            var timeSec     = incubation.time * 60   // ⚠️ 分钟 -> 秒

            // 5. 解析 C_T_POS（JSON 字符串）
            var ctPos = {}
            try {
                ctPos = JSON.parse(row.C_T_POS)
            } catch (e) {
                console.log("parse C_T_POS failed:", e)
                return
            }

            var C1 = ctPos.C1
            var C2 = ctPos.C2
            var T1 = ctPos.T1
            var T2 = ctPos.T2

            // 6. 打印检查（非常重要，先确认值对不对）
            console.log("projectId =", projectId)
            console.log("batchCode =", batchCode)
            console.log("methodName =", methodName)
            console.log("temperature =", temperature)
            console.log("timeSec =", timeSec)
            console.log("C1 =", C1, "T1 =", T1, "C2 =", C2, "T2 =", T2)
            root.visible = false 
            // 7. 组装 cfg（和 C++ upsert 完全一致）
            var cfg = {
                projectId: projectId,
                batchCode: batchCode,
                projectName: methodName,
                methodName: methodName,
                methodData: methodData,
                updated_at: updatedAt,
                temperature: temperature,
                timeSec: timeSec,
                C1: C1,
                T1: T1,
                C2: C2,
                T2: T2
            }

            // 8. 投递写数据库任务（关键）
            dbWorker.postSaveQrMethodConfig(cfg)
                    }
                    onErrorOccured: {
                        console.log("LabKey error:", msg)
                        _closing = false
                    }
                }
    // ===== 控制开始/停止扫描 =====
    // 第一次显示时启动，再次显示时也会重新启动
    onVisibleChanged: {
        if (visible) {
            _closing = false  
            qrScanner.startScan()
            resultLabel.text = "请对准二维码"
        } else {
            qrScanner.stopScan()
        }
    }
}
