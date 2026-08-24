/**
 * @file CalculateSDG.cpp
 * @brief 空间化SDG指标计算模块实现 / Spatialized SDG Indicator Calculation Module Implementation
 */

#include "CalculateSDG.h"
#include "Logger.h"
#include <iostream>
#include "ogrsf_frmts.h"
#include "gdal_priv.h"
#include "ogr_geometry.h"
#include "ogr_srs_api.h"
#include "ogr_feature.h"
#include "ogr_spatialref.h"
#include <unordered_set>
#include <cmath>
#include <stdexcept>
#include <filesystem>

using namespace std;

// ============================================================================
// 构造/析构 / Constructor / Destructor
// ============================================================================

CalculateSDG::CalculateSDG()
{
}

CalculateSDG::~CalculateSDG()
{
}

double CalculateSDG::calculateLandProportionIndicator(
	std::string qstrFileName,
	double dMaxThreshold,
	double dMinThreshold,
	std::unordered_set<int> mqsetSelectLUCCTypes)
{
	Logger::instance().info("calculateLandProportionIndicator: Starting...");
	Logger::instance().debug("  file = " + qstrFileName);

	// 检查输入路径 / Validate input path
	if (qstrFileName.empty())
	{
		Logger::instance().error("calculateLandProportionIndicator: File path is empty");
		return 0.0;
	}

	double dResults = 0;
	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	CPLSetConfigOption("SHAPE_ENCODING", "");

	// 使用GA_ReadOnly替代GA_Update，避免意外修改源数据
	// Use GA_ReadOnly instead of GA_Update to prevent accidental source modification
	GDALDataset *poDS = (GDALDataset *)GDALOpen(qstrFileName.c_str(), GA_ReadOnly);
	if (poDS == NULL)
	{
		Logger::instance().error("calculateLandProportionIndicator: Cannot open file: " + qstrFileName);
		return dResults;
	}

	int nCols = poDS->GetRasterXSize();
	int nRows = poDS->GetRasterYSize();

	if (nCols <= 0 || nRows <= 0)
	{
		Logger::instance().error("calculateLandProportionIndicator: Invalid raster dimensions");
		GDALClose(poDS);
		return 0.0;
	}

	unsigned char* pData = new unsigned char[nCols * nRows];
	poDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, nCols, nRows, pData, nCols, nRows, GDT_Byte, 0, 0);
	double dNodata = poDS->GetRasterBand(1)->GetNoDataValue();
	GDALClose(poDS);
	poDS = NULL;

	long long nCount = 0;
	long long nNoCount = 0;
	for (int i = 0; i < nCols * nRows; i++)
	{
		if (pData[i] == (unsigned char)dNodata) continue;
		bool _bAdd = mqsetSelectLUCCTypes.find(pData[i]) != mqsetSelectLUCCTypes.end();
		if (_bAdd) nCount++;
		else nNoCount++;
	}

	long long nTotal = nCount + nNoCount;
	if (nTotal == 0)
	{
		Logger::instance().warn("calculateLandProportionIndicator: No valid pixels");
		delete[] pData;
		return 0.0;
	}

	dResults = 100.0 * double(nCount) / double(nTotal);
	delete[] pData;

	double score = normalization(dResults, dMinThreshold, dMaxThreshold);
	Logger::instance().result("calculateLandProportionIndicator", "proportion", dResults);
	Logger::instance().result("calculateLandProportionIndicator", "score", score);
	Logger::instance().info("calculateLandProportionIndicator: Completed.");
	return score;
}

double CalculateSDG::calculateLandConversionIndicator(
	std::string qstrInputOriginal,
	std::string qstrInputChanged,
	std::unordered_map<int, std::vector<int>> mqsetSelectLUCCTransitionTypes,
	double dMaxThreshold,
	double dMinThreshold,
	bool bState)
{
	Logger::instance().info("calculateLandConversionIndicator: Starting...");

	// 检查输入路径 / Validate input paths
	if (qstrInputOriginal.empty() || qstrInputChanged.empty())
	{
		Logger::instance().error("calculateLandConversionIndicator: Input path is empty");
		return 0.0;
	}

	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	CPLSetConfigOption("SHAPE_ENCODING", "");

	GDALDataset *poOriginalLandUseDS = (GDALDataset *)GDALOpen(qstrInputOriginal.c_str(), GA_ReadOnly);
	if (poOriginalLandUseDS == nullptr)
	{
		Logger::instance().error("calculateLandConversionIndicator: Cannot open original data: " + qstrInputOriginal);
		return 0.0;
	}
	if (poOriginalLandUseDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
	{
		Logger::instance().error("calculateLandConversionIndicator: Original data not GDT_Byte");
		GDALClose(poOriginalLandUseDS);
		return 0.0;
	}

	GDALDataset *poChangedLandUseDS = (GDALDataset *)GDALOpen(qstrInputChanged.c_str(), GA_ReadOnly);
	if (poChangedLandUseDS == nullptr)
	{
		Logger::instance().error("calculateLandConversionIndicator: Cannot open changed data: " + qstrInputChanged);
		GDALClose(poOriginalLandUseDS);
		return 0.0;
	}
	if (poChangedLandUseDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
	{
		Logger::instance().error("calculateLandConversionIndicator: Changed data not GDT_Byte");
		GDALClose(poOriginalLandUseDS);
		GDALClose(poChangedLandUseDS);
		return 0.0;
	}

	int nColsOriginal = poOriginalLandUseDS->GetRasterXSize();
	int nRowsOriginal = poOriginalLandUseDS->GetRasterYSize();
	int nColsChanged = poChangedLandUseDS->GetRasterXSize();
	int nRowsChanged = poChangedLandUseDS->GetRasterYSize();

	if (nColsOriginal != nColsChanged || nRowsOriginal != nRowsChanged)
	{
		Logger::instance().error("calculateLandConversionIndicator: Dimension mismatch");
		GDALClose(poOriginalLandUseDS);
		GDALClose(poChangedLandUseDS);
		return 0.0;
	}

	int nCols = nColsOriginal;
	int nRows = nRowsOriginal;
	unsigned char *_pOriginalValue = new unsigned char[nCols * nRows];
	unsigned char *_pChangedValue = new unsigned char[nCols * nRows];

	poOriginalLandUseDS->RasterIO(GF_Read, 0, 0, nCols, nRows, _pOriginalValue, nCols, nRows, GDT_Byte, 1, 0, 0, 0, 0);
	poChangedLandUseDS->RasterIO(GF_Read, 0, 0, nCols, nRows, _pChangedValue, nCols, nRows, GDT_Byte, 1, 0, 0, 0, 0);
	double dNodataOriginalLandUse = poOriginalLandUseDS->GetRasterBand(1)->GetNoDataValue();
	double dNodataChangedLandUse = poChangedLandUseDS->GetRasterBand(1)->GetNoDataValue();

	GDALClose(poChangedLandUseDS);
	GDALClose(poOriginalLandUseDS);

	long long nCount = 0;
	long long nAllCount = 0;
	for (int i = 0; i < nCols; i++)
	{
		for (int j = 0; j < nRows; j++)
		{
			if (_pOriginalValue[i + j * nCols] == dNodataOriginalLandUse || _pChangedValue[i + j * nCols] == dNodataChangedLandUse)
				continue;
			if (_pOriginalValue[i + j * nCols] != _pChangedValue[i + j * nCols])
			{
				bool _bAdd = false;
				std::pair<int, int> _LUCCPair = std::make_pair(_pOriginalValue[i + j * nCols], _pChangedValue[i + j * nCols]);
				auto it = mqsetSelectLUCCTransitionTypes.find(_LUCCPair.first);
				if (it != mqsetSelectLUCCTransitionTypes.end())
				{
					std::vector<int> _vTargetLuccTypes = it->second;
					std::unordered_set<int> targetLuccTypesSet(_vTargetLuccTypes.begin(), _vTargetLuccTypes.end());
					if (targetLuccTypesSet.find(it->first) != targetLuccTypesSet.end()) nAllCount++;
					if (targetLuccTypesSet.find(_LUCCPair.second) != targetLuccTypesSet.end()) _bAdd = true;
				}
				if (_bAdd)
					nCount++;
			}
		}
	}

	double dResults = 0.0;
	if (nAllCount > 0)
	{
		dResults = 100.0 * double(nCount) / double(nAllCount);
	}
	else
	{
		Logger::instance().warn("calculateLandConversionIndicator: nAllCount=0, no matching transition types");
		dResults = 0.0;
	}

	delete[] _pOriginalValue;
	delete[] _pChangedValue;

	double score;
	if (bState) score = normalization(dResults, dMinThreshold, dMaxThreshold);
	else score = normalizationNegative(dResults, dMinThreshold, dMaxThreshold);

	Logger::instance().result("calculateLandConversionIndicator", "conversion%", dResults);
	Logger::instance().result("calculateLandConversionIndicator", "score", score);
	Logger::instance().info("calculateLandConversionIndicator: Completed.");
	return score;
}

double CalculateSDG::calculateBufferZoneIndicator(
	std::string qstrInputData,
	std::string qstrBufferZoneData,
	std::unordered_set<int> mqsetSelectLUCCTypes,
	double dMaxThreshold,
	double dMinThreshold)
{
	Logger::instance().info("calculateBufferZoneIndicator: Starting...");

	// 检查输入路径 / Validate input paths
	if (qstrInputData.empty() || qstrBufferZoneData.empty())
	{
		Logger::instance().error("calculateBufferZoneIndicator: Input path is empty");
		return 0.0;
	}

	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	CPLSetConfigOption("SHAPE_ENCODING", "");
	GDALDataset *poDS = (GDALDataset *)GDALOpen(qstrInputData.c_str(), GA_ReadOnly);
	if (poDS == NULL)
	{
		Logger::instance().error("calculateBufferZoneIndicator: Cannot open input data: " + qstrInputData);
		return 0.0;
	}

	GDALDataType dataType = poDS->GetRasterBand(1)->GetRasterDataType();
	if (dataType != GDT_Byte && dataType != GDT_Float32 && dataType != GDT_Float64)
	{
		Logger::instance().error("calculateBufferZoneIndicator: Unsupported data type");
		GDALClose(poDS);
		return 0.0;
	}

	int nCols = poDS->GetRasterXSize();
	int nRows = poDS->GetRasterYSize();
	double dOriSum = 0.0;
	double dBufferCoverSum = 0.0;

	if (dataType == GDT_Byte)
	{
		unsigned char *_pOriValue = new unsigned char[nCols * nRows];
		unsigned char *_pBufferValue = new unsigned char[nCols * nRows];

		poDS->RasterIO(GF_Read, 0, 0, nCols, nRows, _pOriValue, nCols, nRows, GDT_Byte, 1, 0, 0, 0, 0);
		double dNodata = poDS->GetRasterBand(1)->GetNoDataValue();
		GDALDataset *poBufferDS = (GDALDataset*)GDALOpen(qstrBufferZoneData.c_str(), GA_ReadOnly);
		if (poBufferDS == NULL)
		{
			Logger::instance().error("calculateBufferZoneIndicator: Cannot open buffer data");
			GDALClose(poDS);
			delete[] _pOriValue;
			delete[] _pBufferValue;
			return 0.0;
		}

		int nLayers = poBufferDS->GetRasterCount();
		if (nLayers <= 0)
		{
			Logger::instance().warn("calculateBufferZoneIndicator: Buffer data has no raster bands");
			GDALClose(poDS);
			GDALClose(poBufferDS);
			delete[] _pOriValue;
			delete[] _pBufferValue;
			return 0.0;
		}

		poBufferDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, nCols, nRows, _pBufferValue, nCols, nRows, GDT_Byte, 0, 0);
		double _dBufferNoData = poBufferDS->GetRasterBand(1)->GetNoDataValue();
		for (int i = 0; i < nCols; i++)
		{
			for (int j = 0; j < nRows; j++)
			{
				if (_pOriValue[i + j * nCols] == dNodata) continue;
				bool _bCalculate = mqsetSelectLUCCTypes.find(_pOriValue[i + j * nCols]) != mqsetSelectLUCCTypes.end();
				if (!_bCalculate) continue;
				dOriSum++;
				if (_pBufferValue[i + j * nCols] != _dBufferNoData)
				{
					dBufferCoverSum++;
				}
			}
		}
		delete[] _pOriValue;
		delete[] _pBufferValue;
		GDALClose(poBufferDS);
	}
	else if (dataType == GDT_Float32)
	{
		float* _pOriValueFloat = new float[nCols * nRows];
		unsigned char* _pBufferValue = new unsigned char[nCols * nRows];
		poDS->RasterIO(GF_Read, 0, 0, nCols, nRows, _pOriValueFloat, nCols, nRows, GDT_Float32, 1, 0, 0, 0, 0);
		double dNodata = poDS->GetRasterBand(1)->GetNoDataValue();
		GDALDataset *poBufferDS = (GDALDataset*)GDALOpen(qstrBufferZoneData.c_str(), GA_ReadOnly);
		if (poBufferDS == NULL)
		{
			Logger::instance().error("calculateBufferZoneIndicator: Cannot open buffer data (Float32)");
			GDALClose(poDS);
			delete[] _pOriValueFloat;
			delete[] _pBufferValue;
			return 0.0;
		}

		int nLayers = poBufferDS->GetRasterCount();
		if (nLayers <= 0)
		{
			GDALClose(poDS);
			GDALClose(poBufferDS);
			delete[] _pOriValueFloat;
			delete[] _pBufferValue;
			return 0.0;
		}

		poBufferDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, nCols, nRows, _pBufferValue, nCols, nRows, GDT_Byte, 0, 0);
		double _dBufferNoData = poBufferDS->GetRasterBand(1)->GetNoDataValue();
		for (int i = 0; i < nCols; i++)
		{
			for (int j = 0; j < nRows; j++)
			{
				if (_pOriValueFloat[i + j * nCols] == dNodata) continue;
				dOriSum += _pOriValueFloat[i + j * nCols];
				if (_pBufferValue[i + j * nCols] != _dBufferNoData)
				{
					dBufferCoverSum += _pOriValueFloat[i + j * nCols];
				}
			}
		}
		delete[] _pOriValueFloat;
		delete[] _pBufferValue;
		GDALClose(poBufferDS);
	}
	else if (dataType == GDT_Float64)
	{
		double* _pOriValueFloat = new double[nCols * nRows];
		unsigned char* _pBufferValue = new unsigned char[nCols * nRows];
		poDS->RasterIO(GF_Read, 0, 0, nCols, nRows, _pOriValueFloat, nCols, nRows, GDT_Float64, 1, 0, 0, 0, 0);
		double dNodata = poDS->GetRasterBand(1)->GetNoDataValue();
		GDALDataset *poBufferDS = (GDALDataset*)GDALOpen(qstrBufferZoneData.c_str(), GA_ReadOnly);
		if (poBufferDS == NULL)
		{
			Logger::instance().error("calculateBufferZoneIndicator: Cannot open buffer data (Float64)");
			GDALClose(poDS);
			delete[] _pOriValueFloat;
			delete[] _pBufferValue;
			return 0.0;
		}

		int nLayers = poBufferDS->GetRasterCount();
		if (nLayers <= 0)
		{
			GDALClose(poDS);
			GDALClose(poBufferDS);
			delete[] _pOriValueFloat;
			delete[] _pBufferValue;
			return 0.0;
		}

		poBufferDS->GetRasterBand(1)->RasterIO(GF_Read, 0, 0, nCols, nRows, _pBufferValue, nCols, nRows, GDT_Byte, 0, 0);
		double _dBufferNoData = poBufferDS->GetRasterBand(1)->GetNoDataValue();
		for (int i = 0; i < nCols; i++)
		{
			for (int j = 0; j < nRows; j++)
			{
				if (_pOriValueFloat[i + j * nCols] == dNodata) continue;
				dOriSum += _pOriValueFloat[i + j * nCols];
				if (_pBufferValue[i + j * nCols] != _dBufferNoData)
				{
					dBufferCoverSum += _pOriValueFloat[i + j * nCols];
				}
			}
		}
		delete[] _pOriValueFloat;
		delete[] _pBufferValue;
		GDALClose(poBufferDS);
	}

	GDALClose(poDS);

	// 防止除零 / Prevent division-by-zero
	double dRatio = 0.0;
	if (fabs(dOriSum) > 1e-15)
	{
		dRatio = 100.0 * dBufferCoverSum / dOriSum;
	}
	else
	{
		Logger::instance().warn("calculateBufferZoneIndicator: dOriSum is 0, no valid data");
	}

	double score = normalization(dRatio, dMinThreshold, dMaxThreshold);
	Logger::instance().result("calculateBufferZoneIndicator", "coverage%", dRatio);
	Logger::instance().result("calculateBufferZoneIndicator", "score", score);
	Logger::instance().info("calculateBufferZoneIndicator: Completed.");
	return score;
}

double CalculateSDG::calculateSDG1131Indicator(
	std::string qstrInitialLUCCFileName,
	std::string qstrCurrentLUCCFileName,
	std::string qstrInitialPopulationFileName,
	std::string qstrCurrentPopulationFileName,
	std::unordered_set<int> vLUCCType,
	double dMaxThreshold,
	double dMinThreshold,
	double dBestThreshold)
{
	Logger::instance().info("calculateSDG1131Indicator: Starting...");

	// 检查输入路径 / Validate input paths
	if (qstrInitialLUCCFileName.empty() || qstrCurrentLUCCFileName.empty() ||
	    qstrInitialPopulationFileName.empty() || qstrCurrentPopulationFileName.empty())
	{
		Logger::instance().error("calculateSDG1131Indicator: Input path is empty");
		return 0.0;
	}

	// 检查阈值有效性 / Validate thresholds
	if (fabs(dBestThreshold - dMinThreshold) < 1e-12 || fabs(dBestThreshold - dMaxThreshold) < 1e-12)
	{
		Logger::instance().error("calculateSDG1131Indicator: best must not equal min or max");
		return 0.0;
	}

	std::vector<std::string> vLUCCs;
	std::vector<std::string> vPOPUs;
	vLUCCs.push_back(qstrInitialLUCCFileName);
	vLUCCs.push_back(qstrCurrentLUCCFileName);
	vPOPUs.push_back(qstrInitialPopulationFileName);
	vPOPUs.push_back(qstrCurrentPopulationFileName);

	std::vector<double> vUrbanSums = getUrbanSum(vLUCCs, vLUCCType);
	std::vector<double> vPOPUSums = getPopuSum(vPOPUs);

	if (vUrbanSums.size() < 2 || vPOPUSums.size() < 2)
	{
		Logger::instance().error("calculateSDG1131Indicator: Data read failed, cannot compute");
		return 0.0;
	}

	std::vector<double> vRatios;
	for (size_t i = 1; i < vLUCCs.size(); i++)
	{
		double _dRatioLUCC, _dRatioPOPU;

		// 防止城市用地增长率为0 / Prevent urban land growth rate division-by-zero
		if (fabs(vUrbanSums[i - 1]) < 1e-12)
		{
			Logger::instance().warn("calculateSDG1131Indicator: Initial urban land is 0");
			_dRatioLUCC = 0.0;
		}
		else
		{
			_dRatioLUCC = double(vUrbanSums[i] - vUrbanSums[i - 1]) / double(vUrbanSums[i - 1]);
		}

		// 防止人口增长率为0 / Prevent population growth rate division-by-zero
		if (fabs(vPOPUSums[i - 1]) < 1e-12)
		{
			Logger::instance().warn("calculateSDG1131Indicator: Initial population is 0");
			_dRatioPOPU = 0.0;
		}
		else
		{
			_dRatioPOPU = double(vPOPUSums[i] - vPOPUSums[i - 1]) / double(vPOPUSums[i - 1]);
		}

		// 防止人口增长率为0导致比值除零 / Prevent ratio division-by-zero
		if (fabs(_dRatioPOPU) < 1e-15)
		{
			Logger::instance().warn("calculateSDG1131Indicator: Population growth rate is 0, cannot compute ratio");
			vRatios.push_back(0.0);
		}
		else
		{
			double _dTmpSDGScore = _dRatioLUCC / _dRatioPOPU;
			vRatios.push_back(_dTmpSDGScore);
		}
	}

	if (vRatios.empty())
	{
		Logger::instance().error("calculateSDG1131Indicator: No valid ratios");
		return 0.0;
	}

	auto calculateNeutralScore = [](double inputValue, double dMaxThreshold, double dMinThreshold, double dBestThreshold) -> double
	{
		if (inputValue < dMinThreshold || inputValue > dMaxThreshold)
		{
			return 0.0;
		}
		else if (inputValue < dBestThreshold)
		{
			double distance = std::abs(inputValue - dBestThreshold);
			double denom = dBestThreshold - dMinThreshold;
			if (fabs(denom) < 1e-12) return 0.0;
			double score = 100 * (1 - (distance / denom));
			return std::max(0.0, score);
		}
		else
		{
			double distance = std::abs(inputValue - dBestThreshold);
			double denom = dMaxThreshold - dBestThreshold;
			if (fabs(denom) < 1e-12) return 0.0;
			double score = 100 * (1 - (distance / denom));
			return std::max(0.0, score);
		}
	};

	double score = calculateNeutralScore(vRatios[0], dMaxThreshold, dMinThreshold, dBestThreshold);
	Logger::instance().result("calculateSDG1131Indicator", "ratio", vRatios[0]);
	Logger::instance().result("calculateSDG1131Indicator", "score", score);
	Logger::instance().info("calculateSDG1131Indicator: Completed.");
	return score;
}

double CalculateSDG::calculateSDG1322Indicator(
	std::string qstrInputOriginal,
	std::string qstrInputChanged,
	std::unordered_map<int, double> vLUCCEmissionScheme,
	double dRatio)
{
	Logger::instance().info("calculateSDG1322Indicator: Starting...");

	// 检查输入路径 / Validate input paths
	if (qstrInputOriginal.empty() || qstrInputChanged.empty())
	{
		Logger::instance().error("calculateSDG1322Indicator: Input path is empty");
		return 0.0;
	}

	// 检查减排比例有效性 / Validate emission reduction ratio
	if (dRatio <= 0.0 || dRatio >= 1.0)
	{
		Logger::instance().warn("calculateSDG1322Indicator: Emission ratio should be in (0,1)");
	}

	// 保存原始排放系数，避免原地修改 / Save original emission factors to avoid in-place modification
	std::unordered_map<int, double> originalScheme = vLUCCEmissionScheme;

	double dEmissionOriginal = getEmissionSum(qstrInputOriginal, originalScheme);

	// 修改副本而非原map / Modify copy instead of original
	std::unordered_map<int, double> modifiedScheme = originalScheme;
	for (auto it = modifiedScheme.begin(); it != modifiedScheme.end(); it++)
	{
		if (it->second > 0)
		{
			it->second = it->second * (1 - dRatio);
		}
		else if (it->second < 0)
		{
			it->second = it->second * (1 + dRatio);
		}
	}
	double dEmissionChanged = getEmissionSum(qstrInputChanged, modifiedScheme);

	double dEmission = dEmissionChanged - dEmissionOriginal;
	if (dEmission <= 0)
	{
		Logger::instance().result("calculateSDG1322Indicator", "emission_change", dEmission);
		Logger::instance().result("calculateSDG1322Indicator", "score", 100.0);
		Logger::instance().info("calculateSDG1322Indicator: Emissions decreased, peaked.");
		return 100.0;
	}
	else
	{
		double denom = std::min(fabs(dEmissionOriginal), fabs(dEmissionChanged));
		if (denom < 1e-15)
		{
			Logger::instance().warn("calculateSDG1322Indicator: Normalization denominator near 0");
			return 0.0;
		}
		double score = normalization(dEmission, 0, denom);
		Logger::instance().result("calculateSDG1322Indicator", "emission_change", dEmission);
		Logger::instance().result("calculateSDG1322Indicator", "score", score);
		Logger::instance().info("calculateSDG1322Indicator: Completed.");
		return score;
	}
}

double CalculateSDG::normalization(double dValue, double dMin, double dMax)
{
	double dResult = 0.0;
	if (fabs(dMax - dMin) < 1e-15)
	{
		// 阈值相同时防止除零 / Prevent division-by-zero when thresholds are equal
		return (dValue >= dMax) ? 100.0 : 0.0;
	}
	if (dValue <= dMin) return 0.0;
	if (dValue >= dMax) return 100.0;
	dResult = (abs(dValue - dMin) / abs(dMax - dMin)) * 100.0;
	return dResult;
}

double CalculateSDG::normalizationNegative(double dValue, double dMin, double dMax)
{
	double dResult = 0.0;
	if (fabs(dMax - dMin) < 1e-15)
	{
		return (dValue <= dMin) ? 100.0 : 0.0;
	}
	if (dValue <= dMin) return 100.0;
	if (dValue >= dMax) return 0.0;
	dResult = ((dMax - dValue) / (dMax - dMin)) * 100.0;
	return dResult;
}

std::vector<double> CalculateSDG::getPopuSum(std::vector<std::string> vPopuFileNames)
{
	std::vector<double> vResults;
	for (size_t i = 0; i < vPopuFileNames.size(); i++)
	{
		GDALAllRegister();
		CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
		CPLSetConfigOption("SHAPE_ENCODING", "");

		if (vPopuFileNames[i].empty())
		{
			Logger::instance().warn("getPopuSum: Empty file path at index " + std::to_string(i));
			continue;
		}

		GDALDataset *poDS = (GDALDataset *)GDALOpen(vPopuFileNames[i].c_str(), GA_ReadOnly);
		if (poDS == NULL)
		{
			Logger::instance().warn("getPopuSum: Cannot open: " + vPopuFileNames[i]);
			continue;
		}
		if (poDS->GetRasterBand(1)->GetRasterDataType() != GDT_Float32)
		{
			Logger::instance().warn("getPopuSum: Data not GDT_Float32: " + vPopuFileNames[i]);
			GDALClose(poDS);
			continue;
		}
		int nCols = poDS->GetRasterXSize();
		int nRows = poDS->GetRasterYSize();
		float *_pOriValue = new float[nCols * nRows];
		poDS->RasterIO(GF_Read, 0, 0, nCols, nRows, _pOriValue, nCols, nRows, GDT_Float32, 1, 0, 0, 0, 0);
		double dNodata = poDS->GetRasterBand(1)->GetNoDataValue();
		double _dResult = 0.0;
		for (int j = 0; j < nCols * nRows; j++)
		{
			if (_pOriValue[j] == dNodata) continue;
			_dResult = _dResult + _pOriValue[j];
		}
		vResults.push_back(_dResult);
		GDALClose(poDS);
		delete[] _pOriValue;
	}
	return vResults;
}

std::vector<double> CalculateSDG::getUrbanSum(std::vector<std::string> vLUCCFileNames, std::unordered_set<int> mqsetSelectLUCCTypes)
{
	std::vector<double> vResults;
	for (size_t i = 0; i < vLUCCFileNames.size(); i++)
	{
		GDALAllRegister();
		CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
		CPLSetConfigOption("SHAPE_ENCODING", "");

		if (vLUCCFileNames[i].empty())
		{
			Logger::instance().warn("getUrbanSum: Empty file path at index " + std::to_string(i));
			continue;
		}

		GDALDataset *poDS = (GDALDataset *)GDALOpen(vLUCCFileNames[i].c_str(), GA_ReadOnly);
		if (poDS == NULL)
		{
			Logger::instance().warn("getUrbanSum: Cannot open: " + vLUCCFileNames[i]);
			continue;
		}
		if (poDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
		{
			Logger::instance().warn("getUrbanSum: Data not GDT_Byte: " + vLUCCFileNames[i]);
			GDALClose(poDS);
			continue;
		}
		int nCols = poDS->GetRasterXSize();
		int nRows = poDS->GetRasterYSize();
		unsigned char *_pOriValue = new unsigned char[nCols * nRows];
		poDS->RasterIO(GF_Read, 0, 0, nCols, nRows, _pOriValue, nCols, nRows, GDT_Byte, 1, 0, 0, 0, 0);
		double dNodata = poDS->GetRasterBand(1)->GetNoDataValue();
		double _dResult = 0.0;
		for (int j = 0; j < nCols * nRows; j++)
		{
			if (_pOriValue[j] == dNodata) continue;
			bool _bAdd = mqsetSelectLUCCTypes.find(_pOriValue[j]) != mqsetSelectLUCCTypes.end();
			if (_bAdd) _dResult++;
		}
		vResults.push_back(_dResult);
		GDALClose(poDS);
		delete[] _pOriValue;
	}
	return vResults;
}

double CalculateSDG::getEmissionSum(std::string qstrInputData, std::unordered_map<int, double> vLUCCEmissionScheme)
{
	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	CPLSetConfigOption("SHAPE_ENCODING", "");
	GDALDataset *poDS = (GDALDataset *)GDALOpen(qstrInputData.c_str(), GA_ReadOnly);
	if (poDS == NULL)
	{
		Logger::instance().warn("getEmissionSum: Cannot open: " + qstrInputData);
		return 0.0;
	}
	if (poDS->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
	{
		Logger::instance().warn("getEmissionSum: Data not GDT_Byte: " + qstrInputData);
		GDALClose(poDS);
		return 0.0;
	}
	int nCols = poDS->GetRasterXSize();
	int nRows = poDS->GetRasterYSize();
	unsigned char *_pOriValue = new unsigned char[nCols * nRows];
	poDS->RasterIO(GF_Read, 0, 0, nCols, nRows, _pOriValue, nCols, nRows, GDT_Byte, 1, 0, 0, 0, 0);
	double dNodata = poDS->GetRasterBand(1)->GetNoDataValue();
	double _dEmission = 0;
	for (int i = 0; i < nCols * nRows; i++)
	{
		if (_pOriValue[i] == dNodata) continue;
		double _dValue = 0;
		auto it = vLUCCEmissionScheme.find(_pOriValue[i]);
		if (it != vLUCCEmissionScheme.end())
		{
			_dValue = it->second;
		}
		_dEmission = _dEmission + _dValue;
	}
	GDALClose(poDS);
	delete[] _pOriValue;
	return _dEmission;
}
