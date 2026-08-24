/**
 * @file GeoTiffInspector.h
 * @brief GeoTIFF 文件元数据与数据质量检查器 / GeoTIFF metadata and data quality inspector
 *
 * 提供 GeoTIFF 文件的预检能力，包括 L1 快速元数据检查（维度、投影、数据类型、
 * NoData）和 L2 值统计检查（全零、全同值、值范围、空值率、地类覆盖、类别完整性）。
 *
 * Provides GeoTIFF pre-check capabilities, including L1 fast metadata checks
 * (dimensions, projection, data type, NoData) and L2 value statistics checks
 * (all-zero, all-same, value range, nodata ratio, type coverage, category completeness).
 *
 * 用法 / Usage:
 *   GeoTiffInspector inspector;
 *   auto report = inspector.inspect("lucc.tif", opts);
 *   cout << report.toOutputLine() << endl;
 */

#pragma once

#include <string>
#include <unordered_set>
#include <vector>
#include <set>
#include <cmath>

/**
 * @brief 检查选项 / Inspection options
 */
struct InspectOptions
{
    std::string refPath;                    ///< 参考文件路径（投影/维度匹配检查）/ Reference file path
    std::unordered_set<int> expectedTypes;  ///< 期望的地类编码集合 / Expected land type codes
    int expectedCategoryCount = 0;          ///< 期望的地类总类别数 / Expected total category count
    bool expectInt = false;                 ///< 期望整型数据 / Expect integer data type
};

/**
 * @brief 检查结果 / Inspection result
 */
struct InspectionReport
{
    // ── L1: 元数据 / Metadata ──
    std::string filePath;
    int         width       = 0;
    int         height      = 0;
    int         bands       = 0;
    std::string projection;         ///< 投影字符串 / Projection string
    double      nodata      = 0.0;
    bool        hasNodata   = false;
    std::string dataType;           ///< GDAL 数据类型名 / GDAL data type name
    bool        isInteger   = true; ///< 是否整型 / Whether integer type

    // ── L2: 值统计 / Value statistics ──
    double      minValue    = 0.0;
    double      maxValue    = 0.0;
    double      meanValue   = 0.0;
    double      nodataPct   = 0.0;  ///< NoData 像素百分比 / NoData pixel percentage
    bool        allZero     = false;
    bool        allSame     = false;
    std::set<int> typeCoverage;     ///< 栅格中出现的离散值集合 / Distinct values in raster
    int         categoryCount = 0;  ///< 实际类别数 / Actual category count

    // ── 匹配检查 / Match checks ──
    bool        projMatch   = true; ///< 投影是否匹配参考文件 / Projection matches reference
    bool        dimMatch    = true; ///< 维度是否匹配参考文件 / Dimensions match reference

    // ── 诊断信息 / Diagnostics ──
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    bool        hasError    = false; ///< 是否有 ERROR 级问题 / Has ERROR-level issue

    /**
     * @brief 生成 key=value 格式输出行 / Generate key=value output line
     * @return 输出字符串 / Output string
     */
    std::string toOutputLine() const;

    /**
     * @brief 生成诊断信息行（# WARN / # ERROR 前缀）/ Generate diagnostic lines
     * @return 诊断信息向量 / Vector of diagnostic lines
     */
    std::vector<std::string> toDiagnosticLines() const;
};

/**
 * @brief GeoTIFF 文件检查器 / GeoTIFF file inspector
 *
 * 对 GeoTIFF 文件执行 L1（元数据）和 L2（值统计）两级检查，
 * 输出结构化检查报告。大文件采用采样策略避免全遍历。
 *
 * Performs L1 (metadata) and L2 (value statistics) checks on GeoTIFF files,
 * outputting structured inspection reports. Uses sampling for large files
 * to avoid full traversal.
 */
class GeoTiffInspector
{
public:
    GeoTiffInspector();
    ~GeoTiffInspector();

    /**
     * @brief 执行完整检查（L1 + L2）/ Perform full inspection (L1 + L2)
     * @param filePath 待检查的 GeoTIFF 路径 / Path to GeoTIFF file
     * @param opts     检查选项 / Inspection options
     * @return 检查报告 / Inspection report
     */
    InspectionReport inspect(const std::string& filePath, const InspectOptions& opts);

private:
    /**
     * @brief L1 元数据检查 / L1 metadata check
     *
     * 读取文件基本元数据：维度、波段数、投影、NoData、数据类型。
     * 不遍历像素，成本低。
     *
     * @param filePath 文件路径 / File path
     * @param report   检查报告（输出）/ Inspection report (output)
     * @return GDALDataset 指针（成功）或 nullptr（失败）/ Dataset pointer or nullptr
     */
    void* performL1Check(const std::string& filePath, InspectionReport& report);

    /**
     * @brief L2 值统计检查 / L2 value statistics check
     *
     * 遍历（或采样）像素，统计值范围、全零/全同值、空值率、类型覆盖。
     * 对大文件采用采样策略（最多采样 100 万像素）。
     *
     * @param dataset  GDAL 数据集 / GDAL dataset
     * @param report   检查报告（输出）/ Inspection report (output)
     * @param opts     检查选项 / Inspection options
     */
    void performL2Check(void* dataset, InspectionReport& report, const InspectOptions& opts);

    /**
     * @brief 参考文件匹配检查 / Reference file match check
     * @param refPath  参考文件路径 / Reference file path
     * @param report   检查报告（输出）/ Inspection report (output)
     */
    void performRefCheck(const std::string& refPath, InspectionReport& report);

    /**
     * @brief 判断 GDAL 数据类型是否整型 / Check if GDAL data type is integer
     * @param dataType GDAL 数据类型枚举值 / GDAL data type enum
     * @return 是否整型 / Whether integer type
     */
    static bool isIntegerType(int dataType);

    /**
     * @brief 获取 GDAL 数据类型名称 / Get GDAL data type name
     * @param dataType GDAL 数据类型枚举值 / GDAL data type enum
     * @return 类型名称字符串 / Type name string
     */
    static std::string dataTypeName(int dataType);

    /**
     * @brief 计算采样步长 / Calculate sampling step
     *
     * 对大文件采用采样策略，确保最多采样约 maxSamples 个像素。
     *
     * @param totalPixels 总像素数 / Total pixel count
     * @param maxSamples  最大采样数 / Max samples
     * @return 采样步长（1 表示全采样）/ Sampling step (1 = full scan)
     */
    static int calculateSampleStep(long long totalPixels, long long maxSamples = 1000000);
};
