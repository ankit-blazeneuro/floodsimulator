#include "HydroFlowWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QPainterPath>
#include <cmath>
#include <algorithm>

namespace MapUI {

// ─── MiniHydrographWidget Implementation ──────────────────────────────────────
MiniHydrographWidget::MiniHydrographWidget(const std::vector<DataPoint>& data, QColor lineColor, QWidget* parent)
    : QWidget(parent), m_data(data), m_lineColor(lineColor) {
    setFixedHeight(120);
    setMinimumWidth(300);
}

void MiniHydrographWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();
    int padL = 50, padR = 10, padT = 10, padB = 22;
    int plotW = w - padL - padR;
    int plotH = h - padT - padB;

    if (m_data.empty() || plotW <= 0 || plotH <= 0) return;

    double maxQ = 0, maxT = 0;
    for (auto& d : m_data) {
        if (d.discharge > maxQ) maxQ = d.discharge;
        if (d.minute > maxT) maxT = d.minute;
    }
    if (maxQ <= 0) maxQ = 1;
    if (maxT <= 0) maxT = 60;

    auto xPos = [&](double t) -> double { return padL + (t / maxT) * plotW; };
    auto yPos = [&](double q) -> double { return padT + plotH - (q / maxQ) * plotH; };

    // Grid lines
    p.setPen(QPen(QColor(51, 65, 85), 0.5, Qt::DashLine));
    for (int i = 0; i <= 4; i++) {
        double yy = padT + (plotH * i / 4.0);
        p.drawLine(QPointF(padL, yy), QPointF(w - padR, yy));
    }

    // Fill area
    QPolygonF fillPoly;
    fillPoly << QPointF(xPos(m_data.front().minute), padT + plotH);
    for (auto& d : m_data) fillPoly << QPointF(xPos(d.minute), yPos(d.discharge));
    fillPoly << QPointF(xPos(m_data.back().minute), padT + plotH);
    QColor fillCol = m_lineColor;
    fillCol.setAlpha(35);
    p.setBrush(fillCol);
    p.setPen(Qt::NoPen);
    p.drawPolygon(fillPoly);

    // Line
    QPainterPath path;
    for (size_t i = 0; i < m_data.size(); i++) {
        QPointF pt(xPos(m_data[i].minute), yPos(m_data[i].discharge));
        if (i == 0) path.moveTo(pt); else path.lineTo(pt);
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(m_lineColor, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(path);

    // Peak marker
    auto peak = std::max_element(m_data.begin(), m_data.end(),
        [](const DataPoint& a, const DataPoint& b) { return a.discharge < b.discharge; });
    if (peak != m_data.end()) {
        QPointF peakPt(xPos(peak->minute), yPos(peak->discharge));
        p.setBrush(m_lineColor);
        p.setPen(QPen(QColor(15, 23, 42), 2));
        p.drawEllipse(peakPt, 4, 4);

        p.setPen(QPen(m_lineColor));
        QFont f = p.font();
        f.setFamily("monospace");
        f.setPixelSize(9);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(peakPt.x() - 40, peakPt.y() - 16, 80, 14), Qt::AlignCenter,
            QString::number(peak->discharge, 'f', 0) + " m³/s");
    }

    // Y-axis labels
    p.setPen(QColor(148, 163, 184));
    QFont axFont;
    axFont.setFamily("monospace");
    axFont.setPixelSize(9);
    p.setFont(axFont);
    for (int i = 0; i <= 4; i++) {
        double val = maxQ * (4 - i) / 4.0;
        double yy = padT + (plotH * i / 4.0);
        p.drawText(QRectF(0, yy - 6, padL - 4, 12), Qt::AlignRight | Qt::AlignVCenter,
            QString::number(val, 'f', 0));
    }

    // X-axis labels
    for (int t = 0; t <= (int)maxT; t += 15) {
        double xx = xPos(t);
        p.drawText(QRectF(xx - 15, h - 18, 30, 14), Qt::AlignCenter, QString::number(t) + "m");
    }

    // Axis lines
    p.setPen(QPen(QColor(71, 85, 105), 1));
    p.drawLine(padL, padT, padL, padT + plotH);
    p.drawLine(padL, padT + plotH, w - padR, padT + plotH);
}

// ─── DamSimCard Implementation ────────────────────────────────────────────────
DamSimCard::DamSimCard(const MapCore::DamPoint& dam, QWidget* parent)
    : QWidget(parent), m_dam(dam) {
    buildUi();
}

void DamSimCard::buildUi() {
    setObjectName("damSimCard");

    // Compute simulation
    MapCore::FloodSimulationState sim = MapCore::DamFloodSimulator::compute60MinSimulation(m_dam);

    // Extract metrics
    double maxDepth = 0, maxVelocity = 0, maxArea = 0;
    for (auto& ts : sim.timeSlices) {
        if (ts.maxDepthM > maxDepth) maxDepth = ts.maxDepthM;
        if (ts.maxVelocityMs > maxVelocity) maxVelocity = ts.maxVelocityMs;
        if (ts.inundatedAreaKm2 > maxArea) maxArea = ts.inundatedAreaKm2;
    }

    double peakQ = sim.peakDischargeQ;
    double totalVol = sim.totalVolumeMCM;
    int dangerCount = static_cast<int>(sim.dangerZones.size());

    // Determine color theme based on risk
    QColor accentColor;
    QString riskLabel;
    if (peakQ > 10000) {
        accentColor = QColor(239, 68, 68);   // red
        riskLabel = "CRITICAL";
    } else if (peakQ > 3000) {
        accentColor = QColor(245, 158, 11);   // amber
        riskLabel = "HIGH";
    } else if (peakQ > 500) {
        accentColor = QColor(59, 130, 246);    // blue
        riskLabel = "MODERATE";
    } else {
        accentColor = QColor(16, 185, 129);    // green
        riskLabel = "LOW";
    }

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* card = new QWidget(this);
    card->setObjectName("innerCard");
    card->setStyleSheet(QString(
        "QWidget#innerCard {"
        "  background-color: #1E1E22;"
        "  border: 1px solid %1;"
        "  border-radius: 8px;"
        "}"
    ).arg(accentColor.name()));

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(10);

    // ── Header row ──
    auto* headerRow = new QHBoxLayout();

    // Status dot
    auto* dot = new QLabel(card);
    dot->setFixedSize(10, 10);
    dot->setStyleSheet(QString(
        "background-color: %1; border-radius: 5px;"
    ).arg(accentColor.name()));
    headerRow->addWidget(dot);

    // Dam name
    auto* lblName = new QLabel(m_dam.name, card);
    lblName->setStyleSheet("font-size: 13px; font-weight: bold; color: #F1F5F9;");
    headerRow->addWidget(lblName);

    headerRow->addStretch();

    // Risk badge
    auto* badge = new QLabel(riskLabel, card);
    badge->setStyleSheet(QString(
        "font-size: 10px; font-weight: bold; color: %1; background-color: rgba(%2, %3, %4, 40);"
        "border: 1px solid %1; border-radius: 4px; padding: 2px 6px;"
    ).arg(accentColor.name())
     .arg(accentColor.red()).arg(accentColor.green()).arg(accentColor.blue()));
    headerRow->addWidget(badge);

    cardLayout->addLayout(headerRow);

    // ── Sub-header: river, basin, state ──
    auto* lblSub = new QLabel(
        QString("%1 • %2 • %3").arg(m_dam.river, m_dam.basin, m_dam.state), card);
    lblSub->setStyleSheet("font-size: 10px; color: #94A3B8;");
    cardLayout->addWidget(lblSub);

    // ── Metrics grid ──
    auto* metricsLayout = new QHBoxLayout();
    metricsLayout->setSpacing(8);

    auto addMetric = [&](const QString& label, const QString& value, QColor valColor) {
        auto* box = new QWidget(card);
        box->setStyleSheet("background-color: #0F172A; border: 1px solid #1E293B; border-radius: 6px;");
        auto* bl = new QVBoxLayout(box);
        bl->setContentsMargins(8, 6, 8, 6);
        bl->setSpacing(2);
        auto* lbl = new QLabel(label, box);
        lbl->setStyleSheet("font-size: 9px; color: #64748B; text-transform: uppercase; font-weight: 600;");
        bl->addWidget(lbl);
        auto* val = new QLabel(value, box);
        val->setStyleSheet(QString("font-size: 14px; font-weight: bold; color: %1; font-family: monospace;")
            .arg(valColor.name()));
        bl->addWidget(val);
        metricsLayout->addWidget(box);
    };

    addMetric("Peak Discharge", QString::number(peakQ, 'f', 0) + " m³/s", accentColor);
    addMetric("Max Depth", QString::number(maxDepth, 'f', 2) + " m", QColor(239, 68, 68));
    addMetric("Max Velocity", QString::number(maxVelocity, 'f', 2) + " m/s", QColor(245, 158, 11));
    addMetric("Flood Area", QString::number(maxArea, 'f', 1) + " km²", QColor(56, 189, 248));
    addMetric("Danger Zones", QString::number(dangerCount), dangerCount > 3 ? QColor(239, 68, 68) : QColor(16, 185, 129));

    cardLayout->addLayout(metricsLayout);

    // ── Hydrograph ──
    auto* lblGraph = new QLabel("  Discharge Hydrograph (60-Min Simulation)", card);
    lblGraph->setStyleSheet("font-size: 10px; font-weight: 600; color: #CBD5E1;");
    cardLayout->addWidget(lblGraph);

    std::vector<MiniHydrographWidget::DataPoint> hydroData;
    for (auto& ts : sim.timeSlices) {
        double q = ts.maxVelocityMs * ts.maxDepthM * 50.0;
        hydroData.push_back({ ts.minute, std::max(q, peakQ * 0.02) });
    }
    if (hydroData.size() < 5) {
        hydroData.clear();
        for (int t = 0; t <= 60; t += 2) {
            double tNorm = t / 60.0;
            double rise = std::pow(tNorm / 0.3, 2.5) * std::exp(-(tNorm / 0.3 - 1.0) * 2.5);
            double decay = std::exp(-((tNorm - 0.3) * 3.5));
            double q = tNorm <= 0.3 ? peakQ * rise : peakQ * decay;
            hydroData.push_back({ t, std::max(q, peakQ * 0.02) });
        }
    }

    auto* graph = new MiniHydrographWidget(hydroData, accentColor, card);
    cardLayout->addWidget(graph);

    // ── Footer: storage info ──
    auto* footerLayout = new QHBoxLayout();
    auto addFooter = [&](const QString& label, const QString& value) {
        auto* box = new QWidget(card);
        box->setStyleSheet("background-color: rgba(15,23,42,0.6); border: 1px solid rgba(30,41,59,0.5); border-radius: 4px;");
        auto* bl = new QHBoxLayout(box);
        bl->setContentsMargins(6, 3, 6, 3);
        auto* lbl = new QLabel(label, box);
        lbl->setStyleSheet("font-size: 9px; color: #64748B;");
        bl->addWidget(lbl);
        auto* val = new QLabel(value, box);
        val->setStyleSheet("font-size: 10px; font-weight: bold; color: #E2E8F0; font-family: monospace;");
        bl->addWidget(val);
        footerLayout->addWidget(box);
    };

    addFooter("Storage:", QString::number(totalVol, 'f', 1) + " MCM");
    addFooter("Height:", QString::number(m_dam.height, 'f', 1) + " m");
    addFooter("Spillway:", QString::number(m_dam.spillwayCap, 'f', 0) + " m³/s");
    addFooter("Lat/Lon:", QString("%1, %2").arg(m_dam.lat, 0, 'f', 3).arg(m_dam.lon, 0, 'f', 3));
    footerLayout->addStretch();

    cardLayout->addLayout(footerLayout);

    mainLayout->addWidget(card);
}

// ─── HydroFlowWidget Implementation ──────────────────────────────────────────
HydroFlowWidget::HydroFlowWidget(MapCore::DamManager* damManager, QWidget* parent)
    : QWidget(parent), m_damManager(damManager) {
    setObjectName("hydroFlowScreen");
    setStyleSheet("QWidget#hydroFlowScreen { background-color: #0F172A; }");
    buildUi();
}

void HydroFlowWidget::setDamManager(MapCore::DamManager* mgr) {
    m_damManager = mgr;
    rebuildUi();
}

void HydroFlowWidget::refresh() {
    rebuildUi();
}

void HydroFlowWidget::rebuildUi() {
    qDeleteAll(children());
    buildUi();
}

void HydroFlowWidget::buildUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Title Bar ──
    auto* titleBar = new QWidget(this);
    titleBar->setFixedHeight(56);
    titleBar->setStyleSheet("background-color: #1E293B; border-bottom: 1px solid #334155;");
    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(20, 0, 20, 0);

    auto* lblTitle = new QLabel("🌊  Hydro Flow — Multi-Dam Simulation Overview", titleBar);
    lblTitle->setStyleSheet("font-size: 15px; font-weight: bold; color: #F1F5F9;");
    titleLayout->addWidget(lblTitle);

    titleLayout->addStretch();

    int damCount = m_damManager ? static_cast<int>(m_damManager->getCount()) : 0;
    auto* lblCount = new QLabel(QString("Showing simulations for %1 dams").arg(damCount), titleBar);
    lblCount->setStyleSheet("font-size: 11px; color: #94A3B8;");
    titleLayout->addWidget(lblCount);

    rootLayout->addWidget(titleBar);

    // ── Scrollable dam cards ──
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background-color: #0F172A; }"
        "QScrollBar:vertical {"
        "  background: #1E293B; width: 8px; border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #475569; border-radius: 4px; min-height: 30px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
    );

    auto* scrollContent = new QWidget(scrollArea);
    auto* cardsLayout = new QVBoxLayout(scrollContent);
    cardsLayout->setContentsMargins(20, 16, 20, 20);
    cardsLayout->setSpacing(12);

    if (m_damManager && m_damManager->hasData()) {
        const auto& dams = m_damManager->getDams();

        std::vector<const MapCore::DamPoint*> sortedDams;
        for (auto& d : dams) sortedDams.push_back(&d);
        std::sort(sortedDams.begin(), sortedDams.end(),
            [](const MapCore::DamPoint* a, const MapCore::DamPoint* b) {
                return a->storage > b->storage;
            });

        int count = std::min(static_cast<int>(sortedDams.size()), 50);
        for (int i = 0; i < count; i++) {
            auto* card = new DamSimCard(*sortedDams[i], scrollContent);
            cardsLayout->addWidget(card);
        }

        if (sortedDams.size() > 50) {
            auto* lblMore = new QLabel(
                QString("... and %1 more dams (showing top 50 by storage capacity)")
                    .arg(sortedDams.size() - 50), scrollContent);
            lblMore->setStyleSheet("font-size: 11px; color: #64748B; padding: 8px;");
            lblMore->setAlignment(Qt::AlignCenter);
            cardsLayout->addWidget(lblMore);
        }
    } else {
        auto* lblEmpty = new QLabel("No dam data loaded. Load dam data to see simulations.", scrollContent);
        lblEmpty->setStyleSheet("font-size: 13px; color: #64748B; padding: 40px;");
        lblEmpty->setAlignment(Qt::AlignCenter);
        cardsLayout->addWidget(lblEmpty);
    }

    cardsLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    rootLayout->addWidget(scrollArea);
}

} // namespace MapUI
