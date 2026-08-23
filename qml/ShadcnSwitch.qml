import QtQuick 2.15

Item {
    id: root

    property bool checked: false
    property string label: "Toggle Layer"
    property string description: ""

    signal toggled(bool isChecked)

    implicitWidth: mainRow.implicitWidth
    implicitHeight: 28

    Row {
        id: mainRow
        anchors.verticalCenter: parent.verticalCenter
        spacing: 12

        // Switch Track
        Rectangle {
            id: track
            width: 38
            height: 20
            radius: 10
            anchors.verticalCenter: parent.verticalCenter
            color: root.checked ? "#FAFAFA" : "#27272A" // zinc-100 when active, zinc-800 when inactive
            border.width: 1
            border.color: root.checked ? "#FFFFFF" : "#3F3F46"

            Behavior on color {
                ColorAnimation { duration: 200 }
            }

            // Switch Thumb
            Rectangle {
                id: thumb
                width: 14
                height: 14
                radius: 7
                y: 2
                x: root.checked ? 21 : 3
                color: root.checked ? "#18181B" : "#A1A1AA"

                Behavior on x {
                    NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
                }

                Behavior on color {
                    ColorAnimation { duration: 200 }
                }
            }
        }

        // Label Column
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: root.label
                color: root.checked ? "#FAFAFA" : "#A1A1AA"
                font.family: "Segoe UI, -apple-system, system-ui, sans-serif"
                font.pixelSize: 13
                font.weight: Font.Medium
            }

            Text {
                visible: root.description.length > 0
                text: root.description
                color: "#71717A" // zinc-500
                font.family: "Segoe UI, -apple-system, system-ui, sans-serif"
                font.pixelSize: 11
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
