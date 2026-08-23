#include "DamFloodSimulation.h"
#include <cmath>
#include <algorithm>

namespace MapCore {

FloodSimulationState DamFloodSimulator::compute60MinSimulation(const DamPoint& dam) {
    FloodSimulationState state;
    state.isActive = true;
    state.dam = dam;

    // 1. Physical Parameter Extraction
    double H0 = (dam.height > 0.0f) ? static_cast<double>(dam.height) : 34.0;
    double V0 = (dam.storage > 0.0f) ? static_cast<double>(dam.storage) : 140.0; // MCM
    double B = 0.0;

    if (dam.spillwayCap > 0.0f) {
        B = std::clamp(static_cast<double>(dam.spillwayCap) / 16.0, 35.0, 320.0);
    } else {
        B = std::clamp(H0 * 2.4, 45.0, 260.0);
    }

    const double g = 9.81;

    // Ritter / Froehlich Peak Breach Discharge: Qp = (8/27) * sqrt(g) * B * H0^(1.5)
    double Qp = (8.0 / 27.0) * std::sqrt(g) * B * std::pow(H0, 1.5);
    // Initial Wave Celerity: c0 = 2 * sqrt(g * H0)
    double c0 = 2.0 * std::sqrt(g * H0); // m/s

    // 2. Initial Ground Elevation above Sea Level (m MSL)
    double damZ = 220.0;
    if (dam.lat > 28.0) damZ = 450.0; // Himalayan foothills / North India
    else if (dam.lat > 24.0 && dam.lon > 89.0) damZ = 135.0; // Assam / Northeast
    else if (dam.lon < 76.0) damZ = 580.0; // Western Ghats
    else if (dam.lat < 18.0) damZ = 320.0; // Peninsular India

    state.initialHeightM = H0;
    state.totalVolumeMCM = V0;
    state.breachWidthM = B;
    state.peakDischargeQ = Qp;
    state.waveSpeedAvgMs = c0 * 0.42;
    state.damBedElevationMSL = damZ;

    // 3. Define 4 Cascading Topographic Depressions / Basins with Saddle Spillway Thresholds
    state.basins.clear();
    state.basins.resize(4);

    state.basins[0].name = "Basin 1: Gorge Foot & Tailrace Depression";
    state.basins[0].startDistKm = 0.0;
    state.basins[0].endDistKm = 6.5;
    state.basins[0].bedElevationMSL = damZ - 18.0;
    state.basins[0].saddleLipElevationMSL = (damZ - 18.0) + 4.8; // 4.8m deep lip
    state.basins[0].storageCapacityMCM = std::min(V0 * 0.15, 14.0);

    state.basins[1].name = "Basin 2: Intermediate Valley Depression";
    state.basins[1].startDistKm = 6.5;
    state.basins[1].endDistKm = 16.0;
    state.basins[1].bedElevationMSL = damZ - 42.0;
    state.basins[1].saddleLipElevationMSL = (damZ - 42.0) + 3.9; // 3.9m deep lip
    state.basins[1].storageCapacityMCM = std::min(V0 * 0.28, 32.0);

    state.basins[2].name = "Basin 3: Lower Floodplain Catchment Basin";
    state.basins[2].startDistKm = 16.0;
    state.basins[2].endDistKm = 28.0;
    state.basins[2].bedElevationMSL = damZ - 72.0;
    state.basins[2].saddleLipElevationMSL = (damZ - 72.0) + 3.2; // 3.2m deep lip
    state.basins[2].storageCapacityMCM = std::min(V0 * 0.35, 52.0);

    state.basins[3].name = "Basin 4: River Confluence & Coastal Plain";
    state.basins[3].startDistKm = 28.0;
    state.basins[3].endDistKm = 42.0;
    state.basins[3].bedElevationMSL = damZ - 98.0;
    state.basins[3].saddleLipElevationMSL = (damZ - 98.0) + 2.4;
    state.basins[3].storageCapacityMCM = std::min(V0 * 0.45, 80.0);

    // 4. Downstream River Valley Drainage Vector Direction
    double flowAngle = 0.0;
    if (dam.lon < 78.0) flowAngle = -40.0 * M_PI / 180.0;
    else if (dam.lat > 25.0 && dam.lon > 88.0) flowAngle = -125.0 * M_PI / 180.0;
    else if (dam.lat > 28.0) flowAngle = -65.0 * M_PI / 180.0;
    else flowAngle = -75.0 * M_PI / 180.0;

    // Discretize River Trajectory (60 Nodes over 42 km)
    const int numNodes = 60;
    const double maxReachKm = 42.0;
    const double dxKm = maxReachKm / (numNodes - 1);

    state.rawNodes.clear();
    state.rawNodes.reserve(numNodes);

    double curLat = dam.lat;
    double curLon = dam.lon;

    for (int i = 0; i < numNodes; ++i) {
        double distKm = i * dxKm;
        double meanderAngle = flowAngle + 0.36 * std::sin(i * 0.38) + 0.16 * std::cos(i * 0.75);

        if (i > 0) {
            double stepM = dxKm * 1000.0;
            double dLat = (stepM * std::cos(meanderAngle)) / 111320.0;
            double dLon = (stepM * std::sin(meanderAngle)) / (111320.0 * std::cos(curLat * M_PI / 180.0));
            curLat += dLat;
            curLon += dLon;
        }

        // Determine which basin this node belongs to
        int bIdx = 0;
        if (distKm > 28.0) bIdx = 3;
        else if (distKm > 16.0) bIdx = 2;
        else if (distKm > 6.5) bIdx = 1;

        FloodWaveNode node;
        node.lat = curLat;
        node.lon = curLon;
        node.distanceKm = distKm;
        node.basinIndex = bIdx;
        node.bedElevationMSL = state.basins[bIdx].bedElevationMSL;
        node.saddleElevationMSL = state.basins[bIdx].saddleLipElevationMSL;

        state.rawNodes.push_back(node);
    }

    // 5. Precompute 60-Minute "Pond & Spill" Hydrodynamic Elevation Model
    state.timeSlices.resize(61);

    double cumulativeDischargedMCM = 0.0;
    double totalPondedMCM = 0.0;

    for (int tMin = 0; tMin <= 60; ++tMin) {
        FloodTimeSlice slice;
        slice.minute = tMin;
        slice.damElevationMSL = damZ;

        if (tMin == 0) {
            slice.inundatedAreaKm2 = 0.0;
            slice.frontDistanceKm = 0.0;
            slice.maxDepthM = 0.0;
            slice.maxVelocityMs = 0.0;
            slice.currentBedZ = damZ;
            slice.currentWSE = damZ;
            slice.saddleLipThresholdMSL = state.basins[0].saddleLipElevationMSL;
            slice.isOvertoppingActive = false;
            slice.activeBasinName = state.basins[0].name;
            slice.depressionFilledPct = 0.0;
            slice.totalPondedVolumeMCM = 0.0;
            slice.leadingFrontPos = QPointF(dam.lat, dam.lon);
            state.timeSlices[0] = slice;
            continue;
        }

        // Cumulative reservoir release hydrograph (MCM)
        double currentQ = Qp * std::exp(-0.018 * tMin);
        double deltaMCM = (currentQ * 60.0) / 1000000.0;
        cumulativeDischargedMCM += deltaMCM;

        // Fill cascading basins sequentially
        double remainingMCM = cumulativeDischargedMCM;
        int activeBasinIdx = 0;
        double frontDist = 0.0;
        double maxDepth = 0.0;
        double maxVel = 0.0;
        bool overtopping = false;

        totalPondedMCM = 0.0;

        for (int b = 0; b < 4; ++b) {
            auto& basin = state.basins[b];
            activeBasinIdx = b;

            double cap = basin.storageCapacityMCM;
            double fillVol = std::min(remainingMCM, cap);
            basin.currentStoredMCM = fillVol;
            totalPondedMCM += fillVol;

            double fillFraction = std::clamp(fillVol / cap, 0.0, 1.0);
            double depth = fillFraction * (basin.saddleLipElevationMSL - basin.bedElevationMSL);

            if (fillVol >= cap * 0.98) {
                // Basin filled to capacity -> OVERTOPPING / SPILLOVER ACTIVE
                basin.isOvertopping = true;
                overtopping = true;
                double extraHead = std::min(2.5, (remainingMCM - cap) / (cap * 0.5));
                depth += extraHead;
                basin.currentDepthM = depth;
                basin.currentWSE = basin.bedElevationMSL + depth;

                remainingMCM -= cap;
                frontDist = basin.endDistKm;
            } else {
                // Basin still filling / ponding depression
                basin.isOvertopping = false;
                overtopping = false;
                basin.currentDepthM = depth;
                basin.currentWSE = basin.bedElevationMSL + depth;

                // Wave front is currently propagating inside this basin
                frontDist = basin.startDistKm + fillFraction * (basin.endDistKm - basin.startDistKm);
                remainingMCM = 0.0;
                break;
            }
        }

        auto& curBasin = state.basins[activeBasinIdx];

        maxDepth = curBasin.currentDepthM;
        maxVel = std::max(1.8, (c0 * 0.45) * std::sqrt(std::max(0.2, maxDepth / H0)));
        double inundatedArea = (frontDist * 0.65) * (1.2 + std::sqrt(frontDist));

        // Locate leading wave front coordinates & construct trapped water pool geometries
        QPointF frontPos(dam.lat, dam.lon);
        std::vector<QPointF> streamline;
        std::vector<PondedPool> trappedPools;

        for (const auto& node : state.rawNodes) {
            if (node.distanceKm <= frontDist) {
                streamline.push_back(QPointF(node.lat, node.lon));
                frontPos = QPointF(node.lat, node.lon);
            } else {
                break;
            }
        }

        // Construct Trapped Waterbodies / Pools in each basin that has received water
        for (int b = 0; b <= activeBasinIdx; ++b) {
            const auto& bInfo = state.basins[b];
            if (bInfo.currentStoredMCM <= 0.05) continue;

            double midKm = (bInfo.startDistKm + bInfo.endDistKm) / 2.0;
            QPointF poolCenter(dam.lat, dam.lon);
            QPointF saddlePos(dam.lat, dam.lon);

            for (const auto& node : state.rawNodes) {
                if (std::abs(node.distanceKm - midKm) < dxKm * 1.2) {
                    poolCenter = QPointF(node.lat, node.lon);
                }
                if (std::abs(node.distanceKm - bInfo.endDistKm) < dxKm * 1.2) {
                    saddlePos = QPointF(node.lat, node.lon);
                }
            }

            double fPct = std::clamp((bInfo.currentStoredMCM / bInfo.storageCapacityMCM) * 100.0, 0.0, 100.0);
            double poolRadiusM = std::clamp(std::sqrt((bInfo.currentStoredMCM * 1000000.0) / (M_PI * std::max(1.0, bInfo.currentDepthM) * 0.45)), 150.0, 1900.0);

            // Generate realistic organic waterbody boundary polygon
            QPolygonF poolPoly;
            const int numPoints = 16;
            for (int p = 0; p < numPoints; ++p) {
                double angle = (p * 2.0 * M_PI) / numPoints;
                double r = poolRadiusM * (1.0 + 0.22 * std::sin(3.0 * angle + b * 1.5) + 0.12 * std::cos(5.0 * angle));
                double dLat = (r * std::cos(angle)) / 111320.0;
                double dLon = (r * std::sin(angle)) / (111320.0 * std::cos(poolCenter.x() * M_PI / 180.0));
                poolPoly.append(QPointF(poolCenter.x() + dLat, poolCenter.y() + dLon));
            }

            PondedPool pool;
            pool.name = bInfo.name;
            pool.centerPos = poolCenter;
            pool.depthM = bInfo.currentDepthM;
            pool.volumeMCM = bInfo.currentStoredMCM;
            pool.capacityMCM = bInfo.storageCapacityMCM;
            pool.fillPercent = fPct;
            pool.isOvertopping = bInfo.isOvertopping;
            pool.saddleSpillPos = saddlePos;
            pool.poolPolygon = poolPoly;

            trappedPools.push_back(pool);
        }

        slice.inundatedAreaKm2 = inundatedArea;
        slice.frontDistanceKm = frontDist;
        slice.maxDepthM = maxDepth;
        slice.maxVelocityMs = maxVel;
        slice.leadingFrontPos = frontPos;
        slice.riverStreamline = streamline;
        slice.trappedPools = trappedPools;

        // Pond & Spill Telemetry
        slice.currentBedZ = curBasin.bedElevationMSL;
        slice.currentWSE = curBasin.currentWSE;
        slice.saddleLipThresholdMSL = curBasin.saddleLipElevationMSL;
        slice.isOvertoppingActive = overtopping;
        slice.activeBasinName = curBasin.name;
        slice.depressionFilledPct = std::clamp((curBasin.currentStoredMCM / curBasin.storageCapacityMCM) * 100.0, 0.0, 100.0);
        slice.totalPondedVolumeMCM = totalPondedMCM;

        state.timeSlices[tMin] = slice;
    }

    state.currentMinute = 0;
    return state;
}

const FloodTimeSlice* DamFloodSimulator::getTimeSlice(const FloodSimulationState& state, int minute) {
    if (!state.isActive || state.timeSlices.empty()) return nullptr;
    int idx = std::clamp(minute, 0, static_cast<int>(state.timeSlices.size()) - 1);
    return &state.timeSlices[idx];
}

} // namespace MapCore
