import QtQuick
import QtQuick.Layouts

RowLayout {
    id: root
    property var theme
    property string eyebrow: "EVOLVE MUSIC"
    property string title: "标题"
    property string subtitle: ""
    property string actionText: ""
    signal actionClicked()
    spacing: 10
    ColumnLayout {
        Layout.fillWidth: true; spacing: 4
        Text { text: root.eyebrow; color: "#8D75FF"; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1.5 }
        Text { text: root.title; color: theme.textColor; font.pixelSize: 30; font.bold: true }
        Text { visible: root.subtitle.length > 0; text: root.subtitle; color: "#85858F"; font.pixelSize: 12 }
    }
    Text {
        visible: root.actionText.length > 0; text: root.actionText; color: "#8D75FF"; font.pixelSize: 12; font.bold: true
        MouseArea { anchors.fill: parent; anchors.margins: -8; cursorShape: Qt.PointingHandCursor; onClicked: root.actionClicked() }
    }
}
