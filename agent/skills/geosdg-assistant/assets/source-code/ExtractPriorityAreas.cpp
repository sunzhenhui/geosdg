#include "ExtractPriorityAreas.h"
#include "Logger.h"
#include <iostream>
#include <filesystem>
#include "ogrsf_frmts.h"
#include "gdal_priv.h"
#include "ogr_geometry.h"
#include "ogr_srs_api.h"
#include "ogr_feature.h"
#include "ogr_spatialref.h"

namespace fs = std::filesystem;
using namespace std;

ExtractPriorityAreas::ExtractPriorityAreas() {}
ExtractPriorityAreas::~ExtractPriorityAreas() {}

// Helper: get projection string from dataset, handling empty case
static std::string getProjectionAndFixTransform(GDALDataset *poDS, double padfTransform0[6], int nRows)
{
	const char* proj = poDS->GetProjectionRef();
	std::string projStr;
	if (proj && proj[0] != '\0') {
		projStr = proj;
	} else {
		padfTransform0[3] = -(nRows * padfTransform0[5]);
	}
	return projStr;
}

void ExtractPriorityAreas::PriorityAreasExtractLUCCLoss(
	std::string qstrInputOriginal, std::string qstrInputChanged,
	std::string qstrOutputFileName, std::unordered_set<int> mqsetSelectLUCCTypes)
{
	Logger::instance().info("PriorityAreasExtractLUCCLoss: Starting...");

	if (qstrInputOriginal.empty() || qstrInputChanged.empty() || qstrOutputFileName.empty())
	{
		Logger::instance().error("PriorityAreasExtractLUCCLoss: Input path is empty");
		return;
	}

	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");
	CPLSetConfigOption("SHAPE_ENCODING", "");

	GDALDataset *poOrig = (GDALDataset *)GDALOpen(qstrInputOriginal.c_str(), GA_ReadOnly);
	if (!poOrig)
	{
		Logger::instance().error("PriorityAreasExtractLUCCLoss: Cannot open original data: " + qstrInputOriginal);
		return;
	}
	if (poOrig->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
	{
		Logger::instance().error("PriorityAreasExtractLUCCLoss: Original data not GDT_Byte");
		GDALClose(poOrig);
		return;
	}

	GDALDataset *poChg = (GDALDataset *)GDALOpen(qstrInputChanged.c_str(), GA_ReadOnly);
	if (!poChg)
	{
		Logger::instance().error("PriorityAreasExtractLUCCLoss: Cannot open changed data: " + qstrInputChanged);
		GDALClose(poOrig);
		return;
	}
	if (poChg->GetRasterBand(1)->GetRasterDataType() != GDT_Byte)
	{
		Logger::instance().error("PriorityAreasExtractLUCCLoss: Changed data not GDT_Byte");
		GDALClose(poOrig); GDALClose(poChg);
		return;
	}

	int nC = poOrig->GetRasterXSize(), nR = poOrig->GetRasterYSize();
	if (nC != poChg->GetRasterXSize() || nR != poChg->GetRasterYSize())
	{
		Logger::instance().error("PriorityAreasExtractLUCCLoss: Dimension mismatch between original and changed data");
		GDALClose(poOrig); GDALClose(poChg);
		return;
	}

	unsigned char *pOri = new unsigned char[nC*nR], *pChg = new unsigned char[nC*nR], *pOut = new unsigned char[nC*nR];
	poOrig->RasterIO(GF_Read,0,0,nC,nR,pOri,nC,nR,GDT_Byte,1,0,0,0,0);
	poChg->RasterIO(GF_Read,0,0,nC,nR,pChg,nC,nR,GDT_Byte,1,0,0,0,0);
	double ndOri = poOrig->GetRasterBand(1)->GetNoDataValue(), ndChg = poChg->GetRasterBand(1)->GetNoDataValue();

	GDALDriver *pDrv = GetGDALDriverManager()->GetDriverByName("GTIFF");
	if (!pDrv)
	{
		Logger::instance().error("PriorityAreasExtractLUCCLoss: Cannot get GTiff driver");
		GDALClose(poOrig); GDALClose(poChg);
		delete[] pOri; delete[] pChg; delete[] pOut;
		return;
	}
	char **opts = nullptr;
	opts = CSLSetNameValue(opts,"BIGTIFF","IF_NEEDED"); opts = CSLSetNameValue(opts,"COMPRESS","DEFLATE");
	opts = CSLSetNameValue(opts,"PREDICTOR","2"); opts = CSLSetNameValue(opts,"ZLEVEL","9");
	GDALDataset *dst = pDrv->Create(qstrOutputFileName.c_str(), nC, nR, 1, GDT_Byte, opts);
	if (!dst)
	{
		Logger::instance().error("PriorityAreasExtractLUCCLoss: Cannot create output file: " + qstrOutputFileName);
		GDALClose(poOrig); GDALClose(poChg);
		delete[] pOri; delete[] pChg; delete[] pOut;
		return;
	}

	double gt[6];
	if (poOrig->GetGeoTransform(gt)==CE_Failure)
	{
		Logger::instance().error("PriorityAreasExtractLUCCLoss: Cannot get GeoTransform from original data");
		GDALClose(poOrig); GDALClose(poChg); GDALClose(dst);
		delete[] pOri; delete[] pChg; delete[] pOut;
		return;
	}
	std::string projStr = getProjectionAndFixTransform(poOrig, gt, nR);
	dst->SetGeoTransform(gt); dst->SetProjection(projStr.c_str());

	for (int i=0;i<nC*nR;i++) {
		pOut[i] = ndOri;
		if (pOri[i]==ndOri||pChg[i]==ndChg) continue;
		if (pOri[i]!=pChg[i] && mqsetSelectLUCCTypes.find(pOri[i])!=mqsetSelectLUCCTypes.end()) pOut[i]=1;
	}
	dst->GetRasterBand(1)->RasterIO(GF_Write,0,0,nC,nR,pOut,nC,nR,GDT_Byte,0,0);
	dst->GetRasterBand(1)->SetNoDataValue(ndOri); dst->GetRasterBand(1)->FlushCache();
	GDALClose(poChg); GDALClose(poOrig); GDALClose(dst);
	delete[]pOri; delete[]pOut; delete[]pChg;

	Logger::instance().info("PriorityAreasExtractLUCCLoss: Completed.");
}

void ExtractPriorityAreas::PriorityAreasExtractLUCCTransition(
	std::string qstrInputOriginal, std::string qstrInputChanged,
	std::string qstrOutputFileName, std::unordered_map<int,std::vector<int>> mqsetSelectLUCCTransitionTypes)
{
	Logger::instance().info("PriorityAreasExtractLUCCTransition: Starting...");

	if (qstrInputOriginal.empty() || qstrInputChanged.empty() || qstrOutputFileName.empty())
	{
		Logger::instance().error("PriorityAreasExtractLUCCTransition: Input path is empty");
		return;
	}

	GDALAllRegister();
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO"); CPLSetConfigOption("SHAPE_ENCODING","");

	GDALDataset *poOrig = (GDALDataset*)GDALOpen(qstrInputOriginal.c_str(), GA_ReadOnly);
	if (!poOrig)
	{
		Logger::instance().error("PriorityAreasExtractLUCCTransition: Cannot open original data: " + qstrInputOriginal);
		return;
	}
	if (poOrig->GetRasterBand(1)->GetRasterDataType()!=GDT_Byte)
	{
		Logger::instance().error("PriorityAreasExtractLUCCTransition: Original data not GDT_Byte");
		GDALClose(poOrig);
		return;
	}
	GDALDataset *poChg = (GDALDataset*)GDALOpen(qstrInputChanged.c_str(), GA_ReadOnly);
	if (!poChg)
	{
		Logger::instance().error("PriorityAreasExtractLUCCTransition: Cannot open changed data: " + qstrInputChanged);
		GDALClose(poOrig);
		return;
	}
	if (poChg->GetRasterBand(1)->GetRasterDataType()!=GDT_Byte)
	{
		Logger::instance().error("PriorityAreasExtractLUCCTransition: Changed data not GDT_Byte");
		GDALClose(poOrig); GDALClose(poChg);
		return;
	}

	int nC=poOrig->GetRasterXSize(), nR=poOrig->GetRasterYSize();
	if (nC!=poChg->GetRasterXSize()||nR!=poChg->GetRasterYSize())
	{
		Logger::instance().error("PriorityAreasExtractLUCCTransition: Dimension mismatch between original and changed data");
		GDALClose(poOrig); GDALClose(poChg);
		return;
	}

	unsigned char *pOri=new unsigned char[nC*nR], *pChg=new unsigned char[nC*nR], *pOut=new unsigned char[nC*nR];
	poOrig->RasterIO(GF_Read,0,0,nC,nR,pOri,nC,nR,GDT_Byte,1,0,0,0,0);
	poChg->RasterIO(GF_Read,0,0,nC,nR,pChg,nC,nR,GDT_Byte,1,0,0,0,0);
	double ndOri=poOrig->GetRasterBand(1)->GetNoDataValue(), ndChg=poChg->GetRasterBand(1)->GetNoDataValue();

	GDALDriver *pDrv=GetGDALDriverManager()->GetDriverByName("GTIFF");
	if (!pDrv)
	{
		Logger::instance().error("PriorityAreasExtractLUCCTransition: Cannot get GTiff driver");
		GDALClose(poOrig); GDALClose(poChg);
		delete[] pOri; delete[] pChg; delete[] pOut;
		return;
	}
	char **opts=nullptr;
	opts=CSLSetNameValue(opts,"BIGTIFF","IF_NEEDED"); opts=CSLSetNameValue(opts,"COMPRESS","DEFLATE");
	opts=CSLSetNameValue(opts,"PREDICTOR","2"); opts=CSLSetNameValue(opts,"ZLEVEL","9");
	GDALDataset *dst=pDrv->Create(qstrOutputFileName.c_str(),nC,nR,1,GDT_Byte,opts);
	if (!dst)
	{
		Logger::instance().error("PriorityAreasExtractLUCCTransition: Cannot create output file: " + qstrOutputFileName);
		GDALClose(poOrig); GDALClose(poChg);
		delete[] pOri; delete[] pChg; delete[] pOut;
		return;
	}

	double gt[6];
	if (poOrig->GetGeoTransform(gt)==CE_Failure)
	{
		Logger::instance().error("PriorityAreasExtractLUCCTransition: Cannot get GeoTransform from original data");
		GDALClose(poOrig); GDALClose(poChg); GDALClose(dst);
		delete[] pOri; delete[] pChg; delete[] pOut;
		return;
	}
	std::string projStr=getProjectionAndFixTransform(poOrig,gt,nR);
	dst->SetGeoTransform(gt); dst->SetProjection(projStr.c_str());

	for (int i=0;i<nC;i++) for (int j=0;j<nR;j++) {
		int idx=i+j*nC; pOut[idx]=ndOri;
		if (pOri[idx]==ndOri||pChg[idx]==ndChg) continue;
		if (pOri[idx]!=pChg[idx]) {
			bool bAdd=false;
			auto it=mqsetSelectLUCCTransitionTypes.find(pOri[idx]);
			if (it!=mqsetSelectLUCCTransitionTypes.end()) {
				std::unordered_set<int> tgt(it->second.begin(),it->second.end());
				if (tgt.find(pChg[idx])!=tgt.end()) bAdd=true;
			}
			if (bAdd) pOut[idx]=1;
		}
	}
	dst->GetRasterBand(1)->RasterIO(GF_Write,0,0,nC,nR,pOut,nC,nR,GDT_Byte,0,0);
	dst->GetRasterBand(1)->SetNoDataValue(ndOri); dst->GetRasterBand(1)->FlushCache();
	GDALClose(poChg); GDALClose(poOrig); GDALClose(dst);
	delete[]pOri; delete[]pOut; delete[]pChg;

	Logger::instance().info("PriorityAreasExtractLUCCTransition: Completed.");
}

void ExtractPriorityAreas::PriorityAreasExtractOutsideBufferArea(
	std::string qstrInputData, std::string qstrBufferZoneData,
	std::string qstrOutputFileName, std::unordered_set<int> mqsetSelectLUCCTypes, double dThresholdPopulation)
{
	Logger::instance().info("PriorityAreasExtractOutsideBufferArea: Starting...");

	if (qstrInputData.empty()||qstrBufferZoneData.empty()||qstrOutputFileName.empty())
	{
		Logger::instance().error("PriorityAreasExtractOutsideBufferArea: Input path is empty");
		return;
	}

	GDALAllRegister(); CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO"); CPLSetConfigOption("SHAPE_ENCODING","");
	GDALDataset *poDS=(GDALDataset*)GDALOpen(qstrInputData.c_str(),GA_ReadOnly);
	if (!poDS)
	{
		Logger::instance().error("PriorityAreasExtractOutsideBufferArea: Cannot open input data: " + qstrInputData);
		return;
	}
	GDALDataType dt=poDS->GetRasterBand(1)->GetRasterDataType();
	GDALClose(poDS);
	if (dt==GDT_Byte)
	{
		Logger::instance().info("PriorityAreasExtractOutsideBufferArea: Detected Byte data, extracting LUCC...");
		extractLUCC(qstrInputData,qstrBufferZoneData,qstrOutputFileName,mqsetSelectLUCCTypes);
	}
	else if (dt==GDT_Float32||dt==GDT_Float64)
	{
		Logger::instance().info("PriorityAreasExtractOutsideBufferArea: Detected Float data, extracting POPU...");
		extractPOPU(qstrInputData,qstrBufferZoneData,qstrOutputFileName,dThresholdPopulation);
	}
	else
	{
		Logger::instance().error("PriorityAreasExtractOutsideBufferArea: Unsupported data type");
		return;
	}

	Logger::instance().info("PriorityAreasExtractOutsideBufferArea: Completed.");
}

void ExtractPriorityAreas::PriorityAreasExtractEmissionNoPeak(
	std::string qstrInputOriginal, std::string qstrInputChanged,
	std::string qstrOutputFileName, std::unordered_map<int,double> vLUCCEmissionScheme, double dRatio, double nNeighborhood)
{
	Logger::instance().info("PriorityAreasExtractEmissionNoPeak: Starting...");

	if (qstrInputOriginal.empty()||qstrInputChanged.empty()||qstrOutputFileName.empty())
	{
		Logger::instance().error("PriorityAreasExtractEmissionNoPeak: Input path is empty");
		return;
	}

	if (dRatio <= 0.0 || dRatio >= 1.0)
	{
		Logger::instance().warn("PriorityAreasExtractEmissionNoPeak: Emission ratio should be in (0,1)");
	}

	try
	{
		calculatePrefixEmisiion(qstrInputOriginal,"../tmp/OriginalPrefix.tif",vLUCCEmissionScheme);
		for (auto &it:vLUCCEmissionScheme) { if(it.second>0) it.second*=(1-dRatio); else if(it.second<0) it.second*=(1+dRatio); }
		calculatePrefixEmisiion(qstrInputChanged,"../tmp/ChangedPrefix.tif",vLUCCEmissionScheme);
		extractEmissionIncreaseLand("../tmp/OriginalPrefix.tif","../tmp/ChangedPrefix.tif","../tmp/Increase.tif",nNeighborhood);
		removeNoDataFromSecondRaster(qstrInputOriginal,"../tmp/Increase.tif",qstrOutputFileName);
		for (auto &f : {"../tmp/OriginalPrefix.tif","../tmp/ChangedPrefix.tif","../tmp/Increase.tif"})
		{
			std::error_code ec;
			fs::remove(f, ec);
			if (ec)
			{
				Logger::instance().warn("PriorityAreasExtractEmissionNoPeak: Failed to remove temp file: " + std::string(f));
			}
		}
	}
	catch (const std::exception& e)
	{
		Logger::instance().error("PriorityAreasExtractEmissionNoPeak: Exception: " + std::string(e.what()));
		return;
	}

	Logger::instance().info("PriorityAreasExtractEmissionNoPeak: Completed.");
}

void ExtractPriorityAreas::PriorityAreasExtractHumanLandRelationship(
	std::string qstrInitialLUCCFileName, std::string qstrCurrentLUCCFileName,
	std::string qstrInitialPopulationFileName, std::string qstrCurrentPopulationFileName,
	int nNeighborhoodRadius, std::string qstrOutputFileName, std::unordered_set<int> vLUCCType)
{
	Logger::instance().info("PriorityAreasExtractHumanLandRelationship: Starting...");

	if (qstrInitialLUCCFileName.empty()||qstrCurrentLUCCFileName.empty()||
	    qstrInitialPopulationFileName.empty()||qstrCurrentPopulationFileName.empty()||
	    qstrOutputFileName.empty())
	{
		Logger::instance().error("PriorityAreasExtractHumanLandRelationship: Input path is empty");
		return;
	}

	GDALAllRegister();
	GDALDataset *preL=(GDALDataset*)GDALOpen(qstrInitialLUCCFileName.c_str(),GA_ReadOnly);
	GDALDataset *nowL=(GDALDataset*)GDALOpen(qstrCurrentLUCCFileName.c_str(),GA_ReadOnly);
	GDALDataset *preP=(GDALDataset*)GDALOpen(qstrInitialPopulationFileName.c_str(),GA_ReadOnly);
	GDALDataset *nowP=(GDALDataset*)GDALOpen(qstrCurrentPopulationFileName.c_str(),GA_ReadOnly);
	if (!preL||!nowL||!preP||!nowP)
	{
		Logger::instance().error("PriorityAreasExtractHumanLandRelationship: Cannot open one or more input datasets");
		if(preL)GDALClose(preL); if(nowL)GDALClose(nowL); if(preP)GDALClose(preP); if(nowP)GDALClose(nowP);
		return;
	}

	int cols=preL->GetRasterXSize(), rows=preL->GetRasterYSize();
	if (cols<=0||rows<=0)
	{
		Logger::instance().error("PriorityAreasExtractHumanLandRelationship: Invalid raster dimensions");
		GDALClose(preL); GDALClose(nowL); GDALClose(preP); GDALClose(nowP);
		return;
	}

	GDALDriver *drv=GetGDALDriverManager()->GetDriverByName("GTiff");
	if (!drv)
	{
		Logger::instance().error("PriorityAreasExtractHumanLandRelationship: Cannot get GTiff driver");
		GDALClose(preL); GDALClose(nowL); GDALClose(preP); GDALClose(nowP);
		return;
	}
	GDALDataset *outDS=drv->Create(qstrOutputFileName.c_str(),cols,rows,1,GDT_Byte,nullptr);
	if (!outDS)
	{
		Logger::instance().error("PriorityAreasExtractHumanLandRelationship: Cannot create output file: " + qstrOutputFileName);
		GDALClose(preL); GDALClose(nowL); GDALClose(preP); GDALClose(nowP);
		return;
	}

	double gt[6];
	if(preL->GetGeoTransform(gt)==CE_Failure)
	{
		Logger::instance().error("PriorityAreasExtractHumanLandRelationship: Cannot get GeoTransform from initial LUCC data");
		GDALClose(preL);GDALClose(nowL);GDALClose(preP);GDALClose(nowP);GDALClose(outDS);
		return;
	}
	std::string projStr=getProjectionAndFixTransform(preL,gt,rows);
	outDS->SetGeoTransform(gt); outDS->SetProjection(projStr.c_str()); outDS->GetRasterBand(1)->SetNoDataValue(0);

	std::vector<unsigned char> preLD(cols*rows),nowLD(cols*rows);
	std::vector<float> prePD(cols*rows),nowPD(cols*rows);
	preL->GetRasterBand(1)->RasterIO(GF_Read,0,0,cols,rows,preLD.data(),cols,rows,GDT_Byte,0,0);
	nowL->GetRasterBand(1)->RasterIO(GF_Read,0,0,cols,rows,nowLD.data(),cols,rows,GDT_Byte,0,0);
	preP->GetRasterBand(1)->RasterIO(GF_Read,0,0,cols,rows,prePD.data(),cols,rows,GDT_Float32,0,0);
	nowP->GetRasterBand(1)->RasterIO(GF_Read,0,0,cols,rows,nowPD.data(),cols,rows,GDT_Float32,0,0);

	std::vector<std::pair<int,int>> offsets;
	for(int dy=-nNeighborhoodRadius;dy<=nNeighborhoodRadius;++dy) for(int dx=-nNeighborhoodRadius;dx<=nNeighborhoodRadius;++dx)
		if(dx||dy) offsets.emplace_back(dx,dy);

	double ndL=preL->GetRasterBand(1)->GetNoDataValue(), ndP=nowP->GetRasterBand(1)->GetNoDataValue();
	std::vector<uint8_t> outD(cols*rows,0);
	for(int y=0;y<rows;++y) for(int x=0;x<cols;++x) {
		int ci=y*cols+x;
		if(preLD[ci]==ndL||nowLD[ci]==ndL||nowPD[ci]==ndP||prePD[ci]==ndP) continue;
		double initU=0,curU=0,popG=0;
		for(auto&o:offsets){ int ny=y+o.second,nx=x+o.first;
			if(ny>=0&&ny<rows&&nx>=0&&nx<cols){ int ni=ny*cols+nx;
				if(preLD[ni]==ndL||nowLD[ni]==ndL) continue;
				if(vLUCCType.find(preLD[ni])!=vLUCCType.end()) initU++;
				if(vLUCCType.find(nowLD[ni])!=vLUCCType.end()) curU++;
				if(nowPD[ni]==ndP||prePD[ni]==ndP) continue;
				popG+=nowPD[ni]-prePD[ni];
			}}
		bool uInc=initU<=curU; bool pInc=popG>0;
		if((pInc&&!uInc)||(!pInc&&uInc)) outD[ci]=1;
	}
	outDS->GetRasterBand(1)->RasterIO(GF_Write,0,0,cols,rows,outD.data(),cols,rows,GDT_Byte,0,0);
	outDS->GetRasterBand(1)->SetNoDataValue(0); outDS->FlushCache();
	GDALClose(preL); GDALClose(nowL); GDALClose(preP); GDALClose(nowP); GDALClose(outDS);

	Logger::instance().info("PriorityAreasExtractHumanLandRelationship: Completed.");
}

void ExtractPriorityAreas::generatePriorityAreas(std::vector<std::string> vLUCCFileNames, std::string qstrOutputFileName)
{
	Logger::instance().info("generatePriorityAreas: Starting...");

	if (vLUCCFileNames.empty() || qstrOutputFileName.empty())
	{
		Logger::instance().error("generatePriorityAreas: Input file list or output path is empty");
		return;
	}

	GDALAllRegister(); CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO"); CPLSetConfigOption("SHAPE_ENCODING","");
	double gt[6]; std::string projStr; unsigned char*pDataDst=nullptr; int nC=0,nR=0; double ndDst=0;
	for(size_t i=0;i<vLUCCFileNames.size();i++) {
		if (vLUCCFileNames[i].empty())
		{
			Logger::instance().warn("generatePriorityAreas: Empty file path at index " + std::to_string(i));
			continue;
		}

		GDALDataset *poDS=(GDALDataset*)GDALOpen(vLUCCFileNames[i].c_str(),GA_ReadOnly);
		if(!poDS)
		{
			Logger::instance().error("generatePriorityAreas: Cannot open file: " + vLUCCFileNames[i]);
			delete[] pDataDst;
			return;
		}
		nC=poDS->GetRasterXSize(); nR=poDS->GetRasterYSize();
		if (nC<=0||nR<=0)
		{
			Logger::instance().error("generatePriorityAreas: Invalid raster dimensions in: " + vLUCCFileNames[i]);
			GDALClose(poDS);
			delete[] pDataDst;
			return;
		}
		if(poDS->GetGeoTransform(gt)==CE_Failure)
		{
			Logger::instance().error("generatePriorityAreas: Cannot get GeoTransform from: " + vLUCCFileNames[i]);
			GDALClose(poDS);
			delete[] pDataDst;
			return;
		}
		if(i==0){ pDataDst=new unsigned char[nC*nR]; memset(pDataDst,0,nC*nR);
			projStr=getProjectionAndFixTransform(poDS,gt,nR); }
		unsigned char*pData=new unsigned char[nC*nR];
		poDS->GetRasterBand(1)->RasterIO(GF_Read,0,0,nC,nR,pData,nC,nR,GDT_Byte,0,0);
		double nd=poDS->GetRasterBand(1)->GetNoDataValue(); ndDst=nd;
		for(int j=0;j<nC*nR;j++) if(pData[j]!=nd) pDataDst[j]++;
		delete[]pData; GDALClose(poDS);
	}

	if (!pDataDst)
	{
		Logger::instance().error("generatePriorityAreas: No valid data accumulated");
		return;
	}

	GDALDriver *pDrv=GetGDALDriverManager()->GetDriverByName("GTIFF");
	if (!pDrv)
	{
		Logger::instance().error("generatePriorityAreas: Cannot get GTiff driver");
		delete[] pDataDst;
		return;
	}
	char**opts=nullptr; opts=CSLSetNameValue(opts,"BIGTIFF","IF_NEEDED"); opts=CSLSetNameValue(opts,"COMPRESS","DEFLATE");
	opts=CSLSetNameValue(opts,"PREDICTOR","2"); opts=CSLSetNameValue(opts,"ZLEVEL","9");
	GDALDataset*dst=pDrv->Create(qstrOutputFileName.c_str(),nC,nR,1,GDT_Byte,opts);
	if (!dst)
	{
		Logger::instance().error("generatePriorityAreas: Cannot create output file: " + qstrOutputFileName);
		delete[] pDataDst;
		return;
	}
	dst->SetGeoTransform(gt); dst->SetProjection(projStr.c_str());
	dst->GetRasterBand(1)->RasterIO(GF_Write,0,0,nC,nR,pDataDst,nC,nR,GDT_Byte,0,0);
	dst->GetRasterBand(1)->SetNoDataValue(ndDst); dst->GetRasterBand(1)->FlushCache();
	delete[]pDataDst; GDALClose(dst);

	Logger::instance().info("generatePriorityAreas: Completed.");
}

void ExtractPriorityAreas::extractLUCC(std::string qstrLUCCPath, std::string qstrBufferPath, std::string qstrOutputPriorityAreasPath, std::unordered_set<int> vLUCCType)
{
	Logger::instance().info("extractLUCC: Starting...");

	if (qstrLUCCPath.empty()||qstrBufferPath.empty()||qstrOutputPriorityAreasPath.empty())
	{
		Logger::instance().error("extractLUCC: Input path is empty");
		return;
	}

	GDALAllRegister(); CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO"); CPLSetConfigOption("SHAPE_ENCODING","");
	GDALDataset *poDS=(GDALDataset*)GDALOpen(qstrLUCCPath.c_str(),GA_ReadOnly);
	GDALDataset *poBuf=(GDALDataset*)GDALOpen(qstrBufferPath.c_str(),GA_ReadOnly);
	if(!poDS||!poBuf)
	{
		Logger::instance().error("extractLUCC: Cannot open input or buffer data");
		if(poDS)GDALClose(poDS);if(poBuf)GDALClose(poBuf);
		return;
	}
	if(poDS->GetRasterBand(1)->GetRasterDataType()!=GDT_Byte||poBuf->GetRasterBand(1)->GetRasterDataType()!=GDT_Byte)
	{
		Logger::instance().error("extractLUCC: Data type not GDT_Byte");
		GDALClose(poDS);GDALClose(poBuf);
		return;
	}

	int nC=poDS->GetRasterXSize(),nR=poDS->GetRasterYSize();
	if (nC<=0||nR<=0)
	{
		Logger::instance().error("extractLUCC: Invalid raster dimensions");
		GDALClose(poDS); GDALClose(poBuf);
		return;
	}

	GDALDriver *pDrv=GetGDALDriverManager()->GetDriverByName("GTIFF");
	if (!pDrv)
	{
		Logger::instance().error("extractLUCC: Cannot get GTiff driver");
		GDALClose(poDS); GDALClose(poBuf);
		return;
	}
	char**opts=nullptr; opts=CSLSetNameValue(opts,"BIGTIFF","IF_NEEDED"); opts=CSLSetNameValue(opts,"COMPRESS","DEFLATE");
	opts=CSLSetNameValue(opts,"PREDICTOR","2"); opts=CSLSetNameValue(opts,"ZLEVEL","9");
	GDALDataset*dst=pDrv->Create(qstrOutputPriorityAreasPath.c_str(),nC,nR,1,GDT_Byte,opts);
	if (!dst)
	{
		Logger::instance().error("extractLUCC: Cannot create output file: " + qstrOutputPriorityAreasPath);
		GDALClose(poDS); GDALClose(poBuf);
		return;
	}

	double gt[6];
	if(poDS->GetGeoTransform(gt)==CE_Failure)
	{
		Logger::instance().error("extractLUCC: Cannot get GeoTransform from LUCC data");
		GDALClose(poDS);GDALClose(poBuf);GDALClose(dst);
		return;
	}
	std::string projStr=getProjectionAndFixTransform(poDS,gt,nR);
	dst->SetGeoTransform(gt); dst->SetProjection(projStr.c_str());

	double nd=poDS->GetRasterBand(1)->GetNoDataValue(), ndB=poBuf->GetRasterBand(1)->GetNoDataValue();
	unsigned char*pO=new unsigned char[nC*nR],*pB=new unsigned char[nC*nR],*pN=new unsigned char[nC*nR];
	poDS->RasterIO(GF_Read,0,0,nC,nR,pO,nC,nR,GDT_Byte,1,0,0,0,0);
	poBuf->RasterIO(GF_Read,0,0,nC,nR,pB,nC,nR,GDT_Byte,1,0,0,0,0);
	for(int i=0;i<nC;i++) for(int j=0;j<nR;j++){
		int idx=i+j*nC; pN[idx]=0;
		if(pO[idx]==nd) continue;
		if(pB[idx]==ndB && vLUCCType.find(pO[idx])!=vLUCCType.end()) pN[idx]=1;
	}
	dst->GetRasterBand(1)->RasterIO(GF_Write,0,0,nC,nR,pN,nC,nR,GDT_Byte,0,0);
	dst->GetRasterBand(1)->SetNoDataValue(0); dst->GetRasterBand(1)->FlushCache();
	GDALClose(poDS); GDALClose(poBuf); GDALClose(dst);
	delete[]pO; delete[]pN; delete[]pB;

	Logger::instance().info("extractLUCC: Completed.");
}

void ExtractPriorityAreas::extractPOPU(std::string qstrPOPUPath, std::string qstrBufferPath, std::string qstrOutputPriorityAreasPath, double dThresholdPopulation)
{
	Logger::instance().info("extractPOPU: Starting...");

	if (qstrPOPUPath.empty()||qstrBufferPath.empty()||qstrOutputPriorityAreasPath.empty())
	{
		Logger::instance().error("extractPOPU: Input path is empty");
		return;
	}

	GDALAllRegister(); CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO"); CPLSetConfigOption("SHAPE_ENCODING","");
	GDALDataset *poP=(GDALDataset*)GDALOpen(qstrPOPUPath.c_str(),GA_ReadOnly);
	GDALDataset *poB=(GDALDataset*)GDALOpen(qstrBufferPath.c_str(),GA_ReadOnly);
	if(!poP||!poB)
	{
		Logger::instance().error("extractPOPU: Cannot open population or buffer data");
		if(poP)GDALClose(poP);if(poB)GDALClose(poB);
		return;
	}
	GDALDataType pt=poP->GetRasterBand(1)->GetRasterDataType();
	if((pt!=GDT_Float32&&pt!=GDT_Float64)||poB->GetRasterBand(1)->GetRasterDataType()!=GDT_Byte)
	{
		Logger::instance().error("extractPOPU: Unsupported data type (population not Float32/Float64 or buffer not Byte)");
		GDALClose(poP);GDALClose(poB);
		return;
	}

	int nC=poP->GetRasterXSize(),nR=poP->GetRasterYSize();
	if (nC<=0||nR<=0)
	{
		Logger::instance().error("extractPOPU: Invalid raster dimensions");
		GDALClose(poP); GDALClose(poB);
		return;
	}

	GDALDriver *pDrv=GetGDALDriverManager()->GetDriverByName("GTIFF");
	if (!pDrv)
	{
		Logger::instance().error("extractPOPU: Cannot get GTiff driver");
		GDALClose(poP); GDALClose(poB);
		return;
	}
	char**opts=nullptr; opts=CSLSetNameValue(opts,"BIGTIFF","IF_NEEDED"); opts=CSLSetNameValue(opts,"COMPRESS","DEFLATE");
	opts=CSLSetNameValue(opts,"PREDICTOR","2"); opts=CSLSetNameValue(opts,"ZLEVEL","9");
	GDALDataset*dst=pDrv->Create(qstrOutputPriorityAreasPath.c_str(),nC,nR,1,GDT_Byte,opts);
	if (!dst)
	{
		Logger::instance().error("extractPOPU: Cannot create output file: " + qstrOutputPriorityAreasPath);
		GDALClose(poP); GDALClose(poB);
		return;
	}

	double gt[6];
	if(poP->GetGeoTransform(gt)==CE_Failure)
	{
		Logger::instance().error("extractPOPU: Cannot get GeoTransform from population data");
		GDALClose(poP);GDALClose(poB);GDALClose(dst);
		return;
	}
	const char*proj=poP->GetProjectionRef(); std::string ps; if(proj&&proj[0]!='\0') ps=proj;
	dst->SetGeoTransform(gt); dst->SetProjection(ps.c_str());

	double ndP=poP->GetRasterBand(1)->GetNoDataValue(), ndB=poB->GetRasterBand(1)->GetNoDataValue();
	unsigned char*pB=new unsigned char[nC*nR],*pN=new unsigned char[nC*nR];
	poB->RasterIO(GF_Read,0,0,nC,nR,pB,nC,nR,GDT_Byte,1,0,0,0,0);
	if(pt==GDT_Float32){
		float*pV=new float[nC*nR]; poP->RasterIO(GF_Read,0,0,nC,nR,pV,nC,nR,GDT_Float32,1,0,0,0,0);
		for(int i=0;i<nC;i++) for(int j=0;j<nR;j++){int idx=i+j*nC; pN[idx]=0;
			if(pV[idx]==ndP)continue; if(pB[idx]==ndB&&pV[idx]>=dThresholdPopulation) pN[idx]=1;}
		delete[]pV;
	} else {
		double*pV=new double[nC*nR]; poP->RasterIO(GF_Read,0,0,nC,nR,pV,nC,nR,GDT_Float64,1,0,0,0,0);
		for(int i=0;i<nC;i++) for(int j=0;j<nR;j++){int idx=i+j*nC; pN[idx]=0;
			if(pV[idx]==ndP)continue; if(pB[idx]==ndB&&pV[idx]>=dThresholdPopulation) pN[idx]=1;}
		delete[]pV;
	}
	dst->GetRasterBand(1)->RasterIO(GF_Write,0,0,nC,nR,pN,nC,nR,GDT_Byte,0,0);
	dst->GetRasterBand(1)->SetNoDataValue(0); dst->GetRasterBand(1)->FlushCache();
	GDALClose(poP); GDALClose(poB); GDALClose(dst);
	delete[]pN; delete[]pB;

	Logger::instance().info("extractPOPU: Completed.");
}

void ExtractPriorityAreas::calculatePrefixEmisiion(std::string qstrOriLUCCFileName, std::string qstrOutputFileName, std::unordered_map<int,double> vLUCCEmissionScheme)
{
	Logger::instance().info("calculatePrefixEmisiion: Starting...");

	if (qstrOriLUCCFileName.empty() || qstrOutputFileName.empty())
	{
		Logger::instance().error("calculatePrefixEmisiion: Input path is empty");
		return;
	}

	GDALAllRegister(); CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO"); CPLSetConfigOption("SHAPE_ENCODING","");
	GDALDataset *poDS=(GDALDataset*)GDALOpen(qstrOriLUCCFileName.c_str(),GA_ReadOnly);
	if(!poDS)
	{
		Logger::instance().error("calculatePrefixEmisiion: Cannot open LUCC data: " + qstrOriLUCCFileName);
		return;
	}
	if(poDS->GetRasterBand(1)->GetRasterDataType()!=GDT_Byte)
	{
		Logger::instance().error("calculatePrefixEmisiion: Data not GDT_Byte");
		GDALClose(poDS);
		return;
	}

	int nC=poDS->GetRasterXSize(),nR=poDS->GetRasterYSize();
	if (nC<=0||nR<=0)
	{
		Logger::instance().error("calculatePrefixEmisiion: Invalid raster dimensions");
		GDALClose(poDS);
		return;
	}

	unsigned char*pO=new unsigned char[nC*nR]; double*pN=new double[nC*nR];
	poDS->RasterIO(GF_Read,0,0,nC,nR,pO,nC,nR,GDT_Byte,1,0,0,0,0);
	GDALDriver *pDrv=GetGDALDriverManager()->GetDriverByName("GTIFF");
	if (!pDrv)
	{
		Logger::instance().error("calculatePrefixEmisiion: Cannot get GTiff driver");
		GDALClose(poDS); delete[] pO; delete[] pN;
		return;
	}
	char**opts=nullptr; opts=CSLSetNameValue(opts,"BIGTIFF","IF_NEEDED"); opts=CSLSetNameValue(opts,"COMPRESS","DEFLATE");
	opts=CSLSetNameValue(opts,"ZLEVEL","9");
	GDALDataset*dst=pDrv->Create(qstrOutputFileName.c_str(),nC,nR,1,GDT_Float64,opts);
	if (!dst)
	{
		Logger::instance().error("calculatePrefixEmisiion: Cannot create output file: " + qstrOutputFileName);
		GDALClose(poDS); delete[] pO; delete[] pN;
		return;
	}

	double gt[6];
	if(poDS->GetGeoTransform(gt)==CE_Failure)
	{
		Logger::instance().error("calculatePrefixEmisiion: Cannot get GeoTransform from LUCC data");
		GDALClose(poDS);GDALClose(dst);delete[]pO;delete[]pN;
		return;
	}
	std::string projStr=getProjectionAndFixTransform(poDS,gt,nR);
	dst->SetGeoTransform(gt); dst->SetProjection(projStr.c_str());

	double nd=poDS->GetRasterBand(1)->GetNoDataValue();
	for(int i=0;i<nC;i++) for(int j=0;j<nR;j++){
		try{ int idx=i+j*nC; pN[idx]=0;
			if(pO[idx]==nd) pO[idx]=0;
			double em=0; auto it=vLUCCEmissionScheme.find(pO[idx]); if(it!=vLUCCEmissionScheme.end()) em=it->second;
			if(i==0&&j==0) pN[0]=0;
			else if(i==0) pN[idx]=pN[i+(j-1)*nC]+pO[idx]*em;
			else if(j==0) pN[idx]=pN[(i-1)+j*nC]+pO[idx]*em;
			else pN[idx]=pN[i+(j-1)*nC]+pN[(i-1)+j*nC]-pN[(i-1)+(j-1)*nC]+pO[idx]*em;
		} catch(const std::exception& e){
			Logger::instance().error("calculatePrefixEmisiion: Exception at (" + std::to_string(i) + "," + std::to_string(j) + "): " + e.what());
			GDALClose(poDS); GDALClose(dst); delete[]pO; delete[]pN;
			return;
		}
	}
	dst->GetRasterBand(1)->RasterIO(GF_Write,0,0,nC,nR,pN,nC,nR,GDT_Float64,0,0);
	dst->GetRasterBand(1)->FlushCache(); GDALClose(poDS); GDALClose(dst);
	delete[]pO; delete[]pN;

	Logger::instance().info("calculatePrefixEmisiion: Completed.");
}

void ExtractPriorityAreas::extractEmissionIncreaseLand(std::string qstrInputOriginal, std::string qstrInputChanged, std::string qstrOutputFileName, int nNeighborhoodRadius)
{
	Logger::instance().info("extractEmissionIncreaseLand: Starting...");

	if (qstrInputOriginal.empty()||qstrInputChanged.empty()||qstrOutputFileName.empty())
	{
		Logger::instance().error("extractEmissionIncreaseLand: Input path is empty");
		return;
	}

	GDALAllRegister(); CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO"); CPLSetConfigOption("SHAPE_ENCODING","");
	GDALDataset *poDS=(GDALDataset*)GDALOpen(qstrInputOriginal.c_str(),GA_ReadOnly);
	GDALDataset *poDN=(GDALDataset*)GDALOpen(qstrInputChanged.c_str(),GA_ReadOnly);
	if(!poDS||!poDN)
	{
		Logger::instance().error("extractEmissionIncreaseLand: Cannot open original or changed data");
		if(poDS)GDALClose(poDS);if(poDN)GDALClose(poDN);
		return;
	}
	if(poDS->GetRasterBand(1)->GetRasterDataType()!=GDT_Float64||poDN->GetRasterBand(1)->GetRasterDataType()!=GDT_Float64)
	{
		Logger::instance().error("extractEmissionIncreaseLand: Data type not GDT_Float64");
		GDALClose(poDS);GDALClose(poDN);
		return;
	}

	int nC=poDS->GetRasterXSize(),nR=poDS->GetRasterYSize();
	if (nC<=0||nR<=0)
	{
		Logger::instance().error("extractEmissionIncreaseLand: Invalid raster dimensions");
		GDALClose(poDS); GDALClose(poDN);
		return;
	}

	GDALDriver *pDrv=GetGDALDriverManager()->GetDriverByName("GTIFF");
	if (!pDrv)
	{
		Logger::instance().error("extractEmissionIncreaseLand: Cannot get GTiff driver");
		GDALClose(poDS); GDALClose(poDN);
		return;
	}
	char**opts=nullptr; opts=CSLSetNameValue(opts,"BIGTIFF","IF_NEEDED"); opts=CSLSetNameValue(opts,"COMPRESS","DEFLATE");
	opts=CSLSetNameValue(opts,"PREDICTOR","2"); opts=CSLSetNameValue(opts,"ZLEVEL","9");
	GDALDataset*dst=pDrv->Create(qstrOutputFileName.c_str(),nC,nR,1,GDT_Byte,opts);
	if (!dst)
	{
		Logger::instance().error("extractEmissionIncreaseLand: Cannot create output file: " + qstrOutputFileName);
		GDALClose(poDS); GDALClose(poDN);
		return;
	}

	double gt[6];
	if(poDS->GetGeoTransform(gt)==CE_Failure)
	{
		Logger::instance().error("extractEmissionIncreaseLand: Cannot get GeoTransform from original data");
		GDALClose(poDS);GDALClose(poDN);GDALClose(dst);
		return;
	}
	std::string projStr=getProjectionAndFixTransform(poDS,gt,nR);
	dst->SetGeoTransform(gt); dst->SetProjection(projStr.c_str());

	unsigned char*pNV=new unsigned char[nC*nR]; double*pOV=new double[nC*nR],*pNVd=new double[nC*nR];
	poDS->RasterIO(GF_Read,0,0,nC,nR,pOV,nC,nR,GDT_Float64,1,0,0,0,0);
	poDN->RasterIO(GF_Read,0,0,nC,nR,pNVd,nC,nR,GDT_Float64,1,0,0,0,0);
	for(int i=0;i<nC;i++) for(int j=0;j<nR;j++){
		int idx=i+j*nC; pNV[idx]=0;
		int x1=std::max(0,i-nNeighborhoodRadius), y1=std::max(0,j-nNeighborhoodRadius);
		int x2=std::min(nC-1,i+nNeighborhoodRadius), y2=std::min(nR-1,j+nNeighborhoodRadius);
		double dO=pOV[x2+y2*nC]-pOV[x2+y1*nC]-pOV[x1+y2*nC]+pOV[x1+y1*nC];
		double dN=pNVd[x2+y2*nC]-pNVd[x2+y1*nC]-pNVd[x1+y2*nC]+pNVd[x1+y1*nC];
		if(dO<dN) pNV[idx]=1;
	}
	dst->GetRasterBand(1)->RasterIO(GF_Write,0,0,nC,nR,pNV,nC,nR,GDT_Byte,0,0);
	dst->GetRasterBand(1)->SetNoDataValue(0); dst->GetRasterBand(1)->FlushCache();
	GDALClose(poDN); GDALClose(poDS); GDALClose(dst);
	delete[]pOV; delete[]pNV; delete[]pNVd;

	Logger::instance().info("extractEmissionIncreaseLand: Completed.");
}

void ExtractPriorityAreas::removeNoDataFromSecondRaster(const std::string &inputFilePath1, const std::string &inputFilePath2, const std::string &outputFilePath)
{
	Logger::instance().info("removeNoDataFromSecondRaster: Starting...");

	if (inputFilePath1.empty()||inputFilePath2.empty()||outputFilePath.empty())
	{
		Logger::instance().error("removeNoDataFromSecondRaster: Input path is empty");
		return;
	}

	GDALAllRegister(); CPLSetConfigOption("GDAL_FILENAME_IS_UTF8","NO"); CPLSetConfigOption("SHAPE_ENCODING","");
	GDALDataset *po1=(GDALDataset*)GDALOpen(inputFilePath1.c_str(),GA_ReadOnly);
	GDALDataset *po2=(GDALDataset*)GDALOpen(inputFilePath2.c_str(),GA_ReadOnly);
	if(!po1||!po2)
	{
		Logger::instance().error("removeNoDataFromSecondRaster: Cannot open one or more input datasets");
		if(po1)GDALClose(po1);if(po2)GDALClose(po2);
		return;
	}

	int nC=po1->GetRasterXSize(),nR=po1->GetRasterYSize();
	if (nC<=0||nR<=0)
	{
		Logger::instance().error("removeNoDataFromSecondRaster: Invalid raster dimensions");
		GDALClose(po1); GDALClose(po2);
		return;
	}

	GDALDriver *pDrv=GetGDALDriverManager()->GetDriverByName("GTIFF");
	if (!pDrv)
	{
		Logger::instance().error("removeNoDataFromSecondRaster: Cannot get GTiff driver");
		GDALClose(po1); GDALClose(po2);
		return;
	}
	char**opts=nullptr; opts=CSLSetNameValue(opts,"BIGTIFF","IF_NEEDED"); opts=CSLSetNameValue(opts,"COMPRESS","DEFLATE");
	opts=CSLSetNameValue(opts,"PREDICTOR","2"); opts=CSLSetNameValue(opts,"ZLEVEL","9");
	GDALDataset*dst=pDrv->Create(outputFilePath.c_str(),nC,nR,1,GDT_Byte,opts);
	if (!dst)
	{
		Logger::instance().error("removeNoDataFromSecondRaster: Cannot create output file: " + outputFilePath);
		GDALClose(po1); GDALClose(po2);
		return;
	}

	double gt[6];
	if(po1->GetGeoTransform(gt)==CE_Failure)
	{
		Logger::instance().error("removeNoDataFromSecondRaster: Cannot get GeoTransform from first input");
		GDALClose(po1);GDALClose(po2);GDALClose(dst);
		return;
	}
	std::string projStr=getProjectionAndFixTransform(po1,gt,nR);
	dst->SetGeoTransform(gt); dst->SetProjection(projStr.c_str());

	unsigned char*p1=new unsigned char[nC*nR],*p2=new unsigned char[nC*nR];
	po1->RasterIO(GF_Read,0,0,nC,nR,p1,nC,nR,GDT_Byte,1,0,0,0,0);
	po2->RasterIO(GF_Read,0,0,nC,nR,p2,nC,nR,GDT_Byte,1,0,0,0,0);
	double nd1=po1->GetRasterBand(1)->GetNoDataValue(), nd2=po2->GetRasterBand(1)->GetNoDataValue();
	for(int i=0;i<nC*nR;i++) if(p1[i]==nd1) p2[i]=nd2;
	dst->GetRasterBand(1)->RasterIO(GF_Write,0,0,nC,nR,p2,nC,nR,GDT_Byte,0,0);
	dst->GetRasterBand(1)->SetNoDataValue(0); dst->GetRasterBand(1)->FlushCache();
	GDALClose(po1); GDALClose(po2); GDALClose(dst);
	delete[]p1; delete[]p2;

	Logger::instance().info("removeNoDataFromSecondRaster: Completed.");
}
