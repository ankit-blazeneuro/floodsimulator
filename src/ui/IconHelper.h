#pragma once

#include <QIcon>
#include <QPixmap>
#include <QColor>
#include <QFile>
#include <QString>
#include <QByteArray>
#include <QPainter>
#include <QRegularExpression>

namespace MapUI {

class IconHelper {
public:
    static QIcon get(const QString& name, const QColor& color = QColor(220, 220, 225), int size = 20) {
        QPixmap pix = getPixmap(name, color, size);
        if (pix.isNull()) return QIcon();
        return QIcon(pix);
    }

    static QPixmap getPixmap(const QString& name, const QColor& color = QColor(220, 220, 225), int size = 20) {
        QString resPath = ":/icons/" + name + ".svg";
        QFile file(resPath);
        if (!file.exists()) {
            resPath = "src/assets/icons/" + name + ".svg";
            file.setFileName(resPath);
        }

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return QPixmap();
        }

        QByteArray rawData = file.readAll();
        file.close();

        QString svg = QString::fromUtf8(rawData);

        // 1. Preserve exact opacity provided in SVG by converting any CSS var(...) to standard SVG opacity
        static QRegularExpression reStyleVar(R"(style="[^"]*opacity:\s*var\(--solar-secondary-opacity,\s*([0-9.]+)\)[^"]*")");
        svg.replace(reStyleVar, "opacity=\"\\1\"");

        static QRegularExpression reVarOpacity(R"(opacity:\s*var\(--solar-secondary-opacity,\s*([0-9.]+)\))");
        svg.replace(reVarOpacity, "opacity: \\1");

        // 2. Replace currentColor with the target hex color
        QString hexColor = color.name(QColor::HexRgb);
        svg.replace("currentColor", hexColor);
        svg.replace("var(--solar-secondary-color, currentColor)", hexColor);

        QByteArray data = svg.toUtf8();
        QPixmap pix;
        if (pix.loadFromData(data, "SVG")) {
            if (size > 0 && (pix.width() != size || pix.height() != size)) {
                return pix.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            return pix;
        }

        // Fallback without explicit format string if needed
        if (pix.loadFromData(data)) {
            if (size > 0 && (pix.width() != size || pix.height() != size)) {
                return pix.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
            return pix;
        }

        return QPixmap();
    }

    // Convenient Typed Icon Accessors
    static QIcon play(const QColor& color = QColor(255, 255, 255), int size = 18) {
        return get("play", color, size);
    }
    static QIcon pause(const QColor& color = QColor(255, 255, 255), int size = 18) {
        return get("pause", color, size);
    }
    static QIcon rewind(const QColor& color = QColor(212, 212, 216), int size = 16) {
        return get("rewind-forward", color, size);
    }
    static QIcon forward(const QColor& color = QColor(212, 212, 216), int size = 16) {
        return get("rewind-forward", color, size);
    }
    static QIcon zoomIn(const QColor& color = QColor(212, 212, 216), int size = 18) {
        return get("zoom-in", color, size);
    }
    static QIcon zoomOut(const QColor& color = QColor(212, 212, 216), int size = 18) {
        return get("zoom-out", color, size);
    }
    static QIcon ruler(const QColor& color = QColor(212, 212, 216), int size = 18) {
        return get("ruler", color, size);
    }
    static QIcon map(const QColor& color = QColor(212, 212, 216), int size = 18) {
        return get("map", color, size);
    }
    static QIcon radar(const QColor& color = QColor(212, 212, 216), int size = 18) {
        return get("radar", color, size);
    }
    static QIcon graph(const QColor& color = QColor(212, 212, 216), int size = 18) {
        return get("graph", color, size);
    }
    static QIcon rain(const QColor& color = QColor(212, 212, 216), int size = 18) {
        return get("rain", color, size);
    }
    static QIcon cloud(const QColor& color = QColor(212, 212, 216), int size = 18) {
        return get("cloud", color, size);
    }
    static QIcon fog(const QColor& color = QColor(212, 212, 216), int size = 18) {
        return get("fog", color, size);
    }
    static QIcon sunFog(const QColor& color = QColor(212, 212, 216), int size = 18) {
        return get("sun-fog", color, size);
    }
};

} // namespace MapUI
