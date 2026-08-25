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
#include "../core/WeatherForecastManager.h"

namespace MapUI {

enum class TimelineMode {
    DamSimulation,
    WeatherForecast
};

class FrameRulerCanvas : public QWidget {
    Q_OBJECT

private:
    TimelineMode timelineMode = TimelineMode::DamSimulation;
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
    TimelineMode getMode() const { return timelineMode; }

    void setMode(TimelineMode mode);
    void setCurrentFrame(int frame);
    void setFrameRange(int start, int end);
    void setBlocked(bool blocked);
    void setSimulationTrack(const QString& name, int start, int end);
    void setWeatherTrack(const QString& name, int start, int end);

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
    TimelineMode timelineMode = TimelineMode::DamSimulation;

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

    QLabel* lblFrame;
    QLabel* lblStart;
    QLabel* lblEnd;
    QSpinBox* spinCurrentFrame;
    QSpinBox* spinStartFrame;
    QSpinBox* spinEndFrame;

    QPushButton* btnFps;
    QPushButton* btnLoop;
    QLabel* lblTimeCode;

    // Frame Ruler Scrubber
    FrameRulerCanvas* rulerCanvas;

    // Simulation / Forecast playback
    QTimer* playTimer;
    bool isPlayingForward = false;
    bool isPlayingReverse = false;
    bool isLooping = true;
    bool isDamLoaded = false;
    bool isWeatherLoaded = false;
    int fps = 24;

    MapCore::WeatherForecastData currentWeatherForecast;

public:
    explicit TimelineWidget(QWidget* parent = nullptr);
    explicit TimelineWidget(TimelineMode mode, QWidget* parent = nullptr);

    TimelineMode getMode() const { return timelineMode; }
    void setMode(TimelineMode mode);

    int getCurrentFrame() const { return rulerCanvas->getCurrentFrame(); }
    int getStartFrame() const { return rulerCanvas->getStartFrame(); }
    int getEndFrame() const { return rulerCanvas->getEndFrame(); }
    bool getIsPlaying() const { return isPlayingForward || isPlayingReverse; }
    bool getIsDamLoaded() const { return isDamLoaded; }
    bool getIsWeatherLoaded() const { return isWeatherLoaded; }

    void setCurrentFrame(int frame);
    void setFrameRange(int start, int end);
    void setDamSelected(bool selected, const QString& trackName = "flood_sim");
    void setWeatherForecast(const MapCore::WeatherForecastData& forecast);

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
    void applyModeConfig();
    void updateTimeCodeDisplay();
    void updateControlsEnabled();
    QString formatTimeCode(int frame) const;
};

} // namespace MapUI
