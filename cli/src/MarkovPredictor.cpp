/**
 * @file MarkovPredictor.cpp
 * @brief Markov 链土地利用需求预测实现 / Markov demand prediction implementation
 */

#include "MarkovPredictor.h"
#include "Logger.h"

#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h>

#include <algorithm>
#include <set>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <cmath>

// alglib headers
#include "dataanalysis.h"

using namespace std;

// ============================================================================
// Constructor / Destructor
// ============================================================================

MarkovPredictor::MarkovPredictor() {}

MarkovPredictor::~MarkovPredictor() {}

// ============================================================================
// Public Interface
// ============================================================================

int MarkovPredictor::predict(const vector<string>& vLUCCPaths,
                              const vector<int>& vYears,
                              int nTargetYear,
                              int nStep,
                              const string& qstrOutputPath,
                              bool bBinaryMode)
{
    vLUCCPaths_  = vLUCCPaths;
    vYears_      = vYears;
    nTargetYear_ = nTargetYear;
    nStep_       = nStep;
    outputPath_  = qstrOutputPath;
    bBinaryMode_ = bBinaryMode;

    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    LOG_INFO("MarkovPredictor: Starting Markov demand prediction");
    LOG_INFO("  LUCC periods: " + to_string(vLUCCPaths_.size()));
    LOG_INFO("  Target year: " + to_string(nTargetYear_));
    LOG_INFO("  Step: " + to_string(nStep_) + " years");

    if (bBinaryMode_) {
        LOG_INFO("MarkovPredictor: Binary mode enabled (infrastructure simulation)");
    }

    // ── Validate inputs ──
    if (vLUCCPaths_.size() < 2) {
        LOG_ERROR("MarkovPredictor: At least 2 LUCC periods required, got " + to_string(vLUCCPaths_.size()));
        return 1;
    }
    if (vLUCCPaths_.size() != vYears_.size()) {
        LOG_ERROR("MarkovPredictor: LUCC paths count (" + to_string(vLUCCPaths_.size()) +
                  ") != years count (" + to_string(vYears_.size()) + ")");
        return 1;
    }
    int nMaxInputYear = *max_element(vYears_.begin(), vYears_.end());
    if (nTargetYear_ <= nMaxInputYear) {
        LOG_WARN("MarkovPredictor: Target year (" + to_string(nTargetYear_) +
                 ") <= max input year (" + to_string(nMaxInputYear) + ")");
    }

    // ── Step 1: Read LUCC counts ──
    int ret = readLUCCCounts();
    if (ret != 0) {
        LOG_ERROR("MarkovPredictor: Failed to read LUCC counts");
        return ret;
    }
    LOG_INFO("MarkovPredictor: LUCC counts read, " + to_string(nTypes_) + " types found");

    // ── Step 2: Build transition matrix ──
    ret = buildTransitionMatrix();
    if (ret != 0) {
        LOG_ERROR("MarkovPredictor: Failed to build transition matrix");
        return ret;
    }
    LOG_INFO("MarkovPredictor: Transition matrix built");

    // ── Step 3: Iterate prediction ──
    iteratePrediction();
    LOG_INFO("MarkovPredictor: Prediction completed");

    // ── Step 4: Write CSV ──
    ret = writeDemandCSV();
    if (ret != 0) {
        LOG_ERROR("MarkovPredictor: Failed to write CSV");
        return ret;
    }
    LOG_INFO("MarkovPredictor: Demand CSV written to " + outputPath_);

    return 0;
}

// ============================================================================
// Private Methods
// ============================================================================

int MarkovPredictor::readLUCCCounts()
{
    int nPeriods = (int)vLUCCPaths_.size();

    // ── First pass: determine grid size and types ──
    GDALDatasetH hFirstDS = GDALOpenEx(vLUCCPaths_[0].c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hFirstDS) {
        LOG_ERROR("MarkovPredictor: Cannot open LUCC: " + vLUCCPaths_[0]);
        return 1;
    }
    nRows_ = GDALGetRasterYSize(hFirstDS);
    nCols_ = GDALGetRasterXSize(hFirstDS);
    GDALClose(hFirstDS);

    // ── Collect all types across all periods ──
    set<int> allTypes;
    for (int p = 0; p < nPeriods; p++) {
        GDALDatasetH hDS = GDALOpenEx(vLUCCPaths_[p].c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
        if (!hDS) {
            LOG_ERROR("MarkovPredictor: Cannot open LUCC: " + vLUCCPaths_[p]);
            return 1;
        }
        if (GDALGetRasterXSize(hDS) != nCols_ || GDALGetRasterYSize(hDS) != nRows_) {
            LOG_ERROR("MarkovPredictor: LUCC size mismatch: " + vLUCCPaths_[p]);
            GDALClose(hDS);
            return 1;
        }

        int nPixels = nRows_ * nCols_;
        vector<int> data(nPixels);
        GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                     data.data(), nCols_, nRows_, GDT_Int32, 0, 0);

        int bHasNoData = FALSE;
        double nd = GDALGetRasterNoDataValue(GDALGetRasterBand(hDS, 1), &bHasNoData);
        if (!bHasNoData) nd = -9999.0;

        for (int i = 0; i < nPixels; i++) {
            if (data[i] != (int)nd && data[i] > 0) {
                allTypes.insert(data[i]);
            }
        }
        GDALClose(hDS);
    }

    vTypes_.assign(allTypes.begin(), allTypes.end());
    nTypes_ = (int)vTypes_.size();

    if (nTypes_ < 2) {
        LOG_ERROR("MarkovPredictor: Less than 2 land types found");
        return 1;
    }

    // ── Second pass: count types per period ──
    vPeriodCounts_.assign(nPeriods, vector<long long>(nTypes_, 0));

    for (int p = 0; p < nPeriods; p++) {
        GDALDatasetH hDS = GDALOpenEx(vLUCCPaths_[p].c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
        if (!hDS) return 1;

        int nPixels = nRows_ * nCols_;
        vector<int> data(nPixels);
        GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                     data.data(), nCols_, nRows_, GDT_Int32, 0, 0);

        int bHasNoData = FALSE;
        double nd = GDALGetRasterNoDataValue(GDALGetRasterBand(hDS, 1), &bHasNoData);
        if (!bHasNoData) nd = -9999.0;
        GDALClose(hDS);

        for (int i = 0; i < nPixels; i++) {
            if (data[i] == (int)nd || data[i] <= 0) continue;
            // Find type index
            for (int k = 0; k < nTypes_; k++) {
                if (vTypes_[k] == data[i]) {
                    vPeriodCounts_[p][k]++;
                    break;
                }
            }
        }
    }

    return 0;
}

int MarkovPredictor::buildTransitionMatrix()
{
    int nPeriods = (int)vLUCCPaths_.size();

    // ── Use alglib mcpd to build Markov transition matrix ──
    // Build transition counts from consecutive period pairs
    // For simplicity, use the last two periods to estimate transition

    // Read the last two periods' LUCC data
    int p1 = nPeriods - 2;
    int p2 = nPeriods - 1;

    GDALDatasetH hDS1 = GDALOpenEx(vLUCCPaths_[p1].c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    GDALDatasetH hDS2 = GDALOpenEx(vLUCCPaths_[p2].c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hDS1 || !hDS2) {
        LOG_ERROR("MarkovPredictor: Cannot open LUCC pair for transition matrix");
        if (hDS1) GDALClose(hDS1);
        if (hDS2) GDALClose(hDS2);
        return 1;
    }

    int nPixels = nRows_ * nCols_;
    vector<int> data1(nPixels), data2(nPixels);
    GDALRasterIO(GDALGetRasterBand(hDS1, 1), GF_Read, 0, 0, nCols_, nRows_,
                 data1.data(), nCols_, nRows_, GDT_Int32, 0, 0);
    GDALRasterIO(GDALGetRasterBand(hDS2, 1), GF_Read, 0, 0, nCols_, nRows_,
                 data2.data(), nCols_, nRows_, GDT_Int32, 0, 0);

    int bHasNoData1 = FALSE, bHasNoData2 = FALSE;
    double nd1 = GDALGetRasterNoDataValue(GDALGetRasterBand(hDS1, 1), &bHasNoData1);
    double nd2 = GDALGetRasterNoDataValue(GDALGetRasterBand(hDS2, 1), &bHasNoData2);
    if (!bHasNoData1) nd1 = -9999.0;
    if (!bHasNoData2) nd2 = -9999.0;

    GDALClose(hDS1);
    GDALClose(hDS2);

    // Build type-to-index map
    unordered_map<int, int> typeToIdx;
    for (int i = 0; i < nTypes_; i++) {
        typeToIdx[vTypes_[i]] = i;
    }

    // ── Count transitions ──
    // transitionCount[from][to] = number of pixels that changed from type 'from' to type 'to'
    vector<vector<long long>> transCount(nTypes_, vector<long long>(nTypes_, 0));

    for (int i = 0; i < nPixels; i++) {
        if (data1[i] == (int)nd1 || data1[i] <= 0) continue;
        if (data2[i] == (int)nd2 || data2[i] <= 0) continue;

        auto it1 = typeToIdx.find(data1[i]);
        auto it2 = typeToIdx.find(data2[i]);
        if (it1 != typeToIdx.end() && it2 != typeToIdx.end()) {
            transCount[it1->second][it2->second]++;
        }
    }

    // ── Build transition probability matrix ──
    transitionMatrix_.assign(nTypes_, vector<double>(nTypes_, 0.0));

    for (int i = 0; i < nTypes_; i++) {
        long long rowSum = 0;
        for (int j = 0; j < nTypes_; j++) {
            rowSum += transCount[i][j];
        }
        if (rowSum > 0) {
            for (int j = 0; j < nTypes_; j++) {
                transitionMatrix_[i][j] = (double)transCount[i][j] / rowSum;
            }
        } else {
            // No data for this type: assume stays the same
            transitionMatrix_[i][i] = 1.0;
        }
    }

    // Log transition matrix
    LOG_INFO("MarkovPredictor: Transition matrix:");
    for (int i = 0; i < nTypes_; i++) {
        string row = "  Type" + to_string(vTypes_[i]) + ": ";
        for (int j = 0; j < nTypes_; j++) {
            if (j > 0) row += ", ";
            row += to_string(vTypes_[j]) + "=" + to_string(transitionMatrix_[i][j]);
        }
        LOG_INFO(row);
    }

    return 0;
}

void MarkovPredictor::iteratePrediction()
{
    // ── Store input period counts as initial results ──
    for (int p = 0; p < (int)vYears_.size(); p++) {
        predictionResults_[vYears_[p]] = vPeriodCounts_[p];
    }

    // ── Start from the last input period ──
    int lastYear = vYears_.back();
    vector<double> currentDemand(nTypes_);
    for (int k = 0; k < nTypes_; k++) {
        currentDemand[k] = (double)vPeriodCounts_.back()[k];
    }

    // ── Iterate year by year ──
    for (int year = lastYear + nStep_; year <= nTargetYear_; year += nStep_) {
        // demand = demand * P (matrix multiplication)
        vector<double> newDemand(nTypes_, 0.0);
        for (int i = 0; i < nTypes_; i++) {
            for (int j = 0; j < nTypes_; j++) {
                newDemand[j] += currentDemand[i] * transitionMatrix_[i][j];
            }
        }

        // Round to integers
        vector<long long> intDemand(nTypes_);
        for (int k = 0; k < nTypes_; k++) {
            intDemand[k] = (long long)round(newDemand[k]);
        }

        predictionResults_[year] = intDemand;

        // Log
        string msg = "Year " + to_string(year) + ": ";
        for (int k = 0; k < nTypes_; k++) {
            if (k > 0) msg += ", ";
            msg += "Type" + to_string(vTypes_[k]) + "=" + to_string(intDemand[k]);
        }
        LOG_INFO("MarkovPredictor: " + msg);

        currentDemand = newDemand;
    }
}

int MarkovPredictor::writeDemandCSV()
{
    // ── Ensure output directory exists ──
    namespace fs = std::filesystem;
    fs::path outPath(outputPath_);
    fs::path outDir = outPath.parent_path();
    if (!outDir.empty() && !fs::exists(outDir)) {
        fs::create_directories(outDir);
    }

    // ── Write CSV ──
    ofstream ofs(outputPath_);
    if (!ofs.is_open()) {
        LOG_ERROR("MarkovPredictor: Cannot open output file: " + outputPath_);
        return 1;
    }

    // Header: Year,Type1,Type2,...
    ofs << "Year";
    for (int k = 0; k < nTypes_; k++) {
        ofs << ",Type" << vTypes_[k];
    }
    ofs << "\n";

    // Data rows
    for (auto& [year, counts] : predictionResults_) {
        ofs << year;
        for (int k = 0; k < nTypes_; k++) {
            ofs << "," << counts[k];
        }
        ofs << "\n";
    }

    ofs.close();
    return 0;
}
