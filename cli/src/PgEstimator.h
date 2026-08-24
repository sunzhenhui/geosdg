/**
 * @file PgEstimator.h
 * @brief 土地利用转换概率挖掘 / Land use transition probability estimation (Pg)
 *
 * 从驱动因素栅格和土地利用变化数据中，使用机器学习算法（随机森林 RF /
 * 人工神经网络 ANN）采样训练，学习各地类在驱动因素空间中的分布规律，
 * 输出每个像元转换为每种地类的发展概率（Pg）。
 *
 * Estimates transition probability (Pg) for each pixel to each land type
 * using Random Forest or Artificial Neural Network trained on driver factors
 * and historical land use change.
 */

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

class PgEstimator
{
public:
    PgEstimator();
    ~PgEstimator();

    /**
     * @brief 执行 Pg 概率挖掘 / Execute Pg probability estimation
     * @param qstrTrainLUCC   训练期 LUCC 栅格路径 / Training period LUCC raster path
     * @param qstrCurrLUCC    当前期 LUCC 栅格路径 / Current period LUCC raster path
     * @param vDriverPaths    驱动因素栅格路径列表 / Driver factor raster paths
     * @param nMethod         0=RF, 1=ANN, 2=Logit / Method: 0=RF, 1=ANN, 2=Logit
     * @param nRFTrees        RF 决策树数量 / Number of RF decision trees
     * @param dSelectRate     RF 特征选择比例 / RF feature selection rate
     * @param qstrSpecialShp  指定区域 shapefile 路径（空=不使用）/ Special region shapefile
     * @param qstrValidate    验证期 LUCC 路径（空=不验证）/ Validation LUCC path
     * @param qstrOutputPath  输出 Pg 多波段 GeoTIFF 路径 / Output Pg multi-band GeoTIFF path
     * @param nMaxPerType     正样本每类型最大采样数（默认5000）/ Max positive samples per type
     * @param nMaxNegPerType  负样本每类型最大采样数（默认5000）/ Max negative samples per type
     * @param bUseOpenMP      是否使用 OpenMP 并行（默认 true）/ Use OpenMP parallelization
     * @param qstrModelPath   模型持久化路径（空=不保存/加载）/ Model persistence path
     * @return 0 成功，非0 失败 / 0 on success, non-zero on failure
     */
    int estimate(const std::string& qstrTrainLUCC,
                 const std::string& qstrCurrLUCC,
                 const std::vector<std::string>& vDriverPaths,
                 int nMethod,
                 int nRFTrees,
                 double dSelectRate,
                 const std::string& qstrSpecialShp,
                 const std::string& qstrValidate,
                 const std::string& qstrOutputPath,
                 int nMaxPerType = 5000,
                 int nMaxNegPerType = 5000,
                 bool bUseOpenMP = true,
                 const std::string& qstrModelPath = "");

    /**
     * @brief 保存模型到文件 / Save model to file
     * @param qstrModelPath 模型文件路径 / Model file path
     * @return 0 成功 / 0 on success
     */
    int saveModel(const std::string& qstrModelPath);

    /**
     * @brief 从文件加载模型 / Load model from file
     * @param qstrModelPath 模型文件路径 / Model file path
     * @return 0 成功 / 0 on success
     */
    int loadModel(const std::string& qstrModelPath);

private:
    /**
     * @brief 校验输入数据一致性 / Validate input data consistency
     * @return true 校验通过 / Validation passed
     */
    bool validateInputs();

    /**
     * @brief 收集采样点（变化像元） / Collect sampling points (changed pixels)
     */
    void collectSamplingPoints();

    /**
     * @brief 训练随机森林模型 / Train Random Forest model
     * @return 0 成功 / 0 on success
     */
    int trainRF();

    /**
     * @brief 训练人工神经网络模型 / Train ANN model
     * @return 0 成功 / 0 on success
     */
    int trainANN();

    /**
     * @brief 训练 Logit 模型 / Train Logit model
     * @return 0 成功 / 0 on success
     */
    int trainLogit();

    /**
     * @brief 预测 Pg 概率 / Predict Pg probabilities
     * @return 0 成功 / 0 on success
     */
    int predictPg();

    /**
     * @brief 写入 Pg 多波段 GeoTIFF / Write Pg multi-band GeoTIFF
     * @return 0 成功 / 0 on success
     */
    int writePgTiff();

    /**
     * @brief 评估精度（若提供验证期） / Evaluate accuracy (if validation provided)
     */
    void evaluateAccuracy();

    // ── 内部状态 / Internal state ──
    std::string trainLUCCPath_;       ///< 训练期 LUCC 路径
    std::string currLUCCPath_;        ///< 当前期 LUCC 路径
    std::vector<std::string> vDriverPaths_; ///< 驱动因素路径
    int nMethod_ = 0;                 ///< 0=RF, 1=ANN, 2=Logit
    int nRFTrees_ = 60;               ///< RF 树数量
    double dSelectRate_ = 0.6;        ///< RF 特征选择比例
    int nMaxPerType_ = 5000;          ///< 正样本每类型最大采样数
    int nMaxNegPerType_ = 5000;       ///< 负样本每类型最大采样数
    bool bUseOpenMP_ = true;          ///< 是否使用 OpenMP 并行
    std::string modelPath_;            ///< 模型持久化路径
    std::string specialShpPath_;      ///< 指定区域 shapefile
    std::string validatePath_;        ///< 验证期 LUCC 路径
    std::string outputPath_;          ///< 输出路径

    int nRows_ = 0;                   ///< 栅格行数
    int nCols_ = 0;                   ///< 栅格列数
    int nDrivers_ = 0;                ///< 驱动因素数量
    int nTypes_ = 0;                  ///< 地类数量
    std::vector<int> vTypes_;         ///< 地类编码列表（升序）
    std::unordered_map<int, int> typeToIdx_; ///< 地类编码 -> 索引
    double dNoData_ = -9999.0;        ///< NoData 值

    ///< 驱动因素数据 [pixel][driver]
    std::vector<std::vector<float>> vDrivers_;
    ///< 归一化后驱动因素
    std::vector<std::vector<float>> vDriversNorm_;
    std::vector<float> vDriverMin_;
    std::vector<float> vDriverMax_;

    ///< 采样数据：每个采样点的驱动因素值 + 目标地类
    std::vector<std::vector<double>> vSampleFeatures_;
    std::vector<int> vSampleLabels_;
    std::vector<int> vSamplePixelIdx_;  ///< 样本对应像素索引
    std::vector<int> vSampleTypeIdx_;    ///< 样本对应类型索引

    ///< RF 模型
    std::vector<std::vector<double>> vForest_;
    std::vector<int> vTreeFeatIdxs_;

    ///< ANN 模型
    std::vector<double> vW1_;
    std::vector<double> vB1_;
    std::vector<double> vW2_;
    std::vector<double> vB2_;
    std::vector<int> vANNHiddenLayers_;

    ///< Logit 模型
    std::vector<double> vLogitW_;
    double dLogitB_ = 0.0;

    ///< Pg 概率矩阵：nRows*nCols 行，nTypes 列
    std::vector<std::vector<float>> vPgData_;
};
