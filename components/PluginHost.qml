import QtQuick

Item {
    id: root
    visible: false

    Repeater {
        model: app.plugins
        delegate: Loader {
            required property var modelData
            active: !!modelData.enabled && String(modelData.entryUrl || "").length > 0
            source: active ? modelData.entryUrl : ""
            asynchronous: true
            onStatusChanged: {
                if (status === Loader.Error)
                    console.warn("Evolve plugin failed:", modelData.id, source)
            }
        }
    }
}
