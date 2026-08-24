/**
 * @file MyIndicator.cpp
 * @brief Example custom SDG indicator plugin implementation
 */

#include "MyIndicator.h"

#include <sstream>
#include <fstream>
#include <cmath>
#include <algorithm>

// GDAL includes
#if !defined(GEOSDG_PLATFORM_WINDOWS)
    #include <cpl_conv.h>
#endif
#include <gdal_priv.h>
#include <cpl_string.h>

// ============================================================================
// Constructor
// ============================================================================

ForestCoverageIndicator::ForestCoverageIndicator()
    : minArea_(0.5) {}

// ============================================================================
// init
// ============================================================================

int ForestCoverageIndicator::init(const std::map<std::string, std::string>& params) {
    // Get required parameters
    auto itLucc = params.find("init-lucc");
    if (itLucc == params.end() || itLucc->second.empty()) {
        error_ = "Missing required parameter: --init-lucc";
        return 1;
    }
    luccPath_ = itLucc->second;

    auto itTypes = params.find("forest-types");
    if (itTypes == params.end() || itTypes->second.empty()) {
        error_ = "Missing required parameter: --forest-types";
        return 1;
    }
    forestTypes_ = parseIntList(itTypes->second);
    if (forestTypes_.empty()) {
        error_ = "No valid forest type codes parsed from --forest-types";
        return 1;
    }

    // Optional parameters
    auto itMinArea = params.find("min-area");
    if (itMinArea != params.end() && !itMinArea->second.empty()) {
        try {
            minArea_ = std::stod(itMinArea->second);
        } catch (...) {
            error_ = "Invalid value for --min-area: " + itMinArea->second;
            return 1;
        }
    }

    if (logger_) logger_("ForestCoverageIndicator initialized with " +
                         std::to_string(forestTypes_.size()) + " forest types");
    return 0;
}

// ============================================================================
// execute
// ============================================================================

int ForestCoverageIndicator::execute() {
    GDALAllRegister();

    GDALDataset* poDS = static_cast<GDALDataset*>(
        GDALOpenEx(luccPath_.c_str(), GA_ReadOnly, nullptr, nullptr, nullptr));
    if (!poDS) {
        error_ = "Cannot open raster: " + luccPath_;
        return 1;
    }

    GDALRasterBand* poBand = poDS->GetRasterBand(1);
    if (!poBand) {
        error_ = "Cannot get raster band 1 from: " + luccPath_;
        GDALClose(poDS);
        return 1;
    }

    int nCols = poBand->GetXSize();
    int nRows = poBand->GetYSize();

    int nNoData = 0;
    int bHasNoData = FALSE;
    double dNoData = poBand->GetNoDataValue(&bHasNoData);

    // Read raster line by line
    int* pLine = new int[nCols];
    long nTotalPixels = 0;
    long nForestPixels = 0;

    for (int iRow = 0; iRow < nRows; ++iRow) {
        CPLErr eErr = poBand->RasterIO(GF_Read, 0, iRow, nCols, 1,
                                        pLine, nCols, 1, GDT_Int32,
                                        0, 0);
        if (eErr != CE_None) {
            error_ = "RasterIO failed at row " + std::to_string(iRow);
            delete[] pLine;
            GDALClose(poDS);
            return 1;
        }

        for (int iCol = 0; iCol < nCols; ++iCol) {
            int val = pLine[iCol];
            if (bHasNoData && val == static_cast<int>(dNoData)) {
                nNoData++;
                continue;
            }
            nTotalPixels++;
            if (std::find(forestTypes_.begin(), forestTypes_.end(), val) != forestTypes_.end()) {
                nForestPixels++;
            }
        }
    }

    delete[] pLine;
    GDALClose(poDS);

    if (nTotalPixels == 0) {
        error_ = "No valid pixels found in raster";
        return 1;
    }

    double dRatio = static_cast<double>(nForestPixels) / static_cast<double>(nTotalPixels);

    // Apply threshold: score = ratio if ratio >= minArea, else 0
    double dScore = (dRatio >= minArea_) ? dRatio : 0.0;

    std::ostringstream oss;
    oss << "forest_ratio=" << dRatio
        << "  forest_pixels=" << nForestPixels
        << "  total_pixels=" << nTotalPixels
        << "  score=" << dScore;
    result_ = oss.str();

    if (logger_) logger_("ForestCoverageIndicator: " + result_);
    return 0;
}

// ============================================================================
// Accessors
// ============================================================================

std::string ForestCoverageIndicator::getResult() const {
    return result_;
}

std::string ForestCoverageIndicator::getError() const {
    return error_;
}

void ForestCoverageIndicator::setLogger(std::function<void(const std::string&)> logger) {
    logger_ = logger;
}

// ============================================================================
// Helpers
// ============================================================================

std::vector<int> ForestCoverageIndicator::parseIntList(const std::string& s) {
    std::vector<int> result;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, ',')) {
        if (!token.empty()) {
            try {
                result.push_back(std::stoi(token));
            } catch (...) {}
        }
    }
    return result;
}

// ============================================================================
// Plugin Factory Functions (C linkage)
// ============================================================================

extern "C" {

/**
 * @brief Create a ForestCoverageIndicator instance
 * @return Pointer to new IIndicator instance
 */
IIndicator* createIndicator() {
    return new ForestCoverageIndicator();
}

/**
 * @brief Destroy a ForestCoverageIndicator instance
 * @param ptr Pointer to instance to destroy
 */
void destroyIndicator(IIndicator* ptr) {
    delete ptr;
}

} // extern "C"
