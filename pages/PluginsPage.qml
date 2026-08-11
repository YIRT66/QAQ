import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as AppC

Item {
    id: root
    property var theme
    property var iconFont
    property string activeEntryUrl: ""
    property string activePluginName: ""
    readonly property color cardColor: theme && theme.isDark ? "#292923" : "#FFFFFF"
    readonly property color softColor: theme && theme.isDark ? "#23231F" : "#F3F5F7"
    readonly property color mutedColor: theme && theme.isDark ? "#969790" : "#777B84"
    readonly property color borderColor: theme && theme.isDark ? "#17FFFFFF" : "#0F000000"

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text { text: app.language === "en-US" ? "Plugins" : "插件"; color: theme.textColor; font.pixelSize: 24; font.bold: true }
                Text {
                    text: app.language === "en-US"
                          ? "Local QML plugins can extend EvolveMusic. Only load plugins you trust."
                          : "通过本地 QML 插件扩展 EvolveMusic。插件会在本机运行，请只安装你信任的插件。"
                    color: root.mutedColor; font.pixelSize: 9
                }
            }
            AppC.AppButton { theme: root.theme; variant: "ghost"; text: app.language === "en-US" ? "Create sample" : "创建示例插件"; onClicked: app.createPluginTemplate() }
            AppC.AppButton { theme: root.theme; variant: "secondary"; text: app.language === "en-US" ? "Open folder" : "打开插件目录"; onClicked: app.openPluginDirectory() }
            AppC.AppButton { theme: root.theme; variant: "primary"; text: app.language === "en-US" ? "Reload" : "重新扫描"; onClicked: app.reloadPlugins() }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            Rectangle {
                Layout.preferredWidth: Math.min(330, Math.max(250, root.width * 0.30))
                Layout.fillHeight: true
                radius: 16
                color: root.cardColor
                border.width: 1
                border.color: root.borderColor

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8
                    Text { text: (app.language === "en-US" ? "Installed" : "已安装") + " · " + app.plugins.length; color: theme.textColor; font.pixelSize: 14; font.bold: true }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 7; model: app.plugins
                        delegate: Rectangle {
                            required property var modelData
                            width: ListView.view.width; height: 88; radius: 12
                            color: pluginHover.hovered ? (theme.isDark ? "#31312A" : "#EEF1F4") : root.softColor
                            border.width: root.activeEntryUrl === (modelData.entryUrl || "") ? 1 : 0
                            border.color: app.accentColor
                            Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 120 : 0 } }
                            ColumnLayout {
                                anchors.fill: parent; anchors.margins: 10; spacing: 2
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text { Layout.fillWidth: true; text: modelData.name || modelData.id || "Plugin"; color: theme.textColor; font.pixelSize: 10; font.bold: true; elide: Text.ElideRight }
                                    Text { text: "v" + (modelData.version || "1.0.0"); color: root.mutedColor; font.pixelSize: 7 }
                                }
                                Text { Layout.fillWidth: true; text: modelData.description || (app.language === "en-US" ? "No description" : "暂无简介"); color: root.mutedColor; font.pixelSize: 8; maximumLineCount: 2; elide: Text.ElideRight; wrapMode: Text.Wrap }
                                Item { Layout.fillHeight: true }
                                AppC.AppButton {
                                    theme: root.theme; variant: "soft"; text: app.language === "en-US" ? "Open" : "打开"
                                    onClicked: { root.activeEntryUrl = modelData.entryUrl || ""; root.activePluginName = modelData.name || modelData.id || "Plugin" }
                                }
                            }
                            HoverHandler { id: pluginHover }
                        }
                    }
                    Text {
                        visible: app.plugins.length === 0
                        Layout.fillWidth: true
                        text: app.language === "en-US"
                              ? "Create a folder with manifest.json and Main.qml in the plugin directory."
                              : "在插件目录创建一个文件夹，放入 manifest.json 和 Main.qml，然后点击重新扫描。"
                        color: root.mutedColor; font.pixelSize: 8; wrapMode: Text.Wrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 16
                color: root.cardColor
                border.width: 1
                border.color: root.borderColor

                Loader {
                    id: pluginLoader
                    anchors.fill: parent
                    anchors.margins: 10
                    source: root.activeEntryUrl
                    asynchronous: true
                }

                Column {
                    visible: root.activeEntryUrl.length === 0
                    anchors.centerIn: parent
                    spacing: 8
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: "⌘"; color: app.accentColor; font.pixelSize: 32; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: app.language === "en-US" ? "Select a plugin" : "选择一个插件"; color: theme.textColor; font.pixelSize: 15; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: app.pluginDirectory; color: root.mutedColor; font.pixelSize: 8 }
                }

                Column {
                    visible: root.activeEntryUrl.length > 0 && pluginLoader.status === Loader.Error
                    anchors.centerIn: parent
                    spacing: 6
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: app.language === "en-US" ? "Plugin failed to load" : "插件加载失败"; color: "#E26C72"; font.pixelSize: 14; font.bold: true }
                    Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.activePluginName; color: root.mutedColor; font.pixelSize: 9 }
                }
            }
        }
    }

    Component.onCompleted: app.reloadPlugins()
}
