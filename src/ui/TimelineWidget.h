#pragma once

#include <QWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPaintEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <vector>

namespace MapUI {

class FrameRulerCanvas : public QWidget {
    Q_OBJECT

private:
    int startFrame = 0;
    int endFrame = 60;
    int currentFrame = 0;
    bool isBlocked = true;
    QString trackName = "flood_sim";

    bool isDragging = false;
    float panOffset = 0.0f;

public:
    explicit FrameRulerCanvas(QWidget* parent = nullptr);

    int getCurrentFrame() const { return currentFrame; }
    int getStartFrame() const { return startFrame; }
    int getEndFrame() const { return endFrame; }
    bool getIsBlocked() const { return isBlocked; }

    void setCurrentFrame(int frame);
    void setFrameRange(int start, int end);
    void setBlocked(bool blocked);
    void setSimulationTrack(const QString& name, int start, int end);

signals:
    void frameChanged(int frame);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    float frameToX(int frame) const;
    int xToFrame(float x) const;
};

class TimelineWidget : public QWidget {
    Q_OBJECT

private:
    // Top Control Toolbar
    QWidget* controlBar;
    QLabel* lblHeaderTitle;
    QLabel* lblTrackBadge;
    QPushButton* btnJumpStart;
    QPushButton* btnStepBack;
    QPushButton* btnPlayReverse;
    QPushButton* btnPlayPause;
    QPushButton* btnStepForward;
    QPushButton* btnJumpEnd;
    QPushButton* btnRecord;

    QSpinBox* spinCurrentFrame;
    QSpinBox* spinStartFrame;
    QSpinBox* spinEndFrame;

    QPushButton* btnFps;
    QPushButton* btnLoop;
    QLabel* lblTimeCode;

    // Frame Ruler Scrubber
    FrameRulerCanvas* rulerCanvas;

    // Simulation playback
    QTimer* playTimer;
    bool isPlayingForward = false;
    bool isPlayingReverse = false;
    bool isLooping = true;
    bool isDamLoaded = false;
    int fps = 24;

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    int getCurrentFrame() const { return rulerCanvas->getCurrentFrame(); }
    int getStartFrame() const { return rulerCanvas->getStartFrame(); }
    int getEndFrame() const { return rulerCanvas->getEndFrame(); }
    bool getIsPlaying() const { return isPlayingForward || isPlayingReverse; }
    bool getIsDamLoaded() const { return isDamLoaded; }

    void setCurrentFrame(int frame);
    void setFrameRange(int start, int end);
    void setDamSelected(bool selected, const QString& trackName = "flood_sim");

public slots:
    void playForward();
    void playReverse();
    void stopPlayback();
    void togglePlayPause();
    void jumpToStart();
    void jumpToEnd();
    void stepForward();
    void stepBackward();
    void cycleFps();

signals:
    void frameChanged(int frame, const QString& timeCode);
    void playbackStateChanged(bool isPlaying);

private slots:
    void onTimerTick();
    void onRulerFrameChanged(int frame);
    void onSpinCurrentChanged(int val);
    void onRangeChanged();

private:
    void setupUi();
    void updateTimeCodeDisplay();
    void updateControlsEnabled();
    QString formatTimeCode(int frame) const;
};

} // namespace MapUI
