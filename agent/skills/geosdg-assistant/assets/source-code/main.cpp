/**
 * @file main.cpp
 * @brief GeoSDG 命令行入口 / GeoSDG CLI entry point
 *
 * 支持子命令调用各计算模块，也可运行完整 demo。
 * Supports sub-command invocation of each calculation module, or full demo run.
 *
 * 用法 / Usage:
 *   GeoSDG <command> [options]
 *   GeoSDG demo [--resume]
 *   GeoSDG ca-precision --ori <path> --sim <path> --real <path>
 *   GeoSDG correlation --data1 <v1,v2,...> --data2 <v1,v2,...>
 *   ...
 */

#include <iostream>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstdlib>

#if !defined(GEOSDG_PLATFORM_WINDOWS)
    #include <cpl_conv.h>   // macOS/Linux: GDAL config (for CPLSetConfigOption)
#endif

#include "CalculateCAPrecision.h"
#include "CalculateSDG.h"
#include "ExtractPriorityAreas.h"
#include "Logger.h"

using namespace std;

// ============================================================================
// 字符串解析工具 / String parsing utilities
// ============================================================================

unordered_set<int> parseIntSet(const string& s)
{
    unordered_set<int> result;
    stringstream ss(s);
    string token;
    while (getline(ss, token, ',')) result.insert(stoi(token));
    return result;
}

vector<double> parseDoubleVector(const string& s)
{
    vector<double> result;
    stringstream ss(s);
    string token;
    while (getline(ss, token, ',')) result.push_back(stod(token));
    return result;
}

unordered_map<int, vector<int>> parseTransitionMap(const string& s)
{
    unordered_map<int, vector<int>> result;
    stringstream ss(s);
    string token;
    while (getline(ss, token, ',')) {
        auto p = token.find(':');
        if (p == string::npos) continue;
        int src = stoi(token.substr(0, p));
        string rest = token.substr(p + 1);
        stringstream ts(rest);
        string t;
        while (getline(ts, t, ':')) result[src].push_back(stoi(t));
    }
    return result;
}

unordered_map<int, double> parseEmissionMap(const string& s)
{
    unordered_map<int, double> result;
    stringstream ss(s);
    string token;
    while (getline(ss, token, ',')) {
        auto p = token.find(':');
        if (p == string::npos) continue;
        result[stoi(token.substr(0, p))] = stod(token.substr(p + 1));
    }
    return result;
}

vector<string> parseFilePaths(const string& s)
{
    vector<string> result;
    stringstream ss(s);
    string token;
    while (getline(ss, token, ',')) result.push_back(token);
    return result;
}

// ============================================================================
// 子命令枚举 / Sub-command enumeration
// ============================================================================

enum class Command
{
    NONE, DEMO, CA_PRECISION, CORRELATION, T_TEST,
    SDG_LAND_PROPORTION, SDG_LAND_CONVERSION, SDG_BUFFER_ZONE,
    SDG_1131, SDG_1322,
    PRIORITY_LOSS, PRIORITY_TRANSITION, PRIORITY_BUFFER,
    PRIORITY_EMISSION, PRIORITY_HUMAN_LAND, PRIORITY_MERGE,
    HELP
};

// ============================================================================
// 命令行参数结构 / CLI arguments structure
// ============================================================================

struct CliArgs
{
    Command  command    = Command::NONE;
    string   logPath    = "logs/geosdg.log";
    bool     resume     = false;

    string   oriPath;           // --ori
    string   simPath;           // --sim
    string   realPath;          // --real
    string   data1Str;          // --data1
    string   data2Str;          // --data2
    string   initLUCC;          // --init-lucc
    string   currLUCC;          // --curr-lucc
    string   initPopu;          // --init-popu
    string   currPopu;          // --curr-popu
    string   bufferPath;        // --buffer
    string   typesStr;          // --types
    string   transStr;          // --transitions
    string   emissionStr;       // --emission
    double   maxThreshold = 100.0;
    double   minThreshold = 0.0;
    double   bestThreshold = 1.12;
    double   ratio         = 0.012;
    int      radius        = 3;
    double   popThreshold  = 1000.0;
    bool     positive      = true;
    string   outputPath;       // -o / --output
    string   filesStr;         // --files
};

// ============================================================================
// 帮助信息 / Help message
// ============================================================================

void printUsage(const string& prog)
{
    cout <<
R"(GeoSDG - Geospatial SDG Indicator Calculation Toolkit

Usage:
  )" << prog << R"( <command> [options]

Commands:
  demo                   Run full demo with sample data

  ca-precision           CA simulation precision (FoM, PA, UA, Kappa, OA)
  correlation            Pearson correlation coefficient
  t-test                 Independent samples t-test

  sdg-land-proportion    SDG land proportion indicator
  sdg-land-conversion    SDG land conversion indicator
  sdg-buffer-zone        SDG buffer zone indicator
  sdg-1131               SDG 11.3.1 Urban/population growth ratio
  sdg-1322               SDG 13.2.2 Carbon emission peak

  priority-loss          Priority areas Rule 1: land encroached
  priority-transition    Priority areas Rule 2: specific transitions
  priority-buffer        Priority areas Rule 3/4: outside infrastructure
  priority-emission      Priority areas Rule 5: emission not peaked
  priority-human-land    Priority areas Rule 6: human-land imbalance
  priority-merge         Merge priority areas into ranking map

  help                   Show this help message

Global options:
  --log <path>           Log file path (default: logs/geosdg.log)

Command-specific options:
  demo:
    --resume             Resume from last checkpoint

  ca-precision:
    --ori <path>         Original LUCC (e.g. 2010.tif)
    --sim <path>         Simulated LUCC (e.g. Simulation.tif)
    --real <path>        Real LUCC (e.g. 2020.tif)

  correlation / t-test:
    --data1 <v1,v2,...>  First sample (comma-separated doubles)
    --data2 <v1,v2,...>  Second sample (comma-separated doubles)

  sdg-land-proportion:
    --init-lucc <path>   LUCC data path
    --types <1,2,...>    Land type codes to observe
    --max <value>        Max threshold (default: 100)
    --min <value>        Min threshold (default: 0)

  sdg-land-conversion:
    --init-lucc <path>   Initial LUCC
    --curr-lucc <path>   Changed LUCC
    --transitions <s:t1:t2,...>  Transition rules (e.g. 2:5:6,4:5)
    --max <value>        Max threshold (default: 100)
    --min <value>        Min threshold (default: 0)
    --positive           Positive indicator (default)
    --negative           Negative indicator

  sdg-buffer-zone:
    --init-lucc <path>   LUCC or population data path
    --buffer <path>      Buffer zone data path
    --types <1,2,...>    Land type codes (for LUCC data)
    --max <value>        Max threshold (default: 100)
    --min <value>        Min threshold (default: 0)

  sdg-1131:
    --init-lucc <path>   Initial LUCC
    --curr-lucc <path>   Current LUCC
    --init-popu <path>   Initial population
    --curr-popu <path>   Current population
    --types <1,2,...>    Urban land type codes
    --max <value>        Max threshold (default: 3.0)
    --min <value>        Min threshold (default: 0)
    --best <value>       Best threshold (default: 1.12)

  sdg-1322:
    --init-lucc <path>   Initial LUCC
    --curr-lucc <path>   Changed LUCC
    --emission <type:factor,...>  Emission factors (e.g. 1:-1,2:-20,5:5)
    --ratio <value>      Emission reduction ratio (default: 0.012)

  priority-loss:
    --init-lucc <path>   Initial LUCC
    --curr-lucc <path>   Changed LUCC
    -o <path>            Output path
    --types <1,2,...>    Encroached land type codes

  priority-transition:
    --init-lucc <path>   Initial LUCC
    --curr-lucc <path>   Changed LUCC
    -o <path>            Output path
    --transitions <s:t1:t2,...>  Transition rules

  priority-buffer:
    --init-lucc <path>   LUCC or population data
    --buffer <path>      Infrastructure coverage data
    -o <path>            Output path
    --types <1,2,...>    Land type codes (Rule 3)
    --pop-threshold <v>  Population threshold (Rule 4, default: 1000)

  priority-emission:
    --init-lucc <path>   Initial LUCC
    --curr-lucc <path>   Changed LUCC
    -o <path>            Output path
    --emission <type:factor,...>  Emission factors
    --ratio <value>      Reduction ratio (default: 0.012)
    --radius <value>     Neighborhood radius in pixels (default: 5)

  priority-human-land:
    --init-lucc <path>   Initial LUCC
    --curr-lucc <path>   Current LUCC
    --init-popu <path>   Initial population
    --curr-popu <path>   Current population
    -o <path>            Output path
    --types <1,2,...>    Urban land type codes
    --radius <value>     Neighborhood radius (default: 3)

  priority-merge:
    --files <p1,p2,...>  Priority area file paths (comma-separated)
    -o <path>            Output ranking map path
)" << endl;
}

// ============================================================================
// 命令行解析 / CLI argument parsing
// ============================================================================

Command parseCommand(const string& cmd)
{
    if (cmd == "demo")                  return Command::DEMO;
    if (cmd == "ca-precision")          return Command::CA_PRECISION;
    if (cmd == "correlation")           return Command::CORRELATION;
    if (cmd == "t-test")                return Command::T_TEST;
    if (cmd == "sdg-land-proportion")    return Command::SDG_LAND_PROPORTION;
    if (cmd == "sdg-land-conversion")    return Command::SDG_LAND_CONVERSION;
    if (cmd == "sdg-buffer-zone")        return Command::SDG_BUFFER_ZONE;
    if (cmd == "sdg-1131")              return Command::SDG_1131;
    if (cmd == "sdg-1322")              return Command::SDG_1322;
    if (cmd == "priority-loss")         return Command::PRIORITY_LOSS;
    if (cmd == "priority-transition")   return Command::PRIORITY_TRANSITION;
    if (cmd == "priority-buffer")       return Command::PRIORITY_BUFFER;
    if (cmd == "priority-emission")     return Command::PRIORITY_EMISSION;
    if (cmd == "priority-human-land")   return Command::PRIORITY_HUMAN_LAND;
    if (cmd == "priority-merge")        return Command::PRIORITY_MERGE;
    if (cmd == "help" || cmd == "--help" || cmd == "-h") return Command::HELP;
    return Command::NONE;
}

CliArgs parseArgs(int argc, char* argv[])
{
    CliArgs args;
    if (argc < 2) { args.command = Command::HELP; return args; }

    args.command = parseCommand(argv[1]);
    if (args.command == Command::NONE) {
        cerr << "Unknown command: " << argv[1] << endl;
        args.command = Command::HELP;
        return args;
    }

    for (int i = 2; i < argc; ++i) {
        string arg = argv[i];
        if      (arg == "--log"           && i+1 < argc) args.logPath      = argv[++i];
        else if (arg == "--resume")                        args.resume      = true;
        else if (arg == "--ori"           && i+1 < argc) args.oriPath     = argv[++i];
        else if (arg == "--sim"           && i+1 < argc) args.simPath     = argv[++i];
        else if (arg == "--real"          && i+1 < argc) args.realPath    = argv[++i];
        else if (arg == "--data1"         && i+1 < argc) args.data1Str    = argv[++i];
        else if (arg == "--data2"         && i+1 < argc) args.data2Str    = argv[++i];
        else if (arg == "--init-lucc"     && i+1 < argc) args.initLUCC    = argv[++i];
        else if (arg == "--curr-lucc"     && i+1 < argc) args.currLUCC    = argv[++i];
        else if (arg == "--init-popu"     && i+1 < argc) args.initPopu    = argv[++i];
        else if (arg == "--curr-popu"     && i+1 < argc) args.currPopu    = argv[++i];
        else if (arg == "--buffer"        && i+1 < argc) args.bufferPath  = argv[++i];
        else if (arg == "--types"         && i+1 < argc) args.typesStr    = argv[++i];
        else if (arg == "--transitions"   && i+1 < argc) args.transStr    = argv[++i];
        else if (arg == "--emission"      && i+1 < argc) args.emissionStr = argv[++i];
        else if (arg == "--max"           && i+1 < argc) args.maxThreshold = stod(argv[++i]);
        else if (arg == "--min"           && i+1 < argc) args.minThreshold = stod(argv[++i]);
        else if (arg == "--best"          && i+1 < argc) args.bestThreshold = stod(argv[++i]);
        else if (arg == "--ratio"         && i+1 < argc) args.ratio       = stod(argv[++i]);
        else if (arg == "--radius"        && i+1 < argc) args.radius      = stoi(argv[++i]);
        else if (arg == "--pop-threshold" && i+1 < argc) args.popThreshold = stod(argv[++i]);
        else if (arg == "--positive")                      args.positive   = true;
        else if (arg == "--negative")                      args.positive   = false;
        else if ((arg == "-o" || arg == "--output") && i+1 < argc) args.outputPath = argv[++i];
        else if (arg == "--files"         && i+1 < argc) args.filesStr    = argv[++i];
        else cerr << "Unknown option: " << arg << endl;
    }
    return args;
}

// ============================================================================
// 子命令实现 / Sub-command implementations
// ============================================================================

int cmdDemo(bool resume)
{
    const int TOTAL_STEPS = 8;

    int startStep = 0;
    if (resume) {
        auto cp = Logger::instance().readLastCheckpoint();
        if (cp.valid) {
            startStep = cp.step;
            LOG_INFO("Resuming from checkpoint: step " + to_string(cp.step) + "/" + to_string(cp.total));
        } else {
            LOG_WARN("No valid checkpoint found, starting from beginning");
        }
    }

    if (startStep < 1) {
        LOG_PROGRESS(1, TOTAL_STEPS, "CA Simulation Precision Assessment");
        auto p = make_unique<CalculateCAPrecision>();
        double FoM, PA, UA, Kappa, OA;
        p->calculatePrecision("../data/LUCC/2010.tif", "../data/LUCC/2020.tif",
                              "../data/LUCC/Simulation.tif", FoM, PA, UA, Kappa, OA);
        LOG_RESULT("calculatePrecision", "FoM", FoM);
        LOG_RESULT("calculatePrecision", "PA", PA);
        LOG_RESULT("calculatePrecision", "UA", UA);
        LOG_RESULT("calculatePrecision", "Kappa", Kappa);
        LOG_RESULT("calculatePrecision", "OA", OA);
        LOG_CHECKPOINT(1, TOTAL_STEPS, "CA precision done");
    }

    if (startStep < 2) {
        LOG_PROGRESS(2, TOTAL_STEPS, "Pearson Correlation & t-test");
        auto p = make_unique<CalculateCAPrecision>();
        vector<double> d1 = {34.7,34.4,51.8,51.1,44.1,42.6,51.7,51.5,48.1,47.4};
        vector<double> d2 = {45.2,45.5,47.6,52.7,48.7,52.5,48.3,46.4,47.4,49.2};
        double R = p->calculateCorrelationCoefficient(d1, d2);
        LOG_RESULT("correlation", "R", R);
        double t = p->tTestIndependent(d1, d2);
        LOG_RESULT("t-test", "t", t);
        LOG_CHECKPOINT(2, TOTAL_STEPS, "Pearson & t-test done");
    }

    if (startStep < 3) {
        LOG_PROGRESS(3, TOTAL_STEPS, "Land Proportion Indicator");
        auto p = make_unique<CalculateSDG>();
        unordered_set<int> tp{2, 3};
        double s1 = p->calculateLandProportionIndicator("../data/LUCC/2025.tif", 100.0, 0.0, tp);
        LOG_RESULT("sdg-land-proportion_2025", "score", s1);
        double s2 = p->calculateLandProportionIndicator("../data/LUCC/2050.tif", 100.0, 0.0, tp);
        LOG_RESULT("sdg-land-proportion_2050", "score", s2);
        LOG_CHECKPOINT(3, TOTAL_STEPS, "Land proportion done");
    }

    if (startStep < 4) {
        LOG_PROGRESS(4, TOTAL_STEPS, "Land Conversion Indicator");
        auto p = make_unique<CalculateSDG>();
        unordered_map<int, vector<int>> tr;
        tr[2] = {2, 5, 6}; tr[4] = {5};
        double sp = p->calculateLandConversionIndicator("../data/LUCC/2025.tif", "../data/LUCC/2050.tif", tr, 100.0, 0.0, true);
        LOG_RESULT("sdg-land-conversion_pos", "score", sp);
        double sn = p->calculateLandConversionIndicator("../data/LUCC/2025.tif", "../data/LUCC/2050.tif", tr, 100.0, 0.0, false);
        LOG_RESULT("sdg-land-conversion_neg", "score", sn);
        LOG_CHECKPOINT(4, TOTAL_STEPS, "Land conversion done");
    }

    if (startStep < 5) {
        LOG_PROGRESS(5, TOTAL_STEPS, "Buffer Zone Indicator");
        auto p = make_unique<CalculateSDG>();
        unordered_set<int> tp{2, 3};
        double sl = p->calculateBufferZoneIndicator("../data/LUCC/2025.tif", "../data/INFRA/roads.tif", tp, 100.0, 0.0);
        LOG_RESULT("sdg-buffer-zone_LUCC", "score", sl);
        double sp = p->calculateBufferZoneIndicator("../data/POPU/2025.tif", "../data/INFRA/roads.tif", tp, 100.0, 0.0);
        LOG_RESULT("sdg-buffer-zone_POPU", "score", sp);
        LOG_CHECKPOINT(5, TOTAL_STEPS, "Buffer zone done");
    }

    if (startStep < 6) {
        LOG_PROGRESS(6, TOTAL_STEPS, "SDG 11.3.1 & 13.2.2");
        auto p = make_unique<CalculateSDG>();
        unordered_set<int> tp{2, 3};
        double s1 = p->calculateSDG1131Indicator("../data/LUCC/2025.tif", "../data/LUCC/2050.tif",
                     "../data/POPU/2025.tif", "../data/POPU/2050.tif", tp, 3.0, 0.0, 1.12);
        LOG_RESULT("sdg-1131", "score", s1);
        unordered_map<int, double> em{{1,-1},{2,-20},{3,-5},{4,-0.5},{5,5},{6,0.3}};
        double s2 = p->calculateSDG1322Indicator("../data/LUCC/2025.tif", "../data/LUCC/2050.tif", em, 0.012);
        LOG_RESULT("sdg-1322", "score", s2);
        LOG_CHECKPOINT(6, TOTAL_STEPS, "SDG 11.3.1 & 13.2.2 done");
    }

    if (startStep < 7) {
        LOG_PROGRESS(7, TOTAL_STEPS, "Priority Areas Rule 1-4");
        auto p = make_unique<ExtractPriorityAreas>();
        unordered_set<int> lt{2, 3};
        p->PriorityAreasExtractLUCCLoss("../data/LUCC/2025.tif", "../data/LUCC/2050.tif", "../data/PriorityAreas-1.tif", lt);
        LOG_INFO("Rule 1 done");
        unordered_map<int, vector<int>> tm; tm[2]={5,6}; tm[4]={5};
        p->PriorityAreasExtractLUCCTransition("../data/LUCC/2025.tif", "../data/LUCC/2050.tif", "../data/PriorityAreas-2.tif", tm);
        LOG_INFO("Rule 2 done");
        unordered_set<int> ct{5};
        p->PriorityAreasExtractOutsideBufferArea("../data/LUCC/2025.tif", "../data/INFRA/roads.tif", "../data/PriorityAreas-3.tif", ct, 1000);
        LOG_INFO("Rule 3 done");
        p->PriorityAreasExtractOutsideBufferArea("../data/POPU/2025.tif", "../data/INFRA/roads.tif", "../data/PriorityAreas-4.tif", ct, 1000);
        LOG_INFO("Rule 4 done");
        LOG_CHECKPOINT(7, TOTAL_STEPS, "Priority areas 1-4 done");
    }

    if (startStep < 8) {
        LOG_PROGRESS(8, TOTAL_STEPS, "Priority Areas Rule 5-6 & Merge");
        auto p = make_unique<ExtractPriorityAreas>();
        unordered_map<int, double> em{{1,-1},{2,-20},{3,-5},{4,-0.5},{5,5},{6,0.3}};
        p->PriorityAreasExtractEmissionNoPeak("../data/LUCC/2025.tif", "../data/LUCC/2050.tif", "../data/PriorityAreas-5.tif", em, 0.012, 5);
        LOG_INFO("Rule 5 done");
        unordered_set<int> ct{5};
        p->PriorityAreasExtractHumanLandRelationship("../data/LUCC/2025.tif", "../data/LUCC/2050.tif",
            "../data/POPU/2025.tif", "../data/POPU/2050.tif", 3, "../data/PriorityAreas-6.tif", ct);
        LOG_INFO("Rule 6 done");
        vector<string> files = {"../data/PriorityAreas-1.tif","../data/PriorityAreas-2.tif",
            "../data/PriorityAreas-3.tif","../data/PriorityAreas-4.tif",
            "../data/PriorityAreas-5.tif","../data/PriorityAreas-6.tif"};
        p->generatePriorityAreas(files, "../data/PriorityAreasRankingMap.tif");
        LOG_INFO("Merge done");
        LOG_CHECKPOINT(8, TOTAL_STEPS, "All steps completed");
    }

    LOG_INFO("Demo completed successfully!");
    return 0;
}

int cmdCAPrecision(const CliArgs& a)
{
    if (a.oriPath.empty() || a.simPath.empty() || a.realPath.empty()) {
        cerr << "Error: --ori, --sim, --real required" << endl; return 1;
    }
    auto p = make_unique<CalculateCAPrecision>();
    double FoM, PA, UA, Kappa, OA;
    p->calculatePrecision(a.oriPath, a.simPath, a.realPath, FoM, PA, UA, Kappa, OA);
    LOG_RESULT("ca-precision", "FoM", FoM);
    LOG_RESULT("ca-precision", "PA", PA);
    LOG_RESULT("ca-precision", "UA", UA);
    LOG_RESULT("ca-precision", "Kappa", Kappa);
    LOG_RESULT("ca-precision", "OA", OA);
    cout << fixed << setprecision(6);
    cout << "FoM=" << FoM << " PA=" << PA << " UA=" << UA << " Kappa=" << Kappa << " OA=" << OA << endl;
    return 0;
}

int cmdCorrelation(const CliArgs& a)
{
    if (a.data1Str.empty() || a.data2Str.empty()) {
        cerr << "Error: --data1 and --data2 required" << endl; return 1;
    }
    auto d1 = parseDoubleVector(a.data1Str);
    auto d2 = parseDoubleVector(a.data2Str);
    auto p = make_unique<CalculateCAPrecision>();
    double R = p->calculateCorrelationCoefficient(d1, d2);
    LOG_RESULT("correlation", "R", R);
    cout << fixed << setprecision(6) << "R=" << R << endl;
    return 0;
}

int cmdTTest(const CliArgs& a)
{
    if (a.data1Str.empty() || a.data2Str.empty()) {
        cerr << "Error: --data1 and --data2 required" << endl; return 1;
    }
    auto d1 = parseDoubleVector(a.data1Str);
    auto d2 = parseDoubleVector(a.data2Str);
    auto p = make_unique<CalculateCAPrecision>();
    double t = p->tTestIndependent(d1, d2);
    LOG_RESULT("t-test", "t", t);
    cout << fixed << setprecision(6) << "t=" << t << endl;
    return 0;
}

int cmdSDGLandProportion(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.typesStr.empty()) {
        cerr << "Error: --init-lucc and --types required" << endl; return 1;
    }
    auto types = parseIntSet(a.typesStr);
    auto p = make_unique<CalculateSDG>();
    double score = p->calculateLandProportionIndicator(a.initLUCC, a.maxThreshold, a.minThreshold, types);
    LOG_RESULT("sdg-land-proportion", "score", score);
    cout << fixed << setprecision(4) << "score=" << score << endl;
    return 0;
}

int cmdSDGLandConversion(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.currLUCC.empty() || a.transStr.empty()) {
        cerr << "Error: --init-lucc, --curr-lucc, --transitions required" << endl; return 1;
    }
    auto trans = parseTransitionMap(a.transStr);
    auto p = make_unique<CalculateSDG>();
    double score = p->calculateLandConversionIndicator(a.initLUCC, a.currLUCC, trans,
                     a.maxThreshold, a.minThreshold, a.positive);
    string dir = a.positive ? "positive" : "negative";
    LOG_RESULT("sdg-land-conversion", dir + " score", score);
    cout << fixed << setprecision(4) << "score(" << dir << ")=" << score << endl;
    return 0;
}

int cmdSDGBufferZone(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.bufferPath.empty() || a.typesStr.empty()) {
        cerr << "Error: --init-lucc, --buffer, --types required" << endl; return 1;
    }
    auto types = parseIntSet(a.typesStr);
    auto p = make_unique<CalculateSDG>();
    double score = p->calculateBufferZoneIndicator(a.initLUCC, a.bufferPath, types, a.maxThreshold, a.minThreshold);
    LOG_RESULT("sdg-buffer-zone", "score", score);
    cout << fixed << setprecision(4) << "score=" << score << endl;
    return 0;
}

int cmdSDG1131(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.currLUCC.empty() || a.initPopu.empty() || a.currPopu.empty() || a.typesStr.empty()) {
        cerr << "Error: --init-lucc, --curr-lucc, --init-popu, --curr-popu, --types required" << endl; return 1;
    }
    auto types = parseIntSet(a.typesStr);
    auto p = make_unique<CalculateSDG>();
    double score = p->calculateSDG1131Indicator(a.initLUCC, a.currLUCC, a.initPopu, a.currPopu,
                     types, a.maxThreshold, a.minThreshold, a.bestThreshold);
    LOG_RESULT("sdg-1131", "score", score);
    cout << fixed << setprecision(4) << "score=" << score << endl;
    return 0;
}

int cmdSDG1322(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.currLUCC.empty() || a.emissionStr.empty()) {
        cerr << "Error: --init-lucc, --curr-lucc, --emission required" << endl; return 1;
    }
    auto em = parseEmissionMap(a.emissionStr);
    auto p = make_unique<CalculateSDG>();
    double score = p->calculateSDG1322Indicator(a.initLUCC, a.currLUCC, em, a.ratio);
    LOG_RESULT("sdg-1322", "score", score);
    cout << fixed << setprecision(4) << "score=" << score << endl;
    return 0;
}

int cmdPriorityLoss(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.currLUCC.empty() || a.outputPath.empty() || a.typesStr.empty()) {
        cerr << "Error: --init-lucc, --curr-lucc, -o, --types required" << endl; return 1;
    }
    auto types = parseIntSet(a.typesStr);
    auto p = make_unique<ExtractPriorityAreas>();
    p->PriorityAreasExtractLUCCLoss(a.initLUCC, a.currLUCC, a.outputPath, types);
    LOG_INFO("Priority area Rule 1 output: " + a.outputPath);
    cout << "Output: " << a.outputPath << endl;
    return 0;
}

int cmdPriorityTransition(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.currLUCC.empty() || a.outputPath.empty() || a.transStr.empty()) {
        cerr << "Error: --init-lucc, --curr-lucc, -o, --transitions required" << endl; return 1;
    }
    auto trans = parseTransitionMap(a.transStr);
    auto p = make_unique<ExtractPriorityAreas>();
    p->PriorityAreasExtractLUCCTransition(a.initLUCC, a.currLUCC, a.outputPath, trans);
    LOG_INFO("Priority area Rule 2 output: " + a.outputPath);
    cout << "Output: " << a.outputPath << endl;
    return 0;
}

int cmdPriorityBuffer(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.bufferPath.empty() || a.outputPath.empty()) {
        cerr << "Error: --init-lucc, --buffer, -o required" << endl; return 1;
    }
    auto types = a.typesStr.empty() ? unordered_set<int>{} : parseIntSet(a.typesStr);
    auto p = make_unique<ExtractPriorityAreas>();
    p->PriorityAreasExtractOutsideBufferArea(a.initLUCC, a.bufferPath, a.outputPath, types, a.popThreshold);
    LOG_INFO("Priority area Rule 3/4 output: " + a.outputPath);
    cout << "Output: " << a.outputPath << endl;
    return 0;
}

int cmdPriorityEmission(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.currLUCC.empty() || a.outputPath.empty() || a.emissionStr.empty()) {
        cerr << "Error: --init-lucc, --curr-lucc, -o, --emission required" << endl; return 1;
    }
    auto em = parseEmissionMap(a.emissionStr);
    auto p = make_unique<ExtractPriorityAreas>();
    p->PriorityAreasExtractEmissionNoPeak(a.initLUCC, a.currLUCC, a.outputPath, em, a.ratio, a.radius);
    LOG_INFO("Priority area Rule 5 output: " + a.outputPath);
    cout << "Output: " << a.outputPath << endl;
    return 0;
}

int cmdPriorityHumanLand(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.currLUCC.empty() || a.initPopu.empty() || a.currPopu.empty() || a.outputPath.empty() || a.typesStr.empty()) {
        cerr << "Error: --init-lucc, --curr-lucc, --init-popu, --curr-popu, -o, --types required" << endl; return 1;
    }
    auto types = parseIntSet(a.typesStr);
    auto p = make_unique<ExtractPriorityAreas>();
    p->PriorityAreasExtractHumanLandRelationship(a.initLUCC, a.currLUCC, a.initPopu, a.currPopu,
               a.radius, a.outputPath, types);
    LOG_INFO("Priority area Rule 6 output: " + a.outputPath);
    cout << "Output: " << a.outputPath << endl;
    return 0;
}

int cmdPriorityMerge(const CliArgs& a)
{
    if (a.filesStr.empty() || a.outputPath.empty()) {
        cerr << "Error: --files and -o required" << endl; return 1;
    }
    auto files = parseFilePaths(a.filesStr);
    auto p = make_unique<ExtractPriorityAreas>();
    p->generatePriorityAreas(files, a.outputPath);
    LOG_INFO("Priority areas ranking map: " + a.outputPath);
    cout << "Output: " << a.outputPath << endl;
    return 0;
}

// ============================================================================
// 主函数 / Main entry point
// ============================================================================

int main(int argc, char* argv[])
{
    // 设置 GDAL_DATA 环境变量
#if !defined(GEOSDG_PLATFORM_WINDOWS) && defined(GEOSDG_GDAL_DATA_DIR)
    if (!getenv("GDAL_DATA") || strlen(getenv("GDAL_DATA")) == 0)
        CPLSetConfigOption("GDAL_DATA", GEOSDG_GDAL_DATA_DIR);
#endif

    CliArgs args = parseArgs(argc, argv);

    if (args.command == Command::HELP) {
        printUsage(argv[0]);
        return 0;
    }

    Logger::instance().init(args.logPath);

    switch (args.command) {
        case Command::DEMO:                return cmdDemo(args.resume);
        case Command::CA_PRECISION:        return cmdCAPrecision(args);
        case Command::CORRELATION:         return cmdCorrelation(args);
        case Command::T_TEST:              return cmdTTest(args);
        case Command::SDG_LAND_PROPORTION:  return cmdSDGLandProportion(args);
        case Command::SDG_LAND_CONVERSION:  return cmdSDGLandConversion(args);
        case Command::SDG_BUFFER_ZONE:      return cmdSDGBufferZone(args);
        case Command::SDG_1131:            return cmdSDG1131(args);
        case Command::SDG_1322:            return cmdSDG1322(args);
        case Command::PRIORITY_LOSS:       return cmdPriorityLoss(args);
        case Command::PRIORITY_TRANSITION: return cmdPriorityTransition(args);
        case Command::PRIORITY_BUFFER:     return cmdPriorityBuffer(args);
        case Command::PRIORITY_EMISSION:   return cmdPriorityEmission(args);
        case Command::PRIORITY_HUMAN_LAND: return cmdPriorityHumanLand(args);
        case Command::PRIORITY_MERGE:      return cmdPriorityMerge(args);
        default:                           printUsage(argv[0]); return 0;
    }
}
