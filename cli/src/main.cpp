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
#include <filesystem>

#if !defined(GEOSDG_PLATFORM_WINDOWS)
    #include <cpl_conv.h>   // macOS/Linux: GDAL config (for CPLSetConfigOption)
    #include <unistd.h>     // fork, execvp, setenv, _exit
    #include <sys/wait.h>   // waitpid
#endif

#include "CalculateCAPrecision.h"
#include "CalculateSDG.h"
#include "ExtractPriorityAreas.h"
#include "GeoTiffInspector.h"
#include "InfraSimulator.h"
#include "Logger.h"
#include "PgEstimator.h"
#include "MarkovPredictor.h"
#include "CASimulator.h"
#include "RasterPreprocessor.h"
#include "PluginManager.h"
#include "BuiltinPlugins.h"

using namespace std;
namespace fs = std::filesystem;

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

vector<int> parseIntVector(const string& s)
{
    vector<int> result;
    stringstream ss(s);
    string token;
    while (getline(ss, token, ',')) result.push_back(stoi(token));
    return result;
}

// ============================================================================
// 子命令枚举 / Sub-command enumeration
// ============================================================================

enum class Command
{
    NONE, DEMO, CA_PG, CA_MARKOV, CA_PRECISION, CA_SIMULATE, SDG_PREDICT, SDG_INFRA_SIMULATE,
    CORRELATION, T_TEST,
    SDG_LAND_PROPORTION, SDG_LAND_CONVERSION, SDG_BUFFER_ZONE,
    SDG_1131, SDG_1322,
    PRIORITY_LOSS, PRIORITY_TRANSITION, PRIORITY_BUFFER,
    PRIORITY_EMISSION, PRIORITY_HUMAN_LAND, PRIORITY_MERGE,
    PRIORITY_STATS,
    CHECK,
    RESAMPLE, NORMALIZE, RECLASS, DETECT_CHANGE, COMPRESS,
    PIPELINE,
    LIST_PLUGINS, PLUGIN_INFO, PLUGIN_RUN,
    VERSION, HELP
};

// ============================================================================
// 版本号 / Version string
// ============================================================================

constexpr const char* GEOSDG_VERSION = "1.0";

// ============================================================================
// 命令行参数结构 / CLI arguments structure
// ============================================================================

struct CliArgs
{
    Command  command    = Command::NONE;
    string   logPath    = "logs/geosdg.log";
    bool     resume     = false;
    string   selfPath;          // argv[0] — executable path for locating plugins

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
    string   rankingPath;      // --ranking  排名图 GeoTIFF 路径

    // ── CA simulation parameters ──
    string   trainLUCC;        // --train-lucc (ca-pg)
    string   driversStr;       // --drivers (ca-pg)
    int      caMethod     = 0; // --method: 0=rf, 1=ann, 2=logit
    int      rfTrees      = 60;// --trees
    double   selectRate   = 0.6; // --select-rate
    int      nMaxPerType  = 5000; // --max-per-type (ca-pg)
    int      nMaxNegPerType = 5000; // --max-neg-per-type (ca-pg)
    bool     bUseOpenMP   = true; // --openmp (ca-pg)
    string   modelPath;          // --model (ca-pg)
    string   specialShp;       // --special (ca-pg)
    string   validatePath;     // --validate (ca-pg, ca-simulate)
    string   luccsStr;         // --luccs (ca-markov)
    string   yearsStr;         // --years (ca-markov)
    int      targetYear   = 0; // --target-year (ca-markov, ca-simulate)
    int      step         = 10;// --step (ca-markov)
    string   pgPath;           // --pg (ca-simulate)
    string   demandPath;       // --demand (ca-simulate)
    int      iterations   = 10;// --iterations (ca-simulate)
    double   convergence = 0.001; // --convergence (ca-simulate)
    int      neighborRadius = 3;  // --neighbor (ca-simulate)
    string   weightsStr;       // --weights (ca-simulate)
    double   decay        = 0.92; // --decay (ca-simulate)
    int      stepSize     = 400;  // --step-size (ca-simulate)
    string   redlinePath;      // --redline (ca-simulate)
    string   dkStr;            // --dk (ca-simulate)
    string   muStr;            // --mu (ca-simulate)
    bool     savePrecision = false; // --save-precision (ca-simulate)
    // Multi-scenario parameters
    string   scenariosStr;     // --scenarios (ca-simulate --multi)
    bool     multiScenario = false; // --multi-scenario (ca-simulate)
    // Infra simulation parameters
    string   infraZonePath;    // --infra-zone (sdg-infra-simulate)
    // check 命令参数 / check command parameters
    string   checkFile;        // positional: 待检查文件路径
    string   refPath;          // --ref      参考文件路径
    string   checkTypesStr;    // --types    期望地类编码（check 命令）
    int      expectedCategory  = 0;  // --category 期望地类总类别数
    bool     expectInt         = false; // --expect-int 期望整型数据

    // ── 预处理参数 / Preprocessing parameters ──
    string   basePath;          // --base (resample)
    string   inputsStr;         // --inputs (resample, normalize)
    string   outputDir;         // --output-dir (resample, normalize)
    string   methodStr;         // --method (resample: nearest|bilinear; compress: deflate|...)
    string   rangeStr;          // --range (normalize: min:max)
    double   manualMinVal       = 0.0;  // --min-val (normalize)
    double   manualMaxVal       = 0.0;  // --max-val (normalize)
    bool     hasManualMin       = false;
    bool     hasManualMax       = false;
    string   rulesPath;         // --rules (reclass: JSON)
    string   remapStr;          // --remap (reclass: k1:v1,k2:v2)
    string   setNodataStr;      // --set-nodata (reclass: v1,v2)
    int      nodataValue        = -9999; // --nodata-value (reclass)
    string   beforePath;        // --before (detect-change)
    string   afterPath;         // --after (detect-change)
    bool     encodeMode         = false; // --encode (detect-change)
    // compress 专用 / compress specific
    int      compressLevel      = 6;    // --level
    int      predictor          = 2;    // --predictor
    double   maxError           = 0.001; // --max-error
    bool     tiled              = false; // --tiled
    int      blockSize          = 256;  // --block-size
    string   bigtiffMode        = "if_needed"; // --bigtiff
    bool     genOverview        = false; // --overview

    // ── Pipeline 参数 / Pipeline parameters ──
    string   pipelineConfig;    // positional: pipeline JSON config path
    string   pipelineSubCmd;    // positional: pipeline sub-command (run/batch/...)
    string   pipelineVar;       // --var KEY=VALUE (pipeline)
    string   pipelineOutput;    // --output (pipeline result JSON)
    int      pipelineWorkers    = 4;    // --workers (pipeline)
    bool     pipelineResume     = false; // --resume (pipeline)
    bool     pipelineDryRun     = false; // --dry-run (pipeline)
    bool     pipelineFailFast   = false; // --fail-fast (pipeline batch)
    bool     pipelineLenient    = false; // --lenient (pipeline validate)
    string   pipelineWorkDir;   // --work-dir (pipeline)
    string   pipelineCliPath;   // --cli-path (pipeline, override self path)

    // ── Plugin 参数 / Plugin parameters ──
    string   pluginName;        // positional: plugin subcommand name (plugin-info, plugin run)
    map<string, string> pluginArgs; // --key value pairs passed to plugin
    string   pluginDir;         // --plugin-dir (override default plugin search path)
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

  ca-pg                  Land use transition probability estimation (RF/ANN/Logit)
  ca-markov              Markov chain land use demand prediction
  ca-precision           CA simulation precision (FoM, PA, UA, Kappa, OA)
  ca-simulate            CA iterative land use simulation (FLUS model)
  sdg-predict           One-click pipeline for all 5 SDG indicators
  sdg-infra-simulate    Infrastructure binary CA simulation
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
  priority-stats         Priority area statistics (area per level)

  check                  Check GeoTIFF metadata and data quality

  resample               Resample rasters to base grid alignment
  normalize              Normalize raster values to target range
  reclass                Reclassify raster values and mark NoData
  detect-change          Detect land use change between two periods
  compress               Compress raster file (DEFLATE/LZW/ZSTD/LERC)

  pipeline               Execute multi-step pipeline (DAG orchestration)

  list-plugins           List all available plugins (built-in and external)
  plugin-info            Show detailed info about a specific plugin
  <plugin-name>          Run a custom plugin by name

  version                Show version information
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

  priority-stats:
    --ranking <path>     Ranking map GeoTIFF path (0-6 values)

  ca-pg:
    --train-lucc <path>  Training period LUCC raster
    --curr-lucc <path>   Current period LUCC raster
    --drivers <p1,p2,...>  Driver factor rasters (comma-separated)
    --method <rf|ann|logit>  ML method (default: rf)
    --trees <n>          RF tree count (default: 60)
    --select-rate <v>    RF feature selection rate (default: 0.6)
    --max-per-type <n>   Max positive samples per type (default: 5000)
    --max-neg-per-type <n>  Max negative samples per type (default: 5000)
    --openmp             Enable OpenMP parallelization (default: on)
    --no-openmp          Disable OpenMP parallelization
    --model <path>       Model persistence path (save/load)
    --special <path>     Special region shapefile (optional)
    --validate <path>    Validation LUCC raster (optional)
    -o <path>            Output Pg multi-band GeoTIFF

  ca-markov:
    --luccs <p1,p2,...>  Historical LUCC rasters (comma-separated, >= 2)
    --years <y1,y2,...>  Corresponding years (comma-separated)
    --target-year <y>    Target prediction year
    --step <n>           Prediction step in years (default: 10)
    -o <path>            Output demand CSV

  ca-simulate:
    --init-lucc <path>   Initial LUCC raster
    --pg <path>          Pg probability raster (ca-pg output)
    --demand <path>      Demand CSV (ca-markov output)
    --target-year <y>    Target year (default: last row in CSV)
    --iterations <n>     Max CA iterations (default: 10)
    --convergence <v>    Convergence threshold (default: 0.001)
    --neighbor <n>       Neighborhood radius (default: 3)
    --weights <w1,w2,...>  Type neighborhood weights
    --decay <v>          Decay factor (default: 0.92)
    --step-size <n>      Adaptive step size (default: 400)
    --redline <path>     Redline raster (0=prohibited, optional)
    --transitions <s:t1:t2,...>  Transition constraints
    --dk <d1,d2,...>     Base Dk values per type
    --mu <m1,m2,...>     New patch threshold per type
    --save-precision     Save precision tracking CSV
    --validate <path>    Validation LUCC raster (optional)
    --multi-scenario     Enable multi-scenario simulation mode
    --scenarios <s1,s2,...>  Scenario names (for multi-scenario mode)
    -o <path>            Output simulated LUCC GeoTIFF

  sdg-predict:
    --init-lucc <path>   Initial LUCC raster
    --curr-lucc <path>   Current LUCC raster
    --train-lucc <path>  Training LUCC raster
    --drivers <p1,p2,...>  Driver factor rasters
    --pop <path>         Population raster (for SDG 11.3.1)
    --emission <type:factor,...>  Emission factors (for SDG 13.2.2)
    --output-dir <path>  Output directory for all indicators
    --method <rf|ann|logit>  ML method for Pg estimation
    --validate <path>    Validation LUCC raster (optional)

  sdg-infra-simulate:
    --init-lucc <path>   Initial LUCC raster
    --infra-zone <path>  Infrastructure priority zone raster (Pg)
    --demand <path>      Demand CSV (binary: occupied pixel count)
    --target-year <y>    Target year (default: last row in CSV)
    --iterations <n>     Max CA iterations (default: 10)
    --convergence <v>    Convergence threshold (default: 0.001)
    --neighbor <n>       Neighborhood radius (default: 3)
    --decay <v>          Decay factor (default: 0.92)
    --validate <path>    Validation LUCC raster (optional)
    -o <path>            Output simulated LUCC GeoTIFF
    
  check:
    <file>               GeoTIFF file path to inspect
    --ref <path>         Reference file (projection/dimension match check)
    --types <1,2,...>    Expected land type codes (type coverage check)
    --category <n>       Expected total category count (completeness check)
    --expect-int         Expect integer data type (warn if Float32/64)

  resample:
    --base <path>        Base raster (target grid reference)
    --inputs <p1,p2,...> Input rasters to resample (comma-separated)
    --method <nearest|bilinear>  Resample method (default: nearest)
    --output-dir <path>  Output directory (auto-named <name>_resampled.tif)
    -o <p1,p2,...>       Output paths (comma-separated, must match --inputs count)

  normalize:
    --inputs <p1,p2,...> Input rasters to normalize (comma-separated)
    --range <min:max>    Target range (default: 0:1)
    --min-val <v>        Manual minimum value (skip auto-scan)
    --max-val <v>        Manual maximum value (skip auto-scan)
    --output-dir <path>  Output directory (auto-named <name>_normalized.tif)
    -o <p1,p2,...>       Output paths (comma-separated)

  reclass:
    --input <path>       Input single-band raster
    --rules <path>       Reclassify rule JSON file
    --remap <k1:v1,...>  Inline remap rules (e.g. 1:10,2:10,3:20)
    --set-nodata <v1,v2,...>  Values to mark as NoData
    --nodata-value <v>   Output NoData value (default: -9999)
    -o <path>            Output raster path

  detect-change:
    --before <path>      Before-period raster
    --after <path>       After-period raster
    --encode             Output encoded change map (before*1000+after)
    -o <path>            Output change raster path

  compress:
    --input <path>       Input raster
    --method <deflate|lzw|zstd|lerc|lerc_zstd>  Compress algorithm (default: deflate)
    --level <1-9>        Compression level (default: 6)
    --predictor <0|2|3>  Predictor for float data (default: 2)
    --max-error <v>      LERC max error (default: 0.001)
    --tiled              Enable tiled storage
    --block-size <n>     Tile block size in pixels (default: 256)
    --bigtiff <yes|no|if_needed>  BigTIFF mode (default: if_needed)
    --overview           Generate internal overviews
    -o <path>            Output compressed raster path

  pipeline:
    pipeline run <config.json> [options]
      Execute a single pipeline config
      --var KEY=VALUE     Override pipeline variable (repeatable)
      --workers <n>       Max parallel workers per level (default: 4)
      --resume            Resume from last checkpoint
      --dry-run           Print execution plan without running
      --work-dir <dir>    Working directory (default: current dir)
      --cli-path <path>   Path to geosdg-cli executable
      -o <path>           Save execution result to JSON

    pipeline batch <dir> [options]
      Execute all pipeline configs in a directory
      --fail-fast         Stop on first pipeline failure
      (also supports --var, --workers, --resume, --dry-run, --work-dir, --cli-path, -o)

    pipeline list-templates
      List available pipeline templates

    pipeline validate <config.json> [--lenient]
      Validate a pipeline config without executing
      --lenient           Allow unknown tool names

    pipeline dry-run <config.json> [options]
      Show execution plan without running (alias for run --dry-run)

  list-plugins:
    List all available plugins (built-in and external)
    --plugin-dir <dir>    Additional plugin search directory

  plugin-info <name>:
    Show detailed information about a specific plugin
    --plugin-dir <dir>    Additional plugin search directory

  <plugin-name> [options]:
    Run a plugin by its subcommand name
    --plugin-dir <dir>    Additional plugin search directory
    --<key> <value>       Plugin-specific parameters (see plugin-info for details)
)" << endl;
}

// ============================================================================
// 命令行解析 / CLI argument parsing
// ============================================================================

Command parseCommand(const string& cmd)
{
    if (cmd == "demo")                  return Command::DEMO;
    if (cmd == "ca-pg")                 return Command::CA_PG;
    if (cmd == "ca-markov")             return Command::CA_MARKOV;
    if (cmd == "ca-precision")          return Command::CA_PRECISION;
    if (cmd == "ca-simulate")           return Command::CA_SIMULATE;
    if (cmd == "sdg-predict")           return Command::SDG_PREDICT;
    if (cmd == "sdg-infra-simulate")    return Command::SDG_INFRA_SIMULATE;
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
    if (cmd == "priority-stats")        return Command::PRIORITY_STATS;
    if (cmd == "check")                 return Command::CHECK;
    if (cmd == "resample")              return Command::RESAMPLE;
    if (cmd == "normalize")             return Command::NORMALIZE;
    if (cmd == "reclass")               return Command::RECLASS;
    if (cmd == "detect-change")         return Command::DETECT_CHANGE;
    if (cmd == "compress")              return Command::COMPRESS;
    if (cmd == "pipeline")              return Command::PIPELINE;
    if (cmd == "list-plugins")          return Command::LIST_PLUGINS;
    if (cmd == "plugin-info")           return Command::PLUGIN_INFO;
    if (cmd == "version" || cmd == "--version" || cmd == "-v") return Command::VERSION;
    if (cmd == "help" || cmd == "--help" || cmd == "-h") return Command::HELP;
    return Command::NONE;
}

CliArgs parseArgs(int argc, char* argv[])
{
    CliArgs args;
    if (argc < 2) { args.command = Command::HELP; return args; }

    args.selfPath = argv[0];

    args.command = parseCommand(argv[1]);
    if (args.command == Command::NONE) {
        // Unknown command — treat as potential plugin name
        args.command = Command::PLUGIN_RUN;
        args.pluginName = argv[1];
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
        else if (arg == "--input"         && i+1 < argc) args.initLUCC    = argv[++i];
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
        else if (arg == "--ranking"       && i+1 < argc) args.rankingPath = argv[++i];
        // ── CA simulation parameters ──
        else if (arg == "--train-lucc"    && i+1 < argc) args.trainLUCC    = argv[++i];
        else if (arg == "--drivers"       && i+1 < argc) args.driversStr   = argv[++i];
        else if (arg == "--method"        && i+1 < argc) {
            args.methodStr = argv[++i];
            if (args.methodStr == "ann") args.caMethod = 1;
            else if (args.methodStr == "logit") args.caMethod = 2;
            else args.caMethod = 0; // rf
        }
        else if (arg == "--trees"         && i+1 < argc) args.rfTrees      = stoi(argv[++i]);
        else if (arg == "--select-rate"   && i+1 < argc) args.selectRate   = stod(argv[++i]);
        else if (arg == "--special"       && i+1 < argc) args.specialShp   = argv[++i];
        else if (arg == "--validate"      && i+1 < argc) args.validatePath = argv[++i];
        else if (arg == "--luccs"         && i+1 < argc) args.luccsStr     = argv[++i];
        else if (arg == "--years"         && i+1 < argc) args.yearsStr     = argv[++i];
        else if (arg == "--target-year"   && i+1 < argc) args.targetYear   = stoi(argv[++i]);
        else if (arg == "--step"          && i+1 < argc) args.step         = stoi(argv[++i]);
        else if (arg == "--pg"            && i+1 < argc) args.pgPath       = argv[++i];
        else if (arg == "--demand"        && i+1 < argc) args.demandPath   = argv[++i];
        else if (arg == "--iterations"    && i+1 < argc) args.iterations   = stoi(argv[++i]);
        else if (arg == "--convergence"   && i+1 < argc) args.convergence  = stod(argv[++i]);
        else if (arg == "--neighbor"      && i+1 < argc) args.neighborRadius = stoi(argv[++i]);
        else if (arg == "--weights"       && i+1 < argc) args.weightsStr   = argv[++i];
        else if (arg == "--decay"         && i+1 < argc) args.decay        = stod(argv[++i]);
        else if (arg == "--step-size"     && i+1 < argc) args.stepSize     = stoi(argv[++i]);
        else if (arg == "--redline"       && i+1 < argc) args.redlinePath  = argv[++i];
        else if (arg == "--dk"            && i+1 < argc) args.dkStr        = argv[++i];
        else if (arg == "--mu"            && i+1 < argc) args.muStr        = argv[++i];
        else if (arg == "--save-precision")                args.savePrecision = true;
        else if (arg == "--max-per-type"  && i+1 < argc) args.nMaxPerType    = stoi(argv[++i]);
        else if (arg == "--max-neg-per-type" && i+1 < argc) args.nMaxNegPerType = stoi(argv[++i]);
        else if (arg == "--openmp")                        args.bUseOpenMP     = true;
        else if (arg == "--no-openmp")                     args.bUseOpenMP     = false;
        else if (arg == "--model"        && i+1 < argc) args.modelPath      = argv[++i];
        else if (arg == "--multi-scenario")                args.multiScenario  = true;
        else if (arg == "--scenarios"    && i+1 < argc) args.scenariosStr   = argv[++i];
        else if (arg == "--infra-zone"   && i+1 < argc) args.infraZonePath  = argv[++i];
        else if (arg == "--ref"           && i+1 < argc) args.refPath      = argv[++i];
        else if (arg == "--category"      && i+1 < argc) args.expectedCategory = stoi(argv[++i]);
        else if (arg == "--expect-int")                     args.expectInt   = true;
        // ── 预处理参数 / Preprocessing parameters ──
        else if (arg == "--base"           && i+1 < argc) args.basePath      = argv[++i];
        else if (arg == "--inputs"         && i+1 < argc) args.inputsStr     = argv[++i];
        else if (arg == "--output-dir"     && i+1 < argc) args.outputDir     = argv[++i];
        else if (arg == "--range"          && i+1 < argc) args.rangeStr      = argv[++i];
        else if (arg == "--min-val"        && i+1 < argc) { args.manualMinVal = stod(argv[++i]); args.hasManualMin = true; }
        else if (arg == "--max-val"        && i+1 < argc) { args.manualMaxVal = stod(argv[++i]); args.hasManualMax = true; }
        else if (arg == "--rules"          && i+1 < argc) args.rulesPath     = argv[++i];
        else if (arg == "--remap"          && i+1 < argc) args.remapStr      = argv[++i];
        else if (arg == "--set-nodata"     && i+1 < argc) args.setNodataStr  = argv[++i];
        else if (arg == "--nodata-value"   && i+1 < argc) args.nodataValue   = stoi(argv[++i]);
        else if (arg == "--before"         && i+1 < argc) args.beforePath    = argv[++i];
        else if (arg == "--after"          && i+1 < argc) args.afterPath     = argv[++i];
        else if (arg == "--encode")                         args.encodeMode   = true;
        else if (arg == "--level"          && i+1 < argc) args.compressLevel = stoi(argv[++i]);
        else if (arg == "--predictor"      && i+1 < argc) args.predictor     = stoi(argv[++i]);
        else if (arg == "--max-error"      && i+1 < argc) args.maxError      = stod(argv[++i]);
        else if (arg == "--tiled")                          args.tiled        = true;
        else if (arg == "--block-size"     && i+1 < argc) args.blockSize     = stoi(argv[++i]);
        else if (arg == "--bigtiff"        && i+1 < argc) args.bigtiffMode   = argv[++i];
        else if (arg == "--overview")                       args.genOverview  = true;
        // ── Pipeline 参数 / Pipeline parameters ──
        else if (arg == "--var"           && i+1 < argc) args.pipelineVar     = argv[++i];
        else if (arg == "--workers"       && i+1 < argc) args.pipelineWorkers = stoi(argv[++i]);
        else if (arg == "--resume")                        args.pipelineResume = true;
        else if (arg == "--dry-run")                       args.pipelineDryRun = true;
        else if (arg == "--fail-fast")                     args.pipelineFailFast = true;
        else if (arg == "--lenient")                       args.pipelineLenient = true;
        else if (arg == "--work-dir"      && i+1 < argc) args.pipelineWorkDir = argv[++i];
        else if (arg == "--cli-path"      && i+1 < argc) args.pipelineCliPath = argv[++i];
        // ── Plugin 参数 / Plugin parameters ──
        else if (arg == "--plugin-dir"    && i+1 < argc) args.pluginDir    = argv[++i];
        else if (arg[0] != '-') {
            // Pipeline: first positional = sub-command, second = config path
            if (args.command == Command::PIPELINE) {
                if (args.pipelineSubCmd.empty()) args.pipelineSubCmd = arg;
                else if (args.pipelineConfig.empty()) args.pipelineConfig = arg;
            }
            // plugin-info: first positional = plugin name
            else if (args.command == Command::PLUGIN_INFO && args.pluginName.empty()) {
                args.pluginName = arg;
            }
            // check command: first positional = file path
            else if (args.checkFile.empty())  args.checkFile   = arg;
        }
        else if (args.command == Command::PLUGIN_RUN && arg.substr(0, 2) == "--") {
            // Plugin run: collect all --key value pairs
            string key = arg.substr(2);
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                args.pluginArgs[key] = argv[++i];
            } else {
                args.pluginArgs[key] = "true";
            }
        }
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

int cmdSDGPredict(const CliArgs& a)
{
    if (a.trainLUCC.empty() || a.currLUCC.empty() || a.initLUCC.empty()) {
        cerr << "Error: --train-lucc, --curr-lucc, --init-lucc required" << endl; return 1;
    }
    if (a.outputPath.empty()) {
        cerr << "Error: --output-dir required" << endl; return 1;
    }

    namespace fs = std::filesystem;
    fs::path outDir(a.outputPath);
    if (!fs::exists(outDir)) {
        fs::create_directories(outDir);
    }

    LOG_INFO("sdg-predict: Starting one-click SDG prediction pipeline");

    // Step 1: Pg estimation
    string pgPath = (outDir / "Pg.tif").string();
    {
        auto vDrivers = parseFilePaths(a.driversStr);
        auto estimator = make_unique<PgEstimator>();
        int ret = estimator->estimate(a.trainLUCC, a.currLUCC, vDrivers,
                                       a.caMethod, a.rfTrees, a.selectRate,
                                       a.specialShp, a.validatePath, pgPath,
                                       a.nMaxPerType, a.nMaxNegPerType,
                                       a.bUseOpenMP, a.modelPath);
        if (ret != 0) {
            cerr << "Error: Pg estimation failed" << endl; return ret;
        }
        LOG_INFO("sdg-predict: Pg estimation completed");
    }

    // Step 2: Markov demand prediction
    string demandPath = (outDir / "demand.csv").string();
    {
        MarkovPredictor mp;
        int ret = mp.predict({a.trainLUCC, a.currLUCC}, {2010, 2020},
                             2030, 10, demandPath, false);
        if (ret != 0) {
            cerr << "Error: Markov prediction failed" << endl; return ret;
        }
        LOG_INFO("sdg-predict: Markov demand prediction completed");
    }

    // Step 3: CA simulation
    string simPath = (outDir / "Simulation.tif").string();
    {
        auto transMap = parseTransitionMap(""); // empty = allow all
        auto weights = parseDoubleVector("1,1,1,1,1,1,1,1");
        auto dk = parseDoubleVector("0,0,0,0,0,0,0,0");
        auto mu = parseDoubleVector("0,0,0,0,0,0,0,0");

        CASimulator sim;
        int ret = sim.simulate(a.initLUCC, pgPath, demandPath,
                               2030, 10, 0.001, 3, weights, 0.92, 400,
                               "", transMap, dk, mu, simPath, false, a.validatePath, false);
        if (ret != 0) {
            cerr << "Error: CA simulation failed" << endl; return ret;
        }
        LOG_INFO("sdg-predict: CA simulation completed");
    }

    // Step 4: SDG indicator calculations
    auto p = make_unique<CalculateSDG>();
    string indicatorPath;

    // SDG 2.4.1 (land proportion)
    if (!a.typesStr.empty()) {
        indicatorPath = (outDir / "SDG_2_4_1.txt").string();
        auto types = parseIntSet(a.typesStr);
        double score = p->calculateLandProportionIndicator(a.initLUCC, 100.0, 0.0, types);
        ofstream of(indicatorPath);
        of << "score=" << fixed << setprecision(4) << score << endl;
        LOG_RESULT("sdg-predict", "SDG_2_4_1", score);
    }

    // SDG 11.3.1 (urban/population ratio)
    if (!a.initPopu.empty() && !a.currPopu.empty()) {
        indicatorPath = (outDir / "SDG_11_3_1.txt").string();
        auto types = parseIntSet(a.typesStr.empty() ? "1,2,3" : a.typesStr);
        double score = p->calculateSDG1131Indicator(a.initLUCC, a.currLUCC, a.initPopu, a.currPopu,
                             types, 3.0, 0.0, 1.12);
        ofstream of(indicatorPath);
        of << "score=" << fixed << setprecision(4) << score << endl;
        LOG_RESULT("sdg-predict", "SDG_11_3_1", score);
    }

    // SDG 13.2.2 (carbon emission)
    if (!a.emissionStr.empty()) {
        indicatorPath = (outDir / "SDG_13_2_2.txt").string();
        auto em = parseEmissionMap(a.emissionStr);
        double score = p->calculateSDG1322Indicator(a.initLUCC, a.currLUCC, em, 0.012);
        ofstream of(indicatorPath);
        of << "score=" << fixed << setprecision(4) << score << endl;
        LOG_RESULT("sdg-predict", "SDG_13_2_2", score);
    }

    LOG_INFO("sdg-predict: All SDG indicators calculated");
    cout << "sdg-predict: Pipeline completed. Results in " << a.outputPath << endl;
    return 0;
}

int cmdSDGInfraSimulate(const CliArgs& a)
{
    if (a.initLUCC.empty() || a.infraZonePath.empty() || a.demandPath.empty() || a.outputPath.empty()) {
        cerr << "Error: --init-lucc, --infra-zone, --demand, -o required" << endl; return 1;
    }

    LOG_INFO("sdg-infra-simulate: Starting infrastructure binary CA simulation");
    auto simulator = make_unique<InfraSimulator>();
    int ret = simulator->simulate(a.initLUCC, a.infraZonePath, a.demandPath,
                                  a.targetYear, a.iterations, a.convergence,
                                  a.neighborRadius, a.decay,
                                  a.outputPath, a.validatePath);
    if (ret != 0) {
        cerr << "Error: Infrastructure simulation failed" << endl; return ret;
    }

    LOG_INFO("sdg-infra-simulate: Simulation completed");
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

int cmdPriorityStats(const CliArgs& a)
{
    if (a.rankingPath.empty()) {
        cerr << "Error: --ranking required" << endl; return 1;
    }
    auto p = make_unique<ExtractPriorityAreas>();
    int ret = p->priorityAreaStats(a.rankingPath);
    return ret;
}

int cmdCheck(const CliArgs& a)
{
    if (a.checkFile.empty()) {
        cerr << "Error: <file> required" << endl; return 1;
    }
    InspectOptions opts;
    opts.refPath = a.refPath;
    if (!a.checkTypesStr.empty()) opts.expectedTypes = parseIntSet(a.checkTypesStr);
    opts.expectedCategoryCount = a.expectedCategory;
    opts.expectInt = a.expectInt;

    GeoTiffInspector inspector;
    auto report = inspector.inspect(a.checkFile, opts);

    cout << report.toOutputLine() << endl;
    for (const auto& line : report.toDiagnosticLines()) {
        cout << line << endl;
    }

    return report.hasError ? 1 : 0;
}

// ============================================================================
// 预处理子命令 / Preprocessing sub-commands
// ============================================================================

int cmdResample(const CliArgs& a)
{
    if (a.basePath.empty()) {
        cerr << "Error: --base required" << endl; return 1;
    }
    if (a.inputsStr.empty()) {
        cerr << "Error: --inputs required" << endl; return 1;
    }

    auto inputs = parseFilePaths(a.inputsStr);
    if (inputs.empty()) {
        cerr << "Error: --inputs list is empty" << endl; return 1;
    }

    vector<string> outputs;
    if (!a.outputPath.empty()) {
        outputs = parseFilePaths(a.outputPath);
        if (outputs.size() != inputs.size()) {
            cerr << "Error: -o count (" << outputs.size()
                 << ") != --inputs count (" << inputs.size() << ")" << endl;
            return 1;
        }
    } else if (!a.outputDir.empty()) {
        for (const auto& inp : inputs) {
            fs::path p(inp);
            string outName = p.stem().string() + "_resampled.tif";
            outputs.push_back((fs::path(a.outputDir) / outName).string());
        }
        fs::create_directories(a.outputDir);
    } else {
        for (const auto& inp : inputs) {
            fs::path p(inp);
            string outName = p.stem().string() + "_resampled.tif";
            outputs.push_back((p.parent_path() / outName).string());
        }
    }

    string method = a.methodStr.empty() ? "nearest" : a.methodStr;
    LOG_INFO("resample: base=" + a.basePath + " method=" + method +
             " files=" + to_string(inputs.size()));

    auto rp = make_unique<RasterPreprocessor>();
    int ret = rp->resampleToBase(a.basePath, inputs, outputs, method);
    if (ret == 0) {
        for (const auto& out : outputs) cout << "Output: " << out << endl;
    }
    return ret;
}

int cmdNormalize(const CliArgs& a)
{
    if (a.inputsStr.empty()) {
        cerr << "Error: --inputs required" << endl; return 1;
    }

    auto inputs = parseFilePaths(a.inputsStr);
    vector<string> outputs;
    if (!a.outputPath.empty()) {
        outputs = parseFilePaths(a.outputPath);
        if (outputs.size() != inputs.size()) {
            cerr << "Error: -o count (" << outputs.size()
                 << ") != --inputs count (" << inputs.size() << ")" << endl;
            return 1;
        }
    } else if (!a.outputDir.empty()) {
        for (const auto& inp : inputs) {
            fs::path p(inp);
            string outName = p.stem().string() + "_normalized.tif";
            outputs.push_back((fs::path(a.outputDir) / outName).string());
        }
        fs::create_directories(a.outputDir);
    } else {
        for (const auto& inp : inputs) {
            fs::path p(inp);
            string outName = p.stem().string() + "_normalized.tif";
            outputs.push_back((p.parent_path() / outName).string());
        }
    }

    double rangeMin = 0.0, rangeMax = 1.0;
    if (!a.rangeStr.empty()) {
        auto p = a.rangeStr.find(':');
        if (p == string::npos) {
            cerr << "Error: --range format should be min:max" << endl; return 1;
        }
        rangeMin = stod(a.rangeStr.substr(0, p));
        rangeMax = stod(a.rangeStr.substr(p + 1));
    }

    if (a.hasManualMin && a.hasManualMax && fabs(a.manualMinVal - a.manualMaxVal) < 1e-15) {
        cerr << "Error: --min-val equals --max-val" << endl; return 1;
    }

    LOG_INFO("normalize: files=" + to_string(inputs.size()) +
             " range=[" + to_string(rangeMin) + "," + to_string(rangeMax) + "]");

    auto rp = make_unique<RasterPreprocessor>();
    int ret = rp->normalizeRaster(inputs, outputs, rangeMin, rangeMax,
                                   a.hasManualMin, a.manualMinVal,
                                   a.hasManualMax, a.manualMaxVal);
    if (ret == 0) {
        for (const auto& out : outputs) cout << "Output: " << out << endl;
    }
    return ret;
}

int cmdReclass(const CliArgs& a)
{
    if (a.initLUCC.empty()) {
        cerr << "Error: --input required" << endl; return 1;
    }
    if (a.outputPath.empty()) {
        cerr << "Error: -o required" << endl; return 1;
    }

    RasterPreprocessor::ReclassRule rule;

    if (!a.rulesPath.empty()) {
        LOG_INFO("reclass: Using rules file: " + a.rulesPath);
        if (!a.remapStr.empty() || !a.setNodataStr.empty()) {
            LOG_INFO("reclass: --remap/--set-nodata ignored because --rules is specified");
        }
        rule = RasterPreprocessor::parseReclassRule(a.rulesPath);
        if (rule.remap.empty() && rule.nodataValues.empty()) {
            cerr << "Error: Failed to parse rules file or empty rules" << endl; return 1;
        }
    } else if (!a.remapStr.empty() || !a.setNodataStr.empty()) {
        rule = RasterPreprocessor::parseReclassRuleFromArgs(a.remapStr, a.setNodataStr);
    } else {
        cerr << "Error: --rules or --remap/--set-nodata required" << endl; return 1;
    }

    LOG_INFO("reclass: input=" + a.initLUCC + " output=" + a.outputPath);

    auto rp = make_unique<RasterPreprocessor>();
    int ret = rp->reclassifyRaster(a.initLUCC, a.outputPath, rule, a.nodataValue);
    if (ret == 0) cout << "Output: " << a.outputPath << endl;
    return ret;
}

int cmdDetectChange(const CliArgs& a)
{
    if (a.beforePath.empty() || a.afterPath.empty()) {
        cerr << "Error: --before and --after required" << endl; return 1;
    }
    if (a.outputPath.empty()) {
        cerr << "Error: -o required" << endl; return 1;
    }

    LOG_INFO("detect-change: before=" + a.beforePath + " after=" + a.afterPath +
             " encode=" + to_string(a.encodeMode));

    auto rp = make_unique<RasterPreprocessor>();
    int ret = rp->detectChange(a.beforePath, a.afterPath, a.outputPath, a.encodeMode);
    if (ret == 0) cout << "Output: " << a.outputPath << endl;
    return ret;
}

int cmdCompress(const CliArgs& a)
{
    if (a.initLUCC.empty()) {
        cerr << "Error: --input required" << endl; return 1;
    }
    if (a.outputPath.empty()) {
        cerr << "Error: -o required" << endl; return 1;
    }

    RasterPreprocessor::CompressOptions opts;
    if (!a.methodStr.empty()) opts.method = a.methodStr;
    opts.level = a.compressLevel;
    opts.predictor = a.predictor;
    opts.maxError = a.maxError;
    opts.tiled = a.tiled;
    opts.blockSize = a.blockSize;
    opts.bigtiff = a.bigtiffMode;
    opts.overview = a.genOverview;

    LOG_INFO("compress: input=" + a.initLUCC + " method=" + opts.method +
             " level=" + to_string(opts.level));

    auto rp = make_unique<RasterPreprocessor>();
    int ret = rp->compressRaster(a.initLUCC, a.outputPath, opts);
    if (ret == 0) cout << "Output: " << a.outputPath << endl;
    return ret;
}

// ============================================================================
// CA simulation sub-commands / CA 模拟子命令
// ============================================================================

int cmdCAPg(const CliArgs& a)
{
    if (a.trainLUCC.empty() || a.currLUCC.empty()) {
        cerr << "Error: --train-lucc and --curr-lucc required" << endl; return 1;
    }
    if (a.driversStr.empty()) {
        cerr << "Error: --drivers required" << endl; return 1;
    }
    if (a.outputPath.empty()) {
        cerr << "Error: -o required" << endl; return 1;
    }

    auto vDrivers = parseFilePaths(a.driversStr);
    if (vDrivers.size() < 2) {
        cerr << "Error: at least 2 driver factors required" << endl; return 1;
    }

    LOG_INFO("ca-pg: Starting Pg estimation");
    auto estimator = make_unique<PgEstimator>();
    int ret = estimator->estimate(a.trainLUCC, a.currLUCC, vDrivers,
                                   a.caMethod, a.rfTrees, a.selectRate,
                                   a.specialShp, a.validatePath, a.outputPath,
                                   a.nMaxPerType, a.nMaxNegPerType,
                                   a.bUseOpenMP, a.modelPath);
    if (ret != 0) {
        cerr << "Error: Pg estimation failed" << endl;
        return ret;
    }
    cout << "Pg output: " << a.outputPath << endl;
    return 0;
}

int cmdCAMarkov(const CliArgs& a)
{
    if (a.luccsStr.empty() || a.yearsStr.empty()) {
        cerr << "Error: --luccs and --years required" << endl; return 1;
    }
    if (a.targetYear == 0) {
        cerr << "Error: --target-year required" << endl; return 1;
    }
    if (a.outputPath.empty()) {
        cerr << "Error: -o required" << endl; return 1;
    }

    auto vLUCCs = parseFilePaths(a.luccsStr);
    auto vYears = parseIntVector(a.yearsStr);

    if (vLUCCs.size() < 2) {
        cerr << "Error: at least 2 LUCC periods required" << endl; return 1;
    }
    if (vLUCCs.size() != vYears.size()) {
        cerr << "Error: LUCC count (" << vLUCCs.size()
             << ") != years count (" << vYears.size() << ")" << endl; return 1;
    }

    LOG_INFO("ca-markov: Starting Markov demand prediction");
    auto predictor = make_unique<MarkovPredictor>();
    int ret = predictor->predict(vLUCCs, vYears, a.targetYear, a.step, a.outputPath);
    if (ret != 0) {
        cerr << "Error: Markov prediction failed" << endl;
        return ret;
    }
    cout << "Demand output: " << a.outputPath << endl;
    return 0;
}

int cmdCASimulate(const CliArgs& a)
{
    if (a.initLUCC.empty()) {
        cerr << "Error: --init-lucc required" << endl; return 1;
    }
    if (a.pgPath.empty()) {
        cerr << "Error: --pg required" << endl; return 1;
    }
    if (a.demandPath.empty()) {
        cerr << "Error: --demand required" << endl; return 1;
    }
    if (a.outputPath.empty()) {
        cerr << "Error: -o required" << endl; return 1;
    }

    auto vWeights = a.weightsStr.empty() ? vector<double>{} : parseDoubleVector(a.weightsStr);
    auto vDk = a.dkStr.empty() ? vector<double>{} : parseDoubleVector(a.dkStr);
    auto vMu = a.muStr.empty() ? vector<double>{} : parseDoubleVector(a.muStr);
    auto transMap = a.transStr.empty() ? unordered_map<int, vector<int>>{} : parseTransitionMap(a.transStr);

    LOG_INFO("ca-simulate: Starting CA simulation");
    auto simulator = make_unique<CASimulator>();
    int ret = simulator->simulate(a.initLUCC, a.pgPath, a.demandPath,
                                   a.targetYear, a.iterations, a.convergence,
                                   a.neighborRadius, vWeights, a.decay, a.stepSize,
                                   a.redlinePath, transMap, vDk, vMu,
                                   a.outputPath, a.savePrecision, a.validatePath);
    if (ret != 0) {
        cerr << "Error: CA simulation failed" << endl;
        return ret;
    }
    cout << "Simulation output: " << a.outputPath << endl;
    return 0;
}

// ============================================================================
// Pipeline 命令 / Pipeline command
// ============================================================================

/**
 * @brief Execute pipeline orchestration by delegating to Python pipeline CLI
 *
 * The pipeline command wraps the Python-based pipeline orchestration engine.
 * It constructs a subprocess call to `python3 -m agent.pipeline.cli` with
 * the appropriate sub-command and arguments.
 *
 * @param a Parsed CLI arguments
 * @return 0 on success, non-zero on failure
 */
int cmdPipeline(const CliArgs& a)
{
    // Determine the path to the Python pipeline module
    // Look for agent/ directory relative to the executable or current working directory
    namespace fs = std::filesystem;

    // Candidate paths for the agent directory
    vector<fs::path> agentPaths;
    if (!a.pipelineCliPath.empty()) {
        // If --cli-path is given, use it as the base for finding agent/
        fs::path cliPath(a.pipelineCliPath);
        agentPaths.push_back(cliPath.parent_path() / ".." / "agent");
        agentPaths.push_back(cliPath.parent_path() / "agent");
    }

    // Try relative to current executable
    fs::path exePath;
#ifdef _WIN32
    char exeBuf[MAX_PATH];
    GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
    exePath = exeBuf;
#else
    exePath = fs::read_symlink("/proc/self/exe");
    if (exePath.empty()) {
        // Fallback: use argv[0] path (passed via a known mechanism)
        // For now, try current directory
    }
#endif

    if (!exePath.empty()) {
        agentPaths.push_back(exePath.parent_path() / ".." / "agent");
        agentPaths.push_back(exePath.parent_path() / "agent");
    }

    // Try current working directory
    agentPaths.push_back(fs::current_path() / "agent");
    agentPaths.push_back(fs::current_path() / ".." / "agent");

    // Find the first existing agent path
    fs::path agentDir;
    for (const auto& p : agentPaths) {
        fs::path candidate = fs::weakly_canonical(p);
        if (fs::exists(candidate / "pipeline" / "cli.py")) {
            agentDir = candidate;
            break;
        }
    }

    if (agentDir.empty()) {
        LOG_ERROR("Cannot find agent/pipeline/cli.py. Please run from the project root or set --cli-path.");
        cerr << "Error: Cannot find pipeline module. Ensure you are running from the GeoSDG project root." << endl;
        return 1;
    }

    // Build Python command
    vector<string> cmdParts;
    cmdParts.push_back("python3");
    cmdParts.push_back("-m");
    cmdParts.push_back("agent.pipeline.cli");

    // Sub-command
    string subCmd = a.pipelineSubCmd.empty() ? "run" : a.pipelineSubCmd;
    cmdParts.push_back(subCmd);

    // Config path (for run/batch/validate/dry-run)
    if (!a.pipelineConfig.empty()) {
        cmdParts.push_back(a.pipelineConfig);
    }

    // Options
    if (!a.pipelineCliPath.empty()) {
        cmdParts.push_back("--cli-path");
        cmdParts.push_back(a.pipelineCliPath);
    } else if (!exePath.empty()) {
        // Use this executable as the geosdg-cli path
        cmdParts.push_back("--cli-path");
        cmdParts.push_back(exePath.string());
    }

    if (!a.pipelineWorkDir.empty()) {
        cmdParts.push_back("--work-dir");
        cmdParts.push_back(a.pipelineWorkDir);
    }

    if (a.pipelineWorkers > 0) {
        cmdParts.push_back("--workers");
        cmdParts.push_back(to_string(a.pipelineWorkers));
    }

    if (a.pipelineResume) {
        cmdParts.push_back("--resume");
    }

    if (a.pipelineDryRun) {
        cmdParts.push_back("--dry-run");
    }

    if (a.pipelineFailFast && subCmd == "batch") {
        cmdParts.push_back("--fail-fast");
    }

    if (a.pipelineLenient && subCmd == "validate") {
        cmdParts.push_back("--lenient");
    }

    if (!a.pipelineVar.empty()) {
        cmdParts.push_back("--var");
        cmdParts.push_back(a.pipelineVar);
    }

    if (!a.pipelineOutput.empty()) {
        cmdParts.push_back("--output");
        cmdParts.push_back(a.pipelineOutput);
    }

    // Build command string for logging
    string cmdStr;
    for (size_t i = 0; i < cmdParts.size(); ++i) {
        if (i > 0) cmdStr += " ";
        // Quote arguments that contain spaces
        if (cmdParts[i].find(' ') != string::npos) {
            cmdStr += "\"" + cmdParts[i] + "\"";
        } else {
            cmdStr += cmdParts[i];
        }
    }

    LOG_INFO("Pipeline command: " + cmdStr);

    // Execute Python pipeline CLI as subprocess
    // Set PYTHONPATH to include the parent of agent/ directory
    string pythonPath = agentDir.parent_path().string();
    string existingPythonPath = getenv("PYTHONPATH") ? getenv("PYTHONPATH") : "";
    if (!existingPythonPath.empty()) {
        pythonPath = pythonPath + ":" + existingPythonPath;
    }

    // Build argv array for execvp
    vector<char*> argvVec;
    for (auto& s : cmdParts) {
        argvVec.push_back(&s[0]);
    }
    argvVec.push_back(nullptr);

    // Fork and exec
    pid_t pid = fork();
    if (pid < 0) {
        LOG_ERROR("Failed to fork process for pipeline execution");
        cerr << "Error: Failed to start pipeline process." << endl;
        return 1;
    }

    if (pid == 0) {
        // Child process
        // Set PYTHONPATH environment variable
        setenv("PYTHONPATH", pythonPath.c_str(), 1);

        execvp("python3", argvVec.data());

        // If execvp returns, it failed
        _exit(127);
    }

    // Parent process - wait for child
    int status = 0;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        int exitCode = WEXITSTATUS(status);
        if (exitCode == 0) {
            LOG_INFO("Pipeline completed successfully");
        } else {
            LOG_ERROR("Pipeline failed with exit code " + to_string(exitCode));
        }
        return exitCode;
    } else if (WIFSIGNALED(status)) {
        LOG_ERROR("Pipeline process killed by signal " + to_string(WTERMSIG(status)));
        cerr << "Error: Pipeline process was killed by signal " << WTERMSIG(status) << endl;
        return 1;
    }

    return 1;
}

// ============================================================================
// Plugin 命令 / Plugin Commands
// ============================================================================

/**
 * @brief Build and configure a PluginManager instance
 * @param args Parsed CLI arguments
 * @return Configured PluginManager (with builtins registered)
 *
 * Creates a PluginManager, registers all built-in indicators,
 * adds the default plugin directory, and discovers/loads plugins.
 */
static PluginManager buildPluginManager(const CliArgs& args)
{
    PluginManager mgr;
    mgr.setCliVersion(GEOSDG_VERSION);

    // Register built-in indicators as plugins
    registerBuiltinPlugins(mgr);

    // Add default plugin directory (next to the executable)
    // Default: <exe_dir>/../plugins or <exe_dir>/plugins
    namespace fs = std::filesystem;
    fs::path exeDir = fs::path(args.selfPath).parent_path();
    fs::path defaultPluginDir = exeDir / ".." / "plugins";
    if (fs::exists(defaultPluginDir)) {
        mgr.addPluginDir(defaultPluginDir.string());
    }
    // Also check <exe_dir>/plugins
    fs::path altPluginDir = exeDir / "plugins";
    if (fs::exists(altPluginDir)) {
        mgr.addPluginDir(altPluginDir.string());
    }

    // Add user-specified plugin directory
    if (!args.pluginDir.empty()) {
        mgr.addPluginDir(args.pluginDir);
    }

    // Discover and load all plugins
    mgr.discoverAndLoad();

    return mgr;
}

/**
 * @brief Execute the list-plugins command
 * @param args Parsed CLI arguments
 * @return 0 on success
 */
static int cmdListPlugins(const CliArgs& args)
{
    LOG_INFO("Listing available plugins...");
    PluginManager mgr = buildPluginManager(args);
    mgr.printPluginList();
    return 0;
}

/**
 * @brief Execute the plugin-info command
 * @param args Parsed CLI arguments
 * @return 0 on success, 1 if plugin not found
 */
static int cmdPluginInfo(const CliArgs& args)
{
    if (args.pluginName.empty()) {
        cerr << "Error: plugin name required. Usage: geosdg-cli plugin-info <name>" << endl;
        return 1;
    }

    PluginManager mgr = buildPluginManager(args);
    if (!mgr.printPluginInfo(args.pluginName)) {
        cerr << "Error: plugin not found: " << args.pluginName << endl;
        return 1;
    }
    return 0;
}

/**
 * @brief Execute a plugin by name
 * @param args Parsed CLI arguments
 * @return 0 on success, non-zero on failure
 */
static int cmdPluginRun(const CliArgs& args)
{
    if (args.pluginName.empty()) {
        cerr << "Error: plugin name required. Usage: geosdg-cli <plugin-name> [options]" << endl;
        return 1;
    }

    PluginManager mgr = buildPluginManager(args);

    if (!mgr.hasPlugin(args.pluginName)) {
        cerr << "Error: unknown command or plugin: " << args.pluginName << endl;
        cerr << "Use 'geosdg-cli list-plugins' to see available plugins." << endl;
        return 1;
    }

    // Handle --help: show plugin help text
    if (args.pluginArgs.find("help") != args.pluginArgs.end()) {
        cout << mgr.generateHelpText(args.pluginName);
        return 0;
    }

    LOG_INFO("Running plugin: " + args.pluginName);

    string result, error;
    int ret = mgr.executePlugin(args.pluginName, args.pluginArgs, result, error);
    if (ret != 0) {
        LOG_ERROR("Plugin execution failed: " + error);
        cerr << "Error: " << error << endl;
        return ret;
    }

    if (!result.empty()) {
        cout << result << endl;
    }
    LOG_INFO("Plugin " + args.pluginName + " result: " + (result.empty() ? "(no output)" : result));
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

    if (args.command == Command::VERSION) {
        cout << "geosdg-cli v" << GEOSDG_VERSION << endl;
        return 0;
    }

    Logger::instance().init(args.logPath);

    // Handle --help for built-in commands via plugin help text
    if (args.command != Command::HELP && args.command != Command::VERSION &&
        args.command != Command::NONE) {
        // Check if --help was passed
        bool hasHelp = false;
        for (int i = 2; i < argc; ++i) {
            if (string(argv[i]) == "--help" || string(argv[i]) == "-h") {
                hasHelp = true;
                break;
            }
        }
        if (hasHelp) {
            // Try to get plugin help text
            PluginManager mgr = buildPluginManager(args);
            string cmdName = argv[1];
            if (mgr.hasPlugin(cmdName)) {
                cout << mgr.generateHelpText(cmdName);
                return 0;
            }
        }
    }

    switch (args.command) {
        case Command::DEMO:                return cmdDemo(args.resume);
        case Command::CA_PG:               return cmdCAPg(args);
        case Command::CA_MARKOV:           return cmdCAMarkov(args);
        case Command::CA_PRECISION:        return cmdCAPrecision(args);
        case Command::CA_SIMULATE:         return cmdCASimulate(args);
        case Command::SDG_PREDICT:         return cmdSDGPredict(args);
        case Command::SDG_INFRA_SIMULATE:  return cmdSDGInfraSimulate(args);
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
        case Command::PRIORITY_STATS:      return cmdPriorityStats(args);
        case Command::CHECK:               return cmdCheck(args);
        case Command::RESAMPLE:            return cmdResample(args);
        case Command::NORMALIZE:           return cmdNormalize(args);
        case Command::RECLASS:             return cmdReclass(args);
        case Command::DETECT_CHANGE:       return cmdDetectChange(args);
        case Command::COMPRESS:            return cmdCompress(args);
        case Command::PIPELINE:            return cmdPipeline(args);
        case Command::LIST_PLUGINS:        return cmdListPlugins(args);
        case Command::PLUGIN_INFO:         return cmdPluginInfo(args);
        case Command::PLUGIN_RUN:          return cmdPluginRun(args);
        case Command::VERSION:             cout << "geosdg-cli v" << GEOSDG_VERSION << endl; return 0;
        default:                           printUsage(argv[0]); return 0;
    }
}
