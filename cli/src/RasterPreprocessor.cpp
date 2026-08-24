/**
 * @file RasterPreprocessor.cpp
 * @brief Raster data preprocessing module implementation
 *
 * Implements 5 preprocessing methods: resample, normalize, reclassify, detect-change, compress.
 */

#include "RasterPreprocessor.h"
#include "Logger.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "gdal_priv.h"
#include "cpl_conv.h"
#include "ogr_spatialref.h"
#include "gdalwarper.h"

namespace fs = std::filesystem;
using namespace std;

// ============================================================================
// Constructor / Destructor
// ============================================================================

RasterPreprocessor::RasterPreprocessor() {}
RasterPreprocessor::~RasterPreprocessor() {}

// ============================================================================
// compressRaster() — Compress
// ============================================================================

int RasterPreprocessor::compressRaster(const string& input,
                                        const string& output,
                                        const CompressOptions& opts)
{
    LOG_INFO("compressRaster: input=" + input + " output=" + output);

    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    auto tStart = chrono::steady_clock::now();

    GDALDataset* poSrc = (GDALDataset*)GDALOpen(input.c_str(), GA_ReadOnly);
    if (!poSrc) {
        LOG_ERROR("compressRaster: Cannot open input: " + input);
        return 1;
    }

    int nBands = poSrc->GetRasterCount();
    int nXSize = poSrc->GetRasterXSize();
    int nYSize = poSrc->GetRasterYSize();
    GDALDataType eDT = poSrc->GetRasterBand(1)->GetRasterDataType();

    double adfGeoTransform[6];
    poSrc->GetGeoTransform(adfGeoTransform);
    const char* pszProj = poSrc->GetProjectionRef();

    // Check if input is already compressed
    const char* pszCompress = poSrc->GetMetadataItem("COMPRESSION", "IMAGE_STRUCTURE");
    if (pszCompress && pszCompress[0] != '\0') {
        LOG_INFO("compressRaster: Input is already compressed (" + string(pszCompress) + "), re-compressing");
    }

    // Build creation options
    GDALDriver* poDrv = GetGDALDriverManager()->GetDriverByName("GTIFF");
    if (!poDrv) {
        LOG_ERROR("compressRaster: Cannot get GTiff driver");
        GDALClose(poSrc);
        return 1;
    }

    string compressMethod;
    if (opts.method == "deflate")       compressMethod = "DEFLATE";
    else if (opts.method == "lzw")      compressMethod = "LZW";
    else if (opts.method == "zstd")     compressMethod = "ZSTD";
    else if (opts.method == "lerc")     compressMethod = "LERC";
    else if (opts.method == "lerc_zstd") compressMethod = "LERC_ZSTD";
    else {
        LOG_ERROR("compressRaster: Unknown compress method: " + opts.method);
        GDALClose(poSrc);
        return 1;
    }

    // Warn if LERC used for integer data
    if ((compressMethod == "LERC" || compressMethod == "LERC_ZSTD") &&
        (eDT == GDT_Byte || eDT == GDT_Int16 || eDT == GDT_UInt16)) {
        LOG_WARN("compressRaster: LERC on integer data, consider using lossless algorithm");
    }

    char** papszOpts = nullptr;
    papszOpts = CSLSetNameValue(papszOpts, "COMPRESS", compressMethod.c_str());
    papszOpts = CSLSetNameValue(papszOpts, "BIGTIFF", opts.bigtiff.c_str());

    if (compressMethod == "DEFLATE" || compressMethod == "LZW" || compressMethod == "ZSTD") {
        papszOpts = CSLSetNameValue(papszOpts, "ZLEVEL", to_string(opts.level).c_str());
    } else if (compressMethod == "LERC" || compressMethod == "LERC_ZSTD") {
        papszOpts = CSLSetNameValue(papszOpts, "MAX_Z_ERROR", to_string(opts.maxError).c_str());
        if (compressMethod == "LERC_ZSTD") {
            papszOpts = CSLSetNameValue(papszOpts, "LERC_ZSTD_COMPRESSION_LEVEL",
                                        to_string(opts.level).c_str());
        }
    }

    if (opts.predictor > 0) {
        papszOpts = CSLSetNameValue(papszOpts, "PREDICTOR", to_string(opts.predictor).c_str());
    }

    if (opts.tiled) {
        papszOpts = CSLSetNameValue(papszOpts, "TILED", "YES");
        papszOpts = CSLSetNameValue(papszOpts, "BLOCKXSIZE", to_string(opts.blockSize).c_str());
        papszOpts = CSLSetNameValue(papszOpts, "BLOCKYSIZE", to_string(opts.blockSize).c_str());
    }

    GDALDataset* poDst = poDrv->Create(output.c_str(), nXSize, nYSize, nBands, eDT, papszOpts);
    CSLDestroy(papszOpts);
    if (!poDst) {
        LOG_ERROR("compressRaster: Cannot create output: " + output);
        GDALClose(poSrc);
        return 1;
    }

    poDst->SetGeoTransform(adfGeoTransform);
    if (pszProj && pszProj[0] != '\0') poDst->SetProjection(pszProj);

    // Copy bands
    int nPixelSize = GDALGetDataTypeSizeBytes(eDT);
    for (int b = 1; b <= nBands; ++b) {
        GDALRasterBand* poSrcBand = poSrc->GetRasterBand(b);
        GDALRasterBand* poDstBand = poDst->GetRasterBand(b);

        int bHasNoData = FALSE;
        double dfNoData = poSrcBand->GetNoDataValue(&bHasNoData);
        if (bHasNoData) poDstBand->SetNoDataValue(dfNoData);

        void* pBuf = CPLMalloc(nXSize * nPixelSize);
        for (int y = 0; y < nYSize; ++y) {
            if (poSrcBand->RasterIO(GF_Read, 0, y, nXSize, 1, pBuf, nXSize, 1, eDT, 0, 0) != CE_None) {
                LOG_ERROR("compressRaster: RasterIO read failed at band " + to_string(b) + " line " + to_string(y));
                CPLFree(pBuf);
                GDALClose(poDst); GDALClose(poSrc);
                return 1;
            }
            if (poDstBand->RasterIO(GF_Write, 0, y, nXSize, 1, pBuf, nXSize, 1, eDT, 0, 0) != CE_None) {
                LOG_ERROR("compressRaster: RasterIO write failed at band " + to_string(b) + " line " + to_string(y));
                CPLFree(pBuf);
                GDALClose(poDst); GDALClose(poSrc);
                return 1;
            }
        }
        CPLFree(pBuf);
    }

    // Build overviews
    if (opts.overview) {
        int anLevels[] = {2, 4, 8, 16};
        poDst->BuildOverviews("NEAREST", 4, anLevels, 0, nullptr, nullptr, nullptr);
        LOG_INFO("compressRaster: Overviews built (2,4,8,16)");
    }

    GDALClose(poDst);
    GDALClose(poSrc);

    // Compression report
    auto tEnd = chrono::steady_clock::now();
    double dElapsed = chrono::duration<double>(tEnd - tStart).count();

    auto nSrcSize = fs::file_size(input);
    auto nDstSize = fs::file_size(output);
    double dSrcMB = nSrcSize / (1024.0 * 1024.0);
    double dDstMB = nDstSize / (1024.0 * 1024.0);
    double dReduction = (1.0 - dDstMB / dSrcMB) * 100.0;

    if (dDstMB > dSrcMB) {
        LOG_WARN("compressRaster: Compressed size > original size (compression ratio < 0)");
    }

    ostringstream oss;
    oss << fixed << setprecision(1);
    oss << "compressRaster: " << fs::path(input).filename().string()
        << ": " << dSrcMB << " MB -> " << dDstMB << " MB"
        << " (" << dReduction << "% reduction, "
        << compressMethod << " L" << opts.level;
    if (opts.tiled) oss << " TILED";
    oss << ", " << fixed << setprecision(1) << dElapsed << "s)";
    LOG_INFO(oss.str());
    cout << oss.str() << endl;

    return 0;
}

// ============================================================================
// detectChange() — Change Detection
// ============================================================================

int RasterPreprocessor::detectChange(const string& before,
                                      const string& after,
                                      const string& output,
                                      bool encodeMode)
{
    LOG_INFO("detectChange: before=" + before + " after=" + after + " encode=" + to_string(encodeMode));

    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    GDALDataset* poBefore = (GDALDataset*)GDALOpen(before.c_str(), GA_ReadOnly);
    if (!poBefore) {
        LOG_ERROR("detectChange: Cannot open before raster: " + before);
        return 1;
    }
    GDALDataset* poAfter = (GDALDataset*)GDALOpen(after.c_str(), GA_ReadOnly);
    if (!poAfter) {
        LOG_ERROR("detectChange: Cannot open after raster: " + after);
        GDALClose(poBefore);
        return 1;
    }

    int nXSize = poBefore->GetRasterXSize();
    int nYSize = poBefore->GetRasterYSize();
    if (nXSize != poAfter->GetRasterXSize() || nYSize != poAfter->GetRasterYSize()) {
        LOG_ERROR("detectChange: Dimension mismatch: before=" +
                  to_string(nXSize) + "x" + to_string(nYSize) +
                  " after=" + to_string(poAfter->GetRasterXSize()) + "x" +
                  to_string(poAfter->GetRasterYSize()));
        GDALClose(poBefore); GDALClose(poAfter);
        return 1;
    }

    if (poBefore->GetRasterCount() != 1 || poAfter->GetRasterCount() != 1) {
        LOG_ERROR("detectChange: Both rasters must be single-band");
        GDALClose(poBefore); GDALClose(poAfter);
        return 1;
    }

    GDALDataType eDT = poBefore->GetRasterBand(1)->GetRasterDataType();
    int nPixelSize = GDALGetDataTypeSizeBytes(eDT);
    int nPixels = nXSize * nYSize;

    void* pBefore = CPLMalloc(nPixels * nPixelSize);
    void* pAfter  = CPLMalloc(nPixels * nPixelSize);
    poBefore->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, nXSize, nYSize, pBefore, nXSize, nYSize, eDT, 0, 0);
    poAfter->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, nXSize, nYSize, pAfter, nXSize, nYSize, eDT, 0, 0);

    int bHasNoDataBefore = FALSE, bHasNoDataAfter = FALSE;
    double dfNoDataBefore = poBefore->GetRasterBand(1)->GetNoDataValue(&bHasNoDataBefore);
    double dfNoDataAfter  = poAfter->GetRasterBand(1)->GetNoDataValue(&bHasNoDataAfter);

    GDALDriver* poDrv = GetGDALDriverManager()->GetDriverByName("GTIFF");
    GDALDataType eOutDT = encodeMode ? GDT_Int32 : GDT_Byte;
    int nOutPixelSize = GDALGetDataTypeSizeBytes(eOutDT);

    char** papszOpts = nullptr;
    papszOpts = CSLSetNameValue(papszOpts, "COMPRESS", "DEFLATE");
    papszOpts = CSLSetNameValue(papszOpts, "PREDICTOR", "2");
    papszOpts = CSLSetNameValue(papszOpts, "ZLEVEL", "9");
    papszOpts = CSLSetNameValue(papszOpts, "BIGTIFF", "IF_NEEDED");

    GDALDataset* poDst = poDrv->Create(output.c_str(), nXSize, nYSize, 1, eOutDT, papszOpts);
    CSLDestroy(papszOpts);
    if (!poDst) {
        LOG_ERROR("detectChange: Cannot create output: " + output);
        CPLFree(pBefore); CPLFree(pAfter);
        GDALClose(poBefore); GDALClose(poAfter);
        return 1;
    }

    double adfGeoTransform[6];
    poBefore->GetGeoTransform(adfGeoTransform);
    poDst->SetGeoTransform(adfGeoTransform);
    const char* pszProj = poBefore->GetProjectionRef();
    if (pszProj && pszProj[0] != '\0') poDst->SetProjection(pszProj);

    void* pOut = CPLMalloc(nPixels * nOutPixelSize);
    long long nChangeCount = 0;
    long long nNoDataCount = 0;

    for (int i = 0; i < nPixels; ++i) {
        double valBefore, valAfter;
        switch (eDT) {
            case GDT_Byte:     valBefore = ((unsigned char*)pBefore)[i]; valAfter = ((unsigned char*)pAfter)[i]; break;
            case GDT_Int16:    valBefore = ((short*)pBefore)[i]; valAfter = ((short*)pAfter)[i]; break;
            case GDT_UInt16:   valBefore = ((unsigned short*)pBefore)[i]; valAfter = ((unsigned short*)pAfter)[i]; break;
            case GDT_Int32:    valBefore = ((int*)pBefore)[i]; valAfter = ((int*)pAfter)[i]; break;
            case GDT_Float32:  valBefore = ((float*)pBefore)[i]; valAfter = ((float*)pAfter)[i]; break;
            default:           valBefore = ((double*)pBefore)[i]; valAfter = ((double*)pAfter)[i]; break;
        }

        bool bIsNoData = false;
        if (bHasNoDataBefore && fabs(valBefore - dfNoDataBefore) < 1e-10) bIsNoData = true;
        if (bHasNoDataAfter  && fabs(valAfter  - dfNoDataAfter)  < 1e-10) bIsNoData = true;

        if (bIsNoData) {
            nNoDataCount++;
            if (encodeMode) ((int*)pOut)[i] = 0;
            else ((unsigned char*)pOut)[i] = 0;
        } else if (valBefore != valAfter) {
            nChangeCount++;
            if (encodeMode) ((int*)pOut)[i] = (int)valBefore * 1000 + (int)valAfter;
            else ((unsigned char*)pOut)[i] = 1;
        } else {
            if (encodeMode) ((int*)pOut)[i] = 0;
            else ((unsigned char*)pOut)[i] = 0;
        }
    }

    poDst->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, nXSize, nYSize, pOut, nXSize, nYSize, eOutDT, 0, 0);
    if (encodeMode) poDst->GetRasterBand(1)->SetNoDataValue(0);

    GDALClose(poDst);
    CPLFree(pBefore); CPLFree(pAfter); CPLFree(pOut);
    GDALClose(poBefore); GDALClose(poAfter);

    double dChangeRatio = (nPixels > 0) ? (double)nChangeCount / nPixels * 100.0 : 0.0;
    ostringstream oss;
    oss << "detectChange: " << nChangeCount << " / " << nPixels
        << " pixels changed (" << fixed << setprecision(2) << dChangeRatio << "%)";
    if (nNoDataCount > 0) oss << ", " << nNoDataCount << " NoData pixels";
    if (nChangeCount == 0) oss << " (no change detected)";
    LOG_INFO(oss.str());
    cout << oss.str() << endl;

    return 0;
}

// ============================================================================
// normalizeRaster() — Normalize
// ============================================================================

int RasterPreprocessor::normalizeRaster(const vector<string>& inputs,
                                          const vector<string>& outputs,
                                          double rangeMin, double rangeMax,
                                          bool manualMin, double minVal,
                                          bool manualMax, double maxVal)
{
    LOG_INFO("normalizeRaster: " + to_string(inputs.size()) + " files, range=[" +
             to_string(rangeMin) + "," + to_string(rangeMax) + "]");

    if (inputs.size() != outputs.size()) {
        LOG_ERROR("normalizeRaster: inputs and outputs count mismatch");
        return 1;
    }

    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    for (size_t f = 0; f < inputs.size(); ++f) {
        const string& qstrInput = inputs[f];
        const string& qstrOutput = outputs[f];

        LOG_INFO("normalizeRaster: Processing " + qstrInput);

        GDALDataset* poSrc = (GDALDataset*)GDALOpen(qstrInput.c_str(), GA_ReadOnly);
        if (!poSrc) {
            LOG_ERROR("normalizeRaster: Cannot open input: " + qstrInput);
            return 1;
        }

        int nXSize = poSrc->GetRasterXSize();
        int nYSize = poSrc->GetRasterYSize();
        int nBands = poSrc->GetRasterCount();
        int nPixels = nXSize * nYSize;

        double adfGeoTransform[6];
        poSrc->GetGeoTransform(adfGeoTransform);
        const char* pszProj = poSrc->GetProjectionRef();

        GDALDriver* poDrv = GetGDALDriverManager()->GetDriverByName("GTIFF");
        char** papszOpts = nullptr;
        papszOpts = CSLSetNameValue(papszOpts, "COMPRESS", "DEFLATE");
        papszOpts = CSLSetNameValue(papszOpts, "PREDICTOR", "2");
        papszOpts = CSLSetNameValue(papszOpts, "ZLEVEL", "9");
        papszOpts = CSLSetNameValue(papszOpts, "BIGTIFF", "IF_NEEDED");

        GDALDataset* poDst = poDrv->Create(qstrOutput.c_str(), nXSize, nYSize, nBands, GDT_Float32, papszOpts);
        CSLDestroy(papszOpts);
        if (!poDst) {
            LOG_ERROR("normalizeRaster: Cannot create output: " + qstrOutput);
            GDALClose(poSrc);
            return 1;
        }

        poDst->SetGeoTransform(adfGeoTransform);
        if (pszProj && pszProj[0] != '\0') poDst->SetProjection(pszProj);

        for (int b = 1; b <= nBands; ++b) {
            GDALRasterBand* poSrcBand = poSrc->GetRasterBand(b);
            GDALDataType eDT = poSrcBand->GetRasterDataType();
            int nPixelSize = GDALGetDataTypeSizeBytes(eDT);

            int bHasNoData = FALSE;
            double dfNoData = poSrcBand->GetNoDataValue(&bHasNoData);

            void* pBuf = CPLMalloc(nPixels * nPixelSize);
            float* pOut = (float*)CPLMalloc(nPixels * sizeof(float));
            poSrcBand->RasterIO(GF_Read, 0, 0, nXSize, nYSize, pBuf, nXSize, nYSize, eDT, 0, 0);

            // Scan min/max
            double dMin = 1e300, dMax = -1e300;
            bool bAllNoData = true;

            for (int i = 0; i < nPixels; ++i) {
                double val;
                switch (eDT) {
                    case GDT_Byte:     val = ((unsigned char*)pBuf)[i]; break;
                    case GDT_Int16:    val = ((short*)pBuf)[i]; break;
                    case GDT_UInt16:   val = ((unsigned short*)pBuf)[i]; break;
                    case GDT_Int32:    val = ((int*)pBuf)[i]; break;
                    case GDT_Float32:  val = ((float*)pBuf)[i]; break;
                    default:           val = ((double*)pBuf)[i]; break;
                }
                if (bHasNoData && fabs(val - dfNoData) < 1e-10) continue;
                bAllNoData = false;
                if (val < dMin) dMin = val;
                if (val > dMax) dMax = val;
            }

            if (bAllNoData) {
                LOG_ERROR("normalizeRaster: All pixels are NoData in band " + to_string(b) + " of " + qstrInput);
                CPLFree(pBuf); CPLFree(pOut);
                GDALClose(poDst); GDALClose(poSrc);
                return 1;
            }

            if (manualMin) dMin = minVal;
            if (manualMax) dMax = maxVal;

            LOG_INFO("normalizeRaster: Band " + to_string(b) + " min=" +
                     to_string(dMin) + " max=" + to_string(dMax));

            if (fabs(dMax - dMin) < 1e-15) {
                LOG_WARN("normalizeRaster: min == max, output will be all zeros");
                for (int i = 0; i < nPixels; ++i) pOut[i] = 0.0f;
            } else {
                double dRange = rangeMax - rangeMin;
                double dSpan = dMax - dMin;
                for (int i = 0; i < nPixels; ++i) {
                    double val;
                    switch (eDT) {
                        case GDT_Byte:     val = ((unsigned char*)pBuf)[i]; break;
                        case GDT_Int16:    val = ((short*)pBuf)[i]; break;
                        case GDT_UInt16:   val = ((unsigned short*)pBuf)[i]; break;
                        case GDT_Int32:    val = ((int*)pBuf)[i]; break;
                        case GDT_Float32:  val = ((float*)pBuf)[i]; break;
                        default:           val = ((double*)pBuf)[i]; break;
                    }
                    if (bHasNoData && fabs(val - dfNoData) < 1e-10) {
                        pOut[i] = (float)dfNoData;
                    } else {
                        pOut[i] = (float)((val - dMin) / dSpan * dRange + rangeMin);
                    }
                }
            }

            poDst->GetRasterBand(b)->RasterIO(GF_Write, 0, 0, nXSize, nYSize, pOut, nXSize, nYSize, GDT_Float32, 0, 0);
            if (bHasNoData) poDst->GetRasterBand(b)->SetNoDataValue(dfNoData);

            CPLFree(pBuf);
            CPLFree(pOut);
        }

        GDALClose(poDst);
        GDALClose(poSrc);
        LOG_INFO("normalizeRaster: Output written: " + qstrOutput);
    }

    return 0;
}

// ============================================================================
// reclassifyRaster() — Reclassify + NoData Marking
// ============================================================================

int RasterPreprocessor::reclassifyRaster(const string& input,
                                           const string& output,
                                           const ReclassRule& rule,
                                           int nodataValue)
{
    LOG_INFO("reclassifyRaster: input=" + input + " output=" + output);

    if (rule.remap.empty() && rule.nodataValues.empty()) {
        LOG_ERROR("reclassifyRaster: Empty reclass rule (no remap and no nodata)");
        return 1;
    }

    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    GDALDataset* poSrc = (GDALDataset*)GDALOpen(input.c_str(), GA_ReadOnly);
    if (!poSrc) {
        LOG_ERROR("reclassifyRaster: Cannot open input: " + input);
        return 1;
    }

    if (poSrc->GetRasterCount() != 1) {
        LOG_ERROR("reclassifyRaster: Only single-band rasters supported, got " +
                  to_string(poSrc->GetRasterCount()) + " bands");
        GDALClose(poSrc);
        return 1;
    }

    int nXSize = poSrc->GetRasterXSize();
    int nYSize = poSrc->GetRasterYSize();
    int nPixels = nXSize * nYSize;
    GDALDataType eDT = poSrc->GetRasterBand(1)->GetRasterDataType();
    int nPixelSize = GDALGetDataTypeSizeBytes(eDT);

    double adfGeoTransform[6];
    poSrc->GetGeoTransform(adfGeoTransform);
    const char* pszProj = poSrc->GetProjectionRef();

    void* pBuf = CPLMalloc(nPixels * nPixelSize);
    void* pOut = CPLMalloc(nPixels * nPixelSize);
    poSrc->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, nXSize, nYSize, pBuf, nXSize, nYSize, eDT, 0, 0);

    unordered_set<int> setNodata(rule.nodataValues.begin(), rule.nodataValues.end());

    unordered_map<int, long long> mapRemapStats;
    long long nNodataCount = 0;

    for (int i = 0; i < nPixels; ++i) {
        int nVal;
        switch (eDT) {
            case GDT_Byte:     nVal = ((unsigned char*)pBuf)[i]; break;
            case GDT_Int16:    nVal = ((short*)pBuf)[i]; break;
            case GDT_UInt16:   nVal = ((unsigned short*)pBuf)[i]; break;
            case GDT_Int32:    nVal = ((int*)pBuf)[i]; break;
            default:           nVal = (int)((float*)pBuf)[i]; break;
        }

        int nOut = nVal;

        if (setNodata.count(nVal)) {
            if (rule.remap.count(nVal)) {
                nOut = rule.remap.at(nVal);
                mapRemapStats[nVal]++;
                LOG_WARN("reclassifyRaster: Value " + to_string(nVal) +
                         " in both remap and nodata, remap takes priority");
            } else {
                nOut = nodataValue;
                nNodataCount++;
            }
        } else if (rule.remap.count(nVal)) {
            nOut = rule.remap.at(nVal);
            mapRemapStats[nVal]++;
        }

        switch (eDT) {
            case GDT_Byte:     ((unsigned char*)pOut)[i] = (unsigned char)nOut; break;
            case GDT_Int16:    ((short*)pOut)[i] = (short)nOut; break;
            case GDT_UInt16:   ((unsigned short*)pOut)[i] = (unsigned short)nOut; break;
            case GDT_Int32:    ((int*)pOut)[i] = nOut; break;
            default:           ((float*)pOut)[i] = (float)nOut; break;
        }
    }

    GDALDriver* poDrv = GetGDALDriverManager()->GetDriverByName("GTIFF");
    char** papszOpts = nullptr;
    papszOpts = CSLSetNameValue(papszOpts, "COMPRESS", "DEFLATE");
    papszOpts = CSLSetNameValue(papszOpts, "PREDICTOR", "2");
    papszOpts = CSLSetNameValue(papszOpts, "ZLEVEL", "9");
    papszOpts = CSLSetNameValue(papszOpts, "BIGTIFF", "IF_NEEDED");

    GDALDataset* poDst = poDrv->Create(output.c_str(), nXSize, nYSize, 1, eDT, papszOpts);
    CSLDestroy(papszOpts);
    if (!poDst) {
        LOG_ERROR("reclassifyRaster: Cannot create output: " + output);
        CPLFree(pBuf); CPLFree(pOut);
        GDALClose(poSrc);
        return 1;
    }

    poDst->SetGeoTransform(adfGeoTransform);
    if (pszProj && pszProj[0] != '\0') poDst->SetProjection(pszProj);
    poDst->GetRasterBand(1)->SetNoDataValue(nodataValue);
    poDst->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, nXSize, nYSize, pOut, nXSize, nYSize, eDT, 0, 0);

    GDALClose(poDst);
    CPLFree(pBuf); CPLFree(pOut);
    GDALClose(poSrc);

    ostringstream oss;
    oss << "reclassifyRaster: Remapped ";
    bool first = true;
    for (const auto& kv : mapRemapStats) {
        if (!first) oss << ", ";
        oss << kv.first << "->" << rule.remap.at(kv.first) << " (" << kv.second << " px)";
        first = false;
    }
    oss << "; NoData marked: " << nNodataCount << " px";
    LOG_INFO(oss.str());
    cout << oss.str() << endl;

    return 0;
}

// ============================================================================
// resampleToBase() — Resample to Base
// ============================================================================

int RasterPreprocessor::resampleToBase(const string& basePath,
                                         const vector<string>& inputs,
                                         const vector<string>& outputs,
                                         const string& method)
{
    LOG_INFO("resampleToBase: base=" + basePath + " method=" + method +
             " inputs=" + to_string(inputs.size()));

    if (inputs.size() != outputs.size()) {
        LOG_ERROR("resampleToBase: inputs and outputs count mismatch");
        return 1;
    }

    GDALAllRegister();
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

    GDALDataset* poBase = (GDALDataset*)GDALOpen(basePath.c_str(), GA_ReadOnly);
    if (!poBase) {
        LOG_ERROR("resampleToBase: Cannot open base raster: " + basePath);
        return 1;
    }

    double adfBaseGT[6];
    poBase->GetGeoTransform(adfBaseGT);
    const char* pszBaseProj = poBase->GetProjectionRef();
    if (!pszBaseProj || pszBaseProj[0] == '\0') {
        LOG_ERROR("resampleToBase: Base raster has no projection info");
        GDALClose(poBase);
        return 1;
    }

    int nBaseX = poBase->GetRasterXSize();
    int nBaseY = poBase->GetRasterYSize();
    string strBaseProj(pszBaseProj);

    GDALResampleAlg eResample = (method == "bilinear") ? GRA_Bilinear : GRA_NearestNeighbour;

    for (size_t i = 0; i < inputs.size(); ++i) {
        const string& qstrInput = inputs[i];
        const string& qstrOutput = outputs[i];

        LOG_INFO("resampleToBase: Processing " + qstrInput + " -> " + qstrOutput);

        GDALDataset* poSrc = (GDALDataset*)GDALOpen(qstrInput.c_str(), GA_ReadOnly);
        if (!poSrc) {
            LOG_ERROR("resampleToBase: Cannot open input: " + qstrInput);
            GDALClose(poBase);
            return 1;
        }

        double adfSrcGT[6];
        poSrc->GetGeoTransform(adfSrcGT);
        const char* pszSrcProj = poSrc->GetProjectionRef();
        int nSrcX = poSrc->GetRasterXSize();
        int nSrcY = poSrc->GetRasterYSize();

        // Check if already aligned
        bool bAlreadyAligned = (nSrcX == nBaseX && nSrcY == nBaseY);
        if (bAlreadyAligned && pszSrcProj && pszSrcProj[0] != '\0') {
            for (int k = 0; k < 6; ++k) {
                if (fabs(adfSrcGT[k] - adfBaseGT[k]) > 1e-10) { bAlreadyAligned = false; break; }
            }
            if (bAlreadyAligned && string(pszSrcProj) != strBaseProj) bAlreadyAligned = false;
        } else {
            bAlreadyAligned = false;
        }

        if (bAlreadyAligned) {
            LOG_INFO("resampleToBase: Already aligned, copying directly: " + qstrInput);
            GDALDriver* poDrv = GetGDALDriverManager()->GetDriverByName("GTIFF");
            char** papszOpts = nullptr;
            papszOpts = CSLSetNameValue(papszOpts, "COMPRESS", "DEFLATE");
            papszOpts = CSLSetNameValue(papszOpts, "PREDICTOR", "2");
            papszOpts = CSLSetNameValue(papszOpts, "ZLEVEL", "9");
            GDALDataset* poDst = poDrv->CreateCopy(qstrOutput.c_str(), poSrc, FALSE, papszOpts, nullptr, nullptr);
            CSLDestroy(papszOpts);
            if (!poDst) {
                LOG_ERROR("resampleToBase: Copy failed for: " + qstrInput);
                GDALClose(poSrc); GDALClose(poBase);
                return 1;
            }
            GDALClose(poDst);
            GDALClose(poSrc);
            continue;
        }

        // Warp using GDALReprojectImage
        int nBands = poSrc->GetRasterCount();
        GDALDataType eDT = poSrc->GetRasterBand(1)->GetRasterDataType();

        GDALDriver* poDrv = GetGDALDriverManager()->GetDriverByName("GTIFF");
        char** papszOpts = nullptr;
        papszOpts = CSLSetNameValue(papszOpts, "COMPRESS", "DEFLATE");
        papszOpts = CSLSetNameValue(papszOpts, "PREDICTOR", "2");
        papszOpts = CSLSetNameValue(papszOpts, "ZLEVEL", "9");
        papszOpts = CSLSetNameValue(papszOpts, "BIGTIFF", "IF_NEEDED");

        GDALDataset* poDst = poDrv->Create(qstrOutput.c_str(), nBaseX, nBaseY, nBands, eDT, papszOpts);
        CSLDestroy(papszOpts);
        if (!poDst) {
            LOG_ERROR("resampleToBase: Cannot create output: " + qstrOutput);
            GDALClose(poSrc); GDALClose(poBase);
            return 1;
        }

        poDst->SetGeoTransform(adfBaseGT);
        poDst->SetProjection(strBaseProj.c_str());

        // Copy NoData from source
        for (int b = 1; b <= nBands; ++b) {
            int bHasNoData = FALSE;
            double dfNoData = poSrc->GetRasterBand(b)->GetNoDataValue(&bHasNoData);
            if (bHasNoData) poDst->GetRasterBand(b)->SetNoDataValue(dfNoData);
        }

        // Execute reprojection + resampling
        CPLErr eErr = GDALReprojectImage(poSrc, pszSrcProj, poDst, strBaseProj.c_str(),
                                          eResample, 0.0, 0.0, nullptr, nullptr, nullptr);

        if (eErr != CE_None) {
            LOG_ERROR("resampleToBase: Warp failed for: " + qstrInput);
            GDALClose(poDst); GDALClose(poSrc); GDALClose(poBase);
            return 1;
        }

        GDALClose(poDst);
        GDALClose(poSrc);
        LOG_INFO("resampleToBase: Output written: " + qstrOutput);
    }

    GDALClose(poBase);
    return 0;
}

// ============================================================================
// JSON Rule Parser — parseReclassRule
// ============================================================================

RasterPreprocessor::ReclassRule RasterPreprocessor::parseReclassRule(const string& jsonPath)
{
    ReclassRule rule;

    ifstream ifs(jsonPath);
    if (!ifs.is_open()) {
        LOG_ERROR("parseReclassRule: Cannot open JSON file: " + jsonPath);
        return rule;
    }

    string content((istreambuf_iterator<char>(ifs)), istreambuf_iterator<char>());
    ifs.close();

    // Simple JSON parser for fixed structure:
    // { "remap": { "1": 1, "2": 1 }, "nodata": [255, 0] }

    // Find "remap" block
    size_t posRemap = content.find("\"remap\"");
    if (posRemap != string::npos) {
        size_t braceStart = content.find('{', posRemap);
        size_t braceEnd = content.find('}', braceStart);
        if (braceStart != string::npos && braceEnd != string::npos) {
            string remapBlock = content.substr(braceStart + 1, braceEnd - braceStart - 1);
            // Parse "key": value pairs
            size_t pos = 0;
            while (pos < remapBlock.size()) {
                size_t keyStart = remapBlock.find('"', pos);
                if (keyStart == string::npos) break;
                size_t keyEnd = remapBlock.find('"', keyStart + 1);
                if (keyEnd == string::npos) break;
                string keyStr = remapBlock.substr(keyStart + 1, keyEnd - keyStart - 1);

                size_t colon = remapBlock.find(':', keyEnd);
                if (colon == string::npos) break;

                size_t valStart = colon + 1;
                while (valStart < remapBlock.size() && (remapBlock[valStart] == ' ' || remapBlock[valStart] == '\t')) valStart++;
                size_t valEnd = valStart;
                while (valEnd < remapBlock.size() && remapBlock[valEnd] != ',' && remapBlock[valEnd] != '}') valEnd++;
                string valStr = remapBlock.substr(valStart, valEnd - valStart);
                // Trim whitespace
                valStr.erase(valStr.find_last_not_of(" \t\n\r") + 1);

                try {
                    int key = stoi(keyStr);
                    int val = stoi(valStr);
                    rule.remap[key] = val;
                } catch (...) {
                    LOG_ERROR("parseReclassRule: Invalid remap entry: " + keyStr + ":" + valStr);
                }

                pos = valEnd;
            }
        }
    }

    // Find "nodata" block
    size_t posNodata = content.find("\"nodata\"");
    if (posNodata != string::npos) {
        size_t bracketStart = content.find('[', posNodata);
        size_t bracketEnd = content.find(']', bracketStart);
        if (bracketStart != string::npos && bracketEnd != string::npos) {
            string nodataBlock = content.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
            stringstream ss(nodataBlock);
            string token;
            while (getline(ss, token, ',')) {
                // Trim whitespace
                token.erase(0, token.find_first_not_of(" \t\n\r"));
                token.erase(token.find_last_not_of(" \t\n\r") + 1);
                if (token.empty()) continue;
                try {
                    rule.nodataValues.push_back(stoi(token));
                } catch (...) {
                    LOG_ERROR("parseReclassRule: Invalid nodata value: " + token);
                }
            }
        }
    }

    LOG_INFO("parseReclassRule: Parsed " + to_string(rule.remap.size()) +
             " remap entries, " + to_string(rule.nodataValues.size()) + " nodata values");

    return rule;
}

RasterPreprocessor::ReclassRule RasterPreprocessor::parseReclassRuleFromArgs(
    const string& remapStr, const string& nodataStr)
{
    ReclassRule rule;

    // Parse remap: "k1:v1,k2:v2"
    if (!remapStr.empty()) {
        stringstream ss(remapStr);
        string token;
        while (getline(ss, token, ',')) {
            auto p = token.find(':');
            if (p == string::npos) {
                LOG_ERROR("parseReclassRuleFromArgs: Invalid remap format: " + token);
                continue;
            }
            try {
                int key = stoi(token.substr(0, p));
                int val = stoi(token.substr(p + 1));
                rule.remap[key] = val;
            } catch (...) {
                LOG_ERROR("parseReclassRuleFromArgs: Invalid remap entry: " + token);
            }
        }
    }

    // Parse nodata: "v1,v2,v3"
    if (!nodataStr.empty()) {
        stringstream ss(nodataStr);
        string token;
        while (getline(ss, token, ',')) {
            try {
                rule.nodataValues.push_back(stoi(token));
            } catch (...) {
                LOG_ERROR("parseReclassRuleFromArgs: Invalid nodata value: " + token);
            }
        }
    }

    return rule;
}
