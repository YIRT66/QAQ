import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../components" as AppC
import "../third_party/EvolveUI/components" as EUI

Rectangle {
    id: root
    property var theme
    property var iconFont
    property var window
    signal closeRequested()

    color: "#D90E0E12"

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: Math.min(parent.width - 64, 760)
        height: Math.min(parent.height - 64, 650)
        radius: 26
        clip: true
        color: theme.isDark ? "#17171C" : "#FFFFFF"
        border.width: 1
        border.color: theme.isDark ? "#22FFFFFF" : "#14000000"

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 62
                Layout.leftMargin: 18
                Layout.rightMargin: 14
                spacing: 10

                Rectangle {
                    width: 34
                    height: 34
                    radius: 10
                    color: "#E93645"
                    Text {
                        anchors.centerIn: parent
                        text: "♪"
                        color: "white"
                        font.pixelSize: 18
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text {
                        text: "登录 网易云音乐"
                        color: theme.textColor
                        font.pixelSize: 14
                        font.bold: true
                    }
                    Text {
                        text: "原生扫码登录 · 不依赖 WebView 网页渲染"
                        color: "#85858F"
                        font.pixelSize: 9
                    }
                }

                EUI.EButton {
                    text: "刷新二维码"
                    size: "xs"
                    shadowEnabled: false
                    enabled: !accounts.qrLoginBusy
                    onClicked: accounts.refreshNativeLogin()
                }

                AppC.PlayerIconButton {
                    width: 36
                    height: 36
                    iconFont: root.iconFont
                    glyph: "\uf00d"
                    glyphColor: theme.textColor
                    hoverColor: "#38D94848"
                    onClicked: root.closeRequested()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: theme.isDark ? "#18FFFFFF" : "#12000000"
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 80, 440)
                    spacing: 12

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 270
                        height: 270
                        radius: 24
                        color: theme.isDark ? "#202027" : "#FAFAFC"
                        border.width: 1
                        border.color: theme.isDark ? "#20FFFFFF" : "#13000000"

                        Rectangle {
                            anchors.centerIn: parent
                            width: 230
                            height: 230
                            radius: 16
                            color: "white"

                            Image {
                                id: qrImage
                                anchors.fill: parent
                                anchors.margins: 10
                                source: accounts.qrImageSource
                                asynchronous: false
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                visible: source.toString().length > 0 && status === Image.Ready
                                smooth: false
                            }

                            BusyIndicator {
                                anchors.centerIn: parent
                                running: accounts.qrLoginBusy && accounts.qrImageSource.length === 0
                                visible: running
                            }

                            Column {
                                anchors.centerIn: parent
                                width: parent.width - 28
                                spacing: 8
                                visible: !qrImage.visible && !accounts.qrLoginBusy
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "\uf071"
                                    font.family: iconFont.name
                                    font.pixelSize: 22
                                    color: "#D69A49"
                                }
                                Text {
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: accounts.qrLoginStatus.length > 0 ? accounts.qrLoginStatus : "二维码尚未生成"
                                    wrapMode: Text.Wrap
                                    color: "#777782"
                                    font.pixelSize: 10
                                }
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "使用网易云音乐 App 扫码"
                        horizontalAlignment: Text.AlignHCenter
                        color: theme.textColor
                        font.pixelSize: 18
                        font.bold: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: "在手机上确认后，Evolve Music 会自动验证账号并关闭此窗口"
                        horizontalAlignment: Text.AlignHCenter
                        color: "#85858F"
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                    }

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 4
                        width: Math.min(statusText.implicitWidth + 34, 380)
                        height: 34
                        radius: 17
                        color: accounts.qrLoginStatus.indexOf("成功") >= 0
                               ? "#1545D483"
                               : (accounts.qrLoginStatus.indexOf("失败") >= 0 || accounts.qrLoginStatus.indexOf("过期") >= 0
                                  ? "#18D94B56" : (theme.isDark ? "#24242B" : "#F2F2F6"))

                        Row {
                            anchors.centerIn: parent
                            spacing: 7
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 7
                                height: 7
                                radius: 4
                                color: accounts.qrLoginBusy ? "#F0B454"
                                      : (accounts.qrLoginStatus.indexOf("失败") >= 0 || accounts.qrLoginStatus.indexOf("过期") >= 0
                                         ? "#D94B56" : "#45D483")
                            }
                            Text {
                                id: statusText
                                anchors.verticalCenter: parent.verticalCenter
                                text: accounts.qrLoginStatus.length > 0 ? accounts.qrLoginStatus : "准备登录…"
                                color: theme.textColor
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: theme.isDark ? "#141419" : "#F7F7F9"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 8

                    Rectangle { width: 7; height: 7; radius: 4; color: "#45D483" }
                    Text {
                        Layout.fillWidth: true
                        text: "登录请求只连接本机网易云源服务 · 127.0.0.1:3000"
                        color: "#85858F"
                        font.pixelSize: 9
                    }
                    Text {
                        text: "不绕过会员 / DRM"
                        color: "#8D75FF"
                        font.pixelSize: 9
                    }
                }
            }
        }
    }

    Component.onDestruction: accounts.cancelLogin()
}
