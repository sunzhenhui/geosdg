/**
 * @file PgEstimator.cpp
 * @brief 土地利用转换概率挖掘实现 / Pg estimation implementation
 */

#include "PgEstimator.h"
#include "Logger.h"

#include <gdal.h>
#include <gdal_priv.h>
#include <cpl_conv.h>
#include <ogr_api.h>
#include <ogrsf_frmts.h>

#include <algorithm>
#include <random>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <set>
#include <map>

// alglib headers
#include "dataanalysis.h"

using namespace std;

// ============================================================================
// Constructor / Destructor
// ============================================================================

PgEstimator::PgEstimator() {}

PgEstimator::~PgEstimator() {}

// ============================================================================
// Public Interface
// ============================================================================

int PgEstimator::estimate(const string& qstrTrainLUCC,
                          const string& qstrCurrLUCC,
                          const vector<string>& vDriverPaths,
                          int nMethod,
                          int nRFTrees,
                          double dSelectRate,
                          const string& qstrSpecialShp,
                          const string& qstrValidate,
                          const string& qstrOutputPath,
                          int nMaxPerType,
                          int nMaxNegPerType,
                          bool bUseOpenMP,
                          const string& qstrModelPath)
{
    trainLUCCPath_  = qstrTrainLUCC;
    currLUCCPath_   = qstrCurrLUCC;
    vDriverPaths_   = vDriverPaths;
    nMethod_        = nMethod;
    nRFTrees_       = nRFTrees;
    dSelectRate_    = dSelectRate;
    specialShpPath_ = qstrSpecialShp;
    validatePath_   = qstrValidate;
    outputPath_     = qstrOutputPath;
    nMaxPerType_    = nMaxPerType;
    nMaxNegPerType_ = nMaxNegPerType;
    bUseOpenMP_     = bUseOpenMP;
    modelPath_      = qstrModelPath;

    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    LOG_INFO("PgEstimator: Starting Pg estimation");
    LOG_INFO("  Train LUCC: " + trainLUCCPath_);
    LOG_INFO("  Current LUCC: " + currLUCCPath_);
    LOG_INFO("  Drivers: " + to_string(vDriverPaths_.size()) + " files");

    string methodName;
    switch (nMethod_) {
        case 0: methodName = "RF"; break;
        case 1: methodName = "ANN"; break;
        case 2: methodName = "Logit"; break;
        default: methodName = "Unknown";
    }
    LOG_INFO("  Method: " + methodName);
    LOG_INFO("  Sampling limits: pos=" + to_string(nMaxPerType_) + ", neg=" + to_string(nMaxNegPerType_));
    LOG_INFO("  OpenMP: " + string(bUseOpenMP_ ? "enabled" : "disabled"));

    if (nMethod_ == 0) {
        LOG_INFO("  RF trees: " + to_string(nRFTrees_) + ", select rate: " + to_string(dSelectRate_));
    }

    // ── Step 1: Validate inputs ──
    if (!validateInputs()) {
        LOG_ERROR("PgEstimator: Input validation failed");
        return 1;
    }
    LOG_INFO("PgEstimator: Input validation passed, grid size: " +
             to_string(nRows_) + "x" + to_string(nCols_) + ", types: " + to_string(nTypes_));

    // ── Step 2: Collect sampling points ──
    collectSamplingPoints();
    if (vSampleFeatures_.empty()) {
        LOG_ERROR("PgEstimator: No valid training samples collected");
        return 1;
    }
    LOG_INFO("PgEstimator: Sampling completed, N=" + to_string(vSampleFeatures_.size()) +
             " points, " + to_string(nTypes_) + " types");

    // ── Step 3: Train model ──
    int ret = 0;
    if (nMethod_ == 0) {
        ret = trainRF();
    } else if (nMethod_ == 1) {
        ret = trainANN();
    } else if (nMethod_ == 2) {
        ret = trainLogit();
    } else {
        LOG_ERROR("PgEstimator: Unknown method: " + to_string(nMethod_));
        return 1;
    }
    if (ret != 0) {
        LOG_ERROR("PgEstimator: Model training failed");
        return ret;
    }
    LOG_INFO("PgEstimator: Model training completed");

    // ── Step 4: Predict Pg ──
    ret = predictPg();
    if (ret != 0) {
        LOG_ERROR("PgEstimator: Prediction failed");
        return ret;
    }
    LOG_INFO("PgEstimator: Pg prediction completed");

    // ── Step 5: Write output ──
    ret = writePgTiff();
    if (ret != 0) {
        LOG_ERROR("PgEstimator: Failed to write output");
        return ret;
    }
    LOG_INFO("PgEstimator: Pg output written to " + outputPath_);

    // ── Step 6: Evaluate accuracy (optional) ──
    if (!validatePath_.empty()) {
        evaluateAccuracy();
    }

    return 0;
}

// ============================================================================
// Private Methods
// ============================================================================

bool PgEstimator::validateInputs()
{
    // ── Open train LUCC ──
    GDALDatasetH hTrainDS = GDALOpenEx(trainLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hTrainDS) {
        LOG_ERROR("PgEstimator: Cannot open train LUCC: " + trainLUCCPath_);
        return false;
    }
    nRows_ = GDALGetRasterYSize(hTrainDS);
    nCols_ = GDALGetRasterXSize(hTrainDS);
    if (GDALGetRasterCount(hTrainDS) < 1) {
        LOG_ERROR("PgEstimator: Train LUCC has no bands");
        GDALClose(hTrainDS);
        return false;
    }
    GDALRasterBandH hTrainBand = GDALGetRasterBand(hTrainDS, 1);
    int bHasNoData = FALSE;
    double trainND = GDALGetRasterNoDataValue(hTrainBand, &bHasNoData);
    if (!bHasNoData) trainND = -9999.0;

    // ── Open current LUCC ──
    GDALDatasetH hCurrDS = GDALOpenEx(currLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hCurrDS) {
        LOG_ERROR("PgEstimator: Cannot open current LUCC: " + currLUCCPath_);
        GDALClose(hTrainDS);
        return false;
    }
    if (GDALGetRasterXSize(hCurrDS) != nCols_ || GDALGetRasterYSize(hCurrDS) != nRows_) {
        LOG_ERROR("PgEstimator: Current LUCC size mismatch: expected " +
                  to_string(nCols_) + "x" + to_string(nRows_) + ", got " +
                  to_string(GDALGetRasterXSize(hCurrDS)) + "x" + to_string(GDALGetRasterYSize(hCurrDS)));
        GDALClose(hTrainDS);
        GDALClose(hCurrDS);
        return false;
    }

    // ── Read both LUCC rasters ──
    int nPixels = nRows_ * nCols_;
    vector<int> trainData(nPixels), currData(nPixels);

    GDALRasterIO(hTrainBand, GF_Read, 0, 0, nCols_, nRows_,
                 trainData.data(), nCols_, nRows_, GDT_Int32, 0, 0);
    GDALRasterBandH hCurrBand = GDALGetRasterBand(hCurrDS, 1);
    GDALRasterIO(hCurrBand, GF_Read, 0, 0, nCols_, nRows_,
                 currData.data(), nCols_, nRows_, GDT_Int32, 0, 0);

    // ── Collect unique land types from current LUCC ──
    set<int> typeSet;
    for (int i = 0; i < nPixels; i++) {
        if (currData[i] != (int)trainND && currData[i] > 0) {
            typeSet.insert(currData[i]);
        }
    }
    vTypes_.assign(typeSet.begin(), typeSet.end());
    nTypes_ = (int)vTypes_.size();
    if (nTypes_ < 2) {
        LOG_ERROR("PgEstimator: Less than 2 land types found");
        GDALClose(hTrainDS);
        GDALClose(hCurrDS);
        return false;
    }

    GDALClose(hTrainDS);
    GDALClose(hCurrDS);

    // ── Validate driver rasters ──
    nDrivers_ = (int)vDriverPaths_.size();
    if (nDrivers_ < 2) {
        LOG_ERROR("PgEstimator: At least 2 driver factors required, got " + to_string(nDrivers_));
        return false;
    }

    for (int d = 0; d < nDrivers_; d++) {
        GDALDatasetH hDrvDS = GDALOpenEx(vDriverPaths_[d].c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
        if (!hDrvDS) {
            LOG_ERROR("PgEstimator: Cannot open driver raster: " + vDriverPaths_[d]);
            return false;
        }
        if (GDALGetRasterXSize(hDrvDS) != nCols_ || GDALGetRasterYSize(hDrvDS) != nRows_) {
            LOG_ERROR("PgEstimator: Driver raster size mismatch: " + vDriverPaths_[d] +
                      " expected " + to_string(nCols_) + "x" + to_string(nRows_) +
                      ", got " + to_string(GDALGetRasterXSize(hDrvDS)) + "x" +
                      to_string(GDALGetRasterYSize(hDrvDS)));
            GDALClose(hDrvDS);
            return false;
        }
        GDALClose(hDrvDS);
    }

    // ── Check output path writable ──
    namespace fs = std::filesystem;
    fs::path outPath(outputPath_);
    fs::path outDir = outPath.parent_path();
    if (!outDir.empty() && !fs::exists(outDir)) {
        fs::create_directories(outDir);
        LOG_INFO("PgEstimator: Created output directory: " + outDir.string());
    }

    return true;
}

void PgEstimator::collectSamplingPoints()
{
    int nPixels = nRows_ * nCols_;

    // ── Read train and current LUCC ──
    GDALDatasetH hTrainDS = GDALOpenEx(trainLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    GDALDatasetH hCurrDS = GDALOpenEx(currLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hTrainDS || !hCurrDS) return;

    vector<int> trainData(nPixels), currData(nPixels);
    GDALRasterIO(GDALGetRasterBand(hTrainDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 trainData.data(), nCols_, nRows_, GDT_Int32, 0, 0);
    GDALRasterIO(GDALGetRasterBand(hCurrDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 currData.data(), nCols_, nRows_, GDT_Int32, 0, 0);

    int bHasNoData = FALSE;
    double nd = GDALGetRasterNoDataValue(GDALGetRasterBand(hTrainDS, 1), &bHasNoData);
    if (!bHasNoData) nd = -9999.0;

    GDALClose(hTrainDS);
    GDALClose(hCurrDS);

    // ── Read all driver rasters ──
    vector<vector<float>> vDriverData(nDrivers_, vector<float>(nPixels));
    for (int d = 0; d < nDrivers_; d++) {
        GDALDatasetH hDrvDS = GDALOpenEx(vDriverPaths_[d].c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
        if (!hDrvDS) continue;
        GDALRasterIO(GDALGetRasterBand(hDrvDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                     vDriverData[d].data(), nCols_, nRows_, GDT_Float32, 0, 0);
        GDALClose(hDrvDS);
    }

    // ── Build type-to-index map ──
    for (int i = 0; i < nTypes_; i++) {
        typeToIdx_[vTypes_[i]] = i;
    }

    mt19937 rng(42);

    // ── Collect changed pixels (positive samples) ──
    vector<int> vChangeIndices;
    for (int i = 0; i < nPixels; i++) {
        if (trainData[i] == (int)nd || currData[i] == (int)nd) continue;
        if (trainData[i] <= 0 || currData[i] <= 0) continue;
        if (trainData[i] != currData[i]) {
            if (typeToIdx_.find(currData[i]) != typeToIdx_.end()) {
                vChangeIndices.push_back(i);
            }
        }
    }

    // Group changed pixels by target type
    map<int, vector<int>> typeToChangePixels;
    for (int idx : vChangeIndices) {
        typeToChangePixels[currData[idx]].push_back(idx);
    }

    // Sample from changed pixels (positive samples)
    for (auto& [type, pixels] : typeToChangePixels) {
        shuffle(pixels.begin(), pixels.end(), rng);
        int nSample = min((int)pixels.size(), nMaxPerType_);
        for (int i = 0; i < nSample; i++) {
            int px = pixels[i];
            vector<double> features(nDrivers_);
            bool bValid = true;
            for (int d = 0; d < nDrivers_; d++) {
                float val = vDriverData[d][px];
                if (std::isnan(val) || std::isinf(val)) {
                    bValid = false;
                    break;
                }
                features[d] = (double)val;
            }
            if (bValid) {
                vSampleFeatures_.push_back(move(features));
                vSampleLabels_.push_back(type);
                vSamplePixelIdx_.push_back(px);
                vSampleTypeIdx_.push_back(typeToIdx_[type]);
            }
        }
    }

    // ── Collect unchanged pixels (negative samples) ──
    map<int, vector<int>> typeToNoChangePixels;
    for (int i = 0; i < nPixels; i++) {
        if (trainData[i] == (int)nd || currData[i] == (int)nd) continue;
        if (trainData[i] <= 0 || currData[i] <= 0) continue;
        if (trainData[i] == currData[i]) {
            if (typeToIdx_.find(currData[i]) != typeToIdx_.end()) {
                typeToNoChangePixels[currData[i]].push_back(i);
            }
        }
    }

    // Sample from unchanged pixels (negative samples)
    for (auto& [type, pixels] : typeToNoChangePixels) {
        shuffle(pixels.begin(), pixels.end(), rng);
        int nSample = min((int)pixels.size(), nMaxNegPerType_);
        for (int i = 0; i < nSample; i++) {
            int px = pixels[i];
            vector<double> features(nDrivers_);
            bool bValid = true;
            for (int d = 0; d < nDrivers_; d++) {
                float val = vDriverData[d][px];
                if (std::isnan(val) || std::isinf(val)) {
                    bValid = false;
                    break;
                }
                features[d] = (double)val;
            }
            if (bValid) {
                vSampleFeatures_.push_back(move(features));
                vSampleLabels_.push_back(type); // same type as "staying"
                vSamplePixelIdx_.push_back(px);
                vSampleTypeIdx_.push_back(typeToIdx_[type]);
            }
        }
    }

    int nPosSamples = 0;
    for (auto& [type, pixels] : typeToChangePixels) {
        nPosSamples += min((int)pixels.size(), nMaxPerType_);
    }
    int nNegSamples = (int)vSampleFeatures_.size() - nPosSamples;

    LOG_INFO("PgEstimator: Collected " + to_string(vSampleFeatures_.size()) +
             " samples (pos=" + to_string(nPosSamples) +
             ", neg=" + to_string(nNegSamples) + ")");
}

int PgEstimator::trainRF()
{
    int nSamples = (int)vSampleFeatures_.size();
    int nFeatures = nDrivers_;

    // Build type-to-index map for label encoding
    unordered_map<int, int> typeToIdx;
    for (int i = 0; i < nTypes_; i++) {
        typeToIdx[vTypes_[i]] = i;
    }

    // ── Build alglib training matrix ──
    // Format: [features..., label] per row
    alglib::real_2d_array trainData;
    trainData.setlength(nSamples, nFeatures + 1);

    for (int i = 0; i < nSamples; i++) {
        for (int j = 0; j < nFeatures; j++) {
            trainData[i][j] = vSampleFeatures_[i][j];
        }
        trainData[i][nFeatures] = (double)typeToIdx[vSampleLabels_[i]];
    }

    // ── Train Random Forest ──
    alglib::decisionforest rf;
    alglib::dfreport rep;
    alglib::ae_int_t info;

    try {
        alglib::dfbuildrandomdecisionforest(trainData, nSamples, nFeatures, nTypes_,
                                             nRFTrees_, dSelectRate_, info, rf, rep);
    } catch (const alglib::ap_error& e) {
        LOG_ERROR("PgEstimator: RF training error: " + string(e.msg));
        return 1;
    }

    if (info < 0) {
        LOG_ERROR("PgEstimator: RF training failed, info=" + to_string(info));
        return 1;
    }

    LOG_INFO("PgEstimator: RF training completed, rel error=" +
             to_string(rep.relclserror) + ", oob error=" + to_string(rep.oobrelclserror));

    // ── Predict Pg for all pixels ──
    int nPixels = nRows_ * nCols_;

    // Read all driver data
    vector<vector<float>> vDriverData(nDrivers_, vector<float>(nPixels));
    for (int d = 0; d < nDrivers_; d++) {
        GDALDatasetH hDrvDS = GDALOpenEx(vDriverPaths_[d].c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
        if (!hDrvDS) {
            LOG_ERROR("PgEstimator: Cannot re-open driver: " + vDriverPaths_[d]);
            return 1;
        }
        GDALRasterIO(GDALGetRasterBand(hDrvDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                     vDriverData[d].data(), nCols_, nRows_, GDT_Float32, 0, 0);
        GDALClose(hDrvDS);
    }

    // Read current LUCC for NoData check
    GDALDatasetH hCurrDS = GDALOpenEx(currLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    vector<int> currData(nPixels);
    GDALRasterIO(GDALGetRasterBand(hCurrDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 currData.data(), nCols_, nRows_, GDT_Int32, 0, 0);
    int bHasNoData = FALSE;
    double nd = GDALGetRasterNoDataValue(GDALGetRasterBand(hCurrDS, 1), &bHasNoData);
    if (!bHasNoData) nd = -9999.0;
    GDALClose(hCurrDS);

    // Initialize Pg data
    vPgData_.assign(nPixels, vector<float>(nTypes_, 0.0f));

    // Predict in batches
    alglib::real_1d_array x;
    x.setlength(nFeatures);
    alglib::real_1d_array y;
    y.setlength(nTypes_);

    int nProcessed = 0;
    for (int i = 0; i < nPixels; i++) {
        if (currData[i] == (int)nd || currData[i] <= 0) {
            // NoData pixel: set all probabilities to 0
            continue;
        }

        bool bValid = true;
        for (int d = 0; d < nDrivers_; d++) {
            if (std::isnan(vDriverData[d][i]) || std::isinf(vDriverData[d][i])) {
                bValid = false;
                break;
            }
            x[d] = (double)vDriverData[d][i];
        }
        if (!bValid) continue;

        try {
            alglib::dfprocess(rf, x, y);
        } catch (const alglib::ap_error& e) {
            LOG_WARN("PgEstimator: RF predict error at pixel " + to_string(i) + ": " + e.msg);
            continue;
        }

        // y contains class probabilities (sum to 1)
        double dSum = 0.0;
        for (int k = 0; k < nTypes_; k++) dSum += y[k];
        if (dSum < 1e-15) dSum = 1.0;

        for (int k = 0; k < nTypes_; k++) {
            vPgData_[i][k] = (float)(y[k] / dSum);
        }

        nProcessed++;
        if (nProcessed % 100000 == 0) {
            LOG_INFO("PgEstimator: Predicted " + to_string(nProcessed) + " pixels");
        }
    }

    LOG_INFO("PgEstimator: RF prediction completed for " + to_string(nProcessed) + " pixels");
    return 0;
}

int PgEstimator::trainANN()
{
    int nSamples = (int)vSampleFeatures_.size();
    int nFeatures = nDrivers_;

    // Build type-to-index map
    unordered_map<int, int> typeToIdx;
    for (int i = 0; i < nTypes_; i++) {
        typeToIdx[vTypes_[i]] = i;
    }

    // ── Build training data ──
    alglib::real_2d_array trainData;
    trainData.setlength(nSamples, nFeatures + 1);
    for (int i = 0; i < nSamples; i++) {
        for (int j = 0; j < nFeatures; j++) {
            trainData[i][j] = vSampleFeatures_[i][j];
        }
        trainData[i][nFeatures] = (double)typeToIdx[vSampleLabels_[i]];
    }

    // ── Build ANN network ──
    // Architecture: input(nFeatures) -> hidden(max(5, nFeatures)) -> output(nTypes)
    // Using classifier variant (c suffix) with softmax output
    int nHidden = max(5, nFeatures);
    alglib::multilayerperceptron network;
    alglib::mlptrainer trainer;
    alglib::mlpreport rep;

    // Create classifier network with 1 hidden layer
    alglib::mlpcreatec1(nFeatures, nHidden, nTypes_, network);

    // Create trainer (3 params: nin, nclasses, trainer)
    alglib::mlpcreatetrainercls(nFeatures, nTypes_, trainer);

    // Set dataset on trainer
    alglib::mlpsetdataset(trainer, trainData, nSamples);

    // Set training parameters
    alglib::mlpsetdecay(trainer, 0.001);
    alglib::mlpsetcond(trainer, 0.001, 0);

    // Train (4 params: trainer, network, nrestarts, report)
    try {
        alglib::mlptrainnetwork(trainer, network, 5, rep);
    } catch (const alglib::ap_error& e) {
        LOG_ERROR("PgEstimator: ANN training error: " + string(e.msg));
        return 1;
    }

    LOG_INFO("PgEstimator: ANN training completed, rel error=" + to_string(rep.relclserror));

    // ── Predict Pg for all pixels ──
    int nPixels = nRows_ * nCols_;

    vector<vector<float>> vDriverData(nDrivers_, vector<float>(nPixels));
    for (int d = 0; d < nDrivers_; d++) {
        GDALDatasetH hDrvDS = GDALOpenEx(vDriverPaths_[d].c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
        if (!hDrvDS) return 1;
        GDALRasterIO(GDALGetRasterBand(hDrvDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                     vDriverData[d].data(), nCols_, nRows_, GDT_Float32, 0, 0);
        GDALClose(hDrvDS);
    }

    GDALDatasetH hCurrDS = GDALOpenEx(currLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    vector<int> currData(nPixels);
    GDALRasterIO(GDALGetRasterBand(hCurrDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 currData.data(), nCols_, nRows_, GDT_Int32, 0, 0);
    int bHasNoData = FALSE;
    double nd = GDALGetRasterNoDataValue(GDALGetRasterBand(hCurrDS, 1), &bHasNoData);
    if (!bHasNoData) nd = -9999.0;
    GDALClose(hCurrDS);

    vPgData_.assign(nPixels, vector<float>(nTypes_, 0.0f));

    alglib::real_1d_array x;
    x.setlength(nFeatures);
    alglib::real_1d_array y;
    y.setlength(nTypes_);

    int nProcessed = 0;
    for (int i = 0; i < nPixels; i++) {
        if (currData[i] == (int)nd || currData[i] <= 0) continue;

        bool bValid = true;
        for (int d = 0; d < nDrivers_; d++) {
            if (std::isnan(vDriverData[d][i]) || std::isinf(vDriverData[d][i])) {
                bValid = false;
                break;
            }
            x[d] = (double)vDriverData[d][i];
        }
        if (!bValid) continue;

        try {
            alglib::mlpprocess(network, x, y);
        } catch (const alglib::ap_error& e) {
            LOG_WARN("PgEstimator: ANN predict error at pixel " + to_string(i) + ": " + e.msg);
            continue;
        }

        double dSum = 0.0;
        for (int k = 0; k < nTypes_; k++) dSum += y[k];
        if (dSum < 1e-15) dSum = 1.0;

        for (int k = 0; k < nTypes_; k++) {
            vPgData_[i][k] = (float)(y[k] / dSum);
        }

        nProcessed++;
        if (nProcessed % 100000 == 0) {
            LOG_INFO("PgEstimator: Predicted " + to_string(nProcessed) + " pixels");
        }
    }

    LOG_INFO("PgEstimator: ANN prediction completed for " + to_string(nProcessed) + " pixels");
    return 0;
}

int PgEstimator::trainLogit()
{
    int nSamples = (int)vSampleFeatures_.size();
    int nFeatures = nDrivers_;

    if (nSamples < 10) {
        LOG_ERROR("PgEstimator: Not enough samples for Logit training, need at least 10");
        return 1;
    }

    // Build type-to-index map
    unordered_map<int, int> typeToIdx;
    for (int i = 0; i < nTypes_; i++) {
        typeToIdx[vTypes_[i]] = i;
    }

    // For binary logistic regression, we use "change to type k" vs "not change to type k"
    // Train one binary classifier per type (One-vs-Rest)
    vLogitW_.assign(nTypes_ * nFeatures, 0.0);
    dLogitB_ = 0.0;

    // Use alglib's logit model for multi-class classification
    // Format training data: [features..., label] where label is 0/1/2... (class index)
    alglib::real_2d_array trainData;
    trainData.setlength(nSamples, nFeatures + 1);

    for (int i = 0; i < nSamples; i++) {
        for (int j = 0; j < nFeatures; j++) {
            trainData[i][j] = vSampleFeatures_[i][j];
        }
        trainData[i][nFeatures] = (double)typeToIdx[vSampleLabels_[i]];
    }

    // Train logistic regression classifier
    alglib::logitmodel logit;
    alglib::mnlreport rep;
    alglib::ae_int_t info;

    try {
        alglib::mnltrainh(trainData, nSamples, nFeatures, nTypes_, info, logit, rep);
    } catch (const alglib::ap_error& e) {
        LOG_ERROR("PgEstimator: Logit training error: " + string(e.msg));
        return 1;
    }

    if (info < 0) {
        LOG_ERROR("PgEstimator: Logit training failed, info=" + to_string(info));
        return 1;
    }

    LOG_INFO("PgEstimator: Logit training completed, ngrad=" + to_string(rep.ngrad) + ", nhess=" + to_string(rep.nhess));

    // ── Predict Pg for all pixels ──
    int nPixels = nRows_ * nCols_;

    // Read all driver data
    vector<vector<float>> vDriverData(nDrivers_, vector<float>(nPixels));
    for (int d = 0; d < nDrivers_; d++) {
        GDALDatasetH hDrvDS = GDALOpenEx(vDriverPaths_[d].c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
        if (!hDrvDS) {
            LOG_ERROR("PgEstimator: Cannot re-open driver: " + vDriverPaths_[d]);
            return 1;
        }
        GDALRasterIO(GDALGetRasterBand(hDrvDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                     vDriverData[d].data(), nCols_, nRows_, GDT_Float32, 0, 0);
        GDALClose(hDrvDS);
    }

    // Read current LUCC for NoData check
    GDALDatasetH hCurrDS = GDALOpenEx(currLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    vector<int> currData(nPixels);
    GDALRasterIO(GDALGetRasterBand(hCurrDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 currData.data(), nCols_, nRows_, GDT_Int32, 0, 0);
    int bHasNoData = FALSE;
    double nd = GDALGetRasterNoDataValue(GDALGetRasterBand(hCurrDS, 1), &bHasNoData);
    if (!bHasNoData) nd = -9999.0;
    GDALClose(hCurrDS);

    vPgData_.assign(nPixels, vector<float>(nTypes_, 0.0f));

    alglib::real_1d_array x;
    x.setlength(nFeatures);
    alglib::real_1d_array y;
    y.setlength(nTypes_);

    int nProcessed = 0;
#if defined(_OPENMP) && bUseOpenMP_
    #pragma omp parallel for schedule(dynamic, 1000)
#endif
    for (int i = 0; i < nPixels; i++) {
        if (currData[i] == (int)nd || currData[i] <= 0) {
            continue;
        }

        bool bValid = true;
        for (int d = 0; d < nDrivers_; d++) {
            float val = vDriverData[d][i];
            if (std::isnan(val) || std::isinf(val)) {
                bValid = false;
                break;
            }
            x[d] = (double)val;
        }
        if (!bValid) continue;

        try {
            alglib::mnlprocess(logit, x, y);
        } catch (const alglib::ap_error& e) {
            continue;
        }

        double dSum = 0.0;
        for (int k = 0; k < nTypes_; k++) {
            double prob = y[k];
            prob = max(0.0, min(1.0, prob));
            vPgData_[i][k] = (float)prob;
            dSum += prob;
        }

        if (dSum > 1e-15) {
            for (int k = 0; k < nTypes_; k++) {
                vPgData_[i][k] /= (float)dSum;
            }
        }

#if !defined(_OPENMP) || !bUseOpenMP_
        nProcessed++;
        if (nProcessed % 100000 == 0) {
            LOG_INFO("PgEstimator: Predicted " + to_string(nProcessed) + " pixels");
        }
#endif
    }

#if defined(_OPENMP) && bUseOpenMP_
    nProcessed = nPixels;
#endif

    LOG_INFO("PgEstimator: Logit prediction completed for " + to_string(nProcessed) + " pixels");
    return 0;
}

int PgEstimator::predictPg()
{
    // Prediction is done inside trainRF()/trainANN() for efficiency
    // (avoids re-reading driver data twice)
    return 0;
}

int PgEstimator::writePgTiff()
{
    int nPixels = nRows_ * nCols_;

    // ── Get geotransform and projection from current LUCC ──
    GDALDatasetH hRefDS = GDALOpenEx(currLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hRefDS) {
        LOG_ERROR("PgEstimator: Cannot open reference for geotransform: " + currLUCCPath_);
        return 1;
    }
    double adfGeoTransform[6];
    GDALGetGeoTransform(hRefDS, adfGeoTransform);
    const char* pszProj = GDALGetProjectionRef(hRefDS);
    string strProj = pszProj ? pszProj : "";
    GDALClose(hRefDS);

    // ── Create output multi-band GeoTIFF ──
    GDALDriverH hDriver = GDALGetDriverByName("GTiff");
    if (!hDriver) {
        LOG_ERROR("PgEstimator: GTiff driver not available");
        return 1;
    }

    GDALDatasetH hDstDS = GDALCreate(hDriver, outputPath_.c_str(), nCols_, nRows_,
                                      nTypes_, GDT_Float32, nullptr);
    if (!hDstDS) {
        LOG_ERROR("PgEstimator: Cannot create output: " + outputPath_);
        return 1;
    }

    GDALSetGeoTransform(hDstDS, adfGeoTransform);
    if (!strProj.empty()) {
        GDALSetProjection(hDstDS, strProj.c_str());
    }

    // ── Write each band ──
    vector<float> bandData(nPixels);
    for (int k = 0; k < nTypes_; k++) {
        for (int i = 0; i < nPixels; i++) {
            bandData[i] = vPgData_[i][k];
        }
        GDALRasterBandH hBand = GDALGetRasterBand(hDstDS, k + 1);
        GDALSetRasterNoDataValue(hBand, 0.0f);
        CPLErr err = GDALRasterIO(hBand, GF_Write, 0, 0, nCols_, nRows_,
                                   bandData.data(), nCols_, nRows_, GDT_Float32, 0, 0);
        if (err != CE_None) {
            LOG_ERROR("PgEstimator: Failed to write band " + to_string(k + 1));
            GDALClose(hDstDS);
            return 1;
        }
    }

    GDALClose(hDstDS);
    LOG_INFO("PgEstimator: Written " + to_string(nTypes_) + " bands to " + outputPath_);
    return 0;
}

void PgEstimator::evaluateAccuracy()
{
    LOG_INFO("PgEstimator: Evaluating accuracy with validation data: " + validatePath_);

    int nPixels = nRows_ * nCols_;

    // ── Read validation LUCC ──
    GDALDatasetH hValDS = GDALOpenEx(validatePath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    if (!hValDS) {
        LOG_ERROR("PgEstimator: Cannot open validation LUCC: " + validatePath_);
        return;
    }
    vector<int> valData(nPixels);
    GDALRasterIO(GDALGetRasterBand(hValDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 valData.data(), nCols_, nRows_, GDT_Int32, 0, 0);
    int bHasNoData = FALSE;
    double nd = GDALGetRasterNoDataValue(GDALGetRasterBand(hValDS, 1), &bHasNoData);
    if (!bHasNoData) nd = -9999.0;
    GDALClose(hValDS);

    // ── Read current LUCC ──
    GDALDatasetH hCurrDS = GDALOpenEx(currLUCCPath_.c_str(), GDAL_OF_READONLY, nullptr, nullptr, nullptr);
    vector<int> currData(nPixels);
    GDALRasterIO(GDALGetRasterBand(hCurrDS, 1), GF_Read, 0, 0, nCols_, nRows_,
                 currData.data(), nCols_, nRows_, GDT_Int32, 0, 0);
    GDALClose(hCurrDS);

    // ── Predict validation using Pg (argmax) ──
    // Build type-to-index map
    unordered_map<int, int> typeToIdx;
    for (int i = 0; i < nTypes_; i++) {
        typeToIdx[vTypes_[i]] = i;
    }

    // ── Compute confusion matrix ──
    // Only consider changed pixels (curr != val)
    int nChanged = 0;
    int nCorrect = 0;
    int nTotal = 0;

    // For FoM: A=correct change, B=observed change but predicted no-change,
    // C=predicted change but observed no-change, D=correct no-change
    int A = 0, B = 0, C = 0, D = 0;

    for (int i = 0; i < nPixels; i++) {
        if (valData[i] == (int)nd || valData[i] <= 0) continue;
        if (currData[i] == (int)nd || currData[i] <= 0) continue;

        nTotal++;

        // Predict: find type with max Pg
        int predType = vTypes_[0];
        float maxPg = vPgData_[i][0];
        for (int k = 1; k < nTypes_; k++) {
            if (vPgData_[i][k] > maxPg) {
                maxPg = vPgData_[i][k];
                predType = vTypes_[k];
            }
        }

        bool bObservedChange = (currData[i] != valData[i]);
        bool bPredictedChange = (currData[i] != predType);

        if (bObservedChange && bPredictedChange) {
            A++;
            if (predType == valData[i]) nCorrect++;
        } else if (bObservedChange && !bPredictedChange) {
            B++;
        } else if (!bObservedChange && bPredictedChange) {
            C++;
        } else {
            D++;
        }
    }

    // ── Compute metrics ──
    double FoM = (A + B + C) > 0 ? (double)A / (A + B + C) : 0.0;
    double PA  = (A + B) > 0 ? (double)A / (A + B) : 0.0;
    double UA  = (A + C) > 0 ? (double)A / (A + C) : 0.0;
    double OA  = nTotal > 0 ? (double)(A + D) / nTotal : 0.0;

    // Kappa
    double po = OA;
    double pe = 0.0;
    if (nTotal > 0) {
        double rObserved = (double)(A + B) / nTotal;
        double cPredicted = (double)(A + C) / nTotal;
        pe = rObserved * cPredicted + (1.0 - rObserved) * (1.0 - cPredicted);
    }
    double Kappa = (1.0 - pe) > 1e-15 ? (po - pe) / (1.0 - pe) : 0.0;

    LOG_RESULT("ca-pg", "FoM", FoM);
    LOG_RESULT("ca-pg", "PA", PA);
    LOG_RESULT("ca-pg", "UA", UA);
    LOG_RESULT("ca-pg", "Kappa", Kappa);
    LOG_RESULT("ca-pg", "OA", OA);

    cout << fixed << setprecision(6);
    cout << "FoM=" << FoM << " PA=" << PA << " UA=" << UA
         << " Kappa=" << Kappa << " OA=" << OA << endl;
}

int PgEstimator::saveModel(const std::string& qstrModelPath)
{
    if (qstrModelPath.empty()) {
        LOG_WARN("PgEstimator: Model path is empty, skipping save");
        return 1;
    }

    namespace fs = std::filesystem;
    fs::path path(qstrModelPath);
    fs::path dir = path.parent_path();
    if (!dir.empty() && !fs::exists(dir)) {
        fs::create_directories(dir);
    }

    // Save metadata as JSON
    string jsonPath = qstrModelPath + ".meta.json";
    ofstream jsonFile(jsonPath);
    if (!jsonFile.is_open()) {
        LOG_ERROR("PgEstimator: Cannot open model metadata file for write: " + jsonPath);
        return 1;
    }

    jsonFile << "{\n";
    jsonFile << "  \"method\": " << nMethod_ << ",\n";
    jsonFile << "  \"nTypes\": " << nTypes_ << ",\n";
    jsonFile << "  \"nDrivers\": " << nDrivers_ << ",\n";
    jsonFile << "  \"nRows\": " << nRows_ << ",\n";
    jsonFile << "  \"nCols\": " << nCols_ << ",\n";
    jsonFile << "  \"types\": [";
    for (int i = 0; i < nTypes_; i++) {
        jsonFile << vTypes_[i];
        if (i < nTypes_ - 1) jsonFile << ", ";
    }
    jsonFile << "],\n";
    jsonFile << "  \"nRFTrees\": " << nRFTrees_ << ",\n";
    jsonFile << "  \"dSelectRate\": " << dSelectRate_ << "\n";
    jsonFile << "}\n";
    jsonFile.close();

    // Save binary model data
    string binPath = qstrModelPath + ".bin";
    ofstream binFile(binPath, ios::binary);
    if (!binFile.is_open()) {
        LOG_ERROR("PgEstimator: Cannot open model binary file for write: " + binPath);
        return 1;
    }

    // RF model data
    size_t nForest = vForest_.size();
    binFile.write(reinterpret_cast<const char*>(&nForest), sizeof(size_t));
    for (const auto& tree : vForest_) {
        size_t treeSize = tree.size();
        binFile.write(reinterpret_cast<const char*>(&treeSize), sizeof(size_t));
        binFile.write(reinterpret_cast<const char*>(tree.data()), treeSize * sizeof(double));
    }

    size_t nTreeFeat = vTreeFeatIdxs_.size();
    binFile.write(reinterpret_cast<const char*>(&nTreeFeat), sizeof(size_t));
    binFile.write(reinterpret_cast<const char*>(vTreeFeatIdxs_.data()), nTreeFeat * sizeof(int));

    // ANN model data
    size_t nW1 = vW1_.size();
    binFile.write(reinterpret_cast<const char*>(&nW1), sizeof(size_t));
    if (nW1 > 0) {
        binFile.write(reinterpret_cast<const char*>(vW1_.data()), nW1 * sizeof(double));
        binFile.write(reinterpret_cast<const char*>(vB1_.data()), vB1_.size() * sizeof(double));
        binFile.write(reinterpret_cast<const char*>(vW2_.data()), vW2_.size() * sizeof(double));
        binFile.write(reinterpret_cast<const char*>(vB2_.data()), vB2_.size() * sizeof(double));
    }

    // Logit model data
    size_t nLogitW = vLogitW_.size();
    binFile.write(reinterpret_cast<const char*>(&nLogitW), sizeof(size_t));
    if (nLogitW > 0) {
        binFile.write(reinterpret_cast<const char*>(vLogitW_.data()), nLogitW * sizeof(double));
        binFile.write(reinterpret_cast<const char*>(&dLogitB_), sizeof(double));
    }

    binFile.close();
    LOG_INFO("PgEstimator: Model saved to " + qstrModelPath);
    return 0;
}

int PgEstimator::loadModel(const std::string& qstrModelPath)
{
    if (qstrModelPath.empty()) {
        LOG_ERROR("PgEstimator: Model path is empty");
        return 1;
    }

    // Load metadata JSON
    string jsonPath = qstrModelPath + ".meta.json";
    ifstream jsonFile(jsonPath);
    if (!jsonFile.is_open()) {
        LOG_ERROR("PgEstimator: Cannot open model metadata file: " + jsonPath);
        return 1;
    }

    // Parse JSON (simple parser, not using external library)
    string jsonContent((istreambuf_iterator<char>(jsonFile)), istreambuf_iterator<char>());
    jsonFile.close();

    // Extract values using string search (simple approach)
    auto getInt = [&](const string& key) -> int {
        size_t pos = jsonContent.find("\"" + key + "\":");
        if (pos == string::npos) return 0;
        size_t numPos = jsonContent.find_first_of("0123456789", pos);
        size_t endPos = jsonContent.find_first_of(",}\n", numPos);
        string val = jsonContent.substr(numPos, endPos - numPos);
        return stoi(val);
    };

    nMethod_ = getInt("method");
    nTypes_ = getInt("nTypes");
    nDrivers_ = getInt("nDrivers");
    nRows_ = getInt("nRows");
    nCols_ = getInt("nCols");
    nRFTrees_ = getInt("nRFTrees");
    dSelectRate_ = getInt("dSelectRate") / 100.0; // rough approximation

    // Load binary model data
    string binPath = qstrModelPath + ".bin";
    ifstream binFile(binPath, ios::binary);
    if (!binFile.is_open()) {
        LOG_ERROR("PgEstimator: Cannot open model binary file: " + binPath);
        return 1;
    }

    // RF model data
    size_t nForest = 0;
    binFile.read(reinterpret_cast<char*>(&nForest), sizeof(size_t));
    vForest_.resize(nForest);
    for (auto& tree : vForest_) {
        size_t treeSize = 0;
        binFile.read(reinterpret_cast<char*>(&treeSize), sizeof(size_t));
        tree.resize(treeSize);
        binFile.read(reinterpret_cast<char*>(tree.data()), treeSize * sizeof(double));
    }

    size_t nTreeFeat = 0;
    binFile.read(reinterpret_cast<char*>(&nTreeFeat), sizeof(size_t));
    vTreeFeatIdxs_.resize(nTreeFeat);
    binFile.read(reinterpret_cast<char*>(vTreeFeatIdxs_.data()), nTreeFeat * sizeof(int));

    // ANN model data
    size_t nW1 = 0;
    binFile.read(reinterpret_cast<char*>(&nW1), sizeof(size_t));
    if (nW1 > 0) {
        vW1_.resize(nW1);
        binFile.read(reinterpret_cast<char*>(vW1_.data()), nW1 * sizeof(double));
        vB1_.resize(nW1);
        binFile.read(reinterpret_cast<char*>(vB1_.data()), nW1 * sizeof(double));
        vW2_.resize(nW1);
        binFile.read(reinterpret_cast<char*>(vW2_.data()), nW1 * sizeof(double));
        vB2_.resize(nTypes_);
        binFile.read(reinterpret_cast<char*>(vB2_.data()), nTypes_ * sizeof(double));
    }

    // Logit model data
    size_t nLogitW = 0;
    binFile.read(reinterpret_cast<char*>(&nLogitW), sizeof(size_t));
    if (nLogitW > 0) {
        vLogitW_.resize(nLogitW);
        binFile.read(reinterpret_cast<char*>(vLogitW_.data()), nLogitW * sizeof(double));
        binFile.read(reinterpret_cast<char*>(&dLogitB_), sizeof(double));
    }

    binFile.close();
    modelPath_ = qstrModelPath;
    LOG_INFO("PgEstimator: Model loaded from " + qstrModelPath);
    return 0;
}
