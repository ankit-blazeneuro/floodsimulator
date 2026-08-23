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
    int endFrame = 72;
    int currentFrame = 0;
    std::vector<int> keyframes = {0, 6, 12, 18, 24, 36, 48, 60, 72};

    bool isDragging = false;
    float panOffset = 0.0f;

public:
    explicit FrameRulerCanvas(QWidget* parent = nullptr);

    int getCurrentFrame() const { return currentFrame; }
    int getStartFrame() const { return startFrame; }
    int getEndFrame() const { return endFrame; }

    void setCurrentFrame(int frame);
    void setFrameRange(int start, int end);
    void setKeyframes(const std::vector<int>& keys);

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
    QPushButton* btnJumpStart;
    QPushButton* btnStepBack;
    QPushButton* btnPlayReverse;
    QPushButton* btnStop;
    QPushButton* btnPlayForward;
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
    int fps = 24;

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    int getCurrentFrame() const { return rulerCanvas->getCurrentFrame(); }
    int getStartFrame() const { return rulerCanvas->getStartFrame(); }
    int getEndFrame() const { return rulerCanvas->getEndFrame(); }
    bool getIsPlaying() const { return isPlayingForward || isPlayingReverse; }

    void setCurrentFrame(int frame);
    void setFrameRange(int start, int end);

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
    QString formatTimeCode(int frame) const;
};

} // namespace MapUI
