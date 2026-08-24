/**
 * @file BuiltinPlugins.cpp
 * @brief Register built-in SDG indicators as plugins
 */

#include "BuiltinPlugins.h"
#include "CalculateSDG.h"
#include "ExtractPriorityAreas.h"
#include "Logger.h"

#include <sstream>
#include <iomanip>
#include <unordered_set>
#include <memory>

using namespace std;

// ============================================================================
// Helper: parse int set from string ("1,2,3" → {1,2,3})
// ============================================================================

static unordered_set<int> parseIntSet(const string& s) {
    unordered_set<int> result;
    istringstream iss(s);
    string token;
    while (getline(iss, token, ',')) {
        if (!token.empty()) {
            try { result.insert(stoi(token)); } catch (...) {}
        }
    }
    return result;
}

// ============================================================================
// Helper: parse transition map ("1>2,3>4" → {(1,2),(3,4)})
// ============================================================================

static unordered_map<int, vector<int>> parseTransitionMap(const string& s) {
    unordered_map<int, vector<int>> result;
    istringstream iss(s);
    string token;
    while (getline(iss, token, ',')) {
        auto pos = token.find('>');
        if (pos != string::npos) {
            try {
                int from = stoi(token.substr(0, pos));
                int to   = stoi(token.substr(pos + 1));
                result[from].push_back(to);
            } catch (...) {}
        }
    }
    return result;
}

// ============================================================================
// Helper: parse emission map ("1:0.5,2:1.2" → {(1,0.5),(2,1.2)})
// ============================================================================

static unordered_map<int, double> parseEmissionMap(const string& s) {
    unordered_map<int, double> result;
    istringstream iss(s);
    string token;
    while (getline(iss, token, ',')) {
        auto pos = token.find(':');
        if (pos != string::npos) {
            try {
                int type = stoi(token.substr(0, pos));
                double em = stod(token.substr(pos + 1));
                result[type] = em;
            } catch (...) {}
        }
    }
    return result;
}

// ============================================================================
// Helper: get string from params map with default
// ============================================================================

static string getParam(const map<string, string>& params, const string& key,
                       const string& def = "") {
    auto it = params.find(key);
    return (it != params.end()) ? it->second : def;
}

static double getDouble(const map<string, string>& params, const string& key,
                        double def = 0.0) {
    string val = getParam(params, key);
    if (val.empty()) return def;
    try { return stod(val); } catch (...) { return def; }
}

static int getInt(const map<string, string>& params, const string& key,
                  int def = 0) {
    string val = getParam(params, key);
    if (val.empty()) return def;
    try { return stoi(val); } catch (...) { return def; }
}

static bool getBool(const map<string, string>& params, const string& key,
                    bool def = false) {
    string val = getParam(params, key);
    if (val.empty()) return def;
    return (val == "true" || val == "1" || val == "yes");
}

// ============================================================================
// Helper: create a basic PluginDescriptor
// ============================================================================

static PluginDescriptor makeDescriptor(const string& name,
                                        const string& description,
                                        const string& category,
                                        const vector<PluginParameter>& params) {
    PluginDescriptor desc;
    desc.name        = name;
    desc.version     = "1.0";
    desc.author      = "GeoSDG Team";
    desc.description = description;
    desc.category    = category;
    desc.library     = "(builtin)";
    desc.className   = "BuiltinIndicatorAdapter";
    desc.minCliVersion = "1.0";
    desc.parameters  = params;
    return desc;
}

static PluginParameter param(const string& name, const string& shortName,
                              const string& type, bool required,
                              const string& def, const string& desc) {
    PluginParameter p;
    p.name = name; p.shortName = shortName; p.type = type;
    p.required = required; p.defaultValue = def; p.description = desc;
    return p;
}

// ============================================================================
// Built-in indicator execution functions
// ============================================================================

static int execLandProportion(const map<string, string>& params,
                               string& result, string& error) {
    string initLUCC = getParam(params, "init-lucc");
    string typesStr = getParam(params, "types");
    if (initLUCC.empty() || typesStr.empty()) {
        error = "Missing required: --init-lucc and --types";
        return 1;
    }
    auto types = parseIntSet(typesStr);
    double maxTh = getDouble(params, "max-threshold", 100.0);
    double minTh = getDouble(params, "min-threshold", 0.0);
    auto p = make_unique<CalculateSDG>();
    double score = p->calculateLandProportionIndicator(initLUCC, maxTh, minTh, types);
    ostringstream oss;
    oss << fixed << setprecision(4) << "score=" << score;
    result = oss.str();
    return 0;
}

static int execLandConversion(const map<string, string>& params,
                               string& result, string& error) {
    string initLUCC = getParam(params, "init-lucc");
    string currLUCC = getParam(params, "curr-lucc");
    string transStr = getParam(params, "transitions");
    if (initLUCC.empty() || currLUCC.empty() || transStr.empty()) {
        error = "Missing required: --init-lucc, --curr-lucc, --transitions";
        return 1;
    }
    auto trans = parseTransitionMap(transStr);
    double maxTh = getDouble(params, "max-threshold", 100.0);
    double minTh = getDouble(params, "min-threshold", 0.0);
    bool positive = getBool(params, "positive", false);
    auto p = make_unique<CalculateSDG>();
    double score = p->calculateLandConversionIndicator(initLUCC, currLUCC, trans,
                                                        maxTh, minTh, positive);
    string dir = positive ? "positive" : "negative";
    ostringstream oss;
    oss << fixed << setprecision(4) << "score(" << dir << ")=" << score;
    result = oss.str();
    return 0;
}

static int execBufferZone(const map<string, string>& params,
                           string& result, string& error) {
    string initLUCC = getParam(params, "init-lucc");
    string bufferPath = getParam(params, "buffer");
    string typesStr = getParam(params, "types");
    if (initLUCC.empty() || bufferPath.empty() || typesStr.empty()) {
        error = "Missing required: --init-lucc, --buffer, --types";
        return 1;
    }
    auto types = parseIntSet(typesStr);
    double maxTh = getDouble(params, "max-threshold", 100.0);
    double minTh = getDouble(params, "min-threshold", 0.0);
    auto p = make_unique<CalculateSDG>();
    double score = p->calculateBufferZoneIndicator(initLUCC, bufferPath, types, maxTh, minTh);
    ostringstream oss;
    oss << fixed << setprecision(4) << "score=" << score;
    result = oss.str();
    return 0;
}

static int execSDG1131(const map<string, string>& params,
                        string& result, string& error) {
    string initLUCC = getParam(params, "init-lucc");
    string currLUCC = getParam(params, "curr-lucc");
    string initPopu = getParam(params, "init-popu");
    string currPopu = getParam(params, "curr-popu");
    string typesStr = getParam(params, "types");
    if (initLUCC.empty() || currLUCC.empty() || initPopu.empty() ||
        currPopu.empty() || typesStr.empty()) {
        error = "Missing required: --init-lucc, --curr-lucc, --init-popu, --curr-popu, --types";
        return 1;
    }
    auto types = parseIntSet(typesStr);
    double maxTh = getDouble(params, "max-threshold", 100.0);
    double minTh = getDouble(params, "min-threshold", 0.0);
    double bestTh = getDouble(params, "best-threshold", 50.0);
    auto p = make_unique<CalculateSDG>();
    double score = p->calculateSDG1131Indicator(initLUCC, currLUCC, initPopu, currPopu,
                                                 types, maxTh, minTh, bestTh);
    ostringstream oss;
    oss << fixed << setprecision(4) << "score=" << score;
    result = oss.str();
    return 0;
}

static int execSDG1322(const map<string, string>& params,
                        string& result, string& error) {
    string initLUCC = getParam(params, "init-lucc");
    string currLUCC = getParam(params, "curr-lucc");
    string emissionStr = getParam(params, "emission");
    if (initLUCC.empty() || currLUCC.empty() || emissionStr.empty()) {
        error = "Missing required: --init-lucc, --curr-lucc, --emission";
        return 1;
    }
    auto em = parseEmissionMap(emissionStr);
    double ratio = getDouble(params, "ratio", 0.5);
    auto p = make_unique<CalculateSDG>();
    double score = p->calculateSDG1322Indicator(initLUCC, currLUCC, em, ratio);
    ostringstream oss;
    oss << fixed << setprecision(4) << "score=" << score;
    result = oss.str();
    return 0;
}

static int execPriorityLoss(const map<string, string>& params,
                              string& result, string& error) {
    string initLUCC = getParam(params, "init-lucc");
    string currLUCC = getParam(params, "curr-lucc");
    string output = getParam(params, "output");
    string typesStr = getParam(params, "types");
    if (initLUCC.empty() || currLUCC.empty() || output.empty() || typesStr.empty()) {
        error = "Missing required: --init-lucc, --curr-lucc, --output, --types";
        return 1;
    }
    auto types = parseIntSet(typesStr);
    auto p = make_unique<ExtractPriorityAreas>();
    p->PriorityAreasExtractLUCCLoss(initLUCC, currLUCC, output, types);
    result = "Output: " + output;
    return 0;
}

static int execPriorityTransition(const map<string, string>& params,
                                    string& result, string& error) {
    string initLUCC = getParam(params, "init-lucc");
    string currLUCC = getParam(params, "curr-lucc");
    string output = getParam(params, "output");
    string transStr = getParam(params, "transitions");
    if (initLUCC.empty() || currLUCC.empty() || output.empty() || transStr.empty()) {
        error = "Missing required: --init-lucc, --curr-lucc, --output, --transitions";
        return 1;
    }
    auto trans = parseTransitionMap(transStr);
    auto p = make_unique<ExtractPriorityAreas>();
    p->PriorityAreasExtractLUCCTransition(initLUCC, currLUCC, output, trans);
    result = "Output: " + output;
    return 0;
}

static int execPriorityBuffer(const map<string, string>& params,
                                string& result, string& error) {
    string initLUCC = getParam(params, "init-lucc");
    string bufferPath = getParam(params, "buffer");
    string output = getParam(params, "output");
    if (initLUCC.empty() || bufferPath.empty() || output.empty()) {
        error = "Missing required: --init-lucc, --buffer, --output";
        return 1;
    }
    string typesStr = getParam(params, "types");
    auto types = typesStr.empty() ? unordered_set<int>{} : parseIntSet(typesStr);
    double popTh = getDouble(params, "pop-threshold", 0.0);
    auto p = make_unique<ExtractPriorityAreas>();
    p->PriorityAreasExtractOutsideBufferArea(initLUCC, bufferPath, output, types, popTh);
    result = "Output: " + output;
    return 0;
}

static int execPriorityEmission(const map<string, string>& params,
                                  string& result, string& error) {
    string initLUCC = getParam(params, "init-lucc");
    string currLUCC = getParam(params, "curr-lucc");
    string output = getParam(params, "output");
    string emissionStr = getParam(params, "emission");
    if (initLUCC.empty() || currLUCC.empty() || output.empty() || emissionStr.empty()) {
        error = "Missing required: --init-lucc, --curr-lucc, --output, --emission";
        return 1;
    }
    auto em = parseEmissionMap(emissionStr);
    double ratio = getDouble(params, "ratio", 0.5);
    double radius = getDouble(params, "radius", 3.0);
    auto p = make_unique<ExtractPriorityAreas>();
    p->PriorityAreasExtractEmissionNoPeak(initLUCC, currLUCC, output, em, ratio, radius);
    result = "Output: " + output;
    return 0;
}

static int execPriorityHumanLand(const map<string, string>& params,
                                   string& result, string& error) {
    string initLUCC = getParam(params, "init-lucc");
    string currLUCC = getParam(params, "curr-lucc");
    string initPopu = getParam(params, "init-popu");
    string currPopu = getParam(params, "curr-popu");
    string output = getParam(params, "output");
    string typesStr = getParam(params, "types");
    if (initLUCC.empty() || currLUCC.empty() || initPopu.empty() ||
        currPopu.empty() || output.empty() || typesStr.empty()) {
        error = "Missing required: --init-lucc, --curr-lucc, --init-popu, --curr-popu, --output, --types";
        return 1;
    }
    auto types = parseIntSet(typesStr);
    int radius = getInt(params, "radius", 3);
    auto p = make_unique<ExtractPriorityAreas>();
    p->PriorityAreasExtractHumanLandRelationship(initLUCC, currLUCC, initPopu, currPopu,
                                                   radius, output, types);
    result = "Output: " + output;
    return 0;
}

static int execPriorityMerge(const map<string, string>& params,
                              string& result, string& error) {
    string filesStr = getParam(params, "files");
    string output = getParam(params, "output");
    if (filesStr.empty() || output.empty()) {
        error = "Missing required: --files, --output";
        return 1;
    }
    // Parse file paths (semicolon-separated)
    vector<string> files;
    istringstream iss(filesStr);
    string token;
    while (getline(iss, token, ';')) {
        if (!token.empty()) files.push_back(token);
    }
    auto p = make_unique<ExtractPriorityAreas>();
    p->generatePriorityAreas(files, output);
    result = "Output: " + output;
    return 0;
}

static int execPriorityStats(const map<string, string>& params,
                              string& result, string& error) {
    string ranking = getParam(params, "ranking");
    if (ranking.empty()) {
        error = "Missing required: --ranking";
        return 1;
    }
    auto p = make_unique<ExtractPriorityAreas>();
    int ret = p->priorityAreaStats(ranking);
    if (ret == 0) result = "Stats computed for: " + ranking;
    else error = "Failed to compute stats";
    return ret;
}

// ============================================================================
// registerBuiltinPlugins
// ============================================================================

void registerBuiltinPlugins(PluginManager& mgr) {
    // SDG 2.4.1 — Land Proportion
    mgr.registerBuiltin(
        makeDescriptor("sdg-land-proportion",
                       "Calculate land proportion indicator (SDG 2.4.1)",
                       "sdg-calc",
                       {
                           param("init-lucc", "i", "file", true, "", "Initial land use/cover map (GeoTIFF)"),
                           param("types", "t", "int_list", true, "", "LUCC type codes (e.g., 1,2,3)"),
                           param("max-threshold", "", "float", false, "100.0", "Maximum threshold"),
                           param("min-threshold", "", "float", false, "0.0", "Minimum threshold"),
                       }),
        execLandProportion);

    // SDG 2.4.1 — Land Conversion
    mgr.registerBuiltin(
        makeDescriptor("sdg-land-conversion",
                       "Calculate land conversion indicator (SDG 2.4.1)",
                       "sdg-calc",
                       {
                           param("init-lucc", "i", "file", true, "", "Initial land use/cover map"),
                           param("curr-lucc", "c", "file", true, "", "Current land use/cover map"),
                           param("transitions", "", "string", true, "", "Transition map (e.g., 1>2,3>4)"),
                           param("max-threshold", "", "float", false, "100.0", "Maximum threshold"),
                           param("min-threshold", "", "float", false, "0.0", "Minimum threshold"),
                           param("positive", "", "string", false, "false", "Positive direction (true/false)"),
                       }),
        execLandConversion);

    // SDG 2.4.1 — Buffer Zone
    mgr.registerBuiltin(
        makeDescriptor("sdg-buffer-zone",
                       "Calculate buffer zone indicator (SDG 2.4.1)",
                       "sdg-calc",
                       {
                           param("init-lucc", "i", "file", true, "", "Initial land use/cover map"),
                           param("buffer", "b", "file", true, "", "Buffer zone raster"),
                           param("types", "t", "int_list", true, "", "LUCC type codes"),
                           param("max-threshold", "", "float", false, "100.0", "Maximum threshold"),
                           param("min-threshold", "", "float", false, "0.0", "Minimum threshold"),
                       }),
        execBufferZone);

    // SDG 11.3.1 — Urban Land / Population
    mgr.registerBuiltin(
        makeDescriptor("sdg-1131",
                       "Calculate SDG 11.3.1 indicator (urban land vs population)",
                       "sdg-calc",
                       {
                           param("init-lucc", "i", "file", true, "", "Initial land use/cover map"),
                           param("curr-lucc", "c", "file", true, "", "Current land use/cover map"),
                           param("init-popu", "", "file", true, "", "Initial population raster"),
                           param("curr-popu", "", "file", true, "", "Current population raster"),
                           param("types", "t", "int_list", true, "", "Urban LUCC type codes"),
                           param("max-threshold", "", "float", false, "100.0", "Maximum threshold"),
                           param("min-threshold", "", "float", false, "0.0", "Minimum threshold"),
                           param("best-threshold", "", "float", false, "50.0", "Best threshold"),
                       }),
        execSDG1131);

    // SDG 13.2.2 — Carbon Emission
    mgr.registerBuiltin(
        makeDescriptor("sdg-1322",
                       "Calculate SDG 13.2.2 indicator (carbon emission from land use)",
                       "sdg-calc",
                       {
                           param("init-lucc", "i", "file", true, "", "Initial land use/cover map"),
                           param("curr-lucc", "c", "file", true, "", "Current land use/cover map"),
                           param("emission", "e", "string", true, "", "Emission coefficients (e.g., 1:0.5,2:1.2)"),
                           param("ratio", "r", "float", false, "0.5", "Emission ratio"),
                       }),
        execSDG1322);

    // Priority Area — LUCC Loss
    mgr.registerBuiltin(
        makeDescriptor("priority-loss",
                       "Extract priority areas: LUCC loss (Rule 1)",
                       "priority-area",
                       {
                           param("init-lucc", "i", "file", true, "", "Initial land use/cover map"),
                           param("curr-lucc", "c", "file", true, "", "Current land use/cover map"),
                           param("output", "o", "file", true, "", "Output raster path"),
                           param("types", "t", "int_list", true, "", "LUCC type codes"),
                       }),
        execPriorityLoss);

    // Priority Area — LUCC Transition
    mgr.registerBuiltin(
        makeDescriptor("priority-transition",
                       "Extract priority areas: LUCC transition (Rule 2)",
                       "priority-area",
                       {
                           param("init-lucc", "i", "file", true, "", "Initial land use/cover map"),
                           param("curr-lucc", "c", "file", true, "", "Current land use/cover map"),
                           param("output", "o", "file", true, "", "Output raster path"),
                           param("transitions", "", "string", true, "", "Transition map (e.g., 1>2,3>4)"),
                       }),
        execPriorityTransition);

    // Priority Area — Buffer
    mgr.registerBuiltin(
        makeDescriptor("priority-buffer",
                       "Extract priority areas: outside buffer zone (Rule 3/4)",
                       "priority-area",
                       {
                           param("init-lucc", "i", "file", true, "", "Initial land use/cover map"),
                           param("buffer", "b", "file", true, "", "Buffer zone raster"),
                           param("output", "o", "file", true, "", "Output raster path"),
                           param("types", "t", "int_list", false, "", "LUCC type codes (optional)"),
                           param("pop-threshold", "", "float", false, "0.0", "Population threshold"),
                       }),
        execPriorityBuffer);

    // Priority Area — Emission No Peak
    mgr.registerBuiltin(
        makeDescriptor("priority-emission",
                       "Extract priority areas: emission not yet peaked (Rule 5)",
                       "priority-area",
                       {
                           param("init-lucc", "i", "file", true, "", "Initial land use/cover map"),
                           param("curr-lucc", "c", "file", true, "", "Current land use/cover map"),
                           param("output", "o", "file", true, "", "Output raster path"),
                           param("emission", "e", "string", true, "", "Emission coefficients"),
                           param("ratio", "r", "float", false, "0.5", "Emission ratio"),
                           param("radius", "", "int", false, "3", "Neighborhood radius"),
                       }),
        execPriorityEmission);

    // Priority Area — Human-Land Relationship
    mgr.registerBuiltin(
        makeDescriptor("priority-human-land",
                       "Extract priority areas: human-land relationship (Rule 6)",
                       "priority-area",
                       {
                           param("init-lucc", "i", "file", true, "", "Initial land use/cover map"),
                           param("curr-lucc", "c", "file", true, "", "Current land use/cover map"),
                           param("init-popu", "", "file", true, "", "Initial population raster"),
                           param("curr-popu", "", "file", true, "", "Current population raster"),
                           param("output", "o", "file", true, "", "Output raster path"),
                           param("types", "t", "int_list", true, "", "LUCC type codes"),
                           param("radius", "", "int", false, "3", "Neighborhood radius"),
                       }),
        execPriorityHumanLand);

    // Priority Area — Merge
    mgr.registerBuiltin(
        makeDescriptor("priority-merge",
                       "Merge multiple priority area rasters into ranking map",
                       "priority-area",
                       {
                           param("files", "f", "string", true, "", "Input files (semicolon-separated)"),
                           param("output", "o", "file", true, "", "Output ranking raster path"),
                       }),
        execPriorityMerge);

    // Priority Area — Stats
    mgr.registerBuiltin(
        makeDescriptor("priority-stats",
                       "Compute statistics for priority area ranking map",
                       "priority-area",
                       {
                           param("ranking", "r", "file", true, "", "Ranking raster path"),
                       }),
        execPriorityStats);
}
