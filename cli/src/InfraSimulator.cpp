/**
 * @file InfraSimulator.cpp
 * @brief 基础设施二值 CA 模拟实现 / Infrastructure binary CA simulation implementation
 */

#include "InfraSimulator.h"
#include "Logger.h"

#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h>

#include <algorithm>
#include <numeric>
#include <filesystem>
#include <cmath>
#include <sstream>
#include <iomanip>

using namespace std;

// ============================================================================
// Constructor / Destructor
// ============================================================================

InfraSimulator::InfraSimulator() {}

InfraSimulator::~InfraSimulator() {}

// ============================================================================
// Public Interface
// ============================================================================

int InfraSimulator::simulate(const string& qstrInitLUCC,
                            const string& qstrInfraZonePath,
                            const string& qstrDemandPath,
                            int nTargetYear,
                            int nIterations,
                            double dConvergence,
                            int nNeighborRadius,
                            double dDecay,
                            const string& qstrOutputPath,
                            const string& qstrValidatePath)
{
    initLUCCPath_   = qstrInitLUCC;
    infraZonePath_  = qstrInfraZonePath;
    demandPath_     = qstrDemandPath;
    nTargetYear_    = nTargetYear;
    nIterations_    = nIterations;
    dConvergence_   = dConvergence;
    nNeighborRadius_ = nNeighborRadius;
    dDecay_         = dDecay;
    outputPath_     = qstrOutputPath;
    validatePath_   = qstrValidatePath;

    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    LOG_INFO("InfraSimulator: Starting infrastructure binary CA simulation");
    LOG_INFO("  Init LUCC: " + initLUCCPath_);
    LOG_INFO("  Infra zone: " + infraZonePath_);
    LOG_INFO("  Demand: " + demandPath_);
    LOG_INFO("  Target year: " + to_string(nTargetYear_));
    LOG_INFO("  Max iterations: " + to_string(nIterations_));
    LOG_INFO("  Neighbor radius: " + to_string(nNeighborRadius_));

    // Step 1: Load initial LUCC
    int ret = loadInitLUCC();
    if (ret != 0) { LOG_ERROR("InfraSimulator: Failed to load initial LUCC"); return ret; }

    // Step 2: Load infrastructure priority zone
    ret = loadInfraZone();
    if (ret != 0) { LOG_ERROR("InfraSimulator: Failed to load infra zone"); return ret; }

    // Step 3: Load demand
    ret = loadDemand();
    if (ret != 0) { LOG_ERROR("InfraSimulator: Failed to load demand"); return ret; }

    // Step 4: Load validation data (optional)
    if (!validatePath_.empty()) {
        GDALDatasetH hValDS = GDALOpenEx(validatePath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
        if (!hValDS) {
            LOG_WARN("InfraSimulator: Cannot open validation LUCC, skipping validation");
        } else {
            vValidateBinary_.resize(nPixels_);
            vector<int> valData(nPixels_);
            GDALRasterIO(GDALGetRasterBand(hValDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                         valData.data(), nCols_, nRows_, GDT_Int32, 0, 0);
            for (int i = 0; i < nPixels_; i++) {
                vValidateBinary_[i] = (valData[i] == infraTypeCode_) ? 1 : 0;
            }
            GDALClose(hValDS);
            bHasValidate_ = true;
        }
    }

    // Open precision tracking CSV if enabled
    string precisionPath = outputPath_ + ".precision.csv";
    precisionCSV_.open(precisionPath);
    if (precisionCSV_.is_open()) {
        precisionCSV_ << "Iteration,Occupied,Difference,Convergence\n";
    }

    // Step 5: Main CA iteration loop
    nConvergeCount_ = 0;
    for (int iter = 0; iter < nIterations_; iter++) {
        LOG_INFO("InfraSimulator: Iteration " + to_string(iter + 1) + "/" + to_string(nIterations_));

        // Calculate neighborhood density
        calNeighbor();

        // Calculate adaptive inertia
        calInertia(iter);

        // Run CA roulette wheel selection + transition
        runCA();

        // Check convergence
        if (checkConvergence()) {
            nConvergeCount_++;
            if (nConvergeCount_ >= 3) {
                LOG_INFO("InfraSimulator: Converged at iteration " + to_string(iter + 1));
                break;
            }
        } else {
            nConvergeCount_ = 0;
        }

        // Write precision tracking
        if (precisionCSV_.is_open()) {
            long long diff = llabs(nCurrentOccupied_ - nDemand_);
            precisionCSV_ << (iter + 1) << "," << nCurrentOccupied_
                        << "," << diff << "," << (nConvergeCount_ >= 3 ? "YES" : "NO") << "\n";
        }
    }

    if (precisionCSV_.is_open()) {
        precisionCSV_.close();
    }

    // Step 6: Save final result
    ret = saveResult(nIterations_);
    if (ret != 0) {
        LOG_ERROR("InfraSimulator: Failed to save result");
        return ret;
    }

    // Step 7: Evaluate precision (if validation data provided)
    if (bHasValidate_) {
        evaluatePrecision(nIterations_);
    }

    LOG_INFO("InfraSimulator: Simulation completed");
    return 0;
}

// ============================================================================
// Private Methods
// ============================================================================

int InfraSimulator::loadInitLUCC()
{
    GDALDatasetH hDS = GDALOpenEx(initLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hDS) {
        LOG_ERROR("InfraSimulator: Cannot open LUCC: " + initLUCCPath_);
        return 1;
    }

    nRows_ = GDALGetRasterYSize(hDS);
    nCols_ = GDALGetRasterXSize(hDS);
    nPixels_ = nRows_ * nCols_;

    // Read as binary: infraTypeCode_ = occupied (1), others = unoccupied (0)
    vector<int> luccData(nPixels_);
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 luccData.data(), nCols_, nRows_, GDT_Int32, 0, 0);

    vInitBinary_.resize(nPixels_);
    vCurrentBinary_.resize(nPixels_);
    nCurrentOccupied_ = 0;

    for (int i = 0; i < nPixels_; i++) {
        vInitBinary_[i] = (luccData[i] == infraTypeCode_) ? 1 : 0;
        vCurrentBinary_[i] = vInitBinary_[i];
        if (vCurrentBinary_[i] == 1) {
            nCurrentOccupied_++;
        }
    }

    GDALClose(hDS);
    LOG_INFO("InfraSimulator: Initial occupied pixels: " + to_string(nCurrentOccupied_));
    return 0;
}

int InfraSimulator::loadInfraZone()
{
    GDALDatasetH hDS = GDALOpenEx(infraZonePath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hDS) {
        LOG_ERROR("InfraSimulator: Cannot open infra zone: " + infraZonePath_);
        return 1;
    }

    if (GDALGetRasterXSize(hDS) != nCols_ || GDALGetRasterYSize(hDS) != nRows_) {
        LOG_ERROR("InfraSimulator: Infra zone size mismatch");
        GDALClose(hDS);
        return 1;
    }

    vInfraZone_.resize(nPixels_);
    GDALRasterIO(GDALGetRasterBand(hDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 vInfraZone_.data(), nCols_, nRows_, GDT_Float32, 0, 0);

    GDALClose(hDS);
    return 0;
}

int InfraSimulator::loadDemand()
{
    ifstream ifs(demandPath_);
    if (!ifs.is_open()) {
        LOG_ERROR("InfraSimulator: Cannot open demand CSV: " + demandPath_);
        return 1;
    }

    string line;
    vector<pair<int, long long>> vDemandData;  // (year, occupied_count)

    while (getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;

        stringstream ss(line);
        string yearStr, type1Str;

        if (!getline(ss, yearStr, ',') || !getline(ss, type1Str, ',')) {
            continue;
        }

        try {
            int year = stoi(yearStr);
            long long count = stoll(type1Str);
            vDemandData.push_back({year, count});
        } catch (...) {
            continue;
        }
    }
    ifs.close();

    if (vDemandData.empty()) {
        LOG_ERROR("InfraSimulator: No demand data found in CSV");
        return 1;
    }

    // Find target year data
    if (nTargetYear_ == 0) {
        // Use last row
        nDemand_ = vDemandData.back().second;
    } else {
        // Find exact year or interpolate
        nDemand_ = 0;
        for (size_t i = 0; i < vDemandData.size(); i++) {
            if (vDemandData[i].first == nTargetYear_) {
                nDemand_ = vDemandData[i].second;
                break;
            }
        }
        if (nDemand_ == 0) {
            // Use closest year
            nDemand_ = vDemandData.back().second;
        }
    }

    LOG_INFO("InfraSimulator: Target demand (occupied pixels): " + to_string(nDemand_));
    return 0;
}

void InfraSimulator::calNeighbor()
{
    int radius = (nNeighborRadius_ - 1) / 2;
    vNeighbor_.assign(nPixels_, 0.0f);

#if defined(_OPENMP)
    #pragma omp parallel for schedule(dynamic, 1000)
#endif
    for (int row = 0; row < nRows_; row++) {
        for (int col = 0; col < nCols_; col++) {
            int idx = row * nCols_ + col;

            // Skip already occupied pixels
            if (vCurrentBinary_[idx] == 1) {
                vNeighbor_[idx] = 1.0f;
                continue;
            }

            // Count neighboring occupied pixels
            int count = 0;
            int nCount = 0;
            for (int dr = -radius; dr <= radius; dr++) {
                for (int dc = -radius; dc <= radius; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = row + dr;
                    int nc = col + dc;
                    if (nr >= 0 && nr < nRows_ && nc >= 0 && nc < nCols_) {
                        int nIdx = nr * nCols_ + nc;
                        if (vCurrentBinary_[nIdx] == 1) {
                            count++;
                        }
                        nCount++;
                    }
                }
            }

            vNeighbor_[idx] = (nCount > 0) ? ((float)count / nCount) : 0.0f;
        }
    }
}

void InfraSimulator::calInertia(int nIteration)
{
    float dK = (float)pow(dDecay_, nIteration);

#if defined(_OPENMP)
    #pragma omp parallel for schedule(dynamic, 1000)
#endif
    for (int i = 0; i < nPixels_; i++) {
        if (vCurrentBinary_[i] == 1) {
            vInertia_[i] = 1.0f;
        } else {
            // Inertia based on distance to demand
            float diff = (float)llabs(nCurrentOccupied_ - nDemand_);
            float dInertia = (diff > 0) ? (1.0f - dK * diff / (diff + 1.0f)) : 0.0f;
            vInertia_[i] = max(0.0f, min(1.0f, dInertia));
        }
    }
}

void InfraSimulator::runCA()
{
    long long nTargetToOccupy = nDemand_ - nCurrentOccupied_;
    if (nTargetToOccupy <= 0) {
        LOG_INFO("InfraSimulator: Demand already satisfied");
        return;
    }

    // Calculate overall probability OP = Pg * neighbor * inertia
    vector<float> vOP(nPixels_, 0.0f);
    float fSum = 0.0f;

#if defined(_OPENMP)
    #pragma omp parallel for reduction(+:fSum)
#endif
    for (int i = 0; i < nPixels_; i++) {
        if (vCurrentBinary_[i] == 0) {
            vOP[i] = vInfraZone_[i] * vNeighbor_[i] * vInertia_[i];
            fSum += vOP[i];
        }
    }

    if (fSum < 1e-10) {
        LOG_WARN("InfraSimulator: Total probability too low, cannot occupy more pixels");
        return;
    }

    // Normalize to get probability
#if defined(_OPENMP)
    #pragma omp parallel for
#endif
    for (int i = 0; i < nPixels_; i++) {
        if (vCurrentBinary_[i] == 0 && fSum > 1e-10) {
            vOP[i] /= fSum;
        }
    }

    // Roulette wheel selection
    uniform_real_distribution<double> dist(0.0, 1.0);
    long long nOccupyCount = 0;

    vector<int> vCandidateIdx;
    for (int i = 0; i < nPixels_; i++) {
        if (vCurrentBinary_[i] == 0 && vOP[i] > 0) {
            vCandidateIdx.push_back(i);
        }
    }

    // Sort by probability descending
    sort(vCandidateIdx.begin(), vCandidateIdx.end(),
         [&vOP](int a, int b) { return vOP[a] > vOP[b]; });

    // Occupy pixels with highest probability until demand is met
    for (int idx : vCandidateIdx) {
        if (nOccupyCount >= nTargetToOccupy) break;

        double rnd = dist(rng_);
        if ((double)vOP[idx] > rnd) {
            vCurrentBinary_[idx] = 1;
            nOccupyCount++;
            nCurrentOccupied_++;
        }
    }

    LOG_INFO("InfraSimulator: Occupied " + to_string(nOccupyCount) +
             " new pixels, total: " + to_string(nCurrentOccupied_));
}

bool InfraSimulator::checkConvergence()
{
    long long diff = llabs(nCurrentOccupied_ - nDemand_);
    double ratio = (nDemand_ > 0) ? ((double)diff / nDemand_) : 0.0;
    return (ratio < dConvergence_);
}

int InfraSimulator::saveResult(int nIteration)
{
    namespace fs = std::filesystem;

    // Create output directory if needed
    fs::path outPath(outputPath_);
    fs::path outDir = outPath.parent_path();
    if (!outDir.empty() && !fs::exists(outDir)) {
        fs::create_directories(outDir);
    }

    // Convert binary result back to LUCC format
    vector<int> outputData(nPixels_);
    for (int i = 0; i < nPixels_; i++) {
        outputData[i] = (vCurrentBinary_[i] == 1) ? infraTypeCode_ : 0;
    }

    // Get driver
    GDALDriverH hDriver = GDALGetDriverByName("GTiff");
    if (!hDriver) {
        LOG_ERROR("InfraSimulator: Cannot get GTiff driver");
        return 1;
    }

    // Create dataset
    GDALDatasetH hOutDS = GDALCreate(hDriver, outputPath_.c_str(), nCols_, nRows_,
                                     1, GDT_Int32, nullptr);
    if (!hOutDS) {
        LOG_ERROR("InfraSimulator: Cannot create output dataset: " + outputPath_);
        return 1;
    }

    // Set geotransform and projection from original if available
    GDALDatasetH hSrcDS = GDALOpenEx(initLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (hSrcDS) {
        double transform[6];
        if (GDALGetGeoTransform(hSrcDS, transform) == CE_None) {
            GDALSetGeoTransform(hOutDS, transform);
        }
        const char* proj = GDALGetProjectionRef(hSrcDS);
        if (proj && strlen(proj) > 0) {
            GDALSetProjection(hOutDS, proj);
        }
        GDALClose(hSrcDS);
    }

    // Write data
    GDALRasterIO(GDALGetRasterBand(hOutDS, 1), GF_Write, 0, 0, nCols_, nRows_,
                 outputData.data(), nCols_, nRows_, GDT_Int32, 0, 0);

    // Set NoData value
    GDALSetRasterNoDataValue(GDALGetRasterBand(hOutDS, 1), 0);

    GDALClose(hOutDS);
    LOG_INFO("InfraSimulator: Result saved to " + outputPath_);

    return 0;
}

void InfraSimulator::evaluatePrecision(int nIteration)
{
    if (!bHasValidate_ || vValidateBinary_.size() != nPixels_) {
        return;
    }

    // Calculate binary accuracy metrics
    long long TP = 0, FP = 0, TN = 0, FN = 0;
    for (int i = 0; i < nPixels_; i++) {
        int pred = vCurrentBinary_[i];
        int actual = vValidateBinary_[i];

        if (pred == 1 && actual == 1) TP++;
        else if (pred == 1 && actual == 0) FP++;
        else if (pred == 0 && actual == 0) TN++;
        else if (pred == 0 && actual == 1) FN++;
    }

    double OA = (double)(TP + TN) / nPixels_;
    double PA = (TP + FN > 0) ? (double)TP / (TP + FN) : 0.0;  // Producer's accuracy
    double UA = (TP + FP > 0) ? (double)TP / (TP + FP) : 0.0;  // User's accuracy
    double Kappa = 0.0;
    double po = OA;
    double pe = (double)(TP + FP) * (TP + FN) + (TN + FP) * (TN + FN);
    pe = pe / (nPixels_ * nPixels_);
    if (1.0 - pe > 1e-15) {
        Kappa = (po - pe) / (1.0 - pe);
    }

    LOG_RESULT("sdg-infra-simulate", "FoM", (double)TP / (TP + FN + 0.1));
    LOG_RESULT("sdg-infra-simulate", "PA", PA);
    LOG_RESULT("sdg-infra-simulate", "UA", UA);
    LOG_RESULT("sdg-infra-simulate", "Kappa", Kappa);
    LOG_RESULT("sdg-infra-simulate", "OA", OA);

    cout << fixed << setprecision(6);
    cout << "FoM=" << (double)TP / (TP + FN + 0.1)
         << " PA=" << PA << " UA=" << UA
         << " Kappa=" << Kappa << " OA=" << OA << endl;
}
