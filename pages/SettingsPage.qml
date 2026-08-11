pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as AppC

Item {
    id: root
    required property var theme
    required property var iconFont

    property int categoryIndex: 0
    readonly property bool compactCategories: width < 860
    readonly property bool shortCategories: height < 700
    readonly property color cardColor: theme.isDark ? "#292923" : "#FFFFFF"
    readonly property color softColor: theme.isDark ? "#23231F" : "#F3F5F7"
    readonly property color muted: theme.isDark ? "#999A93" : "#777B84"
    readonly property color divider: theme.isDark ? "#17FFFFFF" : "#0F000000"

    function categoryModel() {
        if (app.language === "en-US") {
            return [
                { title: "Appearance", subtitle: "Theme, colors and motion", glyph: "✦" },
                { title: "Playback", subtitle: "Quality and recommendations", glyph: "♪" },
                { title: "Cache", subtitle: "Storage and cleanup", glyph: "▣" },
                { title: "Sources", subtitle: "Providers and health", glyph: "⇄" },
                { title: "Desktop", subtitle: "Tray, updates and safety", glyph: "□" },
                { title: "Account", subtitle: "Cloud and account", glyph: "●" },
                { title: "Extensions", subtitle: "Plugins and permissions", glyph: "⌘" },
                { title: "About", subtitle: "Author, project and credits", glyph: "i" }
            ]
        }
        return [
            { title: "外观", subtitle: "主题、颜色与动效", glyph: "✦" },
            { title: "播放与推荐", subtitle: "音质、排序和推荐偏好", glyph: "♪" },
            { title: "缓存与存储", subtitle: "目录、占用与清理", glyph: "▣" },
            { title: "播放来源", subtitle: "音源开关与健康状态", glyph: "⇄" },
            { title: "桌面与更新", subtitle: "托盘、更新与诊断", glyph: "□" },
            { title: "账户与云端", subtitle: "登录与双云节点", glyph: "●" },
            { title: "扩展与插件", subtitle: "底层接口、权限与贡献点", glyph: "⌘" },
            { title: "关于软件", subtitle: "作者、项目与开源致谢", glyph: "i" }
        ]
    }

    component SettingsCard: Rectangle {
        id: card
        property string heading: ""
        property string caption: ""
        default property alias content: cardColumn.data
        Layout.fillWidth: true
        implicitHeight: cardColumn.implicitHeight + 34
        radius: app.themePreset === "paper" ? 14 : 18
        color: root.cardColor
        border.width: 1
        border.color: root.divider

        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: 17
            spacing: 8

            Text {
                Layout.fillWidth: true
                text: card.heading
                color: theme.textColor
                font.pixelSize: 15
                font.bold: true
            }
            Text {
                Layout.fillWidth: true
                visible: card.caption.length > 0
                text: card.caption
                color: root.muted
                font.pixelSize: 9
                wrapMode: Text.Wrap
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: 3
                Layout.bottomMargin: 2
                color: root.divider
            }
        }
    }

    component SettingRow: Item {
        id: row
        property string heading: ""
        property string caption: ""
        default property alias accessory: accessoryHost.data
        Layout.fillWidth: true
        Layout.preferredHeight: caption.length > 0 ? 57 : 48

        RowLayout {
            anchors.fill: parent
            spacing: 16
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 180
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: row.heading
                    color: theme.textColor
                    font.pixelSize: 10
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    visible: row.caption.length > 0
                    text: row.caption
                    color: root.muted
                    font.pixelSize: 8
                    elide: Text.ElideMiddle
                }
            }
            Item {
                id: accessoryHost
                Layout.preferredWidth: root.compactCategories ? Math.min(230, root.width * 0.40) : Math.min(310, root.width * 0.31)
                Layout.fillHeight: true
            }
        }
    }

    component CategoryTile: Rectangle {
        id: tile
        property int category: 0
        property string title: ""
        property string subtitle: ""
        property string glyph: ""
        property bool compact: false
        signal activated()

        implicitWidth: compact ? Math.max(96, titleText.implicitWidth + 34) : 198
        implicitHeight: compact ? 38 : (root.shortCategories ? 50 : 58)
        radius: compact ? 12 : 14
        color: root.categoryIndex === category
               ? (theme.isDark ? "#303B36" : "#E7F6F1")
               : (mouse.containsMouse ? root.softColor : "transparent")
        border.width: root.categoryIndex === category ? 1 : 0
        border.color: root.categoryIndex === category ? app.accentColor : "transparent"
        Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 120 : 0 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: compact ? 10 : 11
            anchors.rightMargin: compact ? 10 : 11
            spacing: 9
            Rectangle {
                visible: !tile.compact
                Layout.preferredWidth: root.shortCategories ? 29 : 32
                Layout.preferredHeight: root.shortCategories ? 29 : 32
                radius: 10
                color: root.categoryIndex === tile.category ? app.accentColor : root.softColor
                Text {
                    anchors.centerIn: parent
                    text: tile.glyph
                    color: root.categoryIndex === tile.category ? "#14201D" : theme.textColor
                    font.pixelSize: 11
                    font.bold: true
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    id: titleText
                    Layout.fillWidth: true
                    text: tile.title
                    color: theme.textColor
                    font.pixelSize: compact ? 9 : 10
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    visible: !tile.compact
                    Layout.fillWidth: true
                    text: tile.subtitle
                    color: root.muted
                    font.pixelSize: 7
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                visible: !tile.compact && root.categoryIndex === tile.category
                width: 4; height: 22; radius: 2; color: app.accentColor
            }
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: tile.activated()
        }
    }

    component CreditRow: Rectangle {
        id: credit
        property string mark: "•"
        property string name: ""
        property string caption: ""
        property string badge: ""
        property string url: ""

        Layout.fillWidth: true
        Layout.preferredHeight: 68
        radius: 13
        color: creditHover.hovered ? root.softColor : "transparent"
        border.width: 1
        border.color: creditHover.hovered ? (theme.isDark ? "#2AFFFFFF" : "#14000000") : root.divider
        Behavior on color { ColorAnimation { duration: app.animationsEnabled ? 120 : 0 } }

        HoverHandler { id: creditHover }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 11

            Rectangle {
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
                radius: 11
                color: theme.isDark ? "#263A34" : "#E6F6F1"
                Text {
                    anchors.centerIn: parent
                    text: credit.mark
                    color: app.accentColor
                    font.pixelSize: 11
                    font.bold: true
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7
                    Text {
                        text: credit.name
                        color: theme.textColor
                        font.pixelSize: 10
                        font.bold: true
                    }
                    Rectangle {
                        visible: credit.badge.length > 0
                        Layout.preferredWidth: badgeText.implicitWidth + 14
                        Layout.preferredHeight: 20
                        radius: 10
                        color: root.softColor
                        Text {
                            id: badgeText
                            anchors.centerIn: parent
                            text: credit.badge
                            color: root.muted
                            font.pixelSize: 7
                            font.bold: true
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
                Text {
                    Layout.fillWidth: true
                    text: credit.caption
                    color: root.muted
                    font.pixelSize: 8
                    elide: Text.ElideRight
                }
            }
            AppC.AppButton {
                theme: root.theme
                text: app.language === "en-US" ? "Open link" : "打开链接"
                iconText: "↗"
                variant: "ghost"
                sidePadding: 11
                onClicked: Qt.openUrlExternally(credit.url)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: app.language === "en-US" ? "Settings" : "设置"
                    color: theme.textColor
                    font.pixelSize: 25
                    font.bold: true
                }
                Text {
                    text: app.language === "en-US" ? "Personalize EvolveMusic without digging through one endless page" : "按分类调整 EvolveMusic，设置更清楚也更好找"
                    color: root.muted
                    font.pixelSize: 9
                }
            }
            Rectangle {
                width: 104; height: 32; radius: 16
                color: theme.isDark ? "#243B34" : "#E2F6EF"
                border.width: 1
                border.color: app.accentColor
                Text { anchors.centerIn: parent; text: "v0.18.1"; color: app.accentColor; font.pixelSize: 9; font.bold: true }
            }
        }

        Flow {
            Layout.fillWidth: true
            visible: root.compactCategories
            spacing: 7
            Repeater {
                model: root.categoryModel()
                delegate: CategoryTile {
                    required property var modelData
                    required property int index
                    compact: true
                    category: index
                    title: modelData.title
                    subtitle: modelData.subtitle
                    glyph: modelData.glyph
                    onActivated: root.categoryIndex = category
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 14

            Rectangle {
                visible: !root.compactCategories
                Layout.preferredWidth: 218
                Layout.fillHeight: true
                radius: 18
                color: root.cardColor
                border.width: 1
                border.color: root.divider

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 5
                    Text {
                        Layout.fillWidth: true
                        Layout.leftMargin: 8
                        Layout.topMargin: 5
                        Layout.bottomMargin: 4
                        text: app.language === "en-US" ? "Categories" : "设置分类"
                        color: root.muted
                        font.pixelSize: 8
                        font.bold: true
                    }
                    Repeater {
                        model: root.categoryModel()
                        delegate: CategoryTile {
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            category: index
                            title: modelData.title
                            subtitle: modelData.subtitle
                            glyph: modelData.glyph
                            onActivated: root.categoryIndex = category
                        }
                    }
                    Item { Layout.fillHeight: true }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.shortCategories ? 50 : 62
                        radius: 13
                        color: root.softColor
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 9
                            Rectangle { width: 30; height: 30; radius: 15; color: app.accentColor; Text { anchors.centerIn: parent; text: "E"; color: "#14201D"; font.bold: true; font.pixelSize: 13 } }
                            ColumnLayout { Layout.fillWidth: true; spacing: 1
                                Text { text: app.accountUsername || "Evolve"; color: theme.textColor; font.pixelSize: 9; font.bold: true }
                                Text { text: app.cloudReady ? "Cloud Online" : "Cloud Checking"; color: app.cloudReady ? app.accentColor : root.muted; font.pixelSize: 7 }
                            }
                        }
                    }
                }
            }

            ScrollView {
                id: contentScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                ColumnLayout {
                    width: contentScroll.availableWidth
                    spacing: 12

                    ColumnLayout {
                        visible: root.categoryIndex === 0
                        Layout.fillWidth: true
                        spacing: 12
                        SettingsCard {
                            heading: app.language === "en-US" ? "Appearance" : "外观与主题"
                            caption: app.language === "en-US" ? "Choose a preset and tune the accent without losing visual consistency." : "选择整体主题，再单独调整强调色；亮色和暗色都会使用统一控件样式。"
                            SettingRow {
                                heading: app.language === "en-US" ? "Theme preset" : "主题风格"
                                caption: app.themePreset
                                AppC.AppComboBox {
                                    theme: root.theme; anchors.fill: parent
                                    model: ["Evolve", "Midnight", "Forest", "Violet", "Paper", "OLED"]
                                    currentIndex: Math.max(0, ["evolve","midnight","forest","violet","paper","oled"].indexOf(app.themePreset))
                                    onActivated: app.setThemePreset(["evolve","midnight","forest","violet","paper","oled"][currentIndex])
                                }
                            }
                            SettingRow {
                                heading: app.language === "en-US" ? "Accent color" : "强调色"
                                caption: app.accentColor
                                Row {
                                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; spacing: 7
                                    Repeater {
                                        model: ["#13D9B0", "#8A72F1", "#E76ACB", "#65A7FF", "#F1B85B", "#F06E6E"]
                                        delegate: Rectangle {
                                            required property var modelData
                                            width: 25; height: 25; radius: 13; color: modelData
                                            border.width: String(app.accentColor).toLowerCase() === String(modelData).toLowerCase() ? 2 : 1
                                            border.color: String(app.accentColor).toLowerCase() === String(modelData).toLowerCase() ? theme.textColor : root.divider
                                            scale: colorMouse.containsMouse ? 1.09 : 1
                                            Behavior on scale { NumberAnimation { duration: app.animationsEnabled ? 100 : 0 } }
                                            MouseArea { id: colorMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: app.setAccentColor(String(modelData)) }
                                        }
                                    }
                                }
                            }
                            SettingRow {
                                heading: app.language === "en-US" ? "Animation intensity" : "动画强度"
                                caption: app.language === "en-US" ? "Off / Balanced / Rich" : "关闭 / 平衡 / 丰富"
                                AppC.AppComboBox {
                                    theme: root.theme; anchors.fill: parent
                                    model: app.language === "en-US" ? ["Off", "Balanced", "Rich"] : ["关闭", "平衡", "丰富"]
                                    currentIndex: Math.max(0, ["off","balanced","rich"].indexOf(app.animationLevel))
                                    onActivated: app.setAnimationLevel(["off","balanced","rich"][currentIndex])
                                }
                            }
                            SettingRow {
                                heading: app.language === "en-US" ? "Language" : "界面语言"
                                caption: app.language === "en-US" ? "English is beta" : "English 当前为 Beta"
                                AppC.AppComboBox {
                                    theme: root.theme; anchors.fill: parent
                                    model: ["简体中文", "English (Beta)"]
                                    currentIndex: app.language === "en-US" ? 1 : 0
                                    onActivated: app.setLanguage(currentIndex === 1 ? "en-US" : "zh-CN")
                                }
                            }
                            SettingRow {
                                heading: app.language === "en-US" ? "Interface font" : "界面字体"
                                caption: app.language === "en-US" ? "Saved immediately; restart EvolveMusic to apply safely" : "选择会立即保存；重启 EvolveMusic 后安全应用"
                                AppC.AppComboBox {
                                    theme: root.theme; anchors.fill: parent
                                    model: app.availableFonts
                                    // Restart-only font selection: initialize once instead of
                                    // maintaining a live binding to app.fontFamily. Qt 6.10
                                    // could recurse through ComboBox/model bindings when cloud
                                    // state arrived and invalidated the property.
                                    Component.onCompleted: currentIndex = Math.max(0, app.availableFonts.indexOf(app.fontFamily))
                                    onActivated: app.setFontFamily(String(app.availableFonts[currentIndex]))
                                }
                            }
                        }
                        SettingsCard {
                            heading: app.language === "en-US" ? "Interface density" : "界面显示"
                            caption: app.language === "en-US" ? "Control how much information is visible at once." : "调整歌曲列表密度和音源状态显示。"
                            SettingRow {
                                heading: app.language === "en-US" ? "Compact track rows" : "紧凑歌曲列表"
                                caption: app.language === "en-US" ? "Fit more tracks on screen" : "同屏显示更多歌曲"
                                AppC.SettingsToggle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; checked: app.compactTrackRows; onToggled: function(v) { app.setCompactTrackRows(v) } }
                            }
                            SettingRow {
                                heading: app.language === "en-US" ? "Source badges" : "显示音源标记"
                                caption: app.language === "en-US" ? "Show Ourcraft platforms and fallback status" : "显示 Ourcraft 的网易/酷狗/酷我及备用来源"
                                AppC.SettingsToggle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; checked: app.showSourceBadges; onToggled: function(v) { app.setShowSourceBadges(v) } }
                            }
                        }
                    }

                    ColumnLayout {
                        visible: root.categoryIndex === 1
                        Layout.fillWidth: true
                        spacing: 12
                        SettingsCard {
                            heading: app.language === "en-US" ? "Playback" : "播放"
                            caption: app.language === "en-US" ? "Choose the default quality used when a source supports it." : "选择默认音质；音源不支持时会自动降级。"
                            SettingRow {
                                heading: app.language === "en-US" ? "Audio quality" : "默认音质"
                                caption: app.qualityLevel
                                AppC.AppComboBox {
                                    theme: root.theme; anchors.fill: parent
                                    model: ["standard", "higher", "exhigh", "lossless"]
                                    currentIndex: Math.max(0, model.indexOf(app.qualityLevel))
                                    onActivated: app.setQualityLevel(model[currentIndex])
                                }
                            }
                        }
                        SettingsCard {
                            heading: app.language === "en-US" ? "Recommendations" : "智能推荐"
                            caption: app.language === "en-US" ? "Lower values stay close to your history; higher values explore farther." : "探索度越低越贴近收藏与最近播放，越高越主动发现新歌曲。"
                            SettingRow {
                                heading: app.language === "en-US" ? "Recommendation diversity" : "推荐探索度"
                                caption: (app.recommendationDiversity < 35 ? "更熟悉" : (app.recommendationDiversity > 72 ? "更多发现" : "平衡")) + " · " + app.recommendationDiversity + "%"
                                Slider {
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                                    from: 0; to: 100; stepSize: 1; value: app.recommendationDiversity
                                    onMoved: app.setRecommendationDiversity(Math.round(value))
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 66; radius: 13
                                color: root.softColor
                                RowLayout { anchors.fill: parent; anchors.margins: 11; spacing: 10
                                    Rectangle { width: 34; height: 34; radius: 11; color: app.accentColor; Text { anchors.centerIn: parent; text: "AI"; color: "#14201D"; font.pixelSize: 9; font.bold: true } }
                                    ColumnLayout { Layout.fillWidth: true; spacing: 2
                                        Text { text: app.language === "en-US" ? "Current recommendation strategy" : "当前推荐策略"; color: theme.textColor; font.pixelSize: 9; font.bold: true }
                                        Text { Layout.fillWidth: true; text: app.language === "en-US" ? "Original/studio first, then DJ/remix, live and covers unless the query asks for a version." : "搜索默认原唱/录音室优先，再到 DJ/Remix、Live 和翻唱；明确搜版本关键词时会反转优先级。"; color: root.muted; font.pixelSize: 8; wrapMode: Text.Wrap }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        visible: root.categoryIndex === 2
                        Layout.fillWidth: true
                        spacing: 12
                        SettingsCard {
                            heading: app.language === "en-US" ? "Cache & storage" : "缓存与存储"
                            caption: app.cacheDirectory
                            SettingRow {
                                heading: app.language === "en-US" ? "Cache folder" : "缓存文件夹"
                                caption: (app.language === "en-US" ? "Current size: " : "当前占用：") + app.cacheSizeText
                                Row {
                                    anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; spacing: 7
                                    AppC.AppButton { theme: root.theme; text: app.language === "en-US" ? "Change" : "更改"; variant: "secondary"; onClicked: app.chooseCacheDirectory() }
                                    AppC.AppButton { theme: root.theme; text: app.language === "en-US" ? "Reset" : "默认"; variant: "ghost"; onClicked: app.resetCacheDirectory() }
                                }
                            }
                            SettingRow {
                                heading: app.language === "en-US" ? "Clear cache" : "清理缓存"
                                caption: app.language === "en-US" ? "Artwork, network and audio cache" : "清理封面、网络和音频缓存"
                                AppC.AppButton { theme: root.theme; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: app.language === "en-US" ? "Clear now" : "立即清理"; variant: "secondary"; onClicked: app.clearCache() }
                            }
                        }
                    }

                    ColumnLayout {
                        visible: root.categoryIndex === 3
                        Layout.fillWidth: true
                        spacing: 12
                        SettingsCard {
                            heading: app.language === "en-US" ? "Music backend" : "音乐后端"
                            caption: app.language === "en-US"
                                     ? "Ourcraft Music API is now the primary music backend. NetEase, KuGou and Kuwo are queried through the same API; Evolve Cloud remains only for accounts, community and Together."
                                     : "现在使用 Ourcraft Music API 作为主音乐后端；网易、酷狗、酷我共用这一套 API。Evolve Cloud 只负责账号、社区、一起听和更新。"
                            SettingRow {
                                heading: app.language === "en-US" ? "Ourcraft API address" : "Ourcraft API 地址"
                                caption: app.language === "en-US" ? "You can replace this with your own deployment of Yuncan050115/ourcraft-music-api." : "可替换为你自己部署的 Yuncan050115/ourcraft-music-api 地址。"
                                RowLayout {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Math.min(470, parent.width * 0.58)
                                    spacing: 7
                                    AppC.AppTextField {
                                        id: ourcraftApiField
                                        theme: root.theme
                                        Layout.fillWidth: true
                                        compact: true
                                        text: app.ourcraftApiUrl
                                        placeholderText: "https://music.yuncan.xyz"
                                        onAccepted: app.setOurcraftApiUrl(text)
                                    }
                                    AppC.AppButton {
                                        theme: root.theme
                                        text: app.language === "en-US" ? "Save" : "保存"
                                        variant: "primary"
                                        onClicked: app.setOurcraftApiUrl(ourcraftApiField.text)
                                    }
                                    AppC.AppButton {
                                        theme: root.theme
                                        text: app.language === "en-US" ? "Default" : "默认"
                                        variant: "ghost"
                                        onClicked: { app.resetOurcraftApiUrl(); ourcraftApiField.text = app.ourcraftApiUrl }
                                    }
                                }
                            }
                            Repeater {
                                model: app.providers
                                delegate: Rectangle {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 68
                                    radius: 14
                                    color: root.softColor
                                    border.width: 1
                                    border.color: root.divider
                                    RowLayout {
                                        anchors.fill: parent; anchors.margins: 10; spacing: 10
                                        Rectangle {
                                            width: 36; height: 36; radius: 11
                                            color: modelData.online ? (theme.isDark ? "#233D35" : "#E3F5EF") : (theme.isDark ? "#30302B" : "#ECEEF1")
                                            Text { anchors.centerIn: parent; text: "♪"; color: modelData.online ? app.accentColor : root.muted; font.pixelSize: 13 }
                                        }
                                        ColumnLayout { Layout.fillWidth: true; spacing: 2
                                            RowLayout { Layout.fillWidth: true
                                                Text { text: modelData.name; color: theme.textColor; font.pixelSize: 10; font.bold: true }
                                                Rectangle { width: 7; height: 7; radius: 4; color: modelData.online ? "#46D59A" : "#E0A85A" }
                                                Item { Layout.fillWidth: true }
                                            }
                                            Text { Layout.fillWidth: true; text: modelData.status || modelData.baseUrl || ""; color: root.muted; font.pixelSize: 8; elide: Text.ElideMiddle }
                                        }
                                        AppC.AppButton { theme: root.theme; visible: modelData.id === "yueting" || modelData.id === "gequhai"; text: app.language === "en-US" ? "Website" : "访问网站"; variant: "ghost"; onClicked: app.openProviderWebsite(modelData.id) }
                                        AppC.AppButton { theme: root.theme; text: app.language === "en-US" ? "Test" : "测试"; variant: "secondary"; onClicked: app.testProvider(modelData.id) }
                                        AppC.SettingsToggle { checked: modelData.enabled; onToggled: function(v) { app.setProviderEnabled(modelData.id, v) } }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        visible: root.categoryIndex === 4
                        Layout.fillWidth: true
                        spacing: 12
                        SettingsCard {
                            heading: app.language === "en-US" ? "Desktop behavior" : "桌面行为"
                            caption: app.language === "en-US" ? "Background behavior and Windows integration." : "后台运行、开机启动与 Windows 集成。"
                            SettingRow {
                                heading: app.language === "en-US" ? "Keep running in tray" : "关闭后在托盘运行"
                                caption: app.language === "en-US" ? "Closing the window keeps playback alive" : "关闭窗口后音乐继续在后台运行"
                                AppC.SettingsToggle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; checked: desktop.backgroundEnabled; onToggled: function(v) { desktop.setBackgroundEnabled(v) } }
                            }
                            SettingRow {
                                heading: app.language === "en-US" ? "Start with Windows" : "开机自启动"
                                AppC.SettingsToggle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; checked: desktop.autoStartEnabled; onToggled: function(v) { desktop.setAutoStartEnabled(v) } }
                            }
                        }
                        SettingsCard {
                            heading: app.language === "en-US" ? "Updates & diagnostics" : "更新与诊断"
                            caption: desktop.updateStatus
                            SettingRow {
                                heading: app.language === "en-US" ? "Automatic updates" : "自动更新"
                                AppC.SettingsToggle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; checked: desktop.autoUpdateEnabled; onToggled: function(v) { desktop.setAutoUpdateEnabled(v) } }
                            }
                            SettingRow {
                                heading: app.language === "en-US" ? "Process protection" : "进程保护"
                                caption: app.language === "en-US" ? "Restart after unexpected exits with crash-loop protection" : "异常退出后自动恢复，同时防止崩溃循环"
                                AppC.SettingsToggle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; checked: desktop.processProtectionEnabled; onToggled: function(v) { desktop.setProcessProtectionEnabled(v) } }
                            }
                            SettingRow {
                                heading: app.language === "en-US" ? "Crash warnings" : "崩溃提示与警告"
                                caption: app.language === "en-US" ? "Show recovery information and keep diagnostics" : "显示恢复信息并保留诊断记录"
                                AppC.SettingsToggle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; checked: desktop.crashWarningsEnabled; onToggled: function(v) { desktop.setCrashWarningsEnabled(v) } }
                            }
                            SettingRow {
                                heading: app.language === "en-US" ? "Runtime logs" : "运行日志"
                                caption: desktop.logDirectory
                                AppC.AppButton { theme: root.theme; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: app.language === "en-US" ? "Open logs" : "打开日志"; variant: "secondary"; onClicked: desktop.openLogFolder() }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.topMargin: 4
                                AppC.AppButton { theme: root.theme; text: app.language === "en-US" ? "Check updates" : "检查更新"; variant: "primary"; enabled: !desktop.updateBusy; onClicked: desktop.checkForUpdates() }
                                AppC.AppButton { theme: root.theme; visible: desktop.updateAvailable; text: app.language === "en-US" ? "Install" : "立即更新"; variant: "secondary"; onClicked: desktop.installAvailableUpdate() }
                                Item { Layout.fillWidth: true }
                            }
                        }
                    }

                    ColumnLayout {
                        visible: root.categoryIndex === 5
                        Layout.fillWidth: true
                        spacing: 12
                        SettingsCard {
                            heading: app.language === "en-US" ? "Evolve account" : "Evolve 账户"
                            caption: app.language === "en-US" ? "Profile, playlists and social data are synchronized through Evolve Cloud." : "个人资料、自建歌单和社交数据通过 Evolve Cloud 同步。"
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 74; radius: 14
                                color: root.softColor
                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 11; spacing: 10
                                    Rectangle { width: 38; height: 38; radius: 19; color: app.accentColor; Text { anchors.centerIn: parent; text: (app.accountUsername || "E").slice(0,1).toUpperCase(); color: "#14201D"; font.pixelSize: 13; font.bold: true } }
                                    ColumnLayout { Layout.fillWidth: true; spacing: 2
                                        Text { text: app.accountUsername || "—"; color: theme.textColor; font.pixelSize: 11; font.bold: true }
                                        Text { Layout.fillWidth: true; text: "Evolve Cloud · " + app.cloudApiUrl; color: root.muted; font.pixelSize: 8; elide: Text.ElideMiddle }
                                    }
                                    AppC.AppButton { theme: root.theme; text: app.language === "en-US" ? "Sign out" : "退出登录"; variant: "ghost"; onClicked: app.logoutAccount() }
                                }
                            }
                        }
                        SettingsCard {
                            heading: app.language === "en-US" ? "Cloud route" : "云端线路"
                            caption: app.language === "en-US" ? "Choose a fixed route, or let EvolveMusic measure both nodes and use the faster available one." : "可固定使用一个节点，也可由 EvolveMusic 测量两个节点并自动选择延迟更低的可用线路。"
                            SettingRow {
                                heading: app.language === "en-US" ? "Route mode" : "线路模式"
                                caption: app.cloudRouteMode === "auto"
                                         ? (app.language === "en-US" ? "Automatic mode keeps failover enabled." : "自动模式会保留请求失败后的备用节点切换。")
                                         : (app.language === "en-US" ? "Manual mode only uses the selected node." : "手动模式只连接所选节点。")
                                AppC.AppComboBox {
                                    theme: root.theme
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 190
                                    model: app.language === "en-US"
                                           ? ["Automatic (latency)", "Workers.dev", "tyxowo.top relay"]
                                           : ["自动（按延迟）", "Workers.dev 主节点", "tyxowo.top 中转"]
                                    currentIndex: app.cloudRouteMode === "primary" ? 1
                                                  : (app.cloudRouteMode === "relay" ? 2 : 0)
                                    onActivated: function(index) {
                                        app.setCloudRouteMode(index === 1 ? "primary"
                                                                  : (index === 2 ? "relay" : "auto"))
                                    }
                                }
                            }
                            SettingRow {
                                heading: "workers.dev"
                                caption: "evolvemusic-cloud.18048369193.workers.dev"
                                Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 104; height: 27; radius: 14; color: app.cloudApiUrl.indexOf("workers.dev") >= 0 ? (theme.isDark ? "#203A32" : "#E2F5EE") : root.softColor; Text { anchors.centerIn: parent; text: app.cloudRouteTesting ? "检测中" : (app.cloudPrimaryLatency >= 0 ? ((app.cloudApiUrl.indexOf("workers.dev") >= 0 ? "使用中 · " : "") + app.cloudPrimaryLatency + " ms") : "不可用"); color: app.cloudApiUrl.indexOf("workers.dev") >= 0 ? app.accentColor : root.muted; font.pixelSize: 8; font.bold: true } }
                            }
                            SettingRow {
                                heading: "中转.tyxowo.top"
                                caption: "xn--fiqq40n.tyxowo.top"
                                Rectangle { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; width: 104; height: 27; radius: 14; color: app.cloudApiUrl.indexOf("tyxowo.top") >= 0 ? (theme.isDark ? "#203A32" : "#E2F5EE") : root.softColor; Text { anchors.centerIn: parent; text: app.cloudRouteTesting ? "检测中" : (app.cloudRelayLatency >= 0 ? ((app.cloudApiUrl.indexOf("tyxowo.top") >= 0 ? "使用中 · " : "") + app.cloudRelayLatency + " ms") : "不可用"); color: app.cloudApiUrl.indexOf("tyxowo.top") >= 0 ? app.accentColor : root.muted; font.pixelSize: 8; font.bold: true } }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                Text { text: app.language === "en-US" ? "Active: " + app.cloudApiUrl : "当前：" + app.cloudApiUrl; color: root.muted; font.pixelSize: 8; elide: Text.ElideMiddle; Layout.maximumWidth: 430 }
                                AppC.AppButton { theme: root.theme; text: app.cloudRouteTesting ? (app.language === "en-US" ? "Testing…" : "测速中…") : (app.language === "en-US" ? "Test routes" : "重新测速"); variant: "secondary"; enabled: !app.cloudRouteTesting; onClicked: app.testCloudRoutes() }
                            }
                        }
                    }

                    ColumnLayout {
                        visible: root.categoryIndex === 6
                        Layout.fillWidth: true
                        spacing: 12
                        SettingsCard {
                            heading: app.language === "en-US" ? "Extension runtime" : "插件扩展底座"
                            caption: app.language === "en-US"
                                     ? "Plugins run as explicitly installed local extensions. Player, network, files and account data are separate permissions."
                                     : "插件作为用户明确安装的本地扩展运行。播放器、网络、文件和账户数据分别授权，不再把插件当作一个独立页面。"
                            RowLayout {
                                Layout.fillWidth: true
                                AppC.AppButton { theme: root.theme; text: app.language === "en-US" ? "Create sample" : "创建示例"; variant: "ghost"; onClicked: app.createPluginTemplate() }
                                AppC.AppButton { theme: root.theme; text: app.language === "en-US" ? "Open folder" : "打开目录"; variant: "secondary"; onClicked: app.openPluginDirectory() }
                                AppC.AppButton { theme: root.theme; text: app.language === "en-US" ? "Rescan" : "重新扫描"; variant: "primary"; onClicked: app.reloadPlugins() }
                                Item { Layout.fillWidth: true }
                                Text { text: app.plugins.length + (app.language === "en-US" ? " installed" : " 个已安装"); color: root.muted; font.pixelSize: 8 }
                            }
                        }
                        Repeater {
                            model: app.plugins
                            delegate: Rectangle {
                                id: pluginCard
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.preferredHeight: 112 + ((modelData.permissions || []).length * 32)
                                radius: 16
                                color: root.cardColor
                                border.width: 1
                                border.color: root.divider
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 14; spacing: 6
                                    RowLayout {
                                        Layout.fillWidth: true
                                        ColumnLayout { Layout.fillWidth: true; spacing: 1
                                            Text { text: (modelData.name || modelData.id) + "  v" + (modelData.version || "1.0.0"); color: theme.textColor; font.pixelSize: 11; font.bold: true }
                                            Text { Layout.fillWidth: true; text: modelData.description || "暂无简介"; color: root.muted; font.pixelSize: 8; elide: Text.ElideRight }
                                        }
                                        AppC.SettingsToggle { checked: !!modelData.enabled; onToggled: function(v) { app.setPluginEnabled(modelData.id, v) } }
                                    }
                                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.divider }
                                    Repeater {
                                        model: modelData.permissions || []
                                        delegate: RowLayout {
                                            required property var modelData
                                            Layout.fillWidth: true; Layout.preferredHeight: 26
                                            Text { Layout.fillWidth: true; text: String(modelData); color: theme.textColor; font.pixelSize: 9 }
                                            Text { text: String(modelData).indexOf("account") === 0 ? "敏感权限" : "扩展权限"; color: root.muted; font.pixelSize: 7 }
                                            AppC.SettingsToggle {
                                                checked: (pluginCard.modelData.grantedPermissions || []).indexOf(String(modelData)) >= 0
                                                onToggled: function(v) { app.setPluginPermission(pluginCard.modelData.id, String(modelData), v) }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        Text { visible: app.plugins.length === 0; Layout.fillWidth: true; text: app.language === "en-US" ? "No local extensions installed." : "还没有安装本地扩展。可先创建示例或打开插件目录。"; color: root.muted; font.pixelSize: 9 }
                    }

                    ColumnLayout {
                        visible: root.categoryIndex === 7
                        Layout.fillWidth: true
                        spacing: 12

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: root.compactCategories ? 218 : 184
                            radius: app.themePreset === "paper" ? 14 : 18
                            color: theme.isDark ? "#25312D" : "#EAF7F3"
                            border.width: 1
                            border.color: theme.isDark ? "#26FFFFFF" : "#12000000"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 18
                                spacing: 11

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 14
                                    Rectangle {
                                        Layout.preferredWidth: 58
                                        Layout.preferredHeight: 58
                                        radius: 17
                                        color: app.accentColor
                                        Text {
                                            anchors.centerIn: parent
                                            text: "E"
                                            color: "#14201D"
                                            font.pixelSize: 27
                                            font.bold: true
                                            font.italic: true
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 3
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8
                                            Text {
                                                text: "EvolveMusic"
                                                color: theme.textColor
                                                font.pixelSize: 20
                                                font.bold: true
                                            }
                                            Rectangle {
                                                Layout.preferredWidth: versionText.implicitWidth + 16
                                                Layout.preferredHeight: 22
                                                radius: 11
                                                color: theme.isDark ? "#193D34" : "#D7F1E9"
                                                Text {
                                                    id: versionText
                                                    anchors.centerIn: parent
                                                    text: "v0.18.1"
                                                    color: app.accentColor
                                                    font.pixelSize: 8
                                                    font.bold: true
                                                }
                                            }
                                            Item { Layout.fillWidth: true }
                                        }
                                        Text {
                                            text: app.language === "en-US" ? "Created by 小学扛把子" : "作者：小学扛把子"
                                            color: theme.textColor
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: app.language === "en-US"
                                                  ? "A Windows desktop music player for discovery, playlists, social listening and extensible playback."
                                                  : "面向 Windows 的桌面音乐播放器，聚合发现、歌单、一起听与可扩展播放能力。"
                                            color: root.muted
                                            font.pixelSize: 8
                                            wrapMode: Text.Wrap
                                        }
                                    }
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    AppC.AppButton {
                                        theme: root.theme
                                        text: app.language === "en-US" ? "Author on Bilibili" : "访问作者 Bilibili"
                                        iconText: "▶"
                                        variant: "primary"
                                        onClicked: Qt.openUrlExternally("https://space.bilibili.com/1247730173")
                                    }
                                    AppC.AppButton {
                                        theme: root.theme
                                        text: app.language === "en-US" ? "GitHub repository" : "查看 GitHub 仓库"
                                        iconText: "↗"
                                        variant: "secondary"
                                        onClicked: Qt.openUrlExternally("https://github.com/YIRT66/QAQ")
                                    }
                                    Item { Layout.fillWidth: true }
                                }
                            }
                        }

                        SettingsCard {
                            heading: app.language === "en-US" ? "Open-source projects and libraries" : "开源项目与第三方库"
                            caption: app.language === "en-US"
                                     ? "Thanks to the projects below. Select a row to review its source and license information."
                                     : "感谢以下项目为 EvolveMusic 提供基础能力。可打开对应主页查看源码与许可说明。"
                            CreditRow {
                                mark: "Qt"
                                name: "Qt 6"
                                badge: "LGPL / GPL"
                                caption: app.language === "en-US" ? "Application framework, QML UI, multimedia and networking" : "应用框架、QML 界面、多媒体播放与网络能力"
                                url: "https://www.qt.io/"
                            }
                            CreditRow {
                                mark: "EU"
                                name: "EvolveUI"
                                badge: "MIT"
                                caption: app.language === "en-US" ? "QML component library by Sudo Evolve" : "Sudo Evolve 开发的 QML 组件库"
                                url: "https://github.com/sudoevolve/EvolveUI"
                            }
                            CreditRow {
                                mark: "NE"
                                name: "NeteaseCloudMusicApiEnhanced"
                                badge: "MIT"
                                caption: app.language === "en-US" ? "Local NetEase Cloud Music API runtime" : "本地网易云音乐 API 运行服务"
                                url: "https://github.com/NeteaseCloudMusicApiEnhanced/api-enhanced"
                            }
                            CreditRow {
                                mark: "FA"
                                name: "Font Awesome Free"
                                badge: "Free License"
                                caption: app.language === "en-US" ? "Icon font resources bundled through EvolveUI" : "通过 EvolveUI 使用的图标字体资源"
                                url: "https://github.com/FortAwesome/Font-Awesome"
                            }
                        }

                        SettingsCard {
                            heading: app.language === "en-US" ? "Music service" : "音乐服务"
                            caption: app.language === "en-US" ? "Online catalog and playback metadata services used by this build." : "当前版本接入的在线音乐目录与播放元数据服务。"
                            CreditRow {
                                mark: "OC"
                                name: "Ourcraft Music API"
                                badge: "Online API"
                                caption: "music.yuncan.xyz"
                                url: "https://music.yuncan.xyz"
                            }
                        }
                    }

                    Item { Layout.fillWidth: true; Layout.preferredHeight: 22 }
                }
            }
        }
    }
}
