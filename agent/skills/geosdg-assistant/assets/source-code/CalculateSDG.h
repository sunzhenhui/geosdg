/**
 * @file CalculateSDG.h
 * @brief 空间化SDG指标计算模块 / Spatialized SDG Indicator Calculation Module
 *
 * 本模块实现5类空间化可持续发展目标（SDG）指标的计算与归一化，
 * 将土地利用、人口、基础设施等空间数据转化为0~100的标准化得分。
 *
 * This module implements calculation and normalization of 5 types of spatialized
 * Sustainable Development Goal (SDG) indicators, converting land use, population,
 * and infrastructure spatial data into standardized 0-100 scores.
 *
 * 对应论文 / Corresponding paper: Figure 9: SDG Indicators, Section 3.2
 *
 * 指标类别与对应SDG目标 / Indicator categories and corresponding SDG targets:
 *   1. Land Proportion（土地比例指标） — SDG 2.1.2 / 6.6.1 / 15.1.1
 *   2. Land Conversion（土地转换指标） — SDG 14.5.1 / 15.2.1 / 15.3.1
 *   3. Buffer Zone（缓冲区指标）     — SDG 2.4.1 / 3.8.1 / 3.c.1 / 4.1.2 / 7.2.1 / 9.1.1 / 9.c.1 / 11.2.1 / 11.7.1
 *   4. Total Statistics（SDG 11.3.1） — 城市用地增长率/人口增长率 / Urban land growth / population growth ratio
 *   5. Total Statistics（SDG 13.2.2） — 碳排放达峰评估 / Carbon emission peak assessment
 *
 * @note 通用注意事项 / General notes:
 *   - 所有输入栅格数据必须为 GeoTIFF 格式 / All input rasters must be GeoTIFF
 *   - LUCC数据要求 GDT_Byte，人口数据要求 GDT_Float32/64
 *     LUCC data requires GDT_Byte; population data requires GDT_Float32/64
 *   - 所有函数不检查输入数据坐标系一致性，需用户自行保证
 *     Functions do not check coordinate system consistency; users must ensure it
 *   - 人口数据与LUCC数据分辨率不同时需预先重采样对齐
 *     Population and LUCC data with different resolutions must be resampled first
 *   - 部分函数以 GA_Update 方式打开输入文件，可能意外修改源数据
 *     Some functions open input files with GA_Update, potentially modifying source data
 *
 * @see GeoSDG-操作问题速查表.md 操作4~8 / Operations 4-8
 * @see GeoSDG-项目结构速查表.md SDG指标计算模块 / SDG Indicator Calculation Module
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

class CalculateSDG
{
public:
    CalculateSDG();
    ~CalculateSDG();

    /**
     * @brief 计算土地比例指标 / Calculate Land Proportion Indicator
     *
     * 计算指定土地利用类型在研究区内的占比，并归一化为0~100的得分。
     * 占比越高（正向指标），得分越高。
     *
     * Calculates the proportion of specified land use types in the study area
     * and normalizes to a 0-100 score. Higher proportion = higher score (positive indicator).
     *
     * 对应SDG / Corresponding SDGs: 2.1.2（农业用地/agricultural land）, 6.6.1（水域/water bodies）, 15.1.1（森林/forest）
     *
     * @param qstrFileName       土地利用数据路径，GeoTIFF，GDT_Byte，单波段
     *                           Land use data path, GeoTIFF, GDT_Byte, single band
     *                           像素值为地类编码（如1=耕地,2=森林,3=草地,4=水域,5=建设用地,6=未利用地）
     *                           Pixel values are land type codes (e.g., 1=cropland, 2=forest, 3=grassland, 4=water, 5=urban, 6=barren)
     * @param dMaxThreshold      最大阈值，占比 >= 此值时得100分 / Max threshold; proportion >= this yields 100
     * @param dMinThreshold      最小阈值，占比 <= 此值时得0分 / Min threshold; proportion <= this yields 0
     * @param mqsetSelectLUCCTypes 观测地类编码集合，如 {2, 4} 表示统计森林和水域
     *                           Observed land type code set, e.g., {2, 4} for forest and water
     *
     * @return 归一化得分，范围 [0, 100] / Normalized score, range [0, 100]
     *
     * @warning 已知问题 / Known issues:
     *   - 文件打开失败时返回0 / Returns 0 on file open failure
     *   - 以 GA_Update 方式打开，可能意外修改输入文件 / Opens with GA_Update, may modify input
     *   - NoData值与Byte类型比较，若NoData非0~255整数可能判断异常
     *     NoData vs Byte comparison may fail if NoData is not an integer in [0,255]
     *   - 阈值设置不当时得分恒为0或100 / Improper thresholds yield constant 0 or 100
     *
     * @see GeoSDG-操作问题速查表.md 操作4 / Operation 4
     */
    double calculateLandProportionIndicator(std::string qstrFileName,
                                            double dMaxThreshold, double dMinThreshold,
                                            std::unordered_set<int> mqsetSelectLUCCTypes);

    /**
     * @brief 计算土地转换指标 / Calculate Land Conversion Indicator
     *
     * 计算指定土地利用转换类型在两期数据间的转换比例，并归一化为0~100的得分。
     * 可通过 bState 参数选择正向或负向归一化。
     *
     * Calculates the proportion of specified land use transitions between two periods,
     * normalized to 0-100. Use bState to select positive or negative normalization.
     *
     * 对应SDG / Corresponding SDGs: 14.5.1 / 15.2.1 / 15.3.1
     *
     * @param qstrInputOriginal  初始期土地利用数据路径，GeoTIFF，GDT_Byte，单波段
     *                           Initial period land use path, GeoTIFF, GDT_Byte, single band
     * @param qstrInputChanged   变化期土地利用数据路径，GeoTIFF，GDT_Byte，单波段
     *                           Changed period land use path, must match initial dimensions
     * @param mqsetSelectLUCCTransitionTypes 转换类型映射 / Transition type mapping
     *                           key=源地类编码/source type, value=目标地类编码列表/target type list
     *                           例如 {{2, {5,6}}, {4, {5}}} = 森林→建设用地/未利用地, 水域→建设用地
     * @param dMaxThreshold      最大阈值，归一化上限 / Max threshold for normalization
     * @param dMinThreshold      最小阈值，归一化下限 / Min threshold for normalization
     * @param bState             正/负向标识 / Positive/negative indicator flag:
     *                           true=正向(越高越好, normalization), false=负向(越低越好, normalizationNegative)
     *                           true=positive (higher is better), false=negative (lower is better)
     *
     * @return 归一化得分，范围 [0, 100] / Normalized score, range [0, 100]
     *
     * @warning 已知问题 / Known issues:
     *   - 两期数据行列数不一致时返回0 / Returns 0 if dimensions don't match
     *   - 若无匹配转换类型（nAllCount=0），除零导致NaN / Division-by-zero if no matching transitions
     *   - bState设置反时归一化方向错误 / Wrong bState inverts normalization direction
     *   - targetTypes包含源类型时nAllCount也计入"保持不变" / nAllCount includes unchanged if source in targets
     *
     * @see GeoSDG-操作问题速查表.md 操作5 / Operation 5
     */
    double calculateLandConversionIndicator(std::string qstrInputOriginal, std::string qstrInputChanged,
                                            std::unordered_map<int, std::vector<int>> mqsetSelectLUCCTransitionTypes,
                                            double dMaxThreshold, double dMinThreshold, bool bState);

    /**
     * @brief 计算缓冲区指标 / Calculate Buffer Zone Indicator
     *
     * 计算基础设施覆盖区（缓冲区）内外特定地类/人口的覆盖率得分。
     * 自动识别输入数据类型：GDT_Byte按地类统计，Float类型按人口值累加。
     *
     * Calculates coverage score of specified land types/population inside infrastructure
     * buffer zones. Auto-detects data type: GDT_Byte for land types, Float for population.
     *
     * 对应SDG / Corresponding SDGs: 2.4.1 / 3.8.1 / 3.c.1 / 4.1.2 / 7.2.1 / 9.1.1 / 9.c.1 / 11.2.1 / 11.7.1
     *
     * @param qstrInputData      土地利用或人口数据路径 / Land use or population data path
     *                           GDT_Byte（地类/land type）或 GDT_Float32/64（人口/population），单波段
     * @param qstrBufferZoneData 缓冲区数据路径 / Buffer zone data path, GeoTIFF, GDT_Byte
     *                           有效值=覆盖区/covered, NoData/0=未覆盖区/uncovered
     * @param mqsetSelectLUCCTypes 观测地类编码集合（仅GDT_Byte数据使用）
     *                           Observed land type set (only for GDT_Byte data)
     * @param dMaxThreshold      最大阈值 / Max threshold
     * @param dMinThreshold      最小阈值 / Min threshold
     *
     * @return 归一化得分，范围 [0, 100] / Normalized score, range [0, 100]
     *
     * @warning 已知问题 / Known issues:
     *   - 数据类型不支持时返回0 / Returns 0 for unsupported data types
     *   - 人口数据（Float）直接累加值，不检查地类类型 / Float data sums values without type filtering
     *   - dOriSum=0时除零导致NaN / Division-by-zero when dOriSum=0
     *   - 缓冲区文件无栅格波段时返回0 / Returns 0 if buffer file has no raster bands
     *
     * @see GeoSDG-操作问题速查表.md 操作6 / Operation 6
     */
    double calculateBufferZoneIndicator(std::string qstrInputData, std::string qstrBufferZoneData,
                                        std::unordered_set<int> mqsetSelectLUCCTypes,
                                        double dMaxThreshold, double dMinThreshold);

    /**
     * @brief 计算SDG 11.3.1指标（城市用地增长率/人口增长率比值）
     *        Calculate SDG 11.3.1 indicator (urban land growth / population growth ratio)
     *
     * 评估城市化协调性：计算城市用地增长率与人口增长率的比值，
     * 比值越接近最优阈值 dBestThreshold 得分越高，超出 [min, max] 范围得0分。
     *
     * Assesses urbanization coordination: computes ratio of urban land growth rate
     * to population growth rate. Closer to dBestThreshold = higher score; outside [min,max] = 0.
     *
     * @param qstrInitialLUCCFileName  初始期LUCC路径 / Initial LUCC path, GeoTIFF, GDT_Byte
     * @param qstrCurrentLUCCFileName  当前期LUCC路径 / Current LUCC path, GeoTIFF, GDT_Byte
     * @param qstrInitialPopulationFileName 初始期人口路径 / Initial population path, GeoTIFF, GDT_Float32
     * @param qstrCurrentPopulationFileName 当前期人口路径 / Current population path, GeoTIFF, GDT_Float32
     * @param vLUCCType          城市地类编码集合 / Urban land type code set, e.g., {5}
     * @param dMaxThreshold      最大阈值 / Max threshold (above this = 0 score)
     * @param dMinThreshold      最小阈值 / Min threshold (below this = 0 score)
     * @param dBestThreshold     最优阈值 / Optimal threshold (= this = 100 score)
     *                           不能等于 dMinThreshold 或 dMaxThreshold，否则除零
     *                           Must not equal dMin or dMax, otherwise division-by-zero
     *
     * @return 归一化得分，范围 [0, 100] / Normalized score, range [0, 100]
     *
     * @warning 已知问题 / Known issues:
     *   - 初始期人口为0或两期人口相同时除零 / Division-by-zero if initial population is 0 or unchanged
     *   - 初始期城市用地为0时除零 / Division-by-zero if initial urban land is 0
     *   - 多期数据仅使用第一对 / Only first pair of multi-period data is used
     *   - dBestThreshold 等于边界阈值时除零导致NaN / NaN if best equals min or max
     *
     * @see GeoSDG-操作问题速查表.md 操作7 / Operation 7
     */
    double calculateSDG1131Indicator(std::string qstrInitialLUCCFileName, std::string qstrCurrentLUCCFileName,
                                     std::string qstrInitialPopulationFileName, std::string qstrCurrentPopulationFileName,
                                     std::unordered_set<int> vLUCCType,
                                     double dMaxThreshold, double dMinThreshold, double dBestThreshold);

    /**
     * @brief 计算SDG 13.2.2指标（碳排放达峰评估）
     *        Calculate SDG 13.2.2 indicator (carbon emission peak assessment)
     *
     * 评估区域碳排放是否达峰：对比两期土地利用的碳排放量，
     * 排放减少返回100分（已达峰），排放增加则按比例归一化。
     *
     * Assesses whether regional carbon emissions have peaked: compares emissions
     * between two periods. Decreased emissions = 100 (peaked); increased = normalized score.
     *
     * @param qstrInputOriginal  初始期LUCC路径 / Initial LUCC path, GeoTIFF, GDT_Byte
     * @param qstrInputChanged   变化期LUCC路径 / Changed LUCC path, GeoTIFF, GDT_Byte
     * @param vLUCCEmissionScheme 各地类排放系数映射 / Emission factor map per land type
     *                           key=地类编码/land type, value=排放系数/emission factor (正=排放/emission, 负=吸收/absorption)
     *                           注意：此map会被原地修改，建议传入副本 / NOTE: map is modified in-place; pass a copy
     * @param dRatio             减排强度比例，范围 (0, 1) / Emission reduction ratio, range (0, 1)
     *
     * @return 得分：排放减少返回100，排放增加返回归一化得分 [0, 100)
     *         Score: 100 if emissions decreased, normalized [0,100) if increased
     *
     * @warning 已知问题 / Known issues:
     *   - vLUCCEmissionScheme 被原地修改 / Emission scheme map is modified in-place
     *   - 排放为0时归一化分母为0 / Division-by-zero if emission is 0
     *   - 依赖 ../tmp/ 临时目录 / Depends on ../tmp/ directory
     *
     * @see GeoSDG-操作问题速查表.md 操作8 / Operation 8
     */
    double calculateSDG1322Indicator(std::string qstrInputOriginal, std::string qstrInputChanged,
                                     std::unordered_map<int, double> vLUCCEmissionScheme, double dRatio);

private:
    /**
     * @brief 正向归一化 / Positive normalization
     *
     * 将值从 [dMin, dMax] 线性映射到 [0, 100]，值越大得分越高。
     * Maps value from [dMin, dMax] to [0, 100]; higher value = higher score.
     */
    double normalization(double dValue, double dMin, double dMax);

    /**
     * @brief 负向归一化 / Negative normalization
     *
     * 将值从 [dMin, dMax] 线性映射到 [100, 0]，值越小得分越高。
     * Maps value from [dMin, dMax] to [100, 0]; lower value = higher score.
     */
    double normalizationNegative(double dValue, double dMin, double dMax);

    /**
     * @brief 读取多期人口栅格数据并求和 / Read multi-period population rasters and sum
     * @param vPopuFileNames  人口数据文件路径列表 / Population file path list
     * @return 各文件人口总和的向量 / Population sum per file
     */
    std::vector<double> getPopuSum(std::vector<std::string> vPopuFileNames);

    /**
     * @brief 读取多期LUCC栅格数据并统计指定地类像元数 / Count specified land type pixels per file
     * @param vLUCCFileNames       LUCC文件路径列表 / LUCC file path list
     * @param mqsetSelectLUCCTypes 观测地类编码集合 / Observed land type set
     * @return 各文件中指定地类像元数 / Pixel count per file
     */
    std::vector<double> getUrbanSum(std::vector<std::string> vLUCCFileNames,
                                    std::unordered_set<int> mqsetSelectLUCCTypes);

    /**
     * @brief 基于排放系数计算土地利用碳排放总量 / Compute total carbon emission from LUCC
     * @param qstrInputData         LUCC数据路径 / LUCC data path, GeoTIFF, GDT_Byte
     * @param vLUCCEmissionScheme   各地类排放系数映射 / Emission factor map per land type
     * @return 碳排放总量（正=净排放，负=净吸收）/ Total emission (positive=net emission, negative=net absorption)
     */
    double getEmissionSum(std::string qstrInputData, std::unordered_map<int, double> vLUCCEmissionScheme);
};
