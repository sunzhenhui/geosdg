/**
 * @file CASimulator.h
 * @brief CA 迭代模拟（FLUS 模型核心）/ CA iterative simulation (FLUS model core)
 *
 * 以初始 LUCC 为起点，以单一目标年份的各土地利用类型需求量为约束，
 * 结合 Pg 转换概率、邻域效应、自适应惯性系数、转换矩阵和随机扰动，
 * 通过多轮 CA 迭代逐步趋近目标需求量，输出模拟土地利用栅格。
 *
 * Simulates land use change via Cellular Automata iteration, converging
 * towards target demand using Pg probability, neighborhood effects,
 * adaptive inertia, transition constraints, and random perturbation.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <fstream>

class CASimulator
{
public:
    CASimulator();
    ~CASimulator();

    /**
     * @brief 执行 CA 迭代模拟（单目标年份）/ Execute CA simulation (single target year)
     * @param qstrInitLUCC      初始 LUCC 栅格路径 / Initial LUCC raster path
     * @param qstrPgPath        Pg 概率栅格路径 / Pg probability raster path
     * @param qstrDemandPath    需求 CSV 路径 / Demand CSV path
     * @param nTargetYear       目标年份（0=取 CSV 最后一行）/ Target year (0=last row)
     * @param nIterations       最大迭代轮数 / Max iterations
     * @param dConvergence      收敛阈值 / Convergence threshold
     * @param nNeighborRadius   邻域搜索半径（奇数）/ Neighborhood radius (odd)
     * @param vTypeWeights      各地类邻域权重 / Type neighborhood weights
     * @param dDecay            衰减因子 δ / Decay factor
     * @param nStepSize         自适应步长 / Adaptive step size
     * @param qstrRedlinePath   限制区域栅格路径（空=无限制）/ Redline raster path
     * @param transMap          转换矩阵约束 / Transition map constraints
     * @param vDk0              各地类基础 Dk 值 / Base Dk values
     * @param vMuk              各地类新斑块生成阈值 μk / New patch threshold μk
     * @param qstrOutputPath    输出模拟结果 GeoTIFF 路径 / Output GeoTIFF path
     * @param bSavePrecision    是否输出精度跟踪 / Save precision tracking
     * @param qstrValidatePath  验证期 LUCC 路径（空=不验证）/ Validation LUCC path
     * @param bBinaryMode       是否使用二值 CA 模式（基础设施模拟）/ Binary CA mode for infra simulation
     * @return 0 成功，非0 失败 / 0 on success, non-zero on failure
     */
    int simulate(const std::string& qstrInitLUCC,
                 const std::string& qstrPgPath,
                 const std::string& qstrDemandPath,
                 int nTargetYear,
                 int nIterations,
                 double dConvergence,
                 int nNeighborRadius,
                 const std::vector<double>& vTypeWeights,
                 double dDecay,
                 int nStepSize,
                 const std::string& qstrRedlinePath,
                 const std::unordered_map<int, std::vector<int>>& transMap,
                 const std::vector<double>& vDk0,
                 const std::vector<double>& vMuk,
                 const std::string& qstrOutputPath,
                 bool bSavePrecision,
                 const std::string& qstrValidatePath,
                 bool bBinaryMode = false);

    /**
     * @brief 执行多情景 CA 模拟 / Execute multi-scenario CA simulation
     * @param qstrInitLUCC      初始 LUCC 栅格路径 / Initial LUCC raster path
     * @param qstrPgPath        Pg 概率栅格路径 / Pg probability raster path
     * @param qstrDemandPath    需求 CSV 路径 / Demand CSV path
     * @param qstrScenarios     情景名称列表（逗号分隔）/ Comma-separated scenario names
     * @param qstrTargetYears   目标年份列表（逗号分隔）/ Comma-separated target years
     * @param nIterations       最大迭代轮数 / Max iterations
     * @param dConvergence      收敛阈值 / Convergence threshold
     * @param nNeighborRadius   邻域搜索半径（奇数）/ Neighborhood radius (odd)
     * @param vTypeWeights      各地类邻域权重 / Type neighborhood weights
     * @param dDecay            衰减因子 δ / Decay factor
     * @param nStepSize         自适应步长 / Adaptive step size
     * @param qstrRedlinePath   限制区域栅格路径（空=无限制）/ Redline raster path
     * @param transMap          转换矩阵约束 / Transition map constraints
     * @param vDk0              各地类基础 Dk 值 / Base Dk values
     * @param vMuk              各地类新斑块生成阈值 μk / New patch threshold μk
     * @param qstrOutputDir     输出目录 / Output directory
     * @param bSavePrecision    是否输出精度跟踪 / Save precision tracking
     * @param qstrValidatePath  验证期 LUCC 路径（空=不验证）/ Validation LUCC path
     * @return 0 成功，非0 失败 / 0 on success, non-zero on failure
     */
    int simulateMultiScenario(const std::string& qstrInitLUCC,
                              const std::string& qstrPgPath,
                              const std::string& qstrDemandPath,
                              const std::string& qstrScenarios,
                              const std::string& qstrTargetYears,
                              int nIterations,
                              double dConvergence,
                              int nNeighborRadius,
                              const std::vector<double>& vTypeWeights,
                              double dDecay,
                              int nStepSize,
                              const std::string& qstrRedlinePath,
                              const std::unordered_map<int, std::vector<int>>& transMap,
                              const std::vector<double>& vDk0,
                              const std::vector<double>& vMuk,
                              const std::string& qstrOutputDir,
                              bool bSavePrecision,
                              const std::string& qstrValidatePath);

private:
    /**
     * @brief 加载初始 LUCC / Load initial LUCC
     * @return 0 成功 / 0 on success
     */
    int loadInitLUCC();

    /**
     * @brief 加载 Pg 概率栅格 / Load Pg probability raster
     * @return 0 成功 / 0 on success
     */
    int loadPg();

    /**
     * @brief 加载需求 CSV / Load demand CSV
     * @return 0 成功 / 0 on success
     */
    int loadDemand();

    /**
     * @brief 加载限制区域 / Load redline raster
     * @return 0 成功 / 0 on success
     */
    int loadRedline();

    /**
     * @brief 构建转换矩阵 / Build transition matrix
     */
    void buildTransitionMatrix();

    /**
     * @brief 计算邻域密度 / Calculate neighborhood density
     */
    void calNeighbor();

    /**
     * @brief 计算自适应惯性 Dk / Calculate adaptive inertia Dk
     * @param nIteration 当前迭代轮次 / Current iteration
     */
    void calDk(int nIteration);

    /**
     * @brief 计算总体概率 OP / Calculate overall probability
     */
    void calOP();

    /**
     * @brief 执行轮盘赌选择 + 转换 / Run roulette wheel selection + transition
     */
    void runCA();

    /**
     * @brief 检查收敛 / Check convergence
     * @return true 已收敛 / Converged
     */
    bool checkConvergence();

    /**
     * @brief 保存结果 / Save result
     * @param nIteration 当前迭代轮次 / Current iteration
     * @return 0 成功 / 0 on success
     */
    int saveResult(int nIteration);

    /**
     * @brief 评估精度 / Evaluate precision
     * @param nIteration 当前迭代轮次 / Current iteration
     */
    void evaluatePrecision(int nIteration);

    /**
     * @brief 确定是否需要分区 / Determine tiling
     * @param nTotalRows 总行数 / Total rows
     * @param nTotalCols 总列数 / Total cols
     * @param nMemBudgetMB 内存预算（MB）/ Memory budget in MB
     */
    void determineTiling(int nTotalRows, int nTotalCols, size_t nMemBudgetMB);

    // ── 内部状态 / Internal state ──
    std::string initLUCCPath_;
    std::string pgPath_;
    std::string demandPath_;
    std::string redlinePath_;
    std::string outputPath_;
    std::string validatePath_;

    int nTargetYear_ = 0;
    int nIterations_ = 10;
    double dConvergence_ = 0.001;
    int nNeighborRadius_ = 3;
    double dDecay_ = 0.92;
    int nStepSize_ = 400;
    bool bSavePrecision_ = false;
    bool bBinaryMode_ = false;  ///< 二值 CA 模式（基础设施模拟）/ Binary CA mode for infra simulation

    int nRows_ = 0;
    int nCols_ = 0;
    int nPixels_ = 0;
    int nTypes_ = 0;
    std::vector<int> vTypes_;           ///< 地类编码列表（升序）
    std::unordered_map<int, int> typeToIdx_; ///< 地类编码 -> 索引

    ///< 初始 LUCC 数据（像元值=地类编码）
    std::vector<int> vInitType_;
    ///< 当前 LUCC 数据（迭代过程中更新）
    std::vector<int> vCurrentType_;
    ///< Pg 概率数据 [pixel][type_index]
    std::vector<std::vector<float>> vPgData_;
    ///< 需求量 [type_index]
    std::vector<long long> vDemand_;
    ///< 限制区域（0=禁止, 1=可发展）
    std::vector<uint8_t> vRedline_;
    bool bHasRedline_ = false;

    ///< 转换约束原始输入: srcType -> {dstType1, dstType2, ...}
    std::unordered_map<int, std::vector<int>> transMap_;
    ///< 转换矩阵约束: transMatrix_[src_idx] = {dst_idx1, dst_idx2, ...}
    std::vector<std::vector<int>> transMatrix_;
    bool bHasTransConstraint_ = false;

    ///< 各地类邻域权重
    std::vector<double> vTypeWeights_;
    ///< 各地类基础 Dk 值
    std::vector<double> vDk0_;
    ///< 各地类新斑块生成阈值 μk
    std::vector<double> vMuk_;

    ///< 当前轮各地类像元数
    std::vector<long long> vTypeNums_;
    ///< 上一轮各地类像元数
    std::vector<long long> vLastTypeNums_;

    ///< 邻域密度 [pixel][type_index]
    std::vector<std::vector<float>> vNeighbor_;
    ///< Dk 惯性系数 [pixel][type_index]
    std::vector<std::vector<float>> vDk_;
    ///< OP 总体概率 [pixel][type_index]
    std::vector<std::vector<float>> vOP_;

    ///< 随机数生成器
    std::mt19937 rng_{42};

    ///< 收敛计数器（连续满足收敛的轮数）
    int nConvergeCount_ = 0;

    ///< 精度跟踪 CSV 文件流
    std::ofstream precisionCSV_;

    ///< 验证期 LUCC 数据
    std::vector<int> vValidateData_;
    bool bHasValidate_ = false;

    ///< 分区相关 / Tiling
    int nTileCols_ = 0;
    int nTileRows_ = 0;
    int nHaloWidth_ = 0;
    bool isTiled() const { return nTileCols_ > 0; }
};
