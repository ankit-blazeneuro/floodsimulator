#include "TimelineWidget.h"
#include "IconHelper.h"
#include <QPainter>
#include <QPolygonF>
#include <cmath>
#include <algorithm>

namespace MapUI {

// ==========================================
// FrameRulerCanvas Implementation
// ==========================================

FrameRulerCanvas::FrameRulerCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(40);
    setMouseTracking(true);
    setCursor(Qt::SplitHCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void FrameRulerCanvas::setCurrentFrame(int frame) {
    currentFrame = std::clamp(frame, startFrame, endFrame);
    update();
    emit frameChanged(currentFrame);
}

void FrameRulerCanvas::setFrameRange(int start, int end) {
    startFrame = start;
    endFrame = std::max(start + 1, end);
    currentFrame = std::clamp(currentFrame, startFrame, endFrame);
    update();
}

void FrameRulerCanvas::setKeyframes(const std::vector<int>& keys) {
    keyframes = keys;
    update();
}

float FrameRulerCanvas::frameToX(int frame) const {
    float margin = 140.0f; // space for channel labels on left
    float availWidth = width() - margin - 20.0f;
    float range = static_cast<float>(endFrame - startFrame);
    if (range <= 0.0f) range = 1.0f;
    return margin + (static_cast<float>(frame - startFrame) / range) * availWidth + panOffset;
}

int FrameRulerCanvas::xToFrame(float x) const {
    float margin = 140.0f;
    float availWidth = width() - margin - 20.0f;
    float range = static_cast<float>(endFrame - startFrame);
    if (range <= 0.0f) range = 1.0f;

    float norm = (x - margin - panOffset) / availWidth;
    int f = startFrame + static_cast<int>(std::round(norm * range));
    return std::clamp(f, startFrame, endFrame);
}

void FrameRulerCanvas::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();
    int rulerH = 26;

    // 1. Background
    painter.fillRect(rect(), QColor(29, 29, 29));

    // 2. Active Playback Range Background
    float xStart = frameToX(startFrame);
    float xEnd = frameToX(endFrame);
    QRectF activeRangeRect(xStart, 0, xEnd - xStart, h);
    painter.fillRect(activeRangeRect, QColor(36, 36, 36));

    // 3. Track Rows (Dopesheet channels if height allows)
    struct TrackInfo {
        QString name;
        std::vector<int> keys;
        QColor color;
    };

    std::vector<TrackInfo> tracks = {
        {"Inundation Level", {0, 12, 24, 36, 48, 60, 72}, QColor(71, 114, 179)},
        {"Precipitation Rate", {0, 6, 18, 30, 42, 54, 66}, QColor(94, 186, 125)},
        {"Breach Dynamics", {24, 36, 48}, QColor(226, 109, 30)},
        {"River Discharge", {0, 24, 48, 72}, QColor(167, 139, 250)},
        {"Evacuation Status", {12, 24, 48}, QColor(248, 113, 113)}
    };

    int trackH = 22;
    int curY = rulerH;

    for (size_t i = 0; i < tracks.size() && (curY + trackH) <= h; ++i) {
        // Row background
        QColor rowBg = (i % 2 == 0) ? QColor(32, 32, 32) : QColor(28, 28, 28);
        painter.fillRect(QRect(0, curY, w, trackH), rowBg);

        // Row horizontal separator
        painter.setPen(QPen(QColor(42, 42, 42), 1.0));
        painter.drawLine(0, curY, w, curY);

        // Track channel header (Left column)
        painter.fillRect(QRect(0, curY, 135, trackH), QColor(24, 24, 24));
        painter.setPen(QPen(QColor(48, 48, 48), 1.0));
        painter.drawLine(135, curY, 135, curY + trackH);

        painter.setFont(QFont("Segoe UI", 8, QFont::DemiBold));
        painter.setPen(QColor(180, 180, 180));
        painter.drawText(QRect(8, curY, 122, trackH), Qt::AlignVCenter | Qt::AlignLeft, tracks[i].name);

        // Draw track keyframes
        for (int kf : tracks[i].keys) {
            if (kf >= startFrame && kf <= endFrame) {
                float kfX = frameToX(kf);
                float kfY = curY + trackH / 2.0f;

                QPolygonF diamond;
                diamond << QPointF(kfX, kfY - 4)
                        << QPointF(kfX + 4, kfY)
                        << QPointF(kfX, kfY + 4)
                        << QPointF(kfX - 4, kfY);

                painter.setBrush(tracks[i].color);
                painter.setPen(QPen(QColor(20, 20, 20), 0.8));
                painter.drawPolygon(diamond);
            }
        }

        curY += trackH;
    }

    // 4. Ruler Top Bar Background & Separator
    painter.fillRect(QRect(0, 0, w, rulerH), QColor(26, 26, 26));
    painter.fillRect(QRect(0, 0, 135, rulerH), QColor(22, 22, 22));
    painter.setPen(QPen(QColor(45, 45, 45), 1.0));
    painter.drawLine(0, rulerH, w, rulerH);
    painter.drawLine(135, 0, 135, h);

    painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
    painter.setPen(QColor(120, 120, 120));
    painter.drawText(QRect(8, 0, 122, rulerH), Qt::AlignVCenter | Qt::AlignLeft, "Channels / Tracks");

    // 5. Frame Grid & Ruler Tick Marks
    int frameStep = 10;
    int totalRange = endFrame - startFrame;
    if (totalRange > 120) frameStep = 20;
    else if (totalRange <= 30) frameStep = 5;

    for (int f = startFrame; f <= endFrame; ++f) {
        float x = frameToX(f);
        if (x < 135 || x > w) continue;

        if (f % frameStep == 0) {
            // Major tick mark
            painter.setPen(QPen(QColor(70, 70, 70), 1.0));
            painter.drawLine(QPointF(x, 0), QPointF(x, h));

            // Frame Number label in ruler bar
            painter.setPen(QColor(180, 180, 180));
            painter.drawText(QRectF(x - 18, 2, 36, 14), Qt::AlignCenter, QString::number(f));
        } else if (f % (frameStep / 2 == 0 ? 1 : frameStep / 2) == 0) {
            // Medium tick mark
            painter.setPen(QPen(QColor(48, 48, 48), 1.0));
            painter.drawLine(QPointF(x, rulerH - 8), QPointF(x, h));
        } else {
            // Minor tick mark
            painter.setPen(QPen(QColor(38, 38, 38), 1.0));
            painter.drawLine(QPointF(x, rulerH - 4), QPointF(x, rulerH));
        }
    }

    // 6. Playhead Indicator (Blue/Cyan line + Top Flag Badge)
    float curX = frameToX(currentFrame);
    if (curX >= 135) {
        // Vertical line across full ruler and all dopesheet tracks
        painter.setPen(QPen(QColor(86, 128, 194), 2.0));
        painter.drawLine(QPointF(curX, 0), QPointF(curX, h));

        // Top flag badge
        QPolygonF playheadFlag;
        playheadFlag << QPointF(curX - 7, 0)
                     << QPointF(curX + 7, 0)
                     << QPointF(curX + 7, 13)
                     << QPointF(curX, 19)
                     << QPointF(curX - 7, 13);

        painter.setBrush(QColor(86, 128, 194));
        painter.setPen(QPen(QColor(170, 210, 255), 1.0));
        painter.drawPolygon(playheadFlag);

        // Frame number inside flag
        painter.setFont(QFont("Segoe UI", 7, QFont::Bold));
        painter.setPen(Qt::white);
        painter.drawText(QRectF(curX - 12, 1, 24, 12), Qt::AlignCenter, QString::number(currentFrame));
    }
}

void FrameRulerCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        int newFrame = xToFrame(static_cast<float>(event->position().x()));
        setCurrentFrame(newFrame);
    }
}

void FrameRulerCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (isDragging) {
        int newFrame = xToFrame(static_cast<float>(event->position().x()));
        setCurrentFrame(newFrame);
    }
}

void FrameRulerCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
    }
}

void FrameRulerCanvas::wheelEvent(QWheelEvent* event) {
    int delta = event->angleDelta().y();
    if (delta > 0) {
        setCurrentFrame(currentFrame + 1);
    } else if (delta < 0) {
        setCurrentFrame(currentFrame - 1);
    }
}

// ==========================================
// TimelineWidget Implementation
// ==========================================

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setupUi();

    playTimer = new QTimer(this);
    connect(playTimer, &QTimer::timeout, this, &TimelineWidget::onTimerTick);
}

void TimelineWidget::setupUi() {
    setMinimumHeight(64);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setStyleSheet(R"(
        QWidget#timelineWidget {
            background-color: #242424;
            border-top: 1px solid #181818;
        }
        QWidget#controlBar {
            background-color: #1F1F1F;
            border-bottom: 1px solid #161616;
            min-height: 28px;
            max-height: 28px;
        }
        QPushButton {
            background-color: #2E2E2E;
            color: #D4D4D8;
            border: 1px solid #1C1C1C;
            border-radius: 3px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            font-weight: bold;
            min-width: 22px;
            max-width: 28px;
            min-height: 20px;
            max-height: 20px;
            padding: 1px 4px;
        }
        QPushButton:hover {
            background-color: #3C3C3C;
            color: #FFFFFF;
            border-color: #4D4D4D;
        }
        QPushButton:pressed {
            background-color: #181818;
        }
        QPushButton#btnPlayActive {
            background-color: #4772B3;
            color: #FFFFFF;
            border-color: #5680C2;
        }
        QPushButton#btnRecord:checked {
            background-color: #E87D0D;
            color: #FFFFFF;
            border-color: #FFA544;
        }
        QSpinBox {
            background-color: #161616;
            color: #E6E6E6;
            border: 1px solid #333333;
            border-radius: 3px;
            font-family: Consolas, 'Segoe UI', monospace;
            font-size: 11px;
            font-weight: bold;
            padding: 1px 4px;
            min-height: 20px;
            max-height: 20px;
            min-width: 50px;
        }
        QSpinBox:hover {
            border-color: #4772B3;
        }
        QSpinBox:focus {
            border-color: #5680C2;
            background-color: #0E0E0E;
        }
        QLabel {
            color: #A0A0A0;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            font-weight: 500;
        }
    )");

    setObjectName("timelineWidget");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Header Control Strip
    controlBar = new QWidget(this);
    controlBar->setObjectName("controlBar");

    auto* controlLayout = new QHBoxLayout(controlBar);
    controlLayout->setContentsMargins(8, 2, 8, 2);
    controlLayout->setSpacing(5);

    auto* lblHeaderTitle = new QLabel("Timeline", controlBar);
    lblHeaderTitle->setStyleSheet("color: #E26D1E; font-weight: bold; font-size: 11px; margin-right: 4px;");
    controlLayout->addWidget(lblHeaderTitle);

    // Playback Buttons with SVG icons
    btnJumpStart = new QPushButton(controlBar);
    btnJumpStart->setIcon(IconHelper::rewind(QColor(212, 212, 216), 14));
    btnJumpStart->setToolTip("Jump to Start (Shift + Left)");

    btnStepBack = new QPushButton(controlBar);
    btnStepBack->setIcon(IconHelper::rewind(QColor(180, 180, 180), 12));
    btnStepBack->setToolTip("Previous Frame (Left Arrow)");

    btnPlayReverse = new QPushButton(controlBar);
    btnPlayReverse->setIcon(IconHelper::playReverse(QColor(212, 212, 216), 14));
    btnPlayReverse->setToolTip("Play Reverse (Shift + Spacebar)");

    btnPlayPause = new QPushButton(controlBar);
    btnPlayPause->setIcon(IconHelper::play(Qt::white, 14));
    btnPlayPause->setToolTip("Play Forward (Spacebar)");

    btnStepForward = new QPushButton(controlBar);
    btnStepForward->setIcon(IconHelper::forward(QColor(180, 180, 180), 12));
    btnStepForward->setToolTip("Next Frame (Right Arrow)");

    btnJumpEnd = new QPushButton(controlBar);
    btnJumpEnd->setIcon(IconHelper::forward(QColor(212, 212, 216), 14));
    btnJumpEnd->setToolTip("Jump to End (Shift + Right)");

    controlLayout->addWidget(btnJumpStart);
    controlLayout->addWidget(btnStepBack);
    controlLayout->addWidget(btnPlayReverse);
    controlLayout->addWidget(btnPlayPause);
    controlLayout->addWidget(btnStepForward);
    controlLayout->addWidget(btnJumpEnd);

    controlLayout->addSpacing(8);

    btnRecord = new QPushButton(controlBar);
    btnRecord->setIcon(IconHelper::radar(QColor(212, 212, 216), 14));
    btnRecord->setObjectName("btnRecord");
    btnRecord->setCheckable(true);
    btnRecord->setToolTip("Automatic Simulation Keyframe Recording");
    controlLayout->addWidget(btnRecord);

    controlLayout->addSpacing(8);

    // Frame inputs
    auto* lblFrame = new QLabel("Frame:", controlBar);
    spinCurrentFrame = new QSpinBox(controlBar);
    spinCurrentFrame->setRange(0, 1000);
    spinCurrentFrame->setValue(0);
    spinCurrentFrame->setButtonSymbols(QAbstractSpinBox::NoButtons);

    controlLayout->addWidget(lblFrame);
    controlLayout->addWidget(spinCurrentFrame);

    controlLayout->addSpacing(6);

    auto* lblStart = new QLabel("Start:", controlBar);
    spinStartFrame = new QSpinBox(controlBar);
    spinStartFrame->setRange(0, 1000);
    spinStartFrame->setValue(0);
    spinStartFrame->setButtonSymbols(QAbstractSpinBox::NoButtons);

    auto* lblEnd = new QLabel("End:", controlBar);
    spinEndFrame = new QSpinBox(controlBar);
    spinEndFrame->setRange(1, 1000);
    spinEndFrame->setValue(72);
    spinEndFrame->setButtonSymbols(QAbstractSpinBox::NoButtons);

    controlLayout->addWidget(lblStart);
    controlLayout->addWidget(spinStartFrame);
    controlLayout->addWidget(lblEnd);
    controlLayout->addWidget(spinEndFrame);

    controlLayout->addStretch();

    // Timecode display
    lblTimeCode = new QLabel("00:00:00.00 (0h)", controlBar);
    lblTimeCode->setStyleSheet("color: #8AB4F8; font-family: Consolas, monospace; font-weight: bold; font-size: 11px;");
    controlLayout->addWidget(lblTimeCode);

    controlLayout->addSpacing(6);

    // Frame rate button
    btnFps = new QPushButton("24 fps", controlBar);
    btnFps->setToolTip("Click to cycle frame rate (24, 30, 60 fps)");
    btnFps->setStyleSheet("min-width: 46px; max-width: 52px;");
    controlLayout->addWidget(btnFps);

    // Loop Toggle
    btnLoop = new QPushButton("Loop", controlBar);
    btnLoop->setCheckable(true);
    btnLoop->setChecked(true);
    btnLoop->setToolTip("Toggle Playback Loop");
    controlLayout->addWidget(btnLoop);

    // 2. Frame Ruler Canvas (Expands freely)
    rulerCanvas = new FrameRulerCanvas(this);
    rulerCanvas->setFrameRange(0, 72);
    rulerCanvas->setCurrentFrame(0);

    mainLayout->addWidget(controlBar);
    mainLayout->addWidget(rulerCanvas, 1);

    // Wire up events
    connect(btnJumpStart, &QPushButton::clicked, this, &TimelineWidget::jumpToStart);
    connect(btnStepBack, &QPushButton::clicked, this, &TimelineWidget::stepBackward);
    connect(btnPlayReverse, &QPushButton::clicked, this, [this]() {
        if (isPlayingReverse) {
            stopPlayback();
        } else {
            playReverse();
        }
    });
    connect(btnPlayPause, &QPushButton::clicked, this, &TimelineWidget::togglePlayPause);
    connect(btnStepForward, &QPushButton::clicked, this, &TimelineWidget::stepForward);
    connect(btnJumpEnd, &QPushButton::clicked, this, &TimelineWidget::jumpToEnd);
    connect(btnFps, &QPushButton::clicked, this, &TimelineWidget::cycleFps);

    connect(rulerCanvas, &FrameRulerCanvas::frameChanged, this, &TimelineWidget::onRulerFrameChanged);
    connect(spinCurrentFrame, QOverload<int>::of(&QSpinBox::valueChanged), this, &TimelineWidget::onSpinCurrentChanged);
    connect(spinStartFrame, QOverload<int>::of(&QSpinBox::valueChanged), this, &TimelineWidget::onRangeChanged);
    connect(spinEndFrame, QOverload<int>::of(&QSpinBox::valueChanged), this, &TimelineWidget::onRangeChanged);

    updateTimeCodeDisplay();
}

void TimelineWidget::setCurrentFrame(int frame) {
    rulerCanvas->setCurrentFrame(frame);
}

void TimelineWidget::setFrameRange(int start, int end) {
    spinStartFrame->setValue(start);
    spinEndFrame->setValue(end);
    rulerCanvas->setFrameRange(start, end);
}

void TimelineWidget::playForward() {
    isPlayingForward = true;
    isPlayingReverse = false;
    btnPlayPause->setIcon(IconHelper::pause(Qt::white, 14));
    btnPlayPause->setToolTip("Pause (Spacebar)");
    btnPlayPause->setStyleSheet("background-color: #4772B3; color: white;");
    btnPlayReverse->setStyleSheet("");

    int intervalMs = std::max(10, 1000 / fps);
    playTimer->start(intervalMs);
    emit playbackStateChanged(true);
}

void TimelineWidget::playReverse() {
    isPlayingReverse = true;
    isPlayingForward = false;
    btnPlayPause->setIcon(IconHelper::pause(Qt::white, 14));
    btnPlayPause->setToolTip("Pause (Spacebar)");
    btnPlayReverse->setStyleSheet("background-color: #4772B3; color: white;");
    btnPlayPause->setStyleSheet("");

    int intervalMs = std::max(10, 1000 / fps);
    playTimer->start(intervalMs);
    emit playbackStateChanged(true);
}

void TimelineWidget::stopPlayback() {
    isPlayingForward = false;
    isPlayingReverse = false;
    btnPlayPause->setIcon(IconHelper::play(Qt::white, 14));
    btnPlayPause->setToolTip("Play Forward (Spacebar)");
    btnPlayPause->setStyleSheet("");
    btnPlayReverse->setStyleSheet("");
    playTimer->stop();
    emit playbackStateChanged(false);
}

void TimelineWidget::togglePlayPause() {
    if (getIsPlaying()) {
        stopPlayback();
    } else {
        if (getCurrentFrame() >= getEndFrame()) {
            jumpToStart();
        }
        playForward();
    }
}

void TimelineWidget::jumpToStart() {
    setCurrentFrame(getStartFrame());
}

void TimelineWidget::jumpToEnd() {
    setCurrentFrame(getEndFrame());
}

void TimelineWidget::stepForward() {
    int cur = getCurrentFrame();
    if (cur < getEndFrame()) {
        setCurrentFrame(cur + 1);
    } else if (btnLoop->isChecked()) {
        setCurrentFrame(getStartFrame());
    }
}

void TimelineWidget::stepBackward() {
    int cur = getCurrentFrame();
    if (cur > getStartFrame()) {
        setCurrentFrame(cur - 1);
    } else if (btnLoop->isChecked()) {
        setCurrentFrame(getEndFrame());
    }
}

void TimelineWidget::cycleFps() {
    if (fps == 24) fps = 30;
    else if (fps == 30) fps = 60;
    else if (fps == 60) fps = 12;
    else fps = 24;

    btnFps->setText(QString("%1 fps").arg(fps));
    if (getIsPlaying()) {
        playTimer->setInterval(1000 / fps);
    }
}

void TimelineWidget::onRulerFrameChanged(int frame) {
    spinCurrentFrame->blockSignals(true);
    spinCurrentFrame->setValue(frame);
    spinCurrentFrame->blockSignals(false);

    updateTimeCodeDisplay();
    emit frameChanged(frame, formatTimeCode(frame));
}

void TimelineWidget::onSpinCurrentChanged(int val) {
    rulerCanvas->setCurrentFrame(val);
}

void TimelineWidget::onRangeChanged() {
    int s = spinStartFrame->value();
    int e = std::max(s + 1, spinEndFrame->value());
    rulerCanvas->setFrameRange(s, e);
}

void TimelineWidget::onTimerTick() {
    int cur = getCurrentFrame();
    int start = getStartFrame();
    int end = getEndFrame();

    if (isPlayingForward) {
        if (cur < end) {
            setCurrentFrame(cur + 1);
        } else {
            if (btnLoop->isChecked()) {
                setCurrentFrame(start);
            } else {
                stopPlayback();
            }
        }
    } else if (isPlayingReverse) {
        if (cur > start) {
            setCurrentFrame(cur - 1);
        } else {
            if (btnLoop->isChecked()) {
                setCurrentFrame(end);
            } else {
                stopPlayback();
            }
        }
    }
}

QString TimelineWidget::formatTimeCode(int frame) const {
    int hours = frame / 60;
    int minutes = frame % 60;

    return QString("T + %1:%2:00 (%3 min)")
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(frame);
}

void TimelineWidget::updateTimeCodeDisplay() {
    lblTimeCode->setText(formatTimeCode(getCurrentFrame()));
}

} // namespace MapUI
