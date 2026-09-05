import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    property string text: "Button"
    property string variant: "default" // "default" | "secondary" | "outline" | "ghost"
    property int buttonHeight: 36
    property bool disabled: false

    signal clicked()

    implicitWidth: contentRow.implicitWidth + 24
    implicitHeight: buttonHeight

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 6
        border.width: root.variant === "outline" ? 1 : 0
        border.color: "#27272A" // shadcn zinc-800

        // Determine background color based on variant and mouse state
        color: {
            if (root.disabled) return "#27272A";

            if (mouseArea.pressed) {
                if (root.variant === "secondary") return "#3F3F46";
                if (root.variant === "outline" || root.variant === "ghost") return "#27272A";
                return "#27272A";
            }

            if (mouseArea.containsMouse) {
                if (root.variant === "secondary") return "#3F3F46";
                if (root.variant === "outline" || root.variant === "ghost") return "#18181B";
                return "#27272A"; // zinc-800
            }

            // Normal state
            if (root.variant === "secondary") return "#27272A";
            if (root.variant === "outline" || root.variant === "ghost") return "transparent";
            return "#18181B"; // zinc-900 dark button
        }

        Behavior on color {
            ColorAnimation { duration: 150 }
        }

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: 6

            Text {
                id: labelText
                text: root.text
                color: root.disabled ? "#71717A" : (mouseArea.containsMouse ? "#FFFFFF" : "#FAFAFA")
                font.family: "Segoe UI, -apple-system, system-ui, sans-serif"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: !root.disabled
        cursorShape: root.disabled ? Qt.ForbiddenCursor : Qt.PointingHandCursor
        enabled: !root.disabled

        onClicked: root.clicked()
    }
}
