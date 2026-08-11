import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    property var theme
    implicitHeight: 38
    implicitWidth: 176
    leftPadding: 13
    rightPadding: 34
    font.pixelSize: 10

    contentItem: Text {
        text: root.displayText
        color: root.theme ? root.theme.textColor : (app.darkMode ? "#F4F4F1" : "#202126")
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        font.pixelSize: 10
    }
    background: Rectangle {
        radius: 11
        color: app.darkMode ? "#24241F" : "#F2F4F7"
        border.width: root.activeFocus || root.popup.visible ? 1.5 : 1
        border.color: root.activeFocus || root.popup.visible ? app.accentColor : (app.darkMode ? "#18FFFFFF" : "#10000000")
    }
    indicator: Text {
        x: root.width - width - 12
        anchors.verticalCenter: parent.verticalCenter
        text: "⌄"
        color: app.darkMode ? "#A2A29B" : "#6F727A"
        font.pixelSize: 15
    }
    popup: Popup {
        y: root.height + 5
        width: root.width
        implicitHeight: contentItem.implicitHeight + 10
        padding: 5
        background: Rectangle {
            radius: 12
            color: app.darkMode ? "#2D2D27" : "#FFFFFF"
            border.color: app.darkMode ? "#20FFFFFF" : "#12000000"
        }
        contentItem: ListView {
            clip: true
            implicitHeight: Math.min(contentHeight, 260)
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
    delegate: ItemDelegate {
        width: root.width - 10
        height: 34
        highlighted: root.highlightedIndex === index
        background: Rectangle {
            radius: 8
            color: parent.highlighted ? (app.darkMode ? "#3A3A33" : "#EEF1F4") : "transparent"
        }
        contentItem: Text {
            text: modelData
            color: root.theme ? root.theme.textColor : (app.darkMode ? "#F4F4F1" : "#202126")
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 9
        }
    }
}
