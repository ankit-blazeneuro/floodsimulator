import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QMapLibre 3.0 as MapLibre

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    title: "MapLibre Vector Map Explorer - Dark Edition"
    color: "#09090B" // shadcn zinc-950

    property double mapLatitude: 26.2006
    property double mapLongitude: 92.5000
    property double mapZoom: 8.5
    property double mapBearing: 0.0
    property double mapPitch: 0.0
    property string serverUrl: "http://localhost:8000"
    property bool serverOnline: true

    // 1. Full-Window MapLibre Map View
    MapLibre.MapView {
        id: map
        anchors.fill: parent

        // Set dark vector style loaded from local FastAPI backend
        styleUrl: window.serverUrl + "/style.json"
        latitude: window.mapLatitude
        longitude: window.mapLongitude
        zoomLevel: window.mapZoom
        bearing: window.mapBearing
        pitch: window.mapPitch

        onLatitudeChanged: window.mapLatitude = latitude
        onLongitudeChanged: window.mapLongitude = longitude
        onZoomLevelChanged: window.mapZoom = zoomLevel
        onBearingChanged: window.mapBearing = bearing
        onPitchChanged: window.mapPitch = pitch

        // Mouse drag and wheel zoom support
        MouseArea {
            anchors.fill: parent
            property real lastX: 0
            property real lastY: 0

            onPressed: {
                lastX = mouse.x
                lastY = mouse.y
            }

            onPositionChanged: {
                if (pressed) {
                    var dx = mouse.x - lastX
                    var dy = mouse.y - lastY
                    lastX = mouse.x
                    lastY = mouse.y
                    map.pan(dx, dy)
                }
            }

            onWheel: {
                if (wheel.angleDelta.y > 0) {
                    map.zoomLevel = Math.min(map.zoomLevel + 0.5, 18.0)
                } else if (wheel.angleDelta.y < 0) {
                    map.zoomLevel = Math.max(map.zoomLevel - 0.5, 2.0)
                }
            }
        }
    }

    // 2. Floating Control HUD (Top-Right shadcn/ui Dark Card)
    Rectangle {
        id: hudCard
        width: 320
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 20
        radius: 8
        color: "#E618181B" // 90% opacity zinc-900
        border.width: 1
        border.color: "#27272A" // zinc-800

        implicitHeight: contentColumn.implicitHeight + 32

        Column {
            id: contentColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 14

            // Header Row
            RowLayout {
                width: parent.width

                Column {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: "Map Explorer"
                        color: "#FAFAFA"
                        font.family: "Segoe UI, -apple-system, sans-serif"
                        font.pixelSize: 15
                        font.weight: Font.Bold
                    }

                    Text {
                        text: "Vector Tiles • MBTiles Engine"
                        color: "#71717A"
                        font.family: "Segoe UI, -apple-system, sans-serif"
                        font.pixelSize: 11
                    }
                }

                // Status Badge (Emerald Online indicator)
                Rectangle {
                    height: 22
                    width: statusRow.implicitWidth + 12
                    radius: 11
                    color: window.serverOnline ? "#052E16" : "#450A0A"
                    border.width: 1
                    border.color: window.serverOnline ? "#10B981" : "#EF4444"

                    Row {
                        id: statusRow
                        anchors.centerIn: parent
                        spacing: 5

                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: window.serverOnline ? "#10B981" : "#EF4444"
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: window.serverOnline ? "LIVE" : "OFFLINE"
                            color: window.serverOnline ? "#34D399" : "#F87171"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                        }
                    }
                }
            }

            // Divider
            Rectangle {
                width: parent.width
                height: 1
                color: "#27272A"
            }

            // Coordinates Display
            Rectangle {
                width: parent.width
                height: 52
                radius: 6
                color: "#09090B"
                border.width: 1
                border.color: "#27272A"

                Column {
                    anchors.centerIn: parent
                    spacing: 3

                    Text {
                        text: "LAT " + window.mapLatitude.toFixed(4) + "°   LON " + window.mapLongitude.toFixed(4) + "°"
                        color: "#A1A1AA"
                        font.family: "Consolas, 'Courier New', monospace"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }

                    Text {
                        text: "ZOOM " + window.mapZoom.toFixed(1) + "x  •  BEARING " + Math.round(window.mapBearing) + "°"
                        color: "#71717A"
                        font.family: "Consolas, 'Courier New', monospace"
                        font.pixelSize: 11
                    }
                }
            }

            // Section: Layer Visibility
            Text {
                text: "LAYERS"
                color: "#71717A"
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 0.8
            }

            Column {
                width: parent.width
                spacing: 8

                ShadcnSwitch {
                    label: "Highways & Roads"
                    description: "National & State Highway networks"
                    checked: true
                    onToggled: function(c) {
                        map.setLayoutProperty("road_major", "visibility", c ? "visible" : "none")
                        map.setLayoutProperty("road_minor", "visibility", c ? "visible" : "none")
                    }
                }

                ShadcnSwitch {
                    label: "Building 3D Footprints"
                    description: "Urban structures (Zoom 13+)"
                    checked: true
                    onToggled: function(c) {
                        map.setLayoutProperty("building", "visibility", c ? "visible" : "none")
                    }
                }

                ShadcnSwitch {
                    label: "Rivers & Waterbodies"
                    description: "Brahmaputra basin & lakes"
                    checked: true
                    onToggled: function(c) {
                        map.setLayoutProperty("water", "visibility", c ? "visible" : "none")
                        map.setLayoutProperty("waterway", "visibility", c ? "visible" : "none")
                    }
                }

                ShadcnSwitch {
                    label: "Place & City Labels"
                    description: "Cities, towns, and localities"
                    checked: true
                    onToggled: function(c) {
                        map.setLayoutProperty("place_label_city", "visibility", c ? "visible" : "none")
                        map.setLayoutProperty("place_label_town", "visibility", c ? "visible" : "none")
                    }
                }
            }

            // Divider
            Rectangle {
                width: parent.width
                height: 1
                color: "#27272A"
            }

            // Section: Navigation Controls
            Text {
                text: "CAMERA PRESETS"
                color: "#71717A"
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 0.8
            }

            RowLayout {
                width: parent.width
                spacing: 8

                ShadcnButton {
                    Layout.fillWidth: true
                    text: "Zoom In (+)"
                    variant: "secondary"
                    onClicked: map.zoomLevel = Math.min(map.zoomLevel + 1.0, 18.0)
                }

                ShadcnButton {
                    Layout.fillWidth: true
                    text: "Zoom Out (−)"
                    variant: "secondary"
                    onClicked: map.zoomLevel = Math.max(map.zoomLevel - 1.0, 2.0)
                }
            }

            RowLayout {
                width: parent.width
                spacing: 8

                ShadcnButton {
                    Layout.fillWidth: true
                    text: "🧭 Reset North"
                    variant: "outline"
                    onClicked: {
                        map.bearing = 0.0
                        map.pitch = 0.0
                    }
                }

                ShadcnButton {
                    Layout.fillWidth: true
                    text: "📍 Center Assam"
                    variant: "outline"
                    onClicked: {
                        map.latitude = 26.2006
                        map.longitude = 92.5000
                        map.zoomLevel = 8.5
                    }
                }
            }
        }
    }
}
