import QtQuick
import QtQuick.Controls

CheckBox {
    id: root
    property var theme
    spacing: 9
    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: root.leftPadding
        y: parent.height / 2 - height / 2
        radius: 6
        color: root.checked ? app.accentColor : (app.darkMode ? "#252520" : "#F1F3F6")
        border.width: 1
        border.color: root.checked ? app.accentColor : (app.darkMode ? "#32FFFFFF" : "#26000000")
        Text {
            anchors.centerIn: parent
            visible: root.checked
            text: "✓"
            color: "#14201D"
            font.pixelSize: 12
            font.bold: true
        }
    }
    contentItem: Text {
        leftPadding: root.indicator.width + root.spacing
        text: root.text
        color: root.theme ? root.theme.textColor : (app.darkMode ? "#F4F4F1" : "#202126")
        font.pixelSize: 9
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
    }
}
