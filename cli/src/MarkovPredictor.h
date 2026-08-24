/**
 * @file MarkovPredictor.h
 * @brief Markov 链土地利用需求预测 / Markov chain land use demand prediction
 *
 * 基于多期历史土地利用数据，构建 Markov 转移概率矩阵，
 * 预测目标年份各地类面积需求量，输出 CSV 结果文件。
 *
 * Predicts future land use demand using Markov chain transition matrix
 * derived from multi-period historical LUCC data.
 */

#pragma once

#include <string>
#include <vector>
#include <map>

class MarkovPredictor
{
public:
    MarkovPredictor();
    ~MarkovPredictor();

    /**
     * @brief 执行 Markov 需求预测 / Execute Markov demand prediction
     * @param vLUCCPaths   历史 LUCC 栅格路径列表（至少 2 期）/ Historical LUCC paths (>= 2)
     * @param vYears       对应年份列表 / Corresponding years
     * @param nTargetYear  预测目标年份 / Target prediction year
     * @param nStep        预测步长（年）/ Prediction step (years)
     * @param qstrOutputPath  输出需求 CSV 路径 / Output demand CSV path
     * @param bBinaryMode  是否使用二值模式（基础设施模拟）/ Use binary mode for infra simulation
     * @return 0 成功，非0 失败 / 0 on success, non-zero on failure
     */
    int predict(const std::vector<std::string>& vLUCCPaths,
                const std::vector<int>& vYears,
                int nTargetYear,
                int nStep,
                const std::string& qstrOutputPath,
                bool bBinaryMode = false);

private:
    /**
     * @brief 读取各期 LUCC 地类统计 / Read LUCC type counts for each period
     * @return 0 成功 / 0 on success
     */
    int readLUCCCounts();

    /**
     * @brief 构建 Markov 转移概率矩阵 / Build Markov transition matrix
     * @return 0 成功 / 0 on success
     */
    int buildTransitionMatrix();

    /**
     * @brief 迭代预测需求量 / Iterate demand prediction
     */
    void iteratePrediction();

    /**
     * @brief 写入需求 CSV / Write demand CSV
     * @return 0 成功 / 0 on success
     */
    int writeDemandCSV();

    // ── 内部状态 / Internal state ──
    std::vector<std::string> vLUCCPaths_;  ///< LUCC 路径列表
    std::vector<int> vYears_;              ///< 年份列表
    int nTargetYear_ = 0;                  ///< 目标年份
    int nStep_ = 10;                       ///< 预测步长
    std::string outputPath_;               ///< 输出路径
    bool bBinaryMode_ = false;             ///< 二值模式（基础设施模拟）/ Binary mode for infra simulation

    std::vector<int> vTypes_;              ///< 地类编码列表（升序）
    int nTypes_ = 0;                       ///< 地类数量
    int nRows_ = 0;                        ///< 栅格行数
    int nCols_ = 0;                        ///< 栅格列数

    ///< 各期各地类像元数: [period][type_index]
    std::vector<std::vector<long long>> vPeriodCounts_;

    ///< Markov 转移概率矩阵 [from][to]
    std::vector<std::vector<double>> transitionMatrix_;

    ///< 预测结果: year -> [type_counts]
    std::map<int, std::vector<long long>> predictionResults_;
};
