#pragma once

#include <QString>
#include <QPointF>
#include <QPolygonF>
#include <vector>
#include "DamManager.h"

namespace MapCore {

struct TopoBasin {
    QString name;
    double startDistKm = 0.0;
    double endDistKm = 0.0;
    double bedElevationMSL = 0.0;      // Bed ground elevation above sea level (m)
    double saddleLipElevationMSL = 0.0; // Spillover threshold elevation (m)
    double storageCapacityMCM = 0.0;    // Depression volume capacity (MCM)
    double currentStoredMCM = 0.0;      // Current water volume accumulated (MCM)
    double currentWSE = 0.0;            // Water Surface Elevation (m MSL) = bed + depth
    double currentDepthM = 0.0;         // Water depth (m)
    bool isOvertopping = false;         // True when WSE >= saddleLipElevationMSL
    double spillDischargeQ = 0.0;       // Outflow cascading to next basin (m^3/s)
};

struct PondedPool {
    QString name;
    QPointF centerPos;          // WGS84 center coordinate of depression
    double depthM = 0.0;        // Current water depth in depression (m)
    double volumeMCM = 0.0;     // Current water stored (MCM)
    double capacityMCM = 0.0;   // Maximum depression capacity (MCM)
    double fillPercent = 0.0;   // Fill percentage (0 - 100%)
    bool isOvertopping = false; // True if spilling over saddle ridge
    QPointF saddleSpillPos;     // WGS84 coordinates of saddle lip
    QPolygonF poolPolygon;      // Organic waterbody pool polygon (WGS84)
};

struct FloodTimeSlice {
    int minute = 0;
    double inundatedAreaKm2 = 0.0;
    double frontDistanceKm = 0.0;
    double maxDepthM = 0.0;
    double maxVelocityMs = 0.0;
    QPointF leadingFrontPos; // WGS84

    // Topographic Depression Storage & Overtopping Hydrodynamics (Sea Level MSL)
    double damElevationMSL = 0.0;           // Dam crest/bed elevation (m MSL)
    double currentBedZ = 0.0;               // Local terrain ground elevation (m MSL)
    double currentWSE = 0.0;                // Water Surface Elevation (m MSL)
    double saddleLipThresholdMSL = 0.0;     // Saddle rim lip elevation to spill (m MSL)
    bool isOvertoppingActive = false;       // Overtopping state
    QString activeBasinName;                // Name of current filling/spilling depression
    double depressionFilledPct = 0.0;       // % of current depression capacity filled
    double totalPondedVolumeMCM = 0.0;      // Total water volume held in depressions

    std::vector<PondedPool> trappedPools;   // Trapped water pools in filled/filling depressions
    std::vector<QPointF> riverStreamline;   // Center flowing river channel trajectory
};

struct FloodWaveNode {
    double lat = 0.0;
    double lon = 0.0;
    double distanceKm = 0.0;        // Downstream distance from dam (km)
    double bedElevationMSL = 0.0;   // Ground height above sea level (m MSL)
    double saddleElevationMSL = 0.0;// Local saddle lip threshold (m MSL)
    double arrivalTimeMin = 0.0;    // Wave front arrival time (minutes)
    double peakDepthM = 0.0;        // Peak water depth (m)
    double velocityMs = 0.0;        // Flow velocity (m/s)
    int basinIndex = 0;             // Index of topographic depression
};

struct DangerZone {
    QString name;                   // e.g. "Gogamukh Riverside Ward", "Lower Subansiri Agricultural Belt"
    QString zoneType;               // "Residential Settlement", "Highway Bridge", "Agricultural Hub", "Health Facility", "Substation"
    double lat = 0.0;
    double lon = 0.0;
    double elevationMSL = 0.0;      // Ground elevation MSL (m)
    double distanceKm = 0.0;        // Distance downstream from dam (km)
    double arrivalTimeMin = 0.0;    // Flood wave ETA (minutes)
    double peakDepthM = 0.0;        // Projected peak flood depth (m)
    double peakVelocityMs = 0.0;    // Flow velocity (m/s)
    int estimatedPopulation = 0;    // Population at risk
    QString riskLevel;              // "CRITICAL", "HIGH", "MODERATE", "WATCH"
    QString evacuationAdvice;       // "Immediate Evacuation to High Ground", "Prepare Emergency Relocation"
    QString criticalInfrastructure; // e.g. "NH-15 Bridge, 33kV Substation"
    bool isCurrentlyInundated = false; // Evaluated dynamically at currentMinute
    double currentDepthM = 0.0;     // Current depth at currentMinute
};

struct FloodSimulationState {
    bool isActive = false;
    DamPoint dam;

    double peakDischargeQ = 0.0;    // Peak breach discharge (m^3/s)
    double totalVolumeMCM = 0.0;    // Reservoir capacity (MCM)
    double breachWidthM = 0.0;      // Effective breach width (m)
    double initialHeightM = 0.0;    // Dam structural height (m)
    double waveSpeedAvgMs = 0.0;    // Average wave celerity (m/s)
    double damBedElevationMSL = 0.0;// Dam base elevation above sea level (m MSL)

    int currentMinute = 0;          // 0 to 60 minutes
    std::vector<FloodTimeSlice> timeSlices; // Precalculated 0 to 60 min cache for zero-compute 60 FPS playback
    std::vector<FloodWaveNode> rawNodes;
    std::vector<TopoBasin> basins;          // Cascading topographic depression basins
    std::vector<DangerZone> dangerZones;    // Downstream populated areas and critical zones in danger
};

class DamFloodSimulator {
public:
    // Fast, vectorized C++ solver implementing Topographic "Pond & Spill" / Depression Storage & Saddle Overtopping
    static FloodSimulationState compute60MinSimulation(const DamPoint& dam);
    static const FloodTimeSlice* getTimeSlice(const FloodSimulationState& state, int minute);
};

} // namespace MapCore
