#include "OsmPbfLoader.h"
#include "GeoTypes.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <cmath>
#include <chrono>
#include <zlib.h>
#include <arpa/inet.h>
#include <protozero/pbf_reader.hpp>

namespace MapCore {

struct RawBlob {
    std::string type;
    std::vector<uint8_t> data;
};

bool OsmPbfLoader::loadPbfFile(const std::string& filepath, SpatialIndex& index,
                               ProgressCallback callback) {
    auto startTime = std::chrono::high_resolution_clock::now();

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "OsmPbfLoader: Cannot open file " << filepath << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    size_t totalFileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    if (callback) callback(0.02f, "Reading OSM PBF headers...");

    index.clear();

    std::unordered_map<int64_t, Point2D> nodeMap;
    nodeMap.reserve(9600000);

    std::vector<uint8_t> readBuf;
    std::vector<uint8_t> uncompressed;

    size_t bytesRead = 0;
    int blockCount = 0;

    // Pass 1: Parse Dense Nodes (Coordinates + Places + POIs)
    while (file.peek() != EOF) {
        uint32_t headerLenNet = 0;
        if (!file.read(reinterpret_cast<char*>(&headerLenNet), 4)) break;
        uint32_t headerLen = ntohl(headerLenNet);

        readBuf.resize(headerLen);
        if (!file.read(reinterpret_cast<char*>(readBuf.data()), headerLen)) break;

        protozero::pbf_reader headerMsg(reinterpret_cast<const char*>(readBuf.data()), headerLen);
        std::string blockType;
        int32_t blobSize = 0;

        while (headerMsg.next()) {
            if (headerMsg.tag() == 1) blockType = headerMsg.get_string();
            else if (headerMsg.tag() == 3) blobSize = headerMsg.get_int32();
            else headerMsg.skip();
        }

        readBuf.resize(blobSize);
        if (!file.read(reinterpret_cast<char*>(readBuf.data()), blobSize)) break;

        bytesRead += 4 + headerLen + blobSize;

        protozero::pbf_reader blobMsg(reinterpret_cast<const char*>(readBuf.data()), blobSize);
        int32_t rawSize = 0;
        protozero::data_view zlibData;
        protozero::data_view rawData;

        while (blobMsg.next()) {
            if (blobMsg.tag() == 1) rawData = blobMsg.get_view();
            else if (blobMsg.tag() == 2) rawSize = blobMsg.get_int32();
            else if (blobMsg.tag() == 3) zlibData = blobMsg.get_view();
            else blobMsg.skip();
        }

        const char* blockBytes = nullptr;
        size_t blockBytesLen = 0;

        if (!zlibData.empty()) {
            uncompressed.resize(rawSize);
            uLongf destLen = rawSize;
            if (uncompress(uncompressed.data(), &destLen,
                           reinterpret_cast<const Bytef*>(zlibData.data()), zlibData.size()) == Z_OK) {
                blockBytes = reinterpret_cast<const char*>(uncompressed.data());
                blockBytesLen = destLen;
            }
        } else if (!rawData.empty()) {
            blockBytes = rawData.data();
            blockBytesLen = rawData.size();
        }

        if (blockType == "OSMData" && blockBytes != nullptr) {
            blockCount++;
            int64_t latOffset = 0, lonOffset = 0;
            int32_t granularity = 100;
            std::vector<std::string> stringTable;
            std::vector<protozero::data_view> groupViews;

            protozero::pbf_reader blockMsg(blockBytes, blockBytesLen);
            while (blockMsg.next()) {
                if (blockMsg.tag() == 1) { // stringtable
                    protozero::pbf_reader stMsg = blockMsg.get_message();
                    while (stMsg.next()) {
                        if (stMsg.tag() == 1) {
                            stringTable.push_back(stMsg.get_string());
                        } else {
                            stMsg.skip();
                        }
                    }
                } else if (blockMsg.tag() == 17) {
                    latOffset = blockMsg.get_int64();
                } else if (blockMsg.tag() == 18) {
                    lonOffset = blockMsg.get_int64();
                } else if (blockMsg.tag() == 19) {
                    granularity = blockMsg.get_int32();
                } else if (blockMsg.tag() == 2) {
                    groupViews.push_back(blockMsg.get_view());
                } else {
                    blockMsg.skip();
                }
            }

            for (const auto& gv : groupViews) {
                protozero::pbf_reader groupMsg(gv.data(), gv.size());
                while (groupMsg.next()) {
                    if (groupMsg.tag() == 2) { // dense nodes
                        protozero::pbf_reader denseMsg = groupMsg.get_message();
                        std::vector<int64_t> ids, lats, lons;
                        std::vector<int32_t> keysVals;

                        while (denseMsg.next()) {
                            if (denseMsg.tag() == 1) {
                                auto pi = denseMsg.get_packed_sint64();
                                ids.assign(pi.begin(), pi.end());
                            } else if (denseMsg.tag() == 8) {
                                auto pi = denseMsg.get_packed_sint64();
                                lats.assign(pi.begin(), pi.end());
                            } else if (denseMsg.tag() == 9) {
                                auto pi = denseMsg.get_packed_sint64();
                                lons.assign(pi.begin(), pi.end());
                            } else if (denseMsg.tag() == 10) {
                                auto pi = denseMsg.get_packed_int32();
                                keysVals.assign(pi.begin(), pi.end());
                            } else {
                                denseMsg.skip();
                            }
                        }

                        int64_t curId = 0, curLat = 0, curLon = 0;
                        size_t kvIdx = 0;

                        for (size_t i = 0; i < ids.size(); ++i) {
                            curId += ids[i];
                            curLat += lats[i];
                            curLon += lons[i];

                            double lat = 1e-9 * (latOffset + (granularity * curLat));
                            double lon = 1e-9 * (lonOffset + (granularity * curLon));
                            Point2D mercPos = Projection::geoToMercator(GeoCoord(lat, lon));

                            nodeMap[curId] = mercPos;
                            index.extent.expand(mercPos);

                            // Extract tags for POIs / Places
                            std::string placeVal, amenityVal, nameVal, tourismVal, railwayVal, aerowayVal;
                            std::vector<std::pair<std::string, std::string>> nodeTags;

                            while (kvIdx < keysVals.size()) {
                                if (keysVals[kvIdx] == 0) {
                                    kvIdx++;
                                    break;
                                }
                                int32_t kIdx = keysVals[kvIdx++];
                                if (kvIdx >= keysVals.size()) break;
                                int32_t vIdx = keysVals[kvIdx++];

                                if (kIdx >= 0 && kIdx < (int)stringTable.size() &&
                                    vIdx >= 0 && vIdx < (int)stringTable.size()) {
                                    const std::string& k = stringTable[kIdx];
                                    const std::string& v = stringTable[vIdx];
                                    nodeTags.emplace_back(k, v);

                                    if (k == "name") nameVal = v;
                                    else if (k == "place") placeVal = v;
                                    else if (k == "amenity") amenityVal = v;
                                    else if (k == "tourism") tourismVal = v;
                                    else if (k == "railway") railwayVal = v;
                                    else if (k == "aeroway") aerowayVal = v;
                                }
                            }

                            // If this node represents a place or POI
                            if (!nameVal.empty() || !placeVal.empty() || !amenityVal.empty() || !aerowayVal.empty()) {
                                MapPoint pt;
                                pt.id = curId;
                                pt.pos = mercPos;
                                pt.name = nameVal;
                                pt.tags = std::move(nodeTags);

                                if (placeVal == "city" || placeVal == "state") {
                                    pt.category = FeatureCategory::PLACE_CITY;
                                    pt.categoryLabel = "City";
                                    pt.priority = 1;
                                    pt.minZoom = 3.0f;
                                    if (pt.name.empty()) pt.name = "City";
                                } else if (placeVal == "town") {
                                    pt.category = FeatureCategory::PLACE_TOWN;
                                    pt.categoryLabel = "Town";
                                    pt.priority = 2;
                                    pt.minZoom = 7.0f;
                                } else if (placeVal == "suburb" || placeVal == "quarter") {
                                    pt.category = FeatureCategory::PLACE_SUBURB;
                                    pt.categoryLabel = "Suburb";
                                    pt.priority = 3;
                                    pt.minZoom = 11.0f;
                                } else if (placeVal == "village" || placeVal == "hamlet" || placeVal == "locality") {
                                    pt.category = FeatureCategory::PLACE_VILLAGE;
                                    pt.categoryLabel = "Village";
                                    pt.priority = 4;
                                    pt.minZoom = 12.0f;
                                } else if (amenityVal == "hospital" || amenityVal == "clinic") {
                                    pt.category = FeatureCategory::POI_HOSPITAL;
                                    pt.categoryLabel = "Hospital";
                                    pt.priority = 3;
                                    pt.minZoom = 12.0f;
                                } else if (amenityVal == "university" || amenityVal == "college") {
                                    pt.category = FeatureCategory::POI_UNIVERSITY;
                                    pt.categoryLabel = "University";
                                    pt.priority = 3;
                                    pt.minZoom = 12.0f;
                                } else if (amenityVal == "school") {
                                    pt.category = FeatureCategory::POI_SCHOOL;
                                    pt.categoryLabel = "School";
                                    pt.priority = 4;
                                    pt.minZoom = 13.0f;
                                } else if (amenityVal == "fuel") {
                                    pt.category = FeatureCategory::POI_FUEL;
                                    pt.categoryLabel = "Fuel Station";
                                    pt.priority = 4;
                                    pt.minZoom = 13.0f;
                                } else if (amenityVal == "bank" || amenityVal == "atm") {
                                    pt.category = FeatureCategory::POI_BANK;
                                    pt.categoryLabel = "Bank / ATM";
                                    pt.priority = 4;
                                    pt.minZoom = 13.0f;
                                } else if (amenityVal == "restaurant" || amenityVal == "fast_food" || amenityVal == "cafe") {
                                    pt.category = FeatureCategory::POI_RESTAURANT;
                                    pt.categoryLabel = "Restaurant / Cafe";
                                    pt.priority = 4;
                                    pt.minZoom = 14.0f;
                                } else if (amenityVal == "place_of_worship") {
                                    pt.category = FeatureCategory::POI_WORSHIP;
                                    pt.categoryLabel = "Place of Worship";
                                    pt.priority = 3;
                                    pt.minZoom = 12.0f;
                                } else if (tourismVal == "attraction" || tourismVal == "theme_park" || tourismVal == "viewpoint") {
                                    pt.category = FeatureCategory::POI_TOURISM;
                                    pt.categoryLabel = "Attraction";
                                    pt.priority = 3;
                                    pt.minZoom = 11.0f;
                                } else if (tourismVal == "hotel" || tourismVal == "guest_house") {
                                    pt.category = FeatureCategory::POI_HOTEL;
                                    pt.categoryLabel = "Hotel / Lodge";
                                    pt.priority = 4;
                                    pt.minZoom = 13.0f;
                                } else if (railwayVal == "station" || railwayVal == "halt") {
                                    pt.category = FeatureCategory::POI_RAILWAY_STATION;
                                    pt.categoryLabel = "Railway Station";
                                    pt.priority = 2;
                                    pt.minZoom = 10.0f;
                                } else if (aerowayVal == "aerodrome" || aerowayVal == "airport" || aerowayVal == "terminal") {
                                    pt.category = FeatureCategory::POI_AIRPORT;
                                    pt.categoryLabel = "Airport";
                                    pt.priority = 1;
                                    pt.minZoom = 6.0f;
                                }

                                if (pt.category != FeatureCategory::UNKNOWN && !pt.name.empty()) {
                                    index.points.push_back(pt);

                                    // Add to search index
                                    SearchItem sItem;
                                    sItem.name = pt.name;
                                    sItem.detail = pt.categoryLabel;
                                    sItem.category = pt.category;
                                    sItem.pos = pt.pos;
                                    sItem.bounds = BoundingBox(pt.pos.x - 0.005f, pt.pos.y - 0.005f,
                                                               pt.pos.x + 0.005f, pt.pos.y + 0.005f);
                                    sItem.zoomTarget = (pt.priority == 1 ? 11.0f : (pt.priority == 2 ? 13.0f : 15.0f));
                                    sItem.priority = pt.priority;
                                    index.searchItems.push_back(sItem);
                                }
                            }
                        }
                    } else {
                        groupMsg.skip();
                    }
                }
            }
        }

        if (callback && totalFileSize > 0 && blockCount % 100 == 0) {
            float prog = 0.05f + 0.45f * (static_cast<float>(bytesRead) / totalFileSize);
            callback(prog, "Indexed " + std::to_string(nodeMap.size()) + " nodes...");
        }
    }

    if (callback) callback(0.55f, "Parsing road networks and geographical features...");

    // Pass 2: Parse Ways (Roads, Water, Landuse, Buildings)
    file.clear();
    file.seekg(0, std::ios::beg);
    bytesRead = 0;
    blockCount = 0;

    while (file.peek() != EOF) {
        uint32_t headerLenNet = 0;
        if (!file.read(reinterpret_cast<char*>(&headerLenNet), 4)) break;
        uint32_t headerLen = ntohl(headerLenNet);

        readBuf.resize(headerLen);
        if (!file.read(reinterpret_cast<char*>(readBuf.data()), headerLen)) break;

        protozero::pbf_reader headerMsg(reinterpret_cast<const char*>(readBuf.data()), headerLen);
        std::string blockType;
        int32_t blobSize = 0;

        while (headerMsg.next()) {
            if (headerMsg.tag() == 1) blockType = headerMsg.get_string();
            else if (headerMsg.tag() == 3) blobSize = headerMsg.get_int32();
            else headerMsg.skip();
        }

        readBuf.resize(blobSize);
        if (!file.read(reinterpret_cast<char*>(readBuf.data()), blobSize)) break;

        bytesRead += 4 + headerLen + blobSize;

        protozero::pbf_reader blobMsg(reinterpret_cast<const char*>(readBuf.data()), blobSize);
        int32_t rawSize = 0;
        protozero::data_view zlibData;
        protozero::data_view rawData;

        while (blobMsg.next()) {
            if (blobMsg.tag() == 1) rawData = blobMsg.get_view();
            else if (blobMsg.tag() == 2) rawSize = blobMsg.get_int32();
            else if (blobMsg.tag() == 3) zlibData = blobMsg.get_view();
            else blobMsg.skip();
        }

        const char* blockBytes = nullptr;
        size_t blockBytesLen = 0;

        if (!zlibData.empty()) {
            uncompressed.resize(rawSize);
            uLongf destLen = rawSize;
            if (uncompress(uncompressed.data(), &destLen,
                           reinterpret_cast<const Bytef*>(zlibData.data()), zlibData.size()) == Z_OK) {
                blockBytes = reinterpret_cast<const char*>(uncompressed.data());
                blockBytesLen = destLen;
            }
        } else if (!rawData.empty()) {
            blockBytes = rawData.data();
            blockBytesLen = rawData.size();
        }

        if (blockType == "OSMData" && blockBytes != nullptr) {
            blockCount++;
            std::vector<std::string> stringTable;
            std::vector<protozero::data_view> groupViews;

            protozero::pbf_reader blockMsg(blockBytes, blockBytesLen);
            while (blockMsg.next()) {
                if (blockMsg.tag() == 1) {
                    protozero::pbf_reader stMsg = blockMsg.get_message();
                    while (stMsg.next()) {
                        if (stMsg.tag() == 1) {
                            stringTable.push_back(stMsg.get_string());
                        } else {
                            stMsg.skip();
                        }
                    }
                } else if (blockMsg.tag() == 2) {
                    groupViews.push_back(blockMsg.get_view());
                } else {
                    blockMsg.skip();
                }
            }

            for (const auto& gv : groupViews) {
                protozero::pbf_reader groupMsg(gv.data(), gv.size());
                while (groupMsg.next()) {
                    if (groupMsg.tag() == 3) { // Ways
                        protozero::pbf_reader wayMsg = groupMsg.get_message();
                        int64_t wayId = 0;
                        std::vector<uint32_t> keys, vals;
                        std::vector<int64_t> refDeltas;

                        while (wayMsg.next()) {
                            if (wayMsg.tag() == 1) wayId = wayMsg.get_int64();
                            else if (wayMsg.tag() == 2) {
                                auto pi = wayMsg.get_packed_uint32();
                                keys.assign(pi.begin(), pi.end());
                            } else if (wayMsg.tag() == 3) {
                                auto pi = wayMsg.get_packed_uint32();
                                vals.assign(pi.begin(), pi.end());
                            } else if (wayMsg.tag() == 8) {
                                auto pi = wayMsg.get_packed_sint64();
                                refDeltas.assign(pi.begin(), pi.end());
                            } else {
                                wayMsg.skip();
                            }
                        }

                        if (refDeltas.size() < 2) continue;

                        std::string highwayVal, naturalVal, waterwayVal, landuseVal, leisureVal;
                        std::string buildingVal, railwayVal, aerowayVal, boundaryVal, nameVal, refVal;
                        std::vector<std::pair<std::string, std::string>> wayTags;

                        for (size_t k = 0; k < keys.size() && k < vals.size(); ++k) {
                            if (keys[k] < stringTable.size() && vals[k] < stringTable.size()) {
                                const std::string& kStr = stringTable[keys[k]];
                                const std::string& vStr = stringTable[vals[k]];
                                wayTags.emplace_back(kStr, vStr);

                                if (kStr == "name") nameVal = vStr;
                                else if (kStr == "ref") refVal = vStr;
                                else if (kStr == "highway") highwayVal = vStr;
                                else if (kStr == "natural") naturalVal = vStr;
                                else if (kStr == "waterway") waterwayVal = vStr;
                                else if (kStr == "landuse") landuseVal = vStr;
                                else if (kStr == "leisure") leisureVal = vStr;
                                else if (kStr == "building") buildingVal = vStr;
                                else if (kStr == "railway") railwayVal = vStr;
                                else if (kStr == "aeroway") aerowayVal = vStr;
                                else if (kStr == "boundary") boundaryVal = vStr;
                            }
                        }

                        // Reconstruct geometry coordinates
                        std::vector<Point2D> coords;
                        coords.reserve(refDeltas.size());
                        int64_t curRef = 0;
                        BoundingBox bbox;

                        for (int64_t delta : refDeltas) {
                            curRef += delta;
                            auto it = nodeMap.find(curRef);
                            if (it != nodeMap.end()) {
                                coords.push_back(it->second);
                                bbox.expand(it->second);
                            }
                        }

                        if (coords.size() < 2 || !bbox.isValid()) continue;

                        bool isClosed = (refDeltas.front() != 0 && coords.front() == coords.back() && coords.size() >= 4);

                        // Determine category and add feature
                        if (!highwayVal.empty()) {
                            MapPolyline poly;
                            poly.id = wayId;
                            poly.name = nameVal;
                            poly.ref = refVal;
                            poly.bbox = bbox;
                            poly.points = std::move(coords);
                            poly.tags = std::move(wayTags);

                            if (highwayVal == "motorway" || highwayVal == "motorway_link") {
                                poly.category = FeatureCategory::HIGHWAY_MOTORWAY;
                                poly.minZoom = 0.0f;
                            } else if (highwayVal == "trunk" || highwayVal == "trunk_link") {
                                poly.category = FeatureCategory::HIGHWAY_TRUNK;
                                poly.minZoom = 0.0f;
                            } else if (highwayVal == "primary" || highwayVal == "primary_link") {
                                poly.category = FeatureCategory::HIGHWAY_PRIMARY;
                                poly.minZoom = 5.0f;
                            } else if (highwayVal == "secondary" || highwayVal == "secondary_link") {
                                poly.category = FeatureCategory::HIGHWAY_SECONDARY;
                                poly.minZoom = 8.0f;
                            } else if (highwayVal == "tertiary" || highwayVal == "tertiary_link") {
                                poly.category = FeatureCategory::HIGHWAY_TERTIARY;
                                poly.minZoom = 10.0f;
                            } else if (highwayVal == "residential" || highwayVal == "living_street") {
                                poly.category = FeatureCategory::HIGHWAY_RESIDENTIAL;
                                poly.minZoom = 12.0f;
                            } else if (highwayVal == "unclassified") {
                                poly.category = FeatureCategory::HIGHWAY_UNCLASSIFIED;
                                poly.minZoom = 11.0f;
                            } else if (highwayVal == "service") {
                                poly.category = FeatureCategory::HIGHWAY_SERVICE;
                                poly.minZoom = 13.0f;
                            } else if (highwayVal == "track") {
                                poly.category = FeatureCategory::HIGHWAY_TRACK;
                                poly.minZoom = 13.0f;
                            } else {
                                poly.category = FeatureCategory::HIGHWAY_PATH;
                                poly.minZoom = 14.0f;
                            }

                            // Generate LOD simplified geometry for low-zoom overview performance
                            if (poly.points.size() > 4 && poly.minZoom <= 10.0f) {
                                poly.lodPoints = GeometryUtils::simplifyPolyline(poly.points, 0.00008f);
                            }

                            index.polylines.push_back(std::move(poly));

                            // Search index for named highways
                            if (!nameVal.empty() || !refVal.empty()) {
                                SearchItem sItem;
                                sItem.name = !nameVal.empty() ? nameVal : ("Highway " + refVal);
                                sItem.detail = !refVal.empty() ? refVal : getCategoryDisplayName(index.polylines.back().category);
                                sItem.category = index.polylines.back().category;
                                sItem.pos = bbox.center();
                                sItem.bounds = bbox;
                                sItem.zoomTarget = (poly.minZoom <= 5.0f ? 11.0f : 14.0f);
                                sItem.priority = (poly.minZoom <= 5.0f ? 2 : 4);
                                index.searchItems.push_back(sItem);
                            }
                        } else if (!waterwayVal.empty() || naturalVal == "water" || naturalVal == "wetland") {
                            if (isClosed || naturalVal == "water" || naturalVal == "wetland") {
                                MapPolygon poly;
                                poly.id = wayId;
                                poly.name = nameVal;
                                poly.bbox = bbox;
                                poly.points = std::move(coords);
                                poly.tags = std::move(wayTags);
                                poly.category = FeatureCategory::WATER_LAKE;
                                poly.minZoom = (bbox.width() > 0.005f ? 4.0f : 9.0f);
                                index.polygons.push_back(std::move(poly));
                            } else {
                                MapPolyline poly;
                                poly.id = wayId;
                                poly.name = nameVal;
                                poly.bbox = bbox;
                                poly.points = std::move(coords);
                                poly.tags = std::move(wayTags);

                                if (waterwayVal == "river") {
                                    poly.category = FeatureCategory::WATER_RIVER;
                                    poly.minZoom = 4.0f;
                                    if (poly.points.size() > 4) {
                                        poly.lodPoints = GeometryUtils::simplifyPolyline(poly.points, 0.00008f);
                                    }
                                } else if (waterwayVal == "canal") {
                                    poly.category = FeatureCategory::WATER_CANAL;
                                    poly.minZoom = 9.0f;
                                } else {
                                    poly.category = FeatureCategory::WATER_STREAM;
                                    poly.minZoom = 12.0f;
                                }
                                index.polylines.push_back(std::move(poly));
                            }

                            if (!nameVal.empty()) {
                                SearchItem sItem;
                                sItem.name = nameVal;
                                sItem.detail = "Waterway / River";
                                sItem.category = FeatureCategory::WATER_RIVER;
                                sItem.pos = bbox.center();
                                sItem.bounds = bbox;
                                sItem.zoomTarget = 11.0f;
                                sItem.priority = 2;
                                index.searchItems.push_back(sItem);
                            }
                        } else if (naturalVal == "wood" || landuseVal == "forest" || leisureVal == "nature_reserve") {
                            MapPolygon poly;
                            poly.id = wayId;
                            poly.name = nameVal;
                            poly.bbox = bbox;
                            poly.points = std::move(coords);
                            poly.tags = std::move(wayTags);
                            poly.category = FeatureCategory::LANDUSE_FOREST;
                            poly.minZoom = (bbox.width() > 0.01f ? 6.0f : 9.0f);
                            index.polygons.push_back(std::move(poly));
                        } else if (leisureVal == "park" || leisureVal == "garden" || landuseVal == "grass" || leisureVal == "pitch") {
                            MapPolygon poly;
                            poly.id = wayId;
                            poly.name = nameVal;
                            poly.bbox = bbox;
                            poly.points = std::move(coords);
                            poly.tags = std::move(wayTags);
                            poly.category = FeatureCategory::LANDUSE_PARK;
                            poly.minZoom = 10.0f;
                            index.polygons.push_back(std::move(poly));
                        } else if (landuseVal == "residential" || landuseVal == "commercial" || landuseVal == "industrial") {
                            MapPolygon poly;
                            poly.id = wayId;
                            poly.name = nameVal;
                            poly.bbox = bbox;
                            poly.points = std::move(coords);
                            poly.tags = std::move(wayTags);
                            poly.category = FeatureCategory::LANDUSE_RESIDENTIAL;
                            poly.minZoom = 11.0f;
                            index.polygons.push_back(std::move(poly));
                        } else if (landuseVal == "farmland" || landuseVal == "farmyard" || landuseVal == "orchard" || landuseVal == "meadow") {
                            MapPolygon poly;
                            poly.id = wayId;
                            poly.name = nameVal;
                            poly.bbox = bbox;
                            poly.points = std::move(coords);
                            poly.tags = std::move(wayTags);
                            poly.category = FeatureCategory::LANDUSE_FARMLAND;
                            poly.minZoom = 10.0f;
                            index.polygons.push_back(std::move(poly));
                        } else if (!buildingVal.empty() && buildingVal != "no") {
                            MapPolygon poly;
                            poly.id = wayId;
                            poly.name = nameVal;
                            poly.bbox = bbox;
                            poly.points = std::move(coords);
                            poly.tags = std::move(wayTags);
                            poly.category = FeatureCategory::BUILDING;
                            poly.minZoom = 15.0f;
                            index.polygons.push_back(std::move(poly));
                        } else if (railwayVal == "rail") {
                            MapPolyline poly;
                            poly.id = wayId;
                            poly.name = nameVal;
                            poly.bbox = bbox;
                            poly.points = std::move(coords);
                            poly.tags = std::move(wayTags);
                            poly.category = FeatureCategory::RAILWAY_MAIN;
                            poly.minZoom = 8.0f;
                            index.polylines.push_back(std::move(poly));
                        } else if (!aerowayVal.empty()) {
                            if (aerowayVal == "runway") {
                                MapPolyline poly;
                                poly.id = wayId;
                                poly.name = nameVal;
                                poly.bbox = bbox;
                                poly.points = std::move(coords);
                                poly.tags = std::move(wayTags);
                                poly.category = FeatureCategory::AEROWAY_RUNWAY;
                                poly.minZoom = 9.0f;
                                index.polylines.push_back(std::move(poly));
                            } else if (isClosed) {
                                MapPolygon poly;
                                poly.id = wayId;
                                poly.name = nameVal;
                                poly.bbox = bbox;
                                poly.points = std::move(coords);
                                poly.tags = std::move(wayTags);
                                poly.category = FeatureCategory::AEROWAY_RUNWAY;
                                poly.minZoom = 10.0f;
                                index.polygons.push_back(std::move(poly));
                            }
                        } else if (boundaryVal == "administrative") {
                            MapPolyline poly;
                            poly.id = wayId;
                            poly.name = nameVal;
                            poly.bbox = bbox;
                            poly.points = std::move(coords);
                            poly.tags = std::move(wayTags);
                            poly.category = FeatureCategory::BOUNDARY_STATE;
                            poly.minZoom = 2.0f;
                            index.polylines.push_back(std::move(poly));
                        }
                    } else {
                        groupMsg.skip();
                    }
                }
            }
        }

        if (callback && totalFileSize > 0 && blockCount % 100 == 0) {
            float prog = 0.55f + 0.35f * (static_cast<float>(bytesRead) / totalFileSize);
            callback(prog, "Extracted " + std::to_string(index.polylines.size()) + " roads/ways...");
        }
    }

    if (callback) callback(0.92f, "Building spatial quad index & search tree...");

    // Build spatial grid index
    index.buildIndex();

    auto endTime = std::chrono::high_resolution_clock::now();
    double dur = std::chrono::duration<double>(endTime - startTime).count();

    std::cout << "OsmPbfLoader: Loaded in " << dur << "s | Polygons: " << index.polygons.size()
              << ", Polylines: " << index.polylines.size() << ", Points: " << index.points.size()
              << ", Search Items: " << index.searchItems.size() << std::endl;

    if (callback) callback(1.0f, "Map data ready!");
    return true;
}

} // namespace MapCore
