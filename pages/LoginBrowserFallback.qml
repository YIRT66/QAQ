import QtQuick
import QtQuick.Layouts
import "../third_party/EvolveUI/components" as EUI

Rectangle {
    id: root
    property var theme
    property var iconFont
    property var window
    signal closeRequested()
    color: "#F20E0E11"

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(600, parent.width - 60)
        height: 300
        radius: 26
        color: theme.isDark ? "#1A1A20" : "#FFFFFF"
        border.color: theme.isDark ? "#22FFFFFF" : "#14000000"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 26
            spacing: 12

            Text {
                text: "内嵌登录浏览器未安装"
                color: theme.textColor
                font.pixelSize: 22
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: "你的 Qt 6.10 MinGW 不需要 Qt WebEngine。Evolve Music v0.3 使用 Qt WebView，在 Windows 上由 Edge WebView2 提供内嵌网页，并且仍然可以继续用 MinGW 编译。"
                color: "#85858F"
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                text: "给 E:\\QT\\6.10.0\\mingw_64 安装 Qt WebView 模块后重新构建；运行 scripts\\check_webview.bat 可以检查当前 Kit。"
                color: "#8D75FF"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }

            Item { Layout.fillHeight: true }

            EUI.EButton {
                Layout.alignment: Qt.AlignRight
                text: "知道了"
                size: "s"
                containerColor: "#7C62FF"
                textColor: "white"
                shadowEnabled: false
                onClicked: root.closeRequested()
            }
        }
    }
}
