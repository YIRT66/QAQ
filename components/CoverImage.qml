import QtQuick

Rectangle {
    id: root
    property string source: ""
    property int requestedSize: {
        var target = Math.max(64, Math.round(Math.max(width, height) * 1.35))
        // Quantize network decode sizes so window resizing/layout animations do
        // not trigger a fresh album-art request for every single pixel.
        return Math.min(1200, Math.ceil(target / 64) * 64)
    }
    property real cornerRadius: 12
    property color placeholderColor: "#202027"
    property color glyphColor: "#6D6D78"
    readonly property bool ready: art.status === Image.Ready
    property int candidateIndex: 0
    property string cloudEndpoint: app.cloudApiUrl

    onSourceChanged: candidateIndex = 0
    onRequestedSizeChanged: candidateIndex = 0

    radius: cornerRadius
    color: placeholderColor
    clip: true

    Text {
        anchors.centerIn: parent
        text: "♪"
        color: root.glyphColor
        font.pixelSize: Math.max(18, Math.min(root.width, root.height) * 0.28)
        opacity: art.status === Image.Ready ? 0 : 0.72
        Behavior on opacity { NumberAnimation { duration: 120 } }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "#0EFFFFFF"
        radius: root.cornerRadius
        z: 3
    }

    Image {
        id: art
        anchors.fill: parent
        source: {
            var endpointDependency = root.cloudEndpoint
            return app.coverCandidateUrl(root.source, root.requestedSize, root.candidateIndex)
        }
        asynchronous: true
        cache: root.requestedSize <= 384
        retainWhileLoading: true
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: root.requestedSize
        sourceSize.height: root.requestedSize
        opacity: status === Image.Ready ? 1 : 0

        onStatusChanged: {
            if (status !== Image.Error || root.source.length === 0)
                return
            var next = app.coverCandidateUrl(root.source, root.requestedSize,
                                             root.candidateIndex + 1)
            if (next.length > 0)
                root.candidateIndex += 1
        }

        Behavior on opacity { NumberAnimation { duration: 150 } }
    }

    Rectangle {
        anchors.fill: parent
        visible: art.status === Image.Loading
        color: "#06000000"
        SequentialAnimation on opacity {
            running: parent.visible
            loops: Animation.Infinite
            NumberAnimation { from: 0.08; to: 0.22; duration: 500 }
            NumberAnimation { from: 0.22; to: 0.08; duration: 500 }
        }
    }
}
