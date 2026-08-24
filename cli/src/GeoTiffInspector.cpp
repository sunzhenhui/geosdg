/**
 * @file GeoTiffInspector.cpp
 * @brief GeoTIFF 文件元数据与数据质量检查器实现 / GeoTIFF metadata and data quality inspector implementation
 */

#include "GeoTiffInspector.h"
#include "Logger.h"
#include "gdal_priv.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <set>
#include <unordered_map>

#if !defined(GEOSDG_PLATFORM_WINDOWS)
    #include <cpl_conv.h>
#endif

using namespace std;

// ============================================================================
// 构造/析构 / Constructor / Destructor
// ============================================================================

GeoTiffInspector::GeoTiffInspector()
{
}

GeoTiffInspector::~GeoTiffInspector()
{
}

// ============================================================================
// InspectionReport 输出方法 / InspectionReport output methods
// ============================================================================

string InspectionReport::toOutputLine() const
{
    ostringstream oss;
    oss << "file=" << filePath
        << " width=" << width
        << " height=" << height
        << " bands=" << bands
        << " proj=" << (projection.empty() ? "NONE" : projection)
        << " nodata=" << (hasNodata ? to_string(static_cast<long long>(nodata)) : "NONE")
        << " dtype=" << dataType;

    // L2 line
    oss << "\nmin=" << fixed << setprecision(2) << minValue
        << " max=" << maxValue
        << " mean=" << meanValue
        << " nodata_pct=" << setprecision(1) << (nodataPct * 100.0)
        << " all_zero=" << (allZero ? "true" : "false")
        << " all_same=" << (allSame ? "true" : "false")
        << " proj_match=" << (projMatch ? "true" : "false")
        << " dim_match=" << (dimMatch ? "true" : "false");

    // Type coverage line
    oss << "\ntype_coverage=";
    if (!typeCoverage.empty()) {
        bool first = true;
        for (int v : typeCoverage) {
            if (!first) oss << ",";
            oss << v;
            first = false;
        }
    }
    oss << " category_count=" << categoryCount
        << " is_integer=" << (isInteger ? "true" : "false");

    return oss.str();
}

vector<string> InspectionReport::toDiagnosticLines() const
{
    vector<string> lines;
    for (const auto& w : warnings) {
        lines.push_back("# WARN: " + w);
    }
    for (const auto& e : errors) {
        lines.push_back("# ERROR: " + e);
    }
    return lines;
}

// ============================================================================
// 公共接口 / Public interface
// ============================================================================

InspectionReport GeoTiffInspector::inspect(const string& filePath, const InspectOptions& opts)
{
    InspectionReport report;
    report.filePath = filePath;

    LOG_INFO("GeoTiffInspector: inspecting " + filePath);

    // L1: 元数据检查
    void* rawDS = performL1Check(filePath, report);
    if (!rawDS) {
        report.hasError = true;
        return report;
    }
    GDALDataset* poDS = static_cast<GDALDataset*>(rawDS);

    // L2: 值统计检查
    performL2Check(poDS, report, opts);

    // 参考文件匹配检查
    if (!opts.refPath.empty()) {
        performRefCheck(opts.refPath, report);
    }

    // --expect-int 检查
    if (opts.expectInt && !report.isInteger) {
        report.warnings.push_back(
            "dtype_float → LUCC data should be integer type, got " + report.dataType +
            ". Check data export pipeline.");
    }

    // --types 地类覆盖检查
    if (!opts.expectedTypes.empty()) {
        set<int> missing;
        for (int t : opts.expectedTypes) {
            if (report.typeCoverage.find(t) == report.typeCoverage.end()) {
                missing.insert(t);
            }
        }
        if (!missing.empty()) {
            ostringstream oss;
            oss << "type_missing → Expected types ";
            bool first = true;
            for (int t : opts.expectedTypes) {
                if (!first) oss << ",";
                oss << t;
                first = false;
            }
            oss << " not all present in data. Missing: ";
            first = true;
            for (int t : missing) {
                if (!first) oss << ",";
                oss << t;
                first = false;
            }
            report.warnings.push_back(oss.str());
        }
    }

    // --category 类别完整性检查
    if (opts.expectedCategoryCount > 0 && report.categoryCount < opts.expectedCategoryCount) {
        ostringstream oss;
        oss << "category_miss → Expected " << opts.expectedCategoryCount
            << " categories, only " << report.categoryCount << " found (";
        bool first = true;
        for (int v : report.typeCoverage) {
            if (!first) oss << ",";
            oss << v;
            first = false;
        }
        oss << "). Missing: ";
        // 计算缺失的类别
        bool firstMissing = true;
        for (int i = 1; i <= opts.expectedCategoryCount; ++i) {
            if (report.typeCoverage.find(i) == report.typeCoverage.end()) {
                if (!firstMissing) oss << ",";
                oss << i;
                firstMissing = false;
            }
        }
        oss << ".";
        report.warnings.push_back(oss.str());
    }

    // 关闭数据集
    GDALClose(poDS);

    // 汇总 hasError
    if (!report.errors.empty()) {
        report.hasError = true;
    }

    LOG_INFO("GeoTiffInspector: inspection complete, errors=" + to_string(report.errors.size())
             + " warnings=" + to_string(report.warnings.size()));

    return report;
}

// ============================================================================
// L1 元数据检查 / L1 metadata check
// ============================================================================

void* GeoTiffInspector::performL1Check(const string& filePath, InspectionReport& report)
{
    GDALAllRegister();
#if !defined(GEOSDG_PLATFORM_WINDOWS)
    CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
#endif

    GDALDataset* poDS = (GDALDataset*)GDALOpenEx(filePath.c_str(), GA_ReadOnly, nullptr, nullptr, nullptr);
    if (poDS == nullptr) {
        report.errors.push_back("open_failed → Cannot open file: " + filePath);
        LOG_ERROR("GeoTiffInspector: Cannot open file: " + filePath);
        return nullptr;
    }

    // 维度
    report.width  = poDS->GetRasterXSize();
    report.height = poDS->GetRasterYSize();
    report.bands  = poDS->GetRasterCount();

    if (report.width <= 0 || report.height <= 0) {
        report.errors.push_back("invalid_dims → Raster dimensions are zero or negative: "
                                + to_string(report.width) + "x" + to_string(report.height));
        LOG_ERROR("GeoTiffInspector: Invalid raster dimensions: "
                  + to_string(report.width) + "x" + to_string(report.height));
    }

    // 投影
    const char* projRef = poDS->GetProjectionRef();
    if (projRef && strlen(projRef) > 0) {
        // 简化投影名称：提取最后一个 AUTHORITY["EPSG","code"] 作为 CRS 标识
        string projStr(projRef);
        // 从后往前查找 AUTHORITY["EPSG" — 最后一个通常是 CRS 本身的 authority
        size_t pos = projStr.rfind("AUTHORITY[\"EPSG\"");
        if (pos != string::npos) {
            // 跳过 AUTHORITY["EPSG" 找到后面的 ,"code"
            size_t searchStart = pos + 16;  // len(AUTHORITY["EPSG") = 16
            // 找到下一个引号（开始 code 的引号）
            size_t codeStart = projStr.find("\"", searchStart);
            if (codeStart != string::npos) {
                size_t codeEnd = projStr.find("\"", codeStart + 1);
                if (codeEnd != string::npos) {
                    report.projection = "EPSG:" + projStr.substr(codeStart + 1, codeEnd - codeStart - 1);
                }
            }
        }
        if (report.projection.empty()) {
            // 回退：使用前 20 字符
            report.projection = projStr.substr(0, min((size_t)20, projStr.length()));
        }
    } else {
        report.warnings.push_back("no_projection → No projection information found");
        report.projection = "";
    }

    // 波段 1 元数据
    if (report.bands >= 1) {
        GDALRasterBand* poBand = poDS->GetRasterBand(1);
        int hasNodataFlag = 0;
        report.nodata = poBand->GetNoDataValue(&hasNodataFlag);
        report.hasNodata = (hasNodataFlag != 0);

        GDALDataType gdalType = poBand->GetRasterDataType();
        report.dataType = dataTypeName(gdalType);
        report.isInteger = isIntegerType(gdalType);
    }

    if (report.bands > 1) {
        report.warnings.push_back("multi_band → File has " + to_string(report.bands)
                                  + " bands, only band 1 will be checked");
    }

    return poDS;
}

// ============================================================================
// L2 值统计检查 / L2 value statistics check
// ============================================================================

void GeoTiffInspector::performL2Check(void* dataset, InspectionReport& report, const InspectOptions& opts)
{
    GDALDataset* poDS = static_cast<GDALDataset*>(dataset);
    if (poDS == nullptr || report.bands < 1) return;

    GDALRasterBand* poBand = poDS->GetRasterBand(1);
    int nCols = report.width;
    int nRows = report.height;
    long long totalPixels = static_cast<long long>(nCols) * nRows;

    // 采样策略
    int sampleStep = calculateSampleStep(totalPixels);
    if (sampleStep > 1) {
        LOG_INFO("GeoTiffInspector: Using sampling step=" + to_string(sampleStep)
                 + " for large raster (" + to_string(totalPixels) + " pixels)");
    }

    // 获取 GDAL 数据类型大小
    GDALDataType gdalType = poBand->GetRasterDataType();
    int typeSize = GDALGetDataTypeSizeBytes(gdalType);
    if (typeSize <= 0) typeSize = 1;

    // 分配行缓冲区
    void* pLineBuf = CPLMalloc(static_cast<size_t>(nCols) * typeSize);

    double dNodata = report.hasNodata ? report.nodata : -9999.0;
    double sum = 0.0;
    long long validCount = 0;
    long long nodataCount = 0;
    long long zeroCount = 0;
    double dMin = 1e300;
    double dMax = -1e300;
    bool firstValid = true;
    double firstValue = 0.0;
    bool allSameSoFar = true;
    unordered_map<int, long long> valueFreq;

    for (int iRow = 0; iRow < nRows; iRow += sampleStep) {
        // 计算本次读取的行数（最多 sampleStep 行，但不超过剩余行）
        int rowsToRead = min(sampleStep, nRows - iRow);

        CPLErr err = poBand->RasterIO(GF_Read, 0, iRow, nCols, rowsToRead,
                                       pLineBuf, nCols, rowsToRead,
                                       gdalType, 0, 0);
        if (err != CE_None) {
            LOG_WARN("GeoTiffInspector: RasterIO failed at row " + to_string(iRow));
            continue;
        }

        // 遍历本块像素
        for (int iR = 0; iR < rowsToRead; ++iR) {
            for (int iC = 0; iC < nCols; ++iC) {
                double val = 0.0;
                size_t offset = static_cast<size_t>(iR * nCols + iC) * typeSize;

                switch (gdalType) {
                    case GDT_Byte:    val = static_cast<double>(*reinterpret_cast<unsigned char*>(static_cast<char*>(pLineBuf) + offset)); break;
                    case GDT_Int16:   val = static_cast<double>(*reinterpret_cast<short*>(static_cast<char*>(pLineBuf) + offset)); break;
                    case GDT_UInt16:  val = static_cast<double>(*reinterpret_cast<unsigned short*>(static_cast<char*>(pLineBuf) + offset)); break;
                    case GDT_Int32:   val = static_cast<double>(*reinterpret_cast<int*>(static_cast<char*>(pLineBuf) + offset)); break;
                    case GDT_UInt32:  val = static_cast<double>(*reinterpret_cast<unsigned int*>(static_cast<char*>(pLineBuf) + offset)); break;
                    case GDT_Float32: val = static_cast<double>(*reinterpret_cast<float*>(static_cast<char*>(pLineBuf) + offset)); break;
                    case GDT_Float64: val = *reinterpret_cast<double*>(static_cast<char*>(pLineBuf) + offset); break;
                    default:          val = static_cast<double>(*reinterpret_cast<int*>(static_cast<char*>(pLineBuf) + offset)); break;
                }

                // NoData 检查
                bool isNodata = false;
                if (report.hasNodata) {
                    isNodata = (fabs(val - dNodata) < 1e-10);
                }

                if (isNodata) {
                    nodataCount++;
                    continue;
                }

                // 有效像素统计
                validCount++;
                sum += val;

                if (val < dMin) dMin = val;
                if (val > dMax) dMax = val;

                if (fabs(val) < 1e-15) zeroCount++;

                // 全同值检测
                if (firstValid) {
                    firstValue = val;
                    firstValid = false;
                } else if (allSameSoFar && fabs(val - firstValue) > 1e-10) {
                    allSameSoFar = false;
                }

                // 离散值频率（仅对整型数据收集）
                if (report.isInteger) {
                    int intVal = static_cast<int>(round(val));
                    valueFreq[intVal]++;
                }
            }
        }
    }

    CPLFree(pLineBuf);

    // 汇总统计
    if (validCount > 0) {
        report.minValue  = dMin;
        report.maxValue  = dMax;
        report.meanValue = sum / validCount;
        report.allZero   = (zeroCount == validCount);
        report.allSame   = allSameSoFar;
    } else {
        report.minValue  = 0;
        report.maxValue  = 0;
        report.meanValue = 0;
        report.allZero   = true;
        report.allSame   = true;
        report.errors.push_back("no_valid_pixels → All pixels are NoData");
    }

    // NoData 百分比
    long long sampledPixels = validCount + nodataCount;
    if (sampledPixels > 0) {
        report.nodataPct = static_cast<double>(nodataCount) / sampledPixels;
    }
    if (report.nodataPct > 0.3) {
        report.warnings.push_back("high_nodata → NoData pixel ratio "
                                  + to_string(static_cast<int>(report.nodataPct * 100)) + "% > 30%");
    }

    // 全零检测
    if (report.allZero && validCount > 0) {
        report.errors.push_back("all_zero → All valid pixels are zero, data may be empty or calculation failed");
    }

    // 全同值检测
    if (report.allSame && validCount > 0 && !report.allZero) {
        report.warnings.push_back("all_same → All valid pixels have the same value ("
                                  + to_string(static_cast<long long>(firstValue)) + ")");
    }

    // 类型覆盖
    for (const auto& kv : valueFreq) {
        report.typeCoverage.insert(kv.first);
    }
    report.categoryCount = static_cast<int>(report.typeCoverage.size());
}

// ============================================================================
// 参考文件匹配检查 / Reference file match check
// ============================================================================

void GeoTiffInspector::performRefCheck(const string& refPath, InspectionReport& report)
{
    GDALDataset* poRefDS = (GDALDataset*)GDALOpenEx(refPath.c_str(), GA_ReadOnly, nullptr, nullptr, nullptr);
    if (poRefDS == nullptr) {
        report.warnings.push_back("ref_open_failed → Cannot open reference file: " + refPath);
        return;
    }

    // 维度匹配
    int refWidth  = poRefDS->GetRasterXSize();
    int refHeight = poRefDS->GetRasterYSize();
    report.dimMatch = (refWidth == report.width && refHeight == report.height);
    if (!report.dimMatch) {
        report.errors.push_back("dim_mismatch → Dimensions differ: "
                                + to_string(report.width) + "x" + to_string(report.height)
                                + " vs ref " + to_string(refWidth) + "x" + to_string(refHeight));
    }

    // 投影匹配
    const char* refProj = poRefDS->GetProjectionRef();
    string refProjStr = (refProj && strlen(refProj) > 0) ? refProj : "";
    string srcProjRef = "";
    // 重新读取源文件投影（因为 L1 中已简化）
    // 简化方式：直接比较 EPSG 代码或原始字符串
    if (!report.projection.empty() && !refProjStr.empty()) {
        // 提取参考文件 EPSG
        string refEpsg;
        auto pos = refProjStr.rfind("AUTHORITY[\"EPSG\"");
        if (pos != string::npos) {
            size_t searchStart = pos + 16;
            size_t codeStart = refProjStr.find("\"", searchStart);
            if (codeStart != string::npos) {
                auto codeEnd = refProjStr.find("\"", codeStart + 1);
                if (codeEnd != string::npos) {
                    refEpsg = "EPSG:" + refProjStr.substr(codeStart + 1, codeEnd - codeStart - 1);
                }
            }
        }
        // 比较 EPSG 代码
        if (!refEpsg.empty()) {
            report.projMatch = (report.projection == refEpsg);
        } else {
            // 回退：比较原始投影字符串
            // 需要重新读取源文件投影
            GDALDataset* poSrcDS = (GDALDataset*)GDALOpenEx(report.filePath.c_str(), GA_ReadOnly, nullptr, nullptr, nullptr);
            if (poSrcDS) {
                const char* srcProj = poSrcDS->GetProjectionRef();
                srcProjRef = (srcProj && strlen(srcProj) > 0) ? srcProj : "";
                report.projMatch = (srcProjRef == refProjStr);
                GDALClose(poSrcDS);
            }
        }
    } else if (report.projection.empty() && refProjStr.empty()) {
        report.projMatch = true;  // 两者都无投影
    } else {
        report.projMatch = false;
    }

    if (!report.projMatch) {
        report.errors.push_back("proj_mismatch → Projection differs between input and reference file");
    }

    GDALClose(poRefDS);
}

// ============================================================================
// 工具方法 / Utility methods
// ============================================================================

bool GeoTiffInspector::isIntegerType(int dataType)
{
    switch (dataType) {
        case GDT_Byte:
        case GDT_Int16:
        case GDT_UInt16:
        case GDT_Int32:
        case GDT_UInt32:
            return true;
        default:
            return false;
    }
}

string GeoTiffInspector::dataTypeName(int dataType)
{
    switch (dataType) {
        case GDT_Byte:    return "Byte";
        case GDT_Int16:   return "Int16";
        case GDT_UInt16:  return "UInt16";
        case GDT_Int32:   return "Int32";
        case GDT_UInt32:  return "UInt32";
        case GDT_Float32: return "Float32";
        case GDT_Float64: return "Float64";
        default:          return "Unknown";
    }
}

int GeoTiffInspector::calculateSampleStep(long long totalPixels, long long maxSamples)
{
    if (totalPixels <= maxSamples) return 1;
    // 按行采样：每隔 step 行读一行
    int step = static_cast<int>(totalPixels / maxSamples);
    if (step < 1) step = 1;
    return step;
}
