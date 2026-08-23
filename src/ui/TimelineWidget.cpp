#include "TimelineWidget.h"
#include "IconHelper.h"
#include <QPainter>
#include <QPolygonF>
#include <QLinearGradient>
#include <QStyle>
#include <cmath>
#include <algorithm>

namespace MapUI {

// ==========================================
// FrameRulerCanvas Implementation
// ==========================================

FrameRulerCanvas::FrameRulerCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(76);
    setMouseTracking(true);
    setCursor(Qt::SplitHCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void FrameRulerCanvas::setCurrentFrame(int frame) {
    if (isBlocked) return;
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

void FrameRulerCanvas::setBlocked(bool blocked) {
    isBlocked = blocked;
    update();
}

void FrameRulerCanvas::setSimulationTrack(const QString& name, int start, int end) {
    trackName = name.isEmpty() ? "flood_sim" : name;
    startFrame = start;
    endFrame = std::max(start + 1, end);
    currentFrame = std::clamp(currentFrame, startFrame, endFrame);
    isBlocked = false;
    update();
}

float FrameRulerCanvas::frameToX(int frame) const {
    float margin = 135.0f; // space for channel labels on left
    float availWidth = width() - margin - 20.0f;
    float range = static_cast<float>(endFrame - startFrame);
    if (range <= 0.0f) range = 1.0f;
    return margin + (static_cast<float>(frame - startFrame) / range) * availWidth + panOffset;
}

int FrameRulerCanvas::xToFrame(float x) const {
    float margin = 135.0f;
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

    // 1. Blocked State (When no dam is selected)
    if (isBlocked) {
        painter.fillRect(rect(), QColor(24, 24, 27)); // shadcn zinc-900

        // Left Channel Locked Header
        painter.fillRect(QRect(0, 0, 135, h), QColor(18, 18, 20));
        painter.setPen(QPen(QColor(39, 39, 42), 1.0));
        painter.drawLine(135, 0, 135, h);
        painter.drawLine(0, rulerH, w, rulerH);

        painter.setFont(QFont("Segoe UI", 8, QFont::DemiBold));
        painter.setPen(QColor(113, 113, 122));
        painter.drawText(QRect(10, 0, 120, rulerH), Qt::AlignVCenter | Qt::AlignLeft, "Channels / Tracks");
        painter.drawText(QRect(10, rulerH + 4, 120, 24), Qt::AlignVCenter | Qt::AlignLeft, "[Locked]");

        // Centered Lock Banner
        QString blockedMsg = "Select a dam on the map or search to load flood_sim timeline track";
        QFont lockFont("Segoe UI", 9, QFont::Medium);
        painter.setFont(lockFont);
        QFontMetrics fm(lockFont);
        int tw = fm.horizontalAdvance(blockedMsg) + 32;
        int th = 30;
        int bx = 135 + (w - 135 - tw) / 2;
        int by = (h - th) / 2;

        QRect lockRect(bx, by, tw, th);
        painter.setBrush(QColor(39, 39, 42, 220));
        painter.setPen(QPen(QColor(63, 63, 70), 1.0));
        painter.drawRoundedRect(lockRect, 6.0, 6.0);

        painter.setPen(QColor(212, 212, 216));
        painter.drawText(lockRect, Qt::AlignCenter, blockedMsg);
        return;
    }

    // 2. Unlocked Active Timeline Background
    painter.fillRect(rect(), QColor(24, 24, 27));

    float xStart = frameToX(startFrame);
    float xEnd = frameToX(endFrame);
    QRectF activeRangeRect(xStart, 0, xEnd - xStart, h);
    painter.fillRect(activeRangeRect, QColor(30, 30, 34));

    // 3. Render Continuous Tracks (flood_sim simulation clips instead of keyframes)
    struct SimTrack {
        QString channelName;
        QString clipLabel;
        QColor startColor;
        QColor endColor;
        QColor borderColor;
        QColor textColor;
    };

    std::vector<SimTrack> tracks = {
        {
            trackName.isEmpty() ? "flood_sim" : trackName,
            QString("%1 · 0 to %2 min Inundation Wave Propagation").arg(trackName).arg(endFrame),
            QColor(2, 132, 199),   // Sky 600
            QColor(3, 105, 161),   // Sky 700
            QColor(56, 189, 248),  // Sky 400
            QColor(240, 249, 255)
        },
        {
            "Discharge Q",
            "Breach Outflow Hydrograph · Continuous Hydraulic Depletion",
            QColor(79, 70, 229),   // Indigo 600
            QColor(67, 56, 202),   // Indigo 700
            QColor(129, 140, 248), // Indigo 400
            QColor(238, 242, 255)
        },
        {
            "Basin Storage",
            "Sequential Depressions 1 - 4 · Topographic Saddle Weir Cascades",
            QColor(5, 150, 105),   // Emerald 600
            QColor(4, 120, 87),    // Emerald 700
            QColor(52, 211, 153),  // Emerald 400
            QColor(236, 253, 245)
        }
    };

    int trackH = 22;
    int curY = rulerH + 2;

    for (size_t i = 0; i < tracks.size() && (curY + trackH) <= h; ++i) {
        const auto& tr = tracks[i];

        // Row background
        QColor rowBg = (i % 2 == 0) ? QColor(28, 28, 31) : QColor(24, 24, 27);
        painter.fillRect(QRect(0, curY, w, trackH), rowBg);

        // Row horizontal separator
        painter.setPen(QPen(QColor(39, 39, 42), 1.0));
        painter.drawLine(0, curY, w, curY);

        // Track channel header (Left column)
        painter.fillRect(QRect(0, curY, 135, trackH), QColor(18, 18, 20));
        painter.setPen(QPen(QColor(39, 39, 42), 1.0));
        painter.drawLine(135, curY, 135, curY + trackH);

        painter.setFont(QFont("Segoe UI", 8, QFont::DemiBold));
        painter.setPen(tr.borderColor);
        painter.drawText(QRect(10, curY, 120, trackH), Qt::AlignVCenter | Qt::AlignLeft, tr.channelName);

        // Render Continuous Simulation Track Bar (Instead of keyframes)
        float clipX = xStart;
        float clipW = std::max(10.0f, xEnd - xStart);
        QRectF clipRect(clipX, curY + 2, clipW, trackH - 4);

        // Track gradient
        QLinearGradient trackGrad(clipRect.topLeft(), clipRect.bottomLeft());
        trackGrad.setColorAt(0.0, tr.startColor);
        trackGrad.setColorAt(1.0, tr.endColor);

        painter.setBrush(trackGrad);
        painter.setPen(QPen(tr.borderColor, 1.0));
        painter.drawRoundedRect(clipRect, 4.0, 4.0);

        // Simulation Progress Fill inside Track Bar
        if (currentFrame > startFrame) {
            float progW = frameToX(currentFrame) - xStart;
            QRectF progRect(clipX, curY + 2, progW, trackH - 4);
            painter.setBrush(QColor(255, 255, 255, 45));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(progRect, 4.0, 4.0);
        }

        // Clip text label inside Track Bar
        painter.setFont(QFont("Segoe UI", 7, QFont::Medium));
        painter.setPen(tr.textColor);
        painter.drawText(clipRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, tr.clipLabel);

        curY += trackH;
    }

    // 4. Ruler Top Bar Background & Separator
    painter.fillRect(QRect(0, 0, w, rulerH), QColor(18, 18, 20));
    painter.fillRect(QRect(0, 0, 135, rulerH), QColor(15, 15, 17));
    painter.setPen(QPen(QColor(39, 39, 42), 1.0));
    painter.drawLine(0, rulerH, w, rulerH);
    painter.drawLine(135, 0, 135, h);

    painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
    painter.setPen(QColor(113, 113, 122));
    painter.drawText(QRect(10, 0, 120, rulerH), Qt::AlignVCenter | Qt::AlignLeft, "Channels / Tracks");

    // 5. Frame Grid & Ruler Tick Marks (0 - 60 Minutes)
    int frameStep = 10;
    int totalRange = endFrame - startFrame;
    if (totalRange > 120) frameStep = 20;
    else if (totalRange <= 30) frameStep = 5;

    for (int f = startFrame; f <= endFrame; ++f) {
        float x = frameToX(f);
        if (x < 135 || x > w) continue;

        if (f % frameStep == 0) {
            // Major tick mark
            painter.setPen(QPen(QColor(63, 63, 70), 1.0));
            painter.drawLine(QPointF(x, 0), QPointF(x, h));

            // Minute label in ruler bar
            painter.setPen(QColor(212, 212, 216));
            painter.drawText(QRectF(x - 18, 2, 36, 14), Qt::AlignCenter, QString("%1m").arg(f));
        } else if (f % (frameStep / 2 == 0 ? 1 : frameStep / 2) == 0) {
            // Medium tick mark
            painter.setPen(QPen(QColor(39, 39, 42), 1.0));
            painter.drawLine(QPointF(x, rulerH - 8), QPointF(x, h));
        } else {
            // Minor tick mark
            painter.setPen(QPen(QColor(32, 32, 35), 1.0));
            painter.drawLine(QPointF(x, rulerH - 4), QPointF(x, rulerH));
        }
    }

    // 6. Playhead Indicator (Glowing Cyan line + Top Flag Badge)
    float curX = frameToX(currentFrame);
    if (curX >= 135) {
        // Vertical line across full ruler and tracks
        painter.setPen(QPen(QColor(0, 229, 255), 2.0));
        painter.drawLine(QPointF(curX, 0), QPointF(curX, h));

        // Top flag badge (shadcn zinc-900 style)
        QPolygonF playheadFlag;
        playheadFlag << QPointF(curX - 14, 0)
                     << QPointF(curX + 14, 0)
                     << QPointF(curX + 14, 13)
                     << QPointF(curX, 19)
                     << QPointF(curX - 14, 13);

        painter.setBrush(QColor(24, 24, 27));
        painter.setPen(QPen(QColor(0, 229, 255), 1.2));
        painter.drawPolygon(playheadFlag);

        // Frame / minute text inside flag
        painter.setFont(QFont("Segoe UI", 7, QFont::Bold));
        painter.setPen(QColor(0, 229, 255));
        painter.drawText(QRectF(curX - 14, 1, 28, 12), Qt::AlignCenter, QString("T+%1m").arg(currentFrame));
    }
}

void FrameRulerCanvas::mousePressEvent(QMouseEvent* event) {
    if (isBlocked) return;
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        int newFrame = xToFrame(static_cast<float>(event->position().x()));
        setCurrentFrame(newFrame);
    }
}

void FrameRulerCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (isBlocked) return;
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
    if (isBlocked) return;
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

    // Initial state: blocked until dam is selected
    setDamSelected(false);
}

void TimelineWidget::setupUi() {
    setMinimumHeight(64);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setStyleSheet(R"(
        QWidget#timelineWidget {
            background-color: #18181B;
            border-top: 1px solid #27272A;
        }
        QWidget#controlBar {
            background-color: #18181B;
            border-bottom: 1px solid #27272A;
            min-height: 28px;
            max-height: 28px;
        }
        QPushButton {
            background-color: #27272A;
            color: #F4F4F5;
            border: 1px solid #3F3F46;
            border-radius: 4px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            font-weight: 500;
            min-width: 24px;
            max-width: 30px;
            min-height: 20px;
            max-height: 20px;
            padding: 1px 4px;
        }
        QPushButton:hover {
            background-color: #3F3F46;
            color: #FFFFFF;
            border-color: #52525B;
        }
        QPushButton:pressed {
            background-color: #09090B;
        }
        QPushButton:disabled {
            background-color: #18181B;
            color: #52525B;
            border-color: #27272A;
        }
        QPushButton#btnPlayActive {
            background-color: #0284C7;
            color: #FFFFFF;
            border-color: #38BDF8;
        }
        QPushButton#btnRecord:checked {
            background-color: #E87D0D;
            color: #FFFFFF;
            border-color: #FFA544;
        }
        QSpinBox {
            background-color: #18181B;
            color: #F4F4F5;
            border: 1px solid #3F3F46;
            border-radius: 4px;
            font-family: Consolas, 'Segoe UI', monospace;
            font-size: 11px;
            font-weight: bold;
            padding: 1px 4px;
            min-height: 20px;
            max-height: 20px;
            min-width: 48px;
        }
        QSpinBox:hover {
            border-color: #0284C7;
        }
        QSpinBox:focus {
            border-color: #38BDF8;
            background-color: #09090B;
        }
        QSpinBox:disabled {
            background-color: #18181B;
            color: #52525B;
            border-color: #27272A;
        }
        QLabel {
            color: #A1A1AA;
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

    lblHeaderTitle = new QLabel("Timeline", controlBar);
    lblHeaderTitle->setStyleSheet("color: #F4F4F5; font-weight: bold; font-size: 11px; margin-right: 2px;");
    controlLayout->addWidget(lblHeaderTitle);

    lblTrackBadge = new QLabel("flood_sim", controlBar);
    lblTrackBadge->setStyleSheet("color: #38BDF8; background-color: #0C4A6E; border: 1px solid #0284C7; border-radius: 4px; padding: 1px 6px; font-weight: bold; font-size: 10px;");
    controlLayout->addWidget(lblTrackBadge);

    controlLayout->addSpacing(6);

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
    btnPlayPause->setToolTip("Play / Pause (Spacebar)");

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

    controlLayout->addSpacing(6);

    btnRecord = new QPushButton(controlBar);
    btnRecord->setIcon(IconHelper::radar(QColor(212, 212, 216), 14));
    btnRecord->setObjectName("btnRecord");
    btnRecord->setCheckable(true);
    btnRecord->setToolTip("Automatic Simulation Recording");
    controlLayout->addWidget(btnRecord);

    controlLayout->addSpacing(6);

    // Frame inputs
    auto* lblFrame = new QLabel("Minute:", controlBar);
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
    spinEndFrame->setValue(60);
    spinEndFrame->setButtonSymbols(QAbstractSpinBox::NoButtons);

    controlLayout->addWidget(lblStart);
    controlLayout->addWidget(spinStartFrame);
    controlLayout->addWidget(lblEnd);
    controlLayout->addWidget(spinEndFrame);

    controlLayout->addStretch();

    // Timecode display
    lblTimeCode = new QLabel("00:00:00 (T+0m)", controlBar);
    lblTimeCode->setStyleSheet("color: #38BDF8; font-family: Consolas, monospace; font-weight: bold; font-size: 11px;");
    controlLayout->addWidget(lblTimeCode);

    controlLayout->addSpacing(6);

    // Frame rate button
    btnFps = new QPushButton("24 fps", controlBar);
    btnFps->setToolTip("Click to cycle frame rate (24, 30, 60 fps)");
    btnFps->setStyleSheet("min-width: 48px; max-width: 54px;");
    controlLayout->addWidget(btnFps);

    // Loop Toggle
    btnLoop = new QPushButton("Loop", controlBar);
    btnLoop->setCheckable(true);
    btnLoop->setChecked(true);
    btnLoop->setToolTip("Toggle Playback Loop");
    btnLoop->setStyleSheet("min-width: 40px; max-width: 46px;");
    controlLayout->addWidget(btnLoop);

    // 2. Frame Ruler Canvas (Expands freely)
    rulerCanvas = new FrameRulerCanvas(this);
    rulerCanvas->setFrameRange(0, 60);
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
}

void TimelineWidget::setDamSelected(bool selected, const QString& trackName) {
    isDamLoaded = selected;
    rulerCanvas->setBlocked(!selected);

    if (selected) {
        QString tName = trackName.isEmpty() ? "flood_sim" : trackName;
        rulerCanvas->setSimulationTrack(tName, 0, 60);
        lblHeaderTitle->setText("Timeline");
        lblTrackBadge->setText(tName);
        lblTrackBadge->setStyleSheet("color: #38BDF8; background-color: #0C4A6E; border: 1px solid #0284C7; border-radius: 4px; padding: 1px 6px; font-weight: bold; font-size: 10px;");
        lblTrackBadge->show();
    } else {
        stopPlayback();
        lblHeaderTitle->setText("Timeline");
        lblTrackBadge->setText("No Dam Selected");
        lblTrackBadge->setStyleSheet("color: #71717A; background-color: #27272A; border: 1px solid #3F3F46; border-radius: 4px; padding: 1px 6px; font-weight: 500; font-size: 10px;");
    }

    updateControlsEnabled();
}

void TimelineWidget::updateControlsEnabled() {
    bool en = isDamLoaded;
    btnJumpStart->setEnabled(en);
    btnStepBack->setEnabled(en);
    btnPlayReverse->setEnabled(en);
    btnPlayPause->setEnabled(en);
    btnStepForward->setEnabled(en);
    btnJumpEnd->setEnabled(en);
    btnRecord->setEnabled(en);
    spinCurrentFrame->setEnabled(en);
    spinStartFrame->setEnabled(en);
    spinEndFrame->setEnabled(en);
    btnFps->setEnabled(en);
    btnLoop->setEnabled(en);
}

void TimelineWidget::setCurrentFrame(int frame) {
    if (!isDamLoaded) return;
    spinCurrentFrame->blockSignals(true);
    spinCurrentFrame->setValue(frame);
    spinCurrentFrame->blockSignals(false);

    rulerCanvas->setCurrentFrame(frame);
    updateTimeCodeDisplay();
    emit frameChanged(frame, formatTimeCode(frame));
}

void TimelineWidget::setFrameRange(int start, int end) {
    spinStartFrame->blockSignals(true);
    spinEndFrame->blockSignals(true);
    spinStartFrame->setValue(start);
    spinEndFrame->setValue(end);
    spinStartFrame->blockSignals(false);
    spinEndFrame->blockSignals(false);

    rulerCanvas->setFrameRange(start, end);
}

void TimelineWidget::playForward() {
    if (!isDamLoaded) return;
    isPlayingForward = true;
    isPlayingReverse = false;
    btnPlayPause->setIcon(IconHelper::pause(Qt::white, 14));
    btnPlayPause->setObjectName("btnPlayActive");
    btnPlayPause->style()->unpolish(btnPlayPause);
    btnPlayPause->style()->polish(btnPlayPause);

    playTimer->start(1000 / fps);
    emit playbackStateChanged(true);
}

void TimelineWidget::playReverse() {
    if (!isDamLoaded) return;
    isPlayingReverse = true;
    isPlayingForward = false;
    btnPlayPause->setIcon(IconHelper::pause(Qt::white, 14));
    btnPlayReverse->setObjectName("btnPlayActive");
    btnPlayReverse->style()->unpolish(btnPlayReverse);
    btnPlayReverse->style()->polish(btnPlayReverse);

    playTimer->start(1000 / fps);
    emit playbackStateChanged(true);
}

void TimelineWidget::stopPlayback() {
    isPlayingForward = false;
    isPlayingReverse = false;
    playTimer->stop();

    btnPlayPause->setIcon(IconHelper::play(Qt::white, 14));
    btnPlayPause->setObjectName("");
    btnPlayPause->style()->unpolish(btnPlayPause);
    btnPlayPause->style()->polish(btnPlayPause);

    btnPlayReverse->setObjectName("");
    btnPlayReverse->style()->unpolish(btnPlayReverse);
    btnPlayReverse->style()->polish(btnPlayReverse);

    emit playbackStateChanged(false);
}

void TimelineWidget::togglePlayPause() {
    if (!isDamLoaded) return;
    if (isPlayingForward || isPlayingReverse) {
        stopPlayback();
    } else {
        playForward();
    }
}

void TimelineWidget::jumpToStart() {
    setCurrentFrame(rulerCanvas->getStartFrame());
}

void TimelineWidget::jumpToEnd() {
    setCurrentFrame(rulerCanvas->getEndFrame());
}

void TimelineWidget::stepForward() {
    int nextF = rulerCanvas->getCurrentFrame() + 1;
    if (nextF <= rulerCanvas->getEndFrame()) {
        setCurrentFrame(nextF);
    }
}

void TimelineWidget::stepBackward() {
    int prevF = rulerCanvas->getCurrentFrame() - 1;
    if (prevF >= rulerCanvas->getStartFrame()) {
        setCurrentFrame(prevF);
    }
}

void TimelineWidget::cycleFps() {
    if (fps == 24) fps = 30;
    else if (fps == 30) fps = 60;
    else fps = 24;

    btnFps->setText(QString("%1 fps").arg(fps));
    if (isPlayingForward || isPlayingReverse) {
        playTimer->setInterval(1000 / fps);
    }
}

void TimelineWidget::onTimerTick() {
    int cur = rulerCanvas->getCurrentFrame();
    int start = rulerCanvas->getStartFrame();
    int end = rulerCanvas->getEndFrame();

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

void TimelineWidget::onRulerFrameChanged(int frame) {
    spinCurrentFrame->blockSignals(true);
    spinCurrentFrame->setValue(frame);
    spinCurrentFrame->blockSignals(false);

    updateTimeCodeDisplay();
    emit frameChanged(frame, formatTimeCode(frame));
}

void TimelineWidget::onSpinCurrentChanged(int val) {
    setCurrentFrame(val);
}

void TimelineWidget::onRangeChanged() {
    setFrameRange(spinStartFrame->value(), spinEndFrame->value());
}

void TimelineWidget::updateTimeCodeDisplay() {
    int f = rulerCanvas->getCurrentFrame();
    lblTimeCode->setText(formatTimeCode(f));
}

QString TimelineWidget::formatTimeCode(int frame) const {
    int totalSec = frame * 60; // Each frame is 1 minute
    int hrs = totalSec / 3600;
    int mins = (totalSec % 3600) / 60;
    int secs = totalSec % 60;

    return QString("%1:%2:%3 (T+%4m)")
        .arg(hrs, 2, 10, QChar('0'))
        .arg(mins, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'))
        .arg(frame);
}

} // namespace MapUI
