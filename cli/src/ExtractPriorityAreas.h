/**
 * @file ExtractPriorityAreas.h
 * @brief 优先区域识别与排名模块 / Priority Area Identification and Ranking Module
 *
 * 本模块基于6种规则识别可持续发展优先关注区域，
 * 并可叠加生成综合排名图，值越高表示该区域越需优先关注。
 *
 * This module identifies sustainable development priority areas based on 6 rules,
 * and can overlay them into a comprehensive ranking map where higher values
 * indicate areas requiring more urgent attention.
 *
 * 对应论文 / Corresponding paper: Figure 11/12: Priority Areas
 *
 * 6种识别规则 / 6 identification rules:
 *   Rule 1: 土地被侵占区域 / Land encroached areas
 *   Rule 2: 特定转换类型区域 / Specific transition areas
 *   Rule 3: 基础设施未覆盖区域(地类) / Outside infrastructure (land type)
 *   Rule 4: 基础设施未覆盖区域(人口) / Outside infrastructure (population)
 *   Rule 5: 碳排放未达峰区域 / Carbon emission not peaked
 *   Rule 6: 人地关系失调区域 / Human-land imbalance
 *   合并: 排名图(0~6) / Merge: ranking map (0~6)
 *
 * @note 通用注意事项 / General notes:
 *   - 所有输入栅格必须为 GeoTIFF / All input rasters must be GeoTIFF
 *   - LUCC: GDT_Byte; 人口: GDT_Float32
 *   - 不检查坐标系一致性 / No coordinate system consistency check
 *   - Rule 5 依赖 ../tmp/ 临时目录 / Rule 5 depends on ../tmp/ directory
 *
 * @see GeoSDG-操作问题速查表.md 操作9~14 / Operations 9-14
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

class ExtractPriorityAreas
{
public:
    ExtractPriorityAreas();
    ~ExtractPriorityAreas();

    /**
     * @brief Rule 1: 土地被侵占区域 / Land encroached areas
     *
     * 识别指定地类被其他类型侵占的区域。标记初始期属于指定地类但变化期不再属于的像元。
     * Identifies areas where specified types converted to others.
     *
     * @param qstrInputOriginal  初始期LUCC / Initial LUCC, GeoTIFF, GDT_Byte
     * @param qstrInputChanged   变化期LUCC / Changed LUCC, GeoTIFF, GDT_Byte
     * @param qstrOutputFileName 输出: 1=优先区域, NoData=无效 / Output: 1=priority, NoData=invalid
     * @param mqsetSelectLUCCTypes 被侵占地类集合 / Encroached land type set, e.g. {2, 4}
     *
     * @see GeoSDG-操作问题速查表.md 操作9 / Operation 9
     */
    void PriorityAreasExtractLUCCLoss(std::string qstrInputOriginal, std::string qstrInputChanged,
                                      std::string qstrOutputFileName,
                                      std::unordered_set<int> mqsetSelectLUCCTypes);

    /**
     * @brief Rule 2: 特定转换类型区域 / Specific transition areas
     *
     * 识别符合指定转换规则(源地类→目标地类)的变化区域。
     * Identifies areas matching specified source→target transition rules.
     *
     * @param qstrInputOriginal  初始期LUCC / Initial LUCC, GeoTIFF, GDT_Byte
     * @param qstrInputChanged   变化期LUCC / Changed LUCC, GeoTIFF, GDT_Byte
     * @param qstrOutputFileName 输出: 1=优先区域, NoData=无效 / Output: 1=priority, NoData=invalid
     * @param mqsetSelectLUCCTransitionTypes 转换映射 / Transition map
     *                           key=源地类/source, value=目标地类列表/target list
     *                           e.g. {{2, {5,6}}, {4, {5}}} = forest→urban/barren, water→urban
     *
     * @see GeoSDG-操作问题速查表.md 操作10 / Operation 10
     */
    void PriorityAreasExtractLUCCTransition(std::string qstrInputOriginal, std::string qstrInputChanged,
                                            std::string qstrOutputFileName,
                                            std::unordered_map<int, std::vector<int>> mqsetSelectLUCCTransitionTypes);

    /**
     * @brief Rule 3/4: 基础设施未覆盖区域 / Outside infrastructure coverage
     *
     * 根据输入数据类型自动分发:
     * Auto-dispatches based on data type:
     *   GDT_Byte → Rule 3: 特定地类不在覆盖区 / specified types not covered
     *   GDT_Float32/64 → Rule 4: 人口超阈值且不在覆盖区 / population above threshold and not covered
     *
     * @param qstrInputData      LUCC或人口数据 / LUCC or population data, GeoTIFF
     * @param qstrBufferZoneData 基础设施覆盖数据 / Infrastructure coverage, GeoTIFF, GDT_Byte
     *                           有效值=覆盖/covered, NoData=未覆盖/uncovered
     * @param qstrOutputFileName 输出: 1=优先区域, 0=非优先 / Output: 1=priority, 0=non-priority
     * @param mqsetSelectLUCCTypes 地类集合(仅Rule 3) / Land type set (Rule 3 only)
     * @param dThresholdPopulation 人口阈值(仅Rule 4) / Population threshold (Rule 4 only)
     *
     * @warning 人口模式不使用地类过滤 / Population mode ignores land type filtering
     *          缓冲区NoData视为未覆盖 / Buffer NoData treated as uncovered
     *
     * @see GeoSDG-操作问题速查表.md 操作11 / Operation 11
     */
    void PriorityAreasExtractOutsideBufferArea(std::string qstrInputData, std::string qstrBufferZoneData,
                                               std::string qstrOutputFileName,
                                               std::unordered_set<int> mqsetSelectLUCCTypes,
                                               double dThresholdPopulation);

    /**
     * @brief Rule 5: 碳排放未达峰区域 / Carbon emission not peaked
     *
     * 利用二维前缀和快速计算邻域碳排放，识别排放仍上升的区域。
     * Uses 2D prefix sums for fast neighborhood emission calculation.
     *
     * @param qstrInputOriginal  初始期LUCC / Initial LUCC, GeoTIFF, GDT_Byte
     * @param qstrInputChanged   变化期LUCC / Changed LUCC, GeoTIFF, GDT_Byte
     * @param qstrOutputFileName 输出: 1=排放增加 / Output: 1=emission increased
     * @param vLUCCEmissionScheme 排放系数映射(会被原地修改!) / Emission factor map (MODIFIED IN-PLACE!)
     *                           正=碳排放/emission, 负=碳吸收/absorption
     * @param dRatio             减排比例(0,1) / Emission reduction ratio
     * @param dNeighborhood      邻域半径(像元) / Neighborhood radius (pixels)
     *
     * @warning 依赖 ../tmp/ 目录 / Depends on ../tmp/ directory
     *          排放系数map被原地修改 / Emission scheme map modified in-place
     *          临时文件清理不完整 / Incomplete temp file cleanup
     *
     * @see GeoSDG-操作问题速查表.md 操作12 / Operation 12
     */
    void PriorityAreasExtractEmissionNoPeak(std::string qstrInputOriginal, std::string qstrInputChanged,
                                            std::string qstrOutputFileName,
                                            std::unordered_map<int, double> vLUCCEmissionScheme,
                                            double dRatio, double dNeighborhood);

    /**
     * @brief Rule 6: 人地关系失调区域 / Human-land imbalance
     *
     * 识别"城市增+人口减"或"城市减+人口增"的邻域。
     * Identifies neighborhoods where urban change and population change are mismatched.
     *
     * @param qstrInitialLUCCFileName  初始期LUCC / Initial LUCC, GeoTIFF, GDT_Byte
     * @param qstrCurrentLUCCFileName  当前期LUCC / Current LUCC, GeoTIFF, GDT_Byte
     * @param qstrInitialPopulationFileName 初始期人口 / Initial population, GeoTIFF, GDT_Float32
     * @param qstrCurrentPopulationFileName 当前期人口 / Current population, GeoTIFF, GDT_Float32
     * @param nNeighborhoodRadius 邻域半径 / Neighborhood radius (pixels)
     * @param qstrOutputFileName  输出: 1=失调, 0=正常 / Output: 1=imbalanced, 0=normal
     * @param vLUCCType           城市地类集合 / Urban land type set, e.g. {5}
     *
     * @warning 人口仅支持Float32 / Population only supports Float32
     *          边界邻域被截断 / Boundary neighborhoods are truncated
     *
     * @see GeoSDG-操作问题速查表.md 操作13 / Operation 13
     */
    void PriorityAreasExtractHumanLandRelationship(std::string qstrInitialLUCCFileName,
                                                   std::string qstrCurrentLUCCFileName,
                                                   std::string qstrInitialPopulationFileName,
                                                   std::string qstrCurrentPopulationFileName,
                                                   int nNeighborhoodRadius,
                                                   std::string qstrOutputFileName,
                                                   std::unordered_set<int> vLUCCType);

    /**
     * @brief 生成优先区域排名图 / Generate priority areas ranking map
     *
     * 叠加6类优先区域文件，像元值=被标记次数(0~6)，值越高越需关注。
     * Overlays 6 priority area files; pixel value = count of rules triggered (0~6).
     *
     * @param vLUCCFileNames    6个优先区域文件路径 / 6 priority area file paths
     *                          每个文件: 1=优先, 0/NoData=非优先 / Each: 1=priority, 0/NoData=non-priority
     * @param qstrOutputFileName 输出排名图 / Output ranking map, GeoTIFF
     *                           像元值 0~6 / Pixel value 0~6
     *
     * @warning 未检查尺寸一致性 / No dimension consistency check
     *          不同NoData值可能误判 / Different NoData values may cause misjudgment
     *
     * @see GeoSDG-操作问题速查表.md 操作14 / Operation 14
     */
    void generatePriorityAreas(std::vector<std::string> vLUCCFileNames, std::string qstrOutputFileName);

    /**
     * @brief 统计优先区域排名图各等级面积 / Compute priority area statistics per level
     *
     * 从排名图 GeoTIFF 中统计各优先等级（0-6）的像元数，
     * 结合空间分辨率换算为面积（km²），输出结构化结果。
     * Computes pixel counts per priority level (0-6) from a ranking map GeoTIFF,
     * converts to area (km²) using spatial resolution, and outputs structured results.
     *
     * @param qstrRankingMapPath 排名图路径 / Ranking map path, GeoTIFF, values 0-6
     * @return 0 on success, non-zero on failure
     *
     * @note 输出格式为 stdout key=value：
     *       level_0_count=... level_0_area=... level_1_count=... ...
     *       total_count=... total_area=... priority_count=... priority_area=... priority_pct=...
     *       Output format is stdout key=value pairs for each level plus totals.
     *
     * @note 经纬度投影的面积换算使用 cos(中心纬度) 修正 / Geographic CRS area uses cos(center latitude) correction
     *
     * @see GeoSDG-操作问题速查表.md 操作14 / Operation 14
     */
    int priorityAreaStats(const std::string& qstrRankingMapPath);

private:
    /** @brief Rule 3辅助: 提取缓冲区外特定地类 / Rule 3 helper: extract land types outside buffer */
    void extractLUCC(std::string qstrLUCCPath, std::string qstrBufferPath,
                     std::string qstrOutputPriorityAreasPath, std::unordered_set<int> vLUCCType);

    /** @brief Rule 4辅助: 提取缓冲区外人口超阈值区域 / Rule 4 helper: extract population above threshold outside buffer */
    void extractPOPU(std::string qstrPOPUPath, std::string qstrBufferPath,
                     std::string qstrOutputPriorityAreasPath, double dThresholdPopulation);

    /** @brief Rule 5辅助: 计算排放前缀和栅格 / Rule 5 helper: compute emission prefix sum raster */
    void calculatePrefixEmisiion(std::string qstrOriLUCCFileName, std::string qstrOutputFileName,
                                 std::unordered_map<int, double> vLUCCEmissionScheme);

    /** @brief Rule 5辅助: 提取排放增长区域 / Rule 5 helper: extract emission increase areas */
    void extractEmissionIncreaseLand(std::string qstrInputOriginal, std::string qstrInputChanged,
                                     std::string qstrOutputFileName, int nNeighborhoodRadius);

    /** @brief 辅助: 用参考栅格NoData掩膜目标栅格 / Helper: mask target raster with reference NoData */
    void removeNoDataFromSecondRaster(const std::string& inputFilePath1,
                                      const std::string& inputFilePath2,
                                      const std::string& outputFilePath);
};
