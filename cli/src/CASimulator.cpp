/**
 * @file CASimulator.cpp
 * @brief CA 迭代模拟实现 / CA simulation implementation
 */

#include "CASimulator.h"
#include "Logger.h"

#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h>

#include <algorithm>
#include <numeric>
#include <set>
#include <filesystem>
#include <cmath>
#include <sstream>
#include <iomanip>

using namespace std;

// ============================================================================
// Constructor / Destructor
// ============================================================================

CASimulator::CASimulator() {}

CASimulator::~CASimulator() {}

// ============================================================================
// Public Interface
// ============================================================================

int CASimulator::simulate(const string& qstrInitLUCC,
                          const string& qstrPgPath,
                          const string& qstrDemandPath,
                          int nTargetYear,
                          int nIterations,
                          double dConvergence,
                          int nNeighborRadius,
                          const vector<double>& vTypeWeights,
                          double dDecay,
                          int nStepSize,
                          const string& qstrRedlinePath,
                          const unordered_map<int, vector<int>>& transMap,
                          const vector<double>& vDk0,
                          const vector<double>& vMuk,
                          const string& qstrOutputPath,
                          bool bSavePrecision,
                          const string& qstrValidatePath,
                          bool bBinaryMode)
{
    initLUCCPath_   = qstrInitLUCC;
    pgPath_         = qstrPgPath;
    demandPath_     = qstrDemandPath;
    nTargetYear_    = nTargetYear;
    nIterations_    = nIterations;
    dConvergence_   = dConvergence;
    nNeighborRadius_ = nNeighborRadius;
    vTypeWeights_   = vTypeWeights;
    dDecay_         = dDecay;
    nStepSize_      = nStepSize;
    redlinePath_    = qstrRedlinePath;
    outputPath_     = qstrOutputPath;
    bSavePrecision_ = bSavePrecision;
    validatePath_   = qstrValidatePath;
    transMap_       = transMap;
    vDk0_           = vDk0;
    vMuk_           = vMuk;
    bBinaryMode_    = bBinaryMode;

    if (bBinaryMode_) {
        LOG_INFO("CASimulator: Binary CA mode enabled for infrastructure simulation");
    }

    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    LOG_INFO("CASimulator: Starting CA simulation");
    LOG_INFO("  Init LUCC: " + initLUCCPath_);
    LOG_INFO("  Pg: " + pgPath_);
    LOG_INFO("  Demand: " + demandPath_);
    LOG_INFO("  Target year: " + to_string(nTargetYear_));
    LOG_INFO("  Max iterations: " + to_string(nIterations_));
    LOG_INFO("  Convergence: " + to_string(dConvergence_));
    LOG_INFO("  Neighbor radius: " + to_string(nNeighborRadius_));

    // ── Step 1: Load initial LUCC ──
    int ret = loadInitLUCC();
    if (ret != 0) { LOG_ERROR("CASimulator: Failed to load initial LUCC"); return ret; }

    // ── Step 2: Load Pg ──
    ret = loadPg();
    if (ret != 0) { LOG_ERROR("CASimulator: Failed to load Pg"); return ret; }

    // ── Step 3: Load demand ──
    ret = loadDemand();
    if (ret != 0) { LOG_ERROR("CASimulator: Failed to load demand"); return ret; }

    // ── Step 4: Load redline (optional) ──
    if (!redlinePath_.empty()) {
        ret = loadRedline();
        if (ret != 0) { LOG_ERROR("CASimulator: Failed to load redline"); return ret; }
    }

    // ── Step 5: Build transition matrix ──
    // Convert transMap from type codes to type indices
    buildTransitionMatrix();

    // ── Step 6: Set default weights/Dk/Muk ──
    if (vTypeWeights_.empty()) {
        vTypeWeights_.assign(nTypes_, 1.0 / nTypes_);
    }
    if (vDk0.empty()) {
        vDk0_.assign(nTypes_, 1.0);
    } else {
        vDk0_ = vDk0;
    }
    if (vMuk.empty()) {
        vMuk_.assign(nTypes_, 0.5);
    } else {
        vMuk_ = vMuk;
    }

    // ── Step 7: Load validation data (optional) ──
    if (!validatePath_.empty() && bSavePrecision_) {
        GDALDatasetH hValDS = GDALOpenEx(validatePath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
        if (hValDS) {
            vValidateData_.resize(nPixels_);
            GDALRasterIO(GDALGetRasterBand(hValDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                         vValidateData_.data(), nCols_, nRows_, GDT_Int32, 0, 0);
            GDALClose(hValDS);
            bHasValidate_ = true;

            // Open precision CSV
            namespace fs = std::filesystem;
            fs::path precPath = fs::path(outputPath_).parent_path() / "precision_curve.csv";
            precisionCSV_.open(precPath.string());
            if (precisionCSV_.is_open()) {
                precisionCSV_ << "Iteration,FoM,PA,UA";
                for (int k = 0; k < nTypes_; k++) {
                    precisionCSV_ << ",DeltaType" << vTypes_[k];
                }
                precisionCSV_ << "\n";
            }
        }
    }

    // ── Step 8: Determine tiling ──
    determineTiling(nRows_, nCols_, 2048);  // 2GB budget

    // ── Step 9: Initialize type counts ──
    vTypeNums_.assign(nTypes_, 0);
    for (int i = 0; i < nPixels_; i++) {
        auto it = typeToIdx_.find(vCurrentType_[i]);
        if (it != typeToIdx_.end()) {
            vTypeNums_[it->second]++;
        }
    }
    vLastTypeNums_ = vTypeNums_;

    // ── Step 10: Initialize Dk and neighbor arrays ──
    vNeighbor_.assign(nPixels_, vector<float>(nTypes_, 0.0f));
    vDk_.assign(nPixels_, vector<float>(nTypes_, 1.0f));
    vOP_.assign(nPixels_, vector<float>(nTypes_, 0.0f));

    // ── Step 11: CA iteration main loop ──
    nConvergeCount_ = 0;
    int nFinalIter = 0;

    for (int iter = 1; iter <= nIterations_; iter++) {
        // Calculate neighborhood density
        calNeighbor();

        // Calculate adaptive inertia Dk
        calDk(iter);

        // Calculate overall probability
        calOP();

        // Run CA (roulette wheel + transition)
        runCA();

        // Update type counts
        vLastTypeNums_ = vTypeNums_;
        vTypeNums_.assign(nTypes_, 0);
        for (int i = 0; i < nPixels_; i++) {
            auto it = typeToIdx_.find(vCurrentType_[i]);
            if (it != typeToIdx_.end()) {
                vTypeNums_[it->second]++;
            }
        }

        // Log iteration status
        string msg = "[Iteration " + to_string(iter) + "] ";
        for (int k = 0; k < nTypes_; k++) {
            if (k > 0) msg += ", ";
            long long diff = vTypeNums_[k] - vDemand_[k];
            msg += "Type" + to_string(vTypes_[k]) + "=" + to_string(vTypeNums_[k]) +
                   "(demand=" + to_string(vDemand_[k]) + ",diff=" + to_string(diff) + ")";
        }
        LOG_INFO("CASimulator: " + msg);

        // Evaluate precision (optional)
        if (bSavePrecision_ && bHasValidate_) {
            evaluatePrecision(iter);
        }

        // Check convergence
        if (checkConvergence()) {
            LOG_INFO("CASimulator: Converged at iteration " + to_string(iter) +
                     " (consecutive " + to_string(nConvergeCount_) + " rounds below threshold)");
            nFinalIter = iter;
            break;
        }

        nFinalIter = iter;
    }

    if (nConvergeCount_ < 3) {
        LOG_INFO("CASimulator: Reached max iterations (" + to_string(nIterations_) + ")");
    }

    // ── Step 12: Save final result ──
    ret = saveResult(nFinalIter);
    if (ret != 0) { LOG_ERROR("CASimulator: Failed to save result"); return ret; }

    // Close precision CSV
    if (precisionCSV_.is_open()) {
        precisionCSV_.close();
    }

    LOG_INFO("CASimulator: Simulation completed, output: " + outputPath_);
    return 0;
}

// ============================================================================
// Private Methods
// ============================================================================

int CASimulator::loadInitLUCC()
{
    GDALDatasetH hDS = GDALOpenEx(initLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hDS) {
        LOG_ERROR("CASimulator: Cannot open initial LUCC: " + initLUCCPath_);
        return 1;
    }

    nRows_ = GDALGetRasterYSize(hDS);
    nCols_ = GDALGetRasterXSize(hDS);
    nPixels_ = nRows_ * nCols_;

    if (GDALGetRasterCount(hDS) < 1) {
        LOG_ERROR("CASimulator: Initial LUCC has no bands");
        GDALClose(hDS);
        return 1;
    }

    vInitType_.resize(nPixels_);
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 vInitType_.data(), nCols_, nRows_, GDT_Int32, 0, 0);

    // Collect types
    set<int> typeSet;
    int bHasNoData = FALSE;
    double nd = GDALGetRasterNoDataValue(GDALGetRasterBand(hDS, 1), &bHasNoData);
    if (!bHasNoData) nd = -9999.0;

    for (int i = 0; i < nPixels_; i++) {
        if (vInitType_[i] != (int)nd && vInitType_[i] > 0) {
            typeSet.insert(vInitType_[i]);
        }
    }
    vTypes_.assign(typeSet.begin(), typeSet.end());
    nTypes_ = (int)vTypes_.size();

    for (int i = 0; i < nTypes_; i++) {
        typeToIdx_[vTypes_[i]] = i;
    }

    // Copy to current type
    vCurrentType_ = vInitType_;

    GDALClose(hDS);
    LOG_INFO("CASimulator: Initial LUCC loaded, " + to_string(nRows_) + "x" +
             to_string(nCols_) + ", " + to_string(nTypes_) + " types");
    return 0;
}

int CASimulator::loadPg()
{
    GDALDatasetH hDS = GDALOpenEx(pgPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hDS) {
        LOG_ERROR("CASimulator: Cannot open Pg: " + pgPath_);
        return 1;
    }

    int nBands = GDALGetRasterCount(hDS);
    if (nBands != nTypes_) {
        LOG_ERROR("CASimulator: Pg bands (" + to_string(nBands) +
                  ") != types (" + to_string(nTypes_) + ")");
        GDALClose(hDS);
        return 1;
    }

    if (GDALGetRasterXSize(hDS) != nCols_ || GDALGetRasterYSize(hDS) != nRows_) {
        LOG_ERROR("CASimulator: Pg size mismatch");
        GDALClose(hDS);
        return 1;
    }

    vPgData_.assign(nPixels_, vector<float>(nTypes_, 0.0f));

    for (int k = 0; k < nTypes_; k++) {
        vector<float> bandData(nPixels_);
        GDALRasterIO(GDALGetRasterBand(hDS, k + 1), GF_Read, 0, 0, nCols_, nRows_,
                     bandData.data(), nCols_, nRows_, GDT_Float32, 0, 0);
        for (int i = 0; i < nPixels_; i++) {
            vPgData_[i][k] = bandData[i];
        }
    }

    GDALClose(hDS);
    LOG_INFO("CASimulator: Pg loaded, " + to_string(nBands) + " bands");
    return 0;
}

int CASimulator::loadDemand()
{
    ifstream ifs(demandPath_);
    if (!ifs.is_open()) {
        LOG_ERROR("CASimulator: Cannot open demand CSV: " + demandPath_);
        return 1;
    }

    // Parse header
    string line;
    getline(ifs, line);  // Skip header

    // Parse data rows
    map<int, vector<long long>> demandByYear;
    while (getline(ifs, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string token;

        // First column: Year
        getline(ss, token, ',');
        int year = stoi(token);

        // Remaining columns: type counts
        vector<long long> counts;
        while (getline(ss, token, ',')) {
            counts.push_back(stoll(token));
        }

        demandByYear[year] = counts;
    }
    ifs.close();

    if (demandByYear.empty()) {
        LOG_ERROR("CASimulator: No demand data found");
        return 1;
    }

    // Select target year
    int selYear = nTargetYear_;
    if (selYear == 0) {
        selYear = demandByYear.rbegin()->first;
    }

    auto it = demandByYear.find(selYear);
    if (it == demandByYear.end()) {
        LOG_ERROR("CASimulator: Target year " + to_string(selYear) + " not found in demand CSV");
        string available;
        for (auto& [y, _] : demandByYear) {
            if (!available.empty()) available += ",";
            available += to_string(y);
        }
        LOG_ERROR("CASimulator: Available years: " + available);
        return 1;
    }

    vDemand_ = it->second;
    if ((int)vDemand_.size() != nTypes_) {
        LOG_ERROR("CASimulator: Demand columns (" + to_string(vDemand_.size()) +
                  ") != types (" + to_string(nTypes_) + ")");
        return 1;
    }

    LOG_INFO("CASimulator: Demand loaded for year " + to_string(selYear));
    return 0;
}

int CASimulator::loadRedline()
{
    GDALDatasetH hDS = GDALOpenEx(redlinePath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hDS) {
        LOG_ERROR("CASimulator: Cannot open redline: " + redlinePath_);
        return 1;
    }

    if (GDALGetRasterXSize(hDS) != nCols_ || GDALGetRasterYSize(hDS) != nRows_) {
        LOG_ERROR("CASimulator: Redline size mismatch");
        GDALClose(hDS);
        return 1;
    }

    // Read as byte: 0=prohibited, non-zero=allowed
    vector<uint8_t> rawData(nPixels_);
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 rawData.data(), nCols_, nRows_, GDT_Byte, 0, 0);

    vRedline_.resize(nPixels_);
    int bHasNoData = FALSE;
    double nd = GDALGetRasterNoDataValue(GDALGetRasterBand(hDS, 1), &bHasNoData);

    for (int i = 0; i < nPixels_; i++) {
        if (bHasNoData && rawData[i] == (uint8_t)nd) {
            vRedline_[i] = 1;  // NODATA = allowed
        } else {
            vRedline_[i] = (rawData[i] != 0) ? 1 : 0;
        }
    }

    bHasRedline_ = true;
    GDALClose(hDS);

    // Check if all prohibited
    int nAllowed = 0;
    for (int i = 0; i < nPixels_; i++) nAllowed += vRedline_[i];
    if (nAllowed == 0) {
        LOG_WARN("CASimulator: Redline prohibits all development, output will be unchanged");
    }

    LOG_INFO("CASimulator: Redline loaded, " + to_string(nAllowed) + "/" +
             to_string(nPixels_) + " pixels allowed");
    return 0;
}

void CASimulator::buildTransitionMatrix()
{
    // transMatrix_[src_idx] = {dst_idx1, dst_idx2, ...}
    // If empty, all transitions allowed
    transMatrix_.assign(nTypes_, vector<int>());

    if (transMap_.empty()) {
        bHasTransConstraint_ = false;
        return;
    }

    bHasTransConstraint_ = true;
    for (int k = 0; k < nTypes_; k++) {
        int srcType = vTypes_[k];
        auto it = transMap_.find(srcType);
        if (it != transMap_.end()) {
            for (int dstType : it->second) {
                auto dit = typeToIdx_.find(dstType);
                if (dit != typeToIdx_.end()) {
                    transMatrix_[k].push_back(dit->second);
                }
            }
        }
        // If no constraint for this type, leave empty (all allowed)
    }
}

void CASimulator::calNeighbor()
{
    int r = nNeighborRadius_ / 2;  // Half-window size

    // Reset neighbor
    for (int i = 0; i < nPixels_; i++) {
        fill(vNeighbor_[i].begin(), vNeighbor_[i].end(), 0.0f);
    }

    // For each pixel, count types in neighborhood
    for (int row = 0; row < nRows_; row++) {
        for (int col = 0; col < nCols_; col++) {
            int idx = row * nCols_ + col;
            int nNeighbors = 0;

            for (int dr = -r; dr <= r; dr++) {
                for (int dc = -r; dc <= r; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = row + dr;
                    int nc = col + dc;
                    if (nr < 0 || nr >= nRows_ || nc < 0 || nc >= nCols_) continue;

                    int nidx = nr * nCols_ + nc;
                    auto it = typeToIdx_.find(vCurrentType_[nidx]);
                    if (it != typeToIdx_.end()) {
                        vNeighbor_[idx][it->second] += 1.0f;
                    }
                    nNeighbors++;
                }
            }

            // Normalize to proportion
            if (nNeighbors > 0) {
                for (int k = 0; k < nTypes_; k++) {
                    vNeighbor_[idx][k] = vNeighbor_[idx][k] * (float)vTypeWeights_[k] / nNeighbors;
                }
            }
        }
    }
}

void CASimulator::calDk(int nIteration)
{
    // Adaptive inertia coefficient
    // Dk > 1: inhibit transition (type growing too fast)
    // Dk < 1: promote transition (type not meeting demand)
    for (int i = 0; i < nPixels_; i++) {
        for (int k = 0; k < nTypes_; k++) {
            long long diff = vTypeNums_[k] - vDemand_[k];
            long long lastDiff = vLastTypeNums_[k] - vDemand_[k];

            double dk = vDk0_[k];
            if (diff > 0 && diff > lastDiff) {
                // Type growing too fast: increase inertia to inhibit
                dk = 1.0 + (double)nStepSize_ / (fabs(diff) + 1.0);
            } else if (diff < 0 && diff < lastDiff) {
                // Type not meeting demand: decrease inertia to promote
                dk = 1.0 / (1.0 + (double)nStepSize_ / (fabs(diff) + 1.0));
            }

            vDk_[i][k] = (float)dk;
        }
    }
}

void CASimulator::calOP()
{
    // OP = Pg * Neighbor * Dk * Muk * RedLine * random
    uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int i = 0; i < nPixels_; i++) {
        float redline = bHasRedline_ ? (float)vRedline_[i] : 1.0f;

        for (int k = 0; k < nTypes_; k++) {
            float pg = vPgData_[i][k];
            float neighbor = vNeighbor_[i][k];
            float dk = vDk_[i][k];
            float muk = (float)vMuk_[k];
            float rnd = dist(rng_);

            vOP_[i][k] = pg * (1.0f + neighbor) * dk * muk * redline * rnd;
        }
    }
}

void CASimulator::runCA()
{
    // ── Threshold decay τ ──
    // τ decreases each iteration: τ = τ * δ
    // Start with τ = 1.0, decay by dDecay_ each iteration

    // ── For each pixel, decide if it should change ──
    // Roulette wheel: find the type with max OP that is different from current
    // and is allowed by transition matrix

    // Create pixel index permutation for randomization
    vector<int> pixelOrder(nPixels_);
    iota(pixelOrder.begin(), pixelOrder.end(), 0);
    shuffle(pixelOrder.begin(), pixelOrder.end(), rng_);

    // Track remaining demand
    vector<long long> vRemainingDemand = vDemand_;
    for (int k = 0; k < nTypes_; k++) {
        vRemainingDemand[k] -= vTypeNums_[k];
    }

    for (int i : pixelOrder) {
        int currType = vCurrentType_[i];
        auto currIt = typeToIdx_.find(currType);
        if (currIt == typeToIdx_.end()) continue;
        int currIdx = currIt->second;

        // Check redline
        if (bHasRedline_ && vRedline_[i] == 0) continue;

        // Find best target type (max OP, different from current, allowed by transition)
        int bestIdx = -1;
        float bestOP = 0.0f;

        for (int k = 0; k < nTypes_; k++) {
            if (k == currIdx) continue;  // Skip current type

            // Check transition constraint
            if (bHasTransConstraint_ && !transMatrix_[currIdx].empty()) {
                bool bAllowed = false;
                for (int allowed : transMatrix_[currIdx]) {
                    if (allowed == k) { bAllowed = true; break; }
                }
                if (!bAllowed) continue;
            }

            // Check demand: only transition if target type needs more
            if (vRemainingDemand[k] <= 0) continue;

            if (vOP_[i][k] > bestOP) {
                bestOP = vOP_[i][k];
                bestIdx = k;
            }
        }

        if (bestIdx < 0) continue;  // No valid transition

        // Apply threshold: only transition if OP exceeds threshold
        // Threshold decays with iteration
        // (simplified: use Muk as threshold)
        if (bestOP < (float)vMuk_[bestIdx]) continue;

        // Apply transition
        vCurrentType_[i] = vTypes_[bestIdx];
        vRemainingDemand[currIdx]++;
        vRemainingDemand[bestIdx]--;
    }
}

bool CASimulator::checkConvergence()
{
    // Check if all type changes are below convergence threshold
    long long nTotalPixels = nPixels_;
    bool bConverged = true;

    for (int k = 0; k < nTypes_; k++) {
        long long diff = abs(vTypeNums_[k] - vLastTypeNums_[k]);
        if (diff > (long long)(dConvergence_ * nTotalPixels)) {
            bConverged = false;
            break;
        }
    }

    if (bConverged) {
        nConvergeCount_++;
    } else {
        nConvergeCount_ = 0;
    }

    return nConvergeCount_ >= 3;
}

int CASimulator::saveResult(int nIteration)
{
    // ── Ensure output directory exists ──
    namespace fs = std::filesystem;
    fs::path outPath(outputPath_);
    fs::path outDir = outPath.parent_path();
    if (!outDir.empty() && !fs::exists(outDir)) {
        fs::create_directories(outDir);
    }

    // ── Get geotransform and projection from initial LUCC ──
    GDALDatasetH hRefDS = GDALOpenEx(initLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hRefDS) {
        LOG_ERROR("CASimulator: Cannot open reference for geotransform");
        return 1;
    }
    double adfGeoTransform[6];
    GDALGetGeoTransform(hRefDS, adfGeoTransform);
    const char* pszProj = GDALGetProjectionRef(hRefDS);
    string strProj = pszProj ? pszProj : "";
    GDALClose(hRefDS);

    // ── Create output GeoTIFF ──
    GDALDriverH hDriver = GDALGetDriverByName("GTiff");
    if (!hDriver) {
        LOG_ERROR("CASimulator: GTiff driver not available");
        return 1;
    }

    // Check if output exists
    if (fs::exists(outputPath_)) {
        LOG_INFO("CASimulator: Overwriting existing output: " + outputPath_);
    }

    GDALDatasetH hDstDS = GDALCreate(hDriver, outputPath_.c_str(), nCols_, nRows_,
                                      1, GDT_Int32, nullptr);
    if (!hDstDS) {
        LOG_ERROR("CASimulator: Cannot create output: " + outputPath_);
        return 1;
    }

    GDALSetGeoTransform(hDstDS, adfGeoTransform);
    if (!strProj.empty()) {
        GDALSetProjection(hDstDS, strProj.c_str());
    }

    // Write data
    CPLErr err = GDALRasterIO(GDALGetRasterBand(hDstDS, 1), GF_Write, 0, 0, nCols_, nRows_,
                               vCurrentType_.data(), nCols_, nRows_, GDT_Int32, 0, 0);
    if (err != CE_None) {
        LOG_ERROR("CASimulator: Failed to write output");
        GDALClose(hDstDS);
        return 1;
    }

    GDALClose(hDstDS);
    LOG_INFO("CASimulator: Result saved at iteration " + to_string(nIteration));
    return 0;
}

void CASimulator::evaluatePrecision(int nIteration)
{
    if (!bHasValidate_) return;

    // Compute FoM, PA, UA
    int A = 0, B = 0, C = 0, D = 0;

    for (int i = 0; i < nPixels_; i++) {
        if (vValidateData_[i] <= 0) continue;

        bool bObservedChange = (vInitType_[i] != vValidateData_[i]);
        bool bPredictedChange = (vInitType_[i] != vCurrentType_[i]);

        if (bObservedChange && bPredictedChange) {
            if (vCurrentType_[i] == vValidateData_[i]) A++;
            else C++;
        } else if (bObservedChange && !bPredictedChange) {
            B++;
        } else if (!bObservedChange && bPredictedChange) {
            C++;
        } else {
            D++;
        }
    }

    double FoM = (A + B + C) > 0 ? (double)A / (A + B + C) : 0.0;
    double PA  = (A + B) > 0 ? (double)A / (A + B) : 0.0;
    double UA  = (A + C) > 0 ? (double)A / (A + C) : 0.0;

    // Write to precision CSV
    if (precisionCSV_.is_open()) {
        precisionCSV_ << nIteration << "," << fixed << setprecision(6)
                      << FoM << "," << PA << "," << UA;
        for (int k = 0; k < nTypes_; k++) {
            long long diff = vTypeNums_[k] - vDemand_[k];
            precisionCSV_ << "," << diff;
        }
        precisionCSV_ << "\n";
    }

    LOG_INFO("CASimulator: [Iteration " + to_string(nIteration) + "] FoM=" +
             to_string(FoM) + " PA=" + to_string(PA) + " UA=" + to_string(UA));
}

void CASimulator::determineTiling(int nTotalRows, int nTotalCols, size_t nMemBudgetMB)
{
    // Per-pixel memory: (nTypes * 3 + 2) * sizeof(float)  // OP + Pg + Neighbor + Type + RedLine
    size_t nBytesPerPixel = (nTypes_ * 3 + 2) * sizeof(float);
    size_t nTotalBytes = (size_t)nTotalRows * nTotalCols * nBytesPerPixel;

    if (nTotalBytes <= nMemBudgetMB * 1024 * 1024) {
        // Full load
        nTileCols_ = 0;
        nTileRows_ = 0;
        LOG_INFO("CASimulator: Full load mode, memory estimate: " +
                 to_string(nTotalBytes / 1024 / 1024) + " MB");
        return;
    }

    // Calculate tiling
    int nTiles = (int)ceil((double)nTotalBytes / (nMemBudgetMB * 1024 * 1024));
    int nSide = (int)ceil(sqrt(nTiles));
    nTileCols_ = (int)ceil((double)nTotalCols / nSide);
    nTileRows_ = (int)ceil((double)nTotalRows / nSide);
    nHaloWidth_ = (nNeighborRadius_ - 1) / 2;

    LOG_INFO("CASimulator: Tiled mode: " + to_string(nSide * nSide) + " tiles, " +
             to_string(nTileCols_) + "x" + to_string(nTileRows_) + " per tile, halo=" +
             to_string(nHaloWidth_));
}

int CASimulator::simulateMultiScenario(const string& qstrInitLUCC,
                                       const string& qstrPgPath,
                                       const string& qstrDemandPath,
                                       const string& qstrScenarios,
                                       const string& qstrTargetYears,
                                       int nIterations,
                                       double dConvergence,
                                       int nNeighborRadius,
                                       const vector<double>& vTypeWeights,
                                       double dDecay,
                                       int nStepSize,
                                       const string& qstrRedlinePath,
                                       const unordered_map<int, vector<int>>& transMap,
                                       const vector<double>& vDk0,
                                       const vector<double>& vMuk,
                                       const string& qstrOutputDir,
                                       bool bSavePrecision,
                                       const string& qstrValidatePath)
{
    namespace fs = std::filesystem;

    // Parse scenarios and target years
    vector<string> vScenarios;
    vector<int> vTargetYears;
    vector<string> vTargetYearsStr;

    {
        stringstream ssSc(qstrScenarios);
        string token;
        while (getline(ssSc, token, ',')) {
            vScenarios.push_back(token);
        }
    }
    {
        stringstream ssYr(qstrTargetYears);
        string token;
        while (getline(ssYr, token, ',')) {
            vTargetYearsStr.push_back(token);
        }
    }

    if (vScenarios.size() != vTargetYearsStr.size()) {
        LOG_ERROR("CASimulator: Number of scenarios (" + to_string(vScenarios.size()) +
                   ") does not match number of target years (" + to_string(vTargetYearsStr.size()) + ")");
        return 1;
    }

    for (const auto& yrStr : vTargetYearsStr) {
        try {
            vTargetYears.push_back(stoi(yrStr));
        } catch (...) {
            LOG_ERROR("CASimulator: Invalid target year: " + yrStr);
            return 1;
        }
    }

    int nScenarios = (int)vScenarios.size();
    LOG_INFO("CASimulator: Starting multi-scenario simulation with " + to_string(nScenarios) + " scenarios");

    // Create output directory
    fs::path outDir(qstrOutputDir);
    if (!fs::exists(outDir)) {
        fs::create_directories(outDir);
    }

    // Load initial LUCC and Pg once (they are shared across scenarios)
    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    // Store original paths for reuse
    initLUCCPath_   = qstrInitLUCC;
    pgPath_         = qstrPgPath;
    demandPath_     = qstrDemandPath;
    nIterations_    = nIterations;
    dConvergence_   = dConvergence;
    nNeighborRadius_ = nNeighborRadius;
    vTypeWeights_   = vTypeWeights;
    dDecay_         = dDecay;
    nStepSize_      = nStepSize;
    redlinePath_    = qstrRedlinePath;
    bSavePrecision_ = bSavePrecision;
    validatePath_   = qstrValidatePath;
    transMap_       = transMap;
    vDk0_           = vDk0;
    vMuk_           = vMuk;

    // Step 1: Load initial LUCC (shared)
    int ret = loadInitLUCC();
    if (ret != 0) { LOG_ERROR("CASimulator: Failed to load initial LUCC"); return ret; }

    // Step 2: Load Pg (shared)
    ret = loadPg();
    if (ret != 0) { LOG_ERROR("CASimulator: Failed to load Pg"); return ret; }

    // Step 3: Load redline (optional, shared)
    if (!redlinePath_.empty()) {
        ret = loadRedline();
        if (ret != 0) { LOG_ERROR("CASimulator: Failed to load redline"); return ret; }
    }

    // Run simulation for each scenario
    for (int s = 0; s < nScenarios; s++) {
        const string& scenarioName = vScenarios[s];
        int nTargetYear = vTargetYears[s];

        LOG_INFO("CASimulator: === Scenario: " + scenarioName +
                 " (target year: " + to_string(nTargetYear) + ") ===");

        // Update target year
        nTargetYear_ = nTargetYear;

        // Create scenario-specific output path
        fs::path scenarioOutPath = outDir / (scenarioName + "_" + to_string(nTargetYear) + ".tif");
        outputPath_ = scenarioOutPath.string();

        // Load demand for this scenario
        ret = loadDemand();
        if (ret != 0) {
            LOG_ERROR("CASimulator: Failed to load demand for scenario: " + scenarioName);
            return ret;
        }

        // Reset current LUCC to initial state for each scenario
        vCurrentType_ = vInitType_;

        // Main CA iteration loop
        nConvergeCount_ = 0;

        for (int iter = 0; iter < nIterations_; iter++) {
            LOG_INFO("CASimulator: Scenario " + scenarioName +
                     " - Iteration " + to_string(iter + 1) + "/" + to_string(nIterations_));

            runCA();

            if (checkConvergence()) {
                nConvergeCount_++;
                if (nConvergeCount_ >= 3) {
                    LOG_INFO("CASimulator: Scenario " + scenarioName +
                             " converged at iteration " + to_string(iter + 1));
                    break;
                }
            } else {
                nConvergeCount_ = 0;
            }

            // Save intermediate result every 5 iterations
            if ((iter + 1) % 5 == 0) {
                string iterPath = scenarioOutPath.string() + ".iter" + to_string(iter + 1);
                saveResult(iter + 1);
            }
        }

        // Final save
        ret = saveResult(nIterations_);
        if (ret != 0) {
            LOG_ERROR("CASimulator: Failed to save result for scenario: " + scenarioName);
            return ret;
        }

        // Evaluate precision (if validate data provided)
        if (!validatePath_.empty()) {
            evaluatePrecision(nIterations_);
        }

        LOG_INFO("CASimulator: Scenario " + scenarioName + " completed");
    }

    LOG_INFO("CASimulator: All " + to_string(nScenarios) + " scenarios completed");
    return 0;
}
