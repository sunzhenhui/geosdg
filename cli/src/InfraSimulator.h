/**
 * @file InfraSimulator.h
 * @brief 基础设施占用/侵占模拟器 / Infrastructure占用 simulation
 *
 * 基于二元CA模拟基础设施（道路、建筑等）对土地转型的响应。
 * 将LUCC分为"已占用"和"未占用"两类，运行二值CA模拟基础设施扩张。
 *
 * Simulates infrastructure (roads, buildings, etc.) occupation/encroachment
 * on land using binary CA. Classifies LUCC into "occupied" and "unoccupied",
 * runs binary CA to simulate infrastructure expansion.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <fstream>

class InfraSimulator
{
public:
    InfraSimulator();
    ~InfraSimulator();

    /**
     * @brief 执行基础设施二值 CA 模拟 / Execute binary CA simulation for infrastructure
     * @param qstrInitLUCC      初始 LUCC 栅格路径 / Initial LUCC raster path
     * @param qstrInfraZonePath  基础设施优先区栅格路径（Pg 概率）/ Infra priority zone raster path
     * @param qstrDemandPath     需求 CSV 路径 / Demand CSV path
     * @param nTargetYear       目标年份（0=取 CSV 最后一行）/ Target year (0=last row)
     * @param nIterations       最大迭代轮数 / Max iterations
     * @param dConvergence      收敛阈值 / Convergence threshold
     * @param nNeighborRadius   邻域搜索半径（奇数）/ Neighborhood radius (odd)
     * @param dDecay            衰减因子 δ / Decay factor
     * @param qstrOutputPath    输出模拟结果 GeoTIFF 路径 / Output GeoTIFF path
     * @param qstrValidatePath  验证期 LUCC 路径（空=不验证）/ Validation LUCC path
     * @return 0 成功，非0 失败 / 0 on success, non-zero on failure
     */
    int simulate(const std::string& qstrInitLUCC,
                const std::string& qstrInfraZonePath,
                const std::string& qstrDemandPath,
                int nTargetYear,
                int nIterations,
                double dConvergence,
                int nNeighborRadius,
                double dDecay,
                const std::string& qstrOutputPath,
                const std::string& qstrValidatePath);

private:
    /**
     * @brief 加载初始 LUCC / Load initial LUCC
     * @return 0 成功 / 0 on success
     */
    int loadInitLUCC();

    /**
     * @brief 加载基础设施优先区 / Load infrastructure priority zone
     * @return 0 成功 / 0 on success
     */
    int loadInfraZone();

    /**
     * @brief 加载需求 CSV / Load demand CSV
     * @return 0 成功 / 0 on success
     */
    int loadDemand();

    /**
     * @brief 计算邻域密度 / Calculate neighborhood density
     */
    void calNeighbor();

    /**
     * @brief 计算适应性惯性 / Calculate adaptive inertia
     * @param nIteration 当前迭代轮次 / Current iteration
     */
    void calInertia(int nIteration);

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

    // ── Internal state / 内部状态 ──
    std::string initLUCCPath_;
    std::string infraZonePath_;
    std::string demandPath_;
    std::string outputPath_;
    std::string validatePath_;

    int nTargetYear_ = 0;
    int nIterations_ = 10;
    double dConvergence_ = 0.001;
    int nNeighborRadius_ = 3;
    double dDecay_ = 0.92;

    int nRows_ = 0;
    int nCols_ = 0;
    int nPixels_ = 0;
    int nTypes_ = 2;  // Binary: 0=unoccupied, 1=occupied

    // Binary classification: 0=unoccupied (any non-zero LUCC), 1=occupied (usually LUCC=1)
    int infraTypeCode_ = 1;  // LUCC value representing infrastructure

    ///< Initial LUCC data (as binary: 0=unoccupied, 1=occupied)
    std::vector<int8_t> vInitBinary_;
    ///< Current LUCC data (binary, updated during iteration)
    std::vector<int8_t> vCurrentBinary_;

    ///< Infrastructure priority zone probability [pixel] (0.0-1.0)
    std::vector<float> vInfraZone_;
    ///< Neighborhood density [pixel]
    std::vector<float> vNeighbor_;
    ///< Inertia coefficient [pixel]
    std::vector<float> vInertia_;

    ///< Binary demand [occupied pixels count]
    long long nDemand_ = 0;
    ///< Current occupied pixel count
    long long nCurrentOccupied_ = 0;

    ///< Random number generator
    std::mt19937 rng_{42};

    ///< Convergence counter
    int nConvergeCount_ = 0;

    ///< Validation data (binary)
    std::vector<int8_t> vValidateBinary_;
    bool bHasValidate_ = false;

    ///< Precision tracking CSV
    std::ofstream precisionCSV_;
};
