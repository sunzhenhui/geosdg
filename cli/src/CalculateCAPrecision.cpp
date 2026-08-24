/**
 * @file CalculateCAPrecision.cpp
 * @brief CA模拟精度评估模块实现 / CA Simulation Precision Assessment Module Implementation
 *
 * 本文件实现CA模拟精度评估、Pearson相关系数计算和独立样本t检验。
 * 所有函数均已添加异常保护，防止crash。
 *
 * This file implements CA simulation accuracy assessment, Pearson correlation
 * coefficient calculation, and independent samples t-test.
 * All functions include exception guards to prevent crashes.
 */

#include "CalculateCAPrecision.h"
#include "Logger.h"

#include "ogrsf_frmts.h"
#include "gdal_priv.h"
#include "ogr_geometry.h"
#include "ogr_srs_api.h"
#include "ogr_feature.h"
#include "ogr_spatialref.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <numeric>
#include <iomanip>
#include <stdexcept>

// ============================================================================
// 构造/析构 / Constructor / Destructor
// ============================================================================

CalculateCAPrecision::CalculateCAPrecision()
{
}

CalculateCAPrecision::~CalculateCAPrecision()
{
}

// ============================================================================
// CA模拟精度评估 / CA Simulation Accuracy Assessment
// ============================================================================

void CalculateCAPrecision::calculatePrecision(
	std::string oriTifPath, std::string simulatedTifPath, std::string realTifpath,
	double &FoM, double &PA, double &UA, double &Kappa, double &OA)
{
	// 初始化输出为0，防止未赋值 / Initialize outputs to 0 to prevent undefined values
	FoM = 0.0; PA = 0.0; UA = 0.0; Kappa = 0.0; OA = 0.0;

	LOG_INFO("calculatePrecision: Starting...");
	LOG_DEBUG("  oriTifPath     = " + oriTifPath);
	LOG_DEBUG("  simulatedTifPath= " + simulatedTifPath);
	LOG_DEBUG("  realTifpath     = " + realTifpath);

	// 检查输入路径是否为空 / Validate input paths are not empty
	if (oriTifPath.empty() || simulatedTifPath.empty() || realTifpath.empty())
	{
		LOG_ERROR("calculatePrecision: Input path is empty");
		return;
	}

	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

	// ── 读取原始期数据 / Read original period data ──
	GDALDataset *poDataset = (GDALDataset*)GDALOpen(oriTifPath.c_str(), GA_ReadOnly);
	if (!poDataset)
	{
		LOG_ERROR("calculatePrecision: Cannot open original data: " + oriTifPath);
		return;
	}
	int iImgSizeX0 = poDataset->GetRasterXSize();
	int iImgSizeY0 = poDataset->GetRasterYSize();
	int nCount = poDataset->GetRasterCount();
	GDALDataType gdal_data_type = poDataset->GetRasterBand(1)->GetRasterDataType();
	if (gdal_data_type != GDT_Byte || nCount != 1)
	{
		LOG_ERROR("calculatePrecision: Original data not GDT_Byte or not single band");
		GDALClose(poDataset);
		return;
	}
	double dnodata = poDataset->GetRasterBand(1)->GetNoDataValue();

	// 检查栅格尺寸合理性 / Validate raster dimensions
	if (iImgSizeX0 <= 0 || iImgSizeY0 <= 0)
	{
		LOG_ERROR("calculatePrecision: Invalid original data dimensions");
		GDALClose(poDataset);
		return;
	}

	// 检查整数溢出风险 / Check for integer overflow risk
	size_t nPixels = (size_t)iImgSizeX0 * iImgSizeY0;
	if (nPixels > (size_t)2 * 1024 * 1024 * 1024)  // > 2 billion pixels
	{
		LOG_WARN("calculatePrecision: Very large pixel count, memory may be insufficient");
	}

	unsigned char *_pOriValue = nullptr;
	unsigned char *_pSimuValue = nullptr;
	unsigned char *_pRealValue = nullptr;
	int *pRealTypeNum = nullptr;
	int *pSimulatedTypeNum = nullptr;

	try
	{
		_pOriValue = new unsigned char[nPixels];
		_pSimuValue = new unsigned char[nPixels];
		_pRealValue = new unsigned char[nPixels];

		poDataset->RasterIO(GF_Read, 0, 0, iImgSizeX0, iImgSizeY0, _pOriValue, iImgSizeX0, iImgSizeY0, gdal_data_type, 1, 0, 0, 0, 0);
		GDALClose(poDataset);
		poDataset = nullptr;

		// ── 读取模拟期数据 / Read simulated period data ──
		poDataset = (GDALDataset*)GDALOpen(simulatedTifPath.c_str(), GA_ReadOnly);
		if (!poDataset)
		{
			LOG_ERROR("calculatePrecision: Cannot open simulated data: " + simulatedTifPath);
			delete[] _pOriValue; delete[] _pSimuValue; delete[] _pRealValue;
			return;
		}
		if (iImgSizeX0 != poDataset->GetRasterXSize() || iImgSizeY0 != poDataset->GetRasterYSize())
		{
			LOG_ERROR("calculatePrecision: Simulated data dimensions mismatch");
			GDALClose(poDataset);
			delete[] _pOriValue; delete[] _pSimuValue; delete[] _pRealValue;
			return;
		}
		nCount = poDataset->GetRasterCount();
		gdal_data_type = poDataset->GetRasterBand(1)->GetRasterDataType();
		if (gdal_data_type != GDT_Byte || nCount != 1)
		{
			LOG_ERROR("calculatePrecision: Simulated data not GDT_Byte or not single band");
			GDALClose(poDataset);
			delete[] _pOriValue; delete[] _pSimuValue; delete[] _pRealValue;
			return;
		}
		poDataset->RasterIO(GF_Read, 0, 0, iImgSizeX0, iImgSizeY0, _pSimuValue, iImgSizeX0, iImgSizeY0, gdal_data_type, 1, 0, 0, 0, 0);
		GDALClose(poDataset);
		poDataset = nullptr;

		// ── 读取真实期数据 / Read real period data ──
		poDataset = (GDALDataset*)GDALOpen(realTifpath.c_str(), GA_ReadOnly);
		if (!poDataset)
		{
			LOG_ERROR("calculatePrecision: Cannot open real data: " + realTifpath);
			delete[] _pOriValue; delete[] _pSimuValue; delete[] _pRealValue;
			return;
		}
		if (iImgSizeX0 != poDataset->GetRasterXSize() || iImgSizeY0 != poDataset->GetRasterYSize())
		{
			LOG_ERROR("calculatePrecision: Real data dimensions mismatch");
			GDALClose(poDataset);
			delete[] _pOriValue; delete[] _pSimuValue; delete[] _pRealValue;
			return;
		}
		nCount = poDataset->GetRasterCount();
		gdal_data_type = poDataset->GetRasterBand(1)->GetRasterDataType();
		if (gdal_data_type != GDT_Byte || nCount != 1)
		{
			LOG_ERROR("calculatePrecision: Real data not GDT_Byte or not single band");
			GDALClose(poDataset);
			delete[] _pOriValue; delete[] _pSimuValue; delete[] _pRealValue;
			return;
		}
		poDataset->RasterIO(GF_Read, 0, 0, iImgSizeX0, iImgSizeY0, _pRealValue, iImgSizeX0, iImgSizeY0, gdal_data_type, 1, 0, 0, 0, 0);
		GDALClose(poDataset);
		poDataset = nullptr;

		// ── 统计地类类型 / Collect land type categories ──
		std::vector<int> vType;
		for (int i = 0; i < iImgSizeX0 * iImgSizeY0; i++)
		{
			int nType = (int)_pRealValue[i];
			int j = 0;
			while (j < (int)vType.size() && vType[j] != nType)
			{
				j++;
			}
			if (j == (int)vType.size() && nType != (int)dnodata)
				vType.push_back(nType);
		}

		// ── 计算A/B/C/D混淆矩阵 / Compute A/B/C/D confusion matrix ──
		// A: 正确模拟的变化(模拟=原始≠真实) / Correctly simulated change
		// B: 命中(模拟=真实≠原始) / Hit
		// C: 误判(模拟≠原始且模拟≠真实) / False alarm
		// D: 漏判(真实=原始且模拟≠原始) / Miss
		int A = 0, B = 0, C = 0, D = 0;
		for (int i = 0; i < iImgSizeX0 * iImgSizeY0; i++)
		{
			int _dtemp1 = _pSimuValue[i];
			int _dtemp2 = _pOriValue[i];
			int _dtemp3 = _pRealValue[i];

			if (_dtemp1 != (int)dnodata && _dtemp2 != (int)dnodata && _dtemp3 != (int)dnodata)
			{
				if (_dtemp3 != _dtemp2 && _dtemp1 == _dtemp2) A++;
				if (_dtemp3 != _dtemp2 && _dtemp1 == _dtemp3) B++;
				if (_dtemp3 != _dtemp2 && _dtemp1 != _dtemp2 && _dtemp1 != _dtemp3) C++;
				if (_dtemp3 == _dtemp2 && _dtemp1 != _dtemp2) D++;
			}
		}

		// ── 计算FoM/PA/UA（防止除零）/ Compute FoM/PA/UA with zero-division guard ──
		int denomFoM = A + B + C + D;
		if (denomFoM > 0)
		{
			FoM = (double)B / denomFoM;
		}
		else
		{
			FoM = 0.0;
			LOG_WARN("calculatePrecision: FoM denominator is 0 (no change area), set to 0");
		}

		int denomPA = A + B + C;
		if (denomPA > 0)
		{
			PA = (double)B / denomPA;
		}
		else
		{
			PA = 0.0;
			LOG_WARN("calculatePrecision: PA denominator is 0, set to 0");
		}

		int denomUA = B + C + D;
		if (denomUA > 0)
		{
			UA = (double)B / denomUA;
		}
		else
		{
			UA = 0.0;
			LOG_WARN("calculatePrecision: UA denominator is 0, set to 0");
		}

		// ── 计算Kappa和OA / Compute Kappa and OA ──
		int nRight = 0, nError = 0, nIndex = 0;
		pRealTypeNum = new int[vType.size()];
		memset(pRealTypeNum, 0, sizeof(int) * vType.size());
		pSimulatedTypeNum = new int[vType.size()];
		memset(pSimulatedTypeNum, 0, sizeof(int) * vType.size());

		for (int i = 0; i < iImgSizeX0 * iImgSizeY0; i++)
		{
			int _dtemp1 = _pSimuValue[i];
			int _dtemp2 = _pOriValue[i];
			int _dtemp3 = _pRealValue[i];

			if (_dtemp1 != (int)dnodata && _dtemp2 != (int)dnodata && _dtemp3 != (int)dnodata)
			{
				if (_dtemp1 == _dtemp3) nRight++;
				else nError++;

				nIndex = 0;
				while (nIndex < (int)vType.size() && _dtemp1 != vType[nIndex]) nIndex++;
				if (nIndex < (int)vType.size()) pSimulatedTypeNum[nIndex]++;

				nIndex = 0;
				while (nIndex < (int)vType.size() && _dtemp3 != vType[nIndex]) nIndex++;
				if (nIndex < (int)vType.size()) pRealTypeNum[nIndex]++;
			}
		}

		double _po = (double)nRight / (nRight + nError);
		double _pe = 0, dSum = 0;
		int _N = 0;
		for (int i = 0; i < (int)vType.size(); i++)
		{
			_N = _N + pSimulatedTypeNum[i];
			dSum = dSum + (double)pSimulatedTypeNum[i] * pRealTypeNum[i];
		}

		if (_N > 0)
		{
			_pe = (double)dSum / _N / _N;
			OA = (double)nRight / _N;
		}
		else
		{
			_pe = 0.0;
			OA = 0.0;
			LOG_WARN("calculatePrecision: _N is 0, OA set to 0");
		}

		// 防止Kappa除零 / Guard Kappa against division-by-zero
		double denomKappa = 1.0 - _pe;
		if (fabs(denomKappa) > 1e-12)
		{
			Kappa = (_po - _pe) / denomKappa;
		}
		else
		{
			Kappa = 0.0;
			LOG_WARN("calculatePrecision: Kappa denominator near 0 (very high chance agreement), set to 0");
		}

		// ── 记录结果 / Log results ──
		LOG_RESULT("calculatePrecision", "FoM", FoM);
		LOG_RESULT("calculatePrecision", "PA", PA);
		LOG_RESULT("calculatePrecision", "UA", UA);
		LOG_RESULT("calculatePrecision", "Kappa", Kappa);
		LOG_RESULT("calculatePrecision", "OA", OA);
		LOG_INFO("calculatePrecision: Completed.");
	}
	catch (const std::bad_alloc& e)
	{
		// 内存分配失败 / Memory allocation failure
		LOG_ERROR(std::string("calculatePrecision: Memory allocation failed: ") + e.what());
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(std::string("calculatePrecision: Exception: ") + e.what());
	}
	catch (...)
	{
		LOG_ERROR("calculatePrecision: Unknown exception");
	}

	// ── 清理内存 / Cleanup memory ──
	// delete[] nullptr 是安全的 / delete[] nullptr is safe
	delete[] _pOriValue;
	delete[] _pSimuValue;
	delete[] _pRealValue;
	delete[] pSimulatedTypeNum;
	delete[] pRealTypeNum;
}

// ============================================================================
// Pearson相关系数 / Pearson Correlation Coefficient
// ============================================================================

double CalculateCAPrecision::calculateCorrelationCoefficient(const std::vector<double>& x, const std::vector<double>& y)
{
	LOG_INFO("calculateCorrelationCoefficient: Starting...");

	// 检查输入有效性 / Validate input
	if (x.size() != y.size() || x.size() == 0)
	{
		LOG_ERROR("calculateCorrelationCoefficient: Vectors must be same size and non-empty");
		return 0.0;  // 返回0而非抛异常，避免crash / Return 0 instead of throwing to prevent crash
	}

	if (x.size() < 2)
	{
		LOG_WARN("calculateCorrelationCoefficient: Sample size < 2, cannot compute");
		return 0.0;
	}

	try
	{
		double n = (double)x.size();
		double sum_x = std::accumulate(x.begin(), x.end(), 0.0);
		double sum_y = std::accumulate(y.begin(), y.end(), 0.0);
		double sum_x2 = std::inner_product(x.begin(), x.end(), x.begin(), 0.0);
		double sum_y2 = std::inner_product(y.begin(), y.end(), y.begin(), 0.0);
		double sum_xy = std::inner_product(x.begin(), x.end(), y.begin(), 0.0);

		double numerator = n * sum_xy - sum_x * sum_y;
		double denominator = sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));

		if (fabs(denominator) < 1e-15)
		{
			LOG_WARN("calculateCorrelationCoefficient: Denominator is 0 (constant sequence), returning 0");
			return 0.0;
		}

		double r = numerator / denominator;
		// 限制范围在[-1,1]（浮点误差修正）/ Clamp to [-1,1] (floating-point correction)
		if (r > 1.0) r = 1.0;
		if (r < -1.0) r = -1.0;

		LOG_RESULT("calculateCorrelationCoefficient", "r", r);
		LOG_INFO("calculateCorrelationCoefficient: Completed.");
		return r;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(std::string("calculateCorrelationCoefficient: Exception: ") + e.what());
		return 0.0;
	}
}

// ============================================================================
// 辅助函数：均值与方差 / Helper: mean and variance
// ============================================================================

double CalculateCAPrecision::mean(const std::vector<double>& data)
{
	if (data.empty()) return 0.0;
	return std::accumulate(data.begin(), data.end(), 0.0) / data.size();
}

double CalculateCAPrecision::variance(const std::vector<double>& data, double mean)
{
	// 防止除以0 / Prevent division by zero
	if (data.size() <= 1) return 0.0;
	double sum_squared_diff = 0.0;
	for (double value : data)
	{
		sum_squared_diff += (value - mean) * (value - mean);
	}
	return sum_squared_diff / (data.size() - 1);
}

// ============================================================================
// 独立样本t检验 / Independent Samples t-test
// ============================================================================

double CalculateCAPrecision::tTestIndependent(const std::vector<double>& data1, const std::vector<double>& data2)
{
	LOG_INFO("tTestIndependent: Starting...");

	// 检查样本量 / Validate sample sizes
	if (data1.size() < 2 || data2.size() < 2)
	{
		LOG_ERROR("tTestIndependent: Sample size < 2, cannot compute");
		return 0.0;
	}

	try
	{
		double mean1 = mean(data1);
		double mean2 = mean(data2);

		double var1 = variance(data1, mean1);
		double var2 = variance(data2, mean2);

		double n1 = (double)data1.size();
		double n2 = (double)data2.size();

		// 防止分母为0 / Prevent division by zero
		double denom = var1 / n1 + var2 / n2;
		if (fabs(denom) < 1e-15)
		{
			LOG_WARN("tTestIndependent: Variance extremely small, denominator near 0");
			return 0.0;
		}

		double t_statistic = (mean1 - mean2) / sqrt(denom);

		LOG_RESULT("tTestIndependent", "t", t_statistic);
		LOG_INFO("tTestIndependent: Completed.");
		return t_statistic;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(std::string("tTestIndependent: Exception: ") + e.what());
		return 0.0;
	}
}
