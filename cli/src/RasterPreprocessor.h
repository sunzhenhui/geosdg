/**
 * @file RasterPreprocessor.h
 * @brief 栅格数据预处理模块 / Raster Data Preprocessing Module
 *
 * 提供 5 项独立的栅格数据预处理能力，以纯算法方式实现，不依赖任何 Qt 组件。
 * 所有 I/O 通过 GDAL C++ API + std::filesystem 完成。
 *
 * Provides 5 independent raster preprocessing capabilities as pure algorithms
 * without any Qt dependency. All I/O is done via GDAL C++ API + std::filesystem.
 *
 * 预处理方法 / Preprocessing methods:
 *   1. resampleToBase()     — 重采样对齐 / Resample to base raster
 *   2. normalizeRaster()    — Min-Max 归一化 / Min-Max normalization
 *   3. reclassifyRaster()   — 重分类 + NoData 标记 / Reclassify + NoData marking
 *   4. detectChange()       — 土地利用变化检测 / Land use change detection
 *   5. compressRaster()     — 栅格压缩 / Raster compression
 *
 * @note 所有方法返回 int：0=成功，非0=失败 / All methods return int: 0=success, non-zero=failure
 * @note 日志输出使用英文 / Log messages are in English
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class RasterPreprocessor
{
public:
    RasterPreprocessor();
    ~RasterPreprocessor();

    // ========================================================================
    // 重采样对齐 / Resample to Base
    // ========================================================================

    /**
     * @brief 将一批栅格重采样到基准栅格的空间参考 / Resample rasters to base raster grid
     *
     * 读取基准栅格的 GeoTransform、Projection、XSize、YSize，
     * 对每个输入栅格执行 GDALWarp 重投影+重采样，输出与基准对齐的栅格。
     * 若输入与基准尺寸/投影完全一致，直接拷贝（跳过 warp）。
     *
     * @param basePath   基准栅格路径 / Base raster path
     * @param inputs     待重采样栅格路径列表 / Input raster paths
     * @param outputs    输出栅格路径列表（与 inputs 一一对应）/ Output raster paths
     * @param method     重采样方法："nearest" 或 "bilinear" / Resample method
     * @return 0 on success, non-zero on failure
     */
    int resampleToBase(const std::string& basePath,
                       const std::vector<std::string>& inputs,
                       const std::vector<std::string>& outputs,
                       const std::string& method);

    // ========================================================================
    // 归一化 / Normalize
    // ========================================================================

    /**
     * @brief Min-Max 归一化栅格到指定范围 / Normalize rasters to target range
     *
     * 对每个输入栅格逐波段执行 Min-Max 归一化：
     *   out = (val - min) / (max - min) * (rangeMax - rangeMin) + rangeMin
     * NoData 像素不参与统计，输出中保持原 NoData 值。
     * 输出数据类型为 Float32。
     *
     * @param inputs      输入栅格路径列表 / Input raster paths
     * @param outputs     输出栅格路径列表 / Output raster paths
     * @param rangeMin    目标范围最小值 / Target range minimum
     * @param rangeMax    目标范围最大值 / Target range maximum
     * @param manualMin   是否手动指定最小值 / Whether min is manually specified
     * @param minVal      手动指定的最小值 / Manually specified minimum
     * @param manualMax   是否手动指定最大值 / Whether max is manually specified
     * @param maxVal      手动指定的最大值 / Manually specified maximum
     * @return 0 on success, non-zero on failure
     */
    int normalizeRaster(const std::vector<std::string>& inputs,
                        const std::vector<std::string>& outputs,
                        double rangeMin, double rangeMax,
                        bool manualMin, double minVal,
                        bool manualMax, double maxVal);

    // ========================================================================
    // 重分类 + NoData 标记 / Reclassify + NoData Marking
    // ========================================================================

    /**
     * @brief 重分类规则结构体 / Reclassify rule structure
     *
     * remap: 原始像素值 → 目标像素值的映射表
     * nodataValues: 要标记为 NoData 的像素值列表
     */
    struct ReclassRule {
        std::unordered_map<int, int> remap;      ///< 原值→目标值映射 / Source→target mapping
        std::vector<int> nodataValues;            ///< 标记为 NoData 的值 / Values to mark as NoData
    };

    /**
     * @brief 对单波段栅格执行重分类和 NoData 标记 / Reclassify single-band raster
     *
     * 逐像素遍历：
     *   - 若像素值在 nodataValues 中 → 写入 nodataValue
     *   - 若像素值在 remap 映射表中 → 写入映射目标值
     *   - 否则保持原值
     *
     * @param input       输入栅格路径（单波段）/ Input raster path (single band)
     * @param output      输出栅格路径 / Output raster path
     * @param rule        重分类规则 / Reclassify rule
     * @param nodataValue 输出的 NoData 值 / Output NoData value
     * @return 0 on success, non-zero on failure
     */
    int reclassifyRaster(const std::string& input,
                         const std::string& output,
                         const ReclassRule& rule,
                         int nodataValue);

    // ========================================================================
    // 变化检测 / Change Detection
    // ========================================================================

    /**
     * @brief 对比两期栅格检测变化区域 / Detect changes between two-period rasters
     *
     * 逐像素对比两期栅格：
     *   - 二值模式（默认）：变化=1，未变化=0
     *   - 编码模式（encodeMode=true）：out = before * 1000 + after，未变化=0
     * 任一为 NoData → 输出 NoData
     *
     * @param before     变化前栅格路径 / Before-period raster path
     * @param after      变化后栅格路径 / After-period raster path
     * @param output     输出变化栅格路径 / Output change raster path
     * @param encodeMode 是否使用编码模式 / Whether to use encoding mode
     * @return 0 on success, non-zero on failure
     */
    int detectChange(const std::string& before,
                     const std::string& after,
                     const std::string& output,
                     bool encodeMode);

    // ========================================================================
    // 压缩 / Compress
    // ========================================================================

    /**
     * @brief 压缩选项结构体 / Compress options structure
     */
    struct CompressOptions {
        std::string method     = "deflate";  ///< 压缩算法 / Compression algorithm
        int         level      = 6;          ///< 压缩级别 1-9 / Compression level
        int         predictor  = 2;          ///< 水平差分预测器 0/2/3 / Predictor
        double      maxError   = 0.001;      ///< LERC 有损精度 / LERC max error
        bool        tiled      = false;      ///< 是否分块存储 / Whether tiled
        int         blockSize  = 256;        ///< 分块大小 / Block size
        std::string bigtiff    = "if_needed"; ///< BigTIFF 模式 / BigTIFF mode
        bool        overview   = false;      ///< 是否生成金字塔 / Whether to generate overviews
    };

    /**
     * @brief 压缩栅格文件 / Compress raster file
     *
     * 读取输入栅格，按指定压缩算法重新编码后写出。
     * 保留所有元数据（GeoTransform、Projection、NoData）。
     * 输出压缩报告（原始大小、压缩后大小、压缩率、耗时）。
     *
     * @param input  输入栅格路径 / Input raster path
     * @param output 输出压缩栅格路径 / Output compressed raster path
     * @param opts   压缩选项 / Compress options
     * @return 0 on success, non-zero on failure
     */
    int compressRaster(const std::string& input,
                       const std::string& output,
                       const CompressOptions& opts);

    // ========================================================================
    // 辅助方法 / Helper Methods
    // ========================================================================

    /**
     * @brief 从 JSON 文件解析重分类规则 / Parse reclassify rule from JSON file
     *
     * 极简手写 JSON 解析器，支持以下格式：
     * {
     *   "remap": { "1": 1, "2": 1, "3": 1 },
     *   "nodata": [255, 0, -9999]
     * }
     *
     * @param jsonPath JSON 规则文件路径 / JSON rule file path
     * @return ReclassRule 结构体，解析失败时 remap 为空 / ReclassRule, empty on failure
     */
    static ReclassRule parseReclassRule(const std::string& jsonPath);

    /**
     * @brief 从命令行参数解析重分类规则 / Parse reclassify rule from CLI string args
     * @param remapStr    重映射字符串 "k1:v1,k2:v2" / Remap string
     * @param nodataStr   NoData 值字符串 "v1,v2" / NoData values string
     * @return ReclassRule 结构体 / ReclassRule structure
     */
    static ReclassRule parseReclassRuleFromArgs(const std::string& remapStr,
                                                 const std::string& nodataStr);

private:
};
