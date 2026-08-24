/**
 * @file CalculateCAPrecision.h
 * @brief CA模拟精度评估模块 / CA Simulation Precision Assessment Module
 *
 * 本模块用于评估CA模型（如FLUS/PLUS）模拟土地利用变化结果的精度，
 * 并提供统计检验功能验证空间化SDG指标与传统统计方法的一致性。
 *
 * This module evaluates the accuracy of CA models (e.g., FLUS/PLUS) for
 * simulating land use change, and provides statistical tests to verify
 * the consistency between spatialized SDG indicators and traditional statistics.
 *
 * 对应论文 / Corresponding paper: Section 3.2: Implementation results
 *
 * 模块包含三类功能 / Module contains three categories:
 *   1. CA模拟精度评估 — 计算 FoM、PA、UA、Kappa、OA 五项指标（操作1）
 *      CA simulation accuracy — computes FoM, PA, UA, Kappa, OA (Operation 1)
 *   2. Pearson相关系数 — 验证空间化SDG与传统统计的一致性（操作2）
 *      Pearson correlation — verifies spatialized vs. traditional SDG consistency (Operation 2)
 *   3. 独立样本t检验 — 检验两组SDG得分差异的显著性（操作3）
 *      Independent t-test — tests significance of SDG score differences (Operation 3)
 *
 * @note 通用注意事项 / General notes:
 *   - 所有输入栅格数据必须为 GeoTIFF 格式 / All input rasters must be GeoTIFF
 *   - 所有函数不检查输入数据坐标系一致性，需用户自行保证
 *     Functions do not check coordinate system consistency; users must ensure it
 *   - GDAL_FILENAME_IS_UTF8=NO 在各函数中设置，中文路径可能异常
 *     GDAL_FILENAME_IS_UTF8=NO is set in each function; Chinese paths may cause issues
 *
 * @see GeoSDG-操作问题速查表.md 操作1~3 / Operations 1-3
 * @see GeoSDG-项目结构速查表.md CA精度评估模块 / CA Precision Assessment Module
 */

#pragma once

#include <string>
#include <vector>

class CalculateCAPrecision
{
public:
    CalculateCAPrecision();
    ~CalculateCAPrecision();

    /**
     * @brief 计算CA模拟结果的精度指标 / Calculate CA simulation accuracy metrics
     *
     * 通过对比原始期、模拟期和真实期三幅土地利用栅格，计算五项精度指标，
     * 用于衡量CA模型的模拟效果。
     *
     * Computes five accuracy metrics by comparing original, simulated, and real
     * land use rasters, measuring the CA model's simulation performance.
     *
     * @param oriTifPath    原始土地利用数据路径（如 2010.tif），GeoTIFF，GDT_Byte，单波段
     *                      Path to original land use data (e.g., 2010.tif), GeoTIFF, GDT_Byte, single band
     * @param simulatedTifPath 模拟结果数据路径（如 Simulation.tif），GeoTIFF，GDT_Byte，单波段
     *                      Path to simulated result (e.g., Simulation.tif), GeoTIFF, GDT_Byte, single band
     * @param realTifpath   真实土地利用数据路径（如 2020.tif），GeoTIFF，GDT_Byte，单波段
     *                      Path to real land use data (e.g., 2020.tif), GeoTIFF, GDT_Byte, single band
     * @param[out] FoM      Figure of Merit，综合精度指标，范围 [0, 1]
     *                      Comprehensive accuracy metric, range [0, 1]
     *                      FoM = B / (A + B + C + D)，其中 A=正确模拟的变化，B=命中，C=误判，D=漏判
     * @param[out] PA       Producer's Accuracy（生产者精度），范围 [0, 1]
     *                      Producer's Accuracy, range [0, 1]; PA = B / (A + B + C)
     * @param[out] UA       User's Accuracy（用户精度），范围 [0, 1]
     *                      User's Accuracy, range [0, 1]; UA = B / (B + C + D)
     * @param[out] Kappa    Kappa系数，范围 [-1, 1]，通常 [0, 1]
     *                      Kappa coefficient, range [-1, 1], typically [0, 1]
     * @param[out] OA       Overall Accuracy（总体精度），范围 [0, 1]
     *                      Overall Accuracy, range [0, 1]; OA = (A + D) / N
     *
     * @note 前置条件 / Preconditions:
     *   - 三幅影像行列数必须完全一致，否则函数静默返回
     *     Three images must have identical dimensions; otherwise silent return
     *   - 输入数据必须为 GDT_Byte 类型且单波段，否则静默返回
     *     Input must be GDT_Byte with single band; otherwise silent return
     *   - 输入文件路径必须有效，GDALOpen失败时输出值全为0
     *     File paths must be valid; outputs are 0 on GDALOpen failure
     *
     * @warning 已知问题 / Known issues:
     *   - 若 A+B+C+D=0（无变化区域），FoM 计算除零导致 NaN
     *     If A+B+C+D=0 (no change area), FoM division-by-zero yields NaN
     *   - 若偶然一致性极高（_pe接近1），Kappa 分母接近0导致异常值
     *     If chance agreement is very high (_pe≈1), Kappa denominator nears 0
     *
     * @see GeoSDG-操作问题速查表.md 操作1 / Operation 1
     */
    void calculatePrecision(std::string oriTifPath, std::string simulatedTifPath, std::string realTifpath,
                            double &FoM, double &PA, double &UA, double &Kappa, double &OA);

    /**
     * @brief 计算两组数据的Pearson相关系数 / Calculate Pearson correlation coefficient
     *
     * 用于验证空间化SDG指标得分与传统统计方法得分之间的线性相关性，
     * 相关系数越接近1表示两种方法一致性越高。
     *
     * Verifies the linear correlation between spatialized SDG scores and
     * traditional statistical scores; closer to 1 means higher consistency.
     *
     * @param x  第一组数据（如传统统计方法SDG得分），长度 >= 2
     *           First dataset (e.g., traditional SDG scores), length >= 2
     * @param y  第二组数据（如GeoSDG空间化SDG得分），长度 >= 2，必须与x长度相同
     *           Second dataset (e.g., GeoSDG spatialized scores), length >= 2, must equal x's length
     * @return Pearson相关系数，范围 [-1, 1] / Pearson correlation coefficient, range [-1, 1]
     *         1 = 完全正相关，0 = 无线性相关，-1 = 完全负相关
     *         1 = perfect positive, 0 = no linear, -1 = perfect negative correlation
     *
     * @throws std::invalid_argument 当两组数据长度不等或为空时抛出异常
     *         Thrown when vector lengths differ or are empty
     *
     * @warning 已知问题 / Known issues:
     *   - 若数据方差为0（常数序列），返回0（无法计算相关性）
     *     If variance is 0 (constant sequence), returns 0 (cannot compute)
     *
     * @see GeoSDG-操作问题速查表.md 操作2 / Operation 2
     */
    double calculateCorrelationCoefficient(const std::vector<double>& x, const std::vector<double>& y);

    /**
     * @brief 独立样本t检验 / Independent samples t-test
     *
     * 检验两组SDG得分是否存在统计学上的显著差异，
     * 返回t统计量，需结合自由度和显著性水平查表判断。
     *
     * Tests whether two groups of SDG scores differ significantly;
     * returns t-statistic for comparison with critical values.
     *
     * @param data1  样本1数据（如传统统计方法SDG得分），长度 >= 2
     *               Sample 1 data (e.g., traditional SDG scores), length >= 2
     * @param data2  样本2数据（如GeoSDG空间化SDG得分），长度 >= 2
     *               Sample 2 data (e.g., GeoSDG spatialized scores), length >= 2
     * @return t统计量 / t-statistic
     *         正值表示data1均值大于data2，负值反之，绝对值越大差异越显著
     *         Positive: data1 mean > data2; negative: opposite; larger |t| = more significant
     *
     * @warning 已知问题 / Known issues:
     *   - 样本量=1时方差计算除以(n-1)=0，导致除零错误（函数未做保护）
     *     When sample size=1, variance divides by (n-1)=0 causing division-by-zero
     *   - 两组方差极小时，t统计量可能溢出为异常大值
     *     With very small variances, t-statistic may overflow
     *
     * @see GeoSDG-操作问题速查表.md 操作3 / Operation 3
     */
    double tTestIndependent(const std::vector<double>& data1, const std::vector<double>& data2);

private:
    /**
     * @brief 计算样本均值 / Calculate sample mean
     * @param data  输入数据向量 / Input data vector
     * @return 算术平均值 / Arithmetic mean
     */
    double mean(const std::vector<double>& data);

    /**
     * @brief 计算样本方差 / Calculate sample variance
     * @param data  输入数据向量 / Input data vector
     * @param mean  已知的样本均值 / Known sample mean
     * @return 样本方差（除以n-1的无偏估计）/ Sample variance (unbiased, divided by n-1)
     */
    double variance(const std::vector<double>& data, double mean);
};
