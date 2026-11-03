/*++

Copyright (C) 2020 Autodesk Inc.

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
	* Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.
	* Neither the name of the Autodesk Inc. nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL AUTODESK INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "amc_api_handler_build.hpp"
#include "amc_api_jsonrequest.hpp"

#include "libmc_interfaceexception.hpp"
#include "libmcdata_dynamic.hpp"

#include "amc_toolpathhandler.hpp"

#include "common_utils.hpp"

#include <vector>
#include <memory>
#include <string>
#include <iostream>

using namespace AMC;

CAPIHandler_Build::CAPIHandler_Build(PSystemState pSystemState)
	: CAPIHandler(pSystemState->getClientHash ()), m_pSystemState(pSystemState)
{
	if (pSystemState.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

}

CAPIHandler_Build::~CAPIHandler_Build()
{
	
}
				
std::string CAPIHandler_Build::getBaseURI () 
{
	return "api/build";
}

APIHandler_BuildType CAPIHandler_Build::parseRequest(const std::string& sURI, const eAPIRequestType requestType, std::string& paramUUID)
{
	// Leave away base URI
 	auto sParameterString = AMCCommon::CUtils::toLowerString (sURI.substr(9));
	paramUUID = "";

	if (requestType == eAPIRequestType::rtPost) {

		if ((sParameterString == "/toolpath") || (sParameterString == "/toolpath/")) {
			return APIHandler_BuildType::btToolpath;
		}

		if ((sParameterString.substr(0, 1) == "/") && (sParameterString.length() == 37)) {
			paramUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(1));
			return APIHandler_BuildType::btBuildJobUpdate;
		}
	}

	if (requestType == eAPIRequestType::rtGet) {

		if ((sParameterString == "/") || (sParameterString == "")) {
			return APIHandler_BuildType::btListJobs;
		}

		if ((sParameterString.substr(0, 1) == "/") && (sParameterString.length() == 37)) {
			paramUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(1));
			return APIHandler_BuildType::btBuildJobDetails;
		}

		if (sParameterString.substr(0, 15) == "/listbuilddata/") {
			paramUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(15));
			return APIHandler_BuildType::btListBuildData;
		}

		if (sParameterString.substr (0, 6) == "/data/") {
			paramUUID = AMCCommon::CUtils::normalizeUUIDString (sParameterString.substr(6));
			return APIHandler_BuildType::btGetBuildData;
		}

	}


	return APIHandler_BuildType::btUnknown;
}


bool CAPIHandler_Build::expectsRawBody(const std::string& sURI, const eAPIRequestType requestType)
{
	std::string jobUUID;

	switch (parseRequest(sURI, requestType, jobUUID)) {
		case APIHandler_BuildType::btToolpath:
			return true;
		case APIHandler_BuildType::btBuildJobUpdate:
			return true;

		default:
			return false;
	}
}


uint32_t CAPIHandler_Build::getFormDataFieldCount(const std::string& sURI, const eAPIRequestType requestType)
{
	return 0;	
}

CAPIFieldDetails CAPIHandler_Build::getFormDataFieldDetails(const std::string& sURI, const eAPIRequestType requestType, const uint32_t nFieldIndex)
{
	throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDINDEX);
}

static double evaluateModifiedValue(double dFactor, double dMinValue, double dMaxValue)
{
	if (dFactor < 0.0)
		dFactor = 0.0;
	if (dFactor > 1.0)
		dFactor = 1.0;

	return (1.0 - dFactor) * dMinValue + dFactor * dMaxValue;
}

// Adds the laser power modulation of a segment: the modification mode, the power of every point
// in watts and, for nonlinear hatches, the sub-interpolation support points of every hatch.
// Segments without factors are written with the mode only and use the nominal laser power.
static void writeSegmentPowerModulation(CJSONWriter& writer, CJSONWriterObject& segmentObject, PToolpathLayerData pLayerData, PToolpathLayerProfile pProfile, uint32_t nSegmentIndex, LibMCEnv::eToolpathSegmentType segmentType, uint32_t nPointCount, const std::string& sLaserPowerValueName)
{
	auto modificationType = pProfile->getModificationType("", sLaserPowerValueName);
	switch (modificationType) {
	case LibMCEnv::eToolpathProfileModificationType::ConstantModification:
		segmentObject.addString(AMC_API_KEY_POWERMODE, AMC_API_KEY_POWERMODE_CONSTANT);
		break;
	case LibMCEnv::eToolpathProfileModificationType::LinearModification:
		segmentObject.addString(AMC_API_KEY_POWERMODE, AMC_API_KEY_POWERMODE_LINEAR);
		break;
	case LibMCEnv::eToolpathProfileModificationType::NonlinearModification:
		segmentObject.addString(AMC_API_KEY_POWERMODE, AMC_API_KEY_POWERMODE_NONLINEAR);
		break;
	default:
		return;
	}

	LibMCEnv::eToolpathProfileModificationFactor modificationFactor = LibMCEnv::eToolpathProfileModificationFactor::Unknown;
	double dMinValue = 0.0;
	double dMaxValue = 0.0;
	pProfile->getModificationInformation("", sLaserPowerValueName, modificationFactor, dMinValue, dMaxValue);

	if (!pLayerData->segmentHasOverrideFactors(nSegmentIndex, modificationFactor))
		return;

	std::vector<double> startFactors;
	std::vector<double> endFactors;
	startFactors.resize(nPointCount);
	endFactors.resize(nPointCount);
	pLayerData->storePointModificationFactorRamps(nSegmentIndex, modificationFactor, startFactors.data(), endFactors.data());

	CJSONWriterArray powerArray(writer);
	for (double dFactor : endFactors)
		powerArray.addDouble("", evaluateModifiedValue(dFactor, dMinValue, dMaxValue));
	segmentObject.addArray(AMC_API_KEY_POWERS, powerArray);

	bool bIsHatch = (segmentType == LibMCEnv::eToolpathSegmentType::Hatch);

	// Polylines: "powers" is the power at the end of the line that ends in a point, "powerstarts" the power at its start.
	if (!bIsHatch && (modificationType != LibMCEnv::eToolpathProfileModificationType::ConstantModification)) {
		CJSONWriterArray powerStartArray(writer);
		for (double dFactor : startFactors)
			powerStartArray.addDouble("", evaluateModifiedValue(dFactor, dMinValue, dMaxValue));
		segmentObject.addArray(AMC_API_KEY_POWERSTARTS, powerStartArray);
	}

	if ((modificationType == LibMCEnv::eToolpathProfileModificationType::NonlinearModification) &&
		(pLayerData->getSegmentTotalSubinterpolationCount(nSegmentIndex, modificationFactor) > 0)) {

		CJSONWriterArray subCountArray(writer);
		CJSONWriterArray subArray(writer);

		// One entry per hatch, or one entry per point for the line that ends in the point.
		uint32_t nElementCount = bIsHatch ? (nPointCount / 2) : nPointCount;
		for (uint32_t nElementIndex = 0; nElementIndex < nElementCount; nElementIndex++) {
			uint32_t nSubInterpolationCount = 0;
			Lib3MF::sHatchModificationInterpolationData* pSubInterpolationData = nullptr;
			if (bIsHatch)
				pLayerData->getHatchSubinterpolationData(nSegmentIndex, nElementIndex, modificationFactor, nSubInterpolationCount, pSubInterpolationData);
			else
				pLayerData->getPointSubinterpolationData(nSegmentIndex, nElementIndex, modificationFactor, nSubInterpolationCount, pSubInterpolationData);

			subCountArray.addInteger(nSubInterpolationCount);
			for (uint32_t nSubIndex = 0; nSubIndex < nSubInterpolationCount; nSubIndex++) {
				subArray.addDouble("", pSubInterpolationData[nSubIndex].m_Parameter);
				subArray.addDouble("", evaluateModifiedValue(pSubInterpolationData[nSubIndex].m_Factor, dMinValue, dMaxValue));
			}
		}

		segmentObject.addArray(AMC_API_KEY_POWERSUBCOUNTS, subCountArray);
		segmentObject.addArray(AMC_API_KEY_POWERSUBS, subArray);
	}
}


void CAPIHandler_Build::handleToolpathRequest(CJSONWriter& writer, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	if (pBodyData == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	CAPIJSONRequest jsonRequest(pBodyData, nBodyDataSize);
	auto sBuildUUID = jsonRequest.getUUID(AMC_API_KEY_BUILDUUID, LIBMC_ERROR_INVALIDBUILDUUID);

	CJSONWriterArray segmentArray(writer);
	CJSONWriterArray partArray(writer);

	if (sBuildUUID != AMCCommon::CUtils::createEmptyUUID()) {


		auto pDataModel = m_pSystemState->getDataModelInstance();
		auto pBuildJobHandler = pDataModel->CreateBuildJobHandler();
		auto pBuildJob = pBuildJobHandler->RetrieveJob(sBuildUUID);
		auto sStreamUUID = pBuildJob->GetStorageStreamUUID();


		auto pToolpathHandler = m_pSystemState->toolpathHandler();

		auto pToolpath = pToolpathHandler->findToolpathEntity(sStreamUUID, false);
		if (pToolpath == nullptr) {
			pToolpath = pToolpathHandler->loadToolpathEntity(sStreamUUID);
		}

		auto disabledParts = pToolpathHandler->getDisabledParts(sStreamUUID);

		uint32_t nPartCount = pToolpath->getPartCount();
		for (uint32_t nPartIndex = 0; nPartIndex < nPartCount; nPartIndex++) {
			auto pToolpathPart = pToolpath->getPart(nPartIndex);
			auto sPartUUID = AMCCommon::CUtils::normalizeUUIDString(pToolpathPart->getUUID());

			CJSONWriterObject partObject(writer);
			partObject.addString(AMC_API_KEY_UPLOAD_BUILDPARTUUID, sPartUUID);
			partObject.addString(AMC_API_KEY_UPLOAD_BUILDPARTNAME, pToolpathPart->getName());
			partObject.addBool(AMC_API_KEY_PARTDISABLED, disabledParts.find(sPartUUID) != disabledParts.end());
			partArray.addObject(partObject);
		}

		double dPowerRangeMin = 0.0;
		double dPowerRangeMax = 0.0;
		std::string sBuildLaserPowerValueName = AMC::CToolpathLayerData::getValueNameByType(LibMCEnv::eToolpathProfileValueType::LaserPower);
		if (pToolpath->getProfileValueRange("", sBuildLaserPowerValueName, dPowerRangeMin, dPowerRangeMax)) {
			writer.addDouble(AMC_API_KEY_POWERRANGEMIN, dPowerRangeMin);
			writer.addDouble(AMC_API_KEY_POWERRANGEMAX, dPowerRangeMax);
		}

		auto nLayerCount = pToolpath->getLayerCount();

		auto nLayerIndex = jsonRequest.getUint64(AMC_API_KEY_LAYERINDEX, 0, nLayerCount, LIBMC_ERROR_INVALIDLAYERINDEX);

		if (nLayerIndex < nLayerCount) {

			auto pLayerData = pToolpath->readLayer((uint32_t)nLayerIndex);
			auto dUnits = pLayerData->getUnits();

			std::string sLaserPowerValueName = AMC::CToolpathLayerData::getValueNameByType(LibMCEnv::eToolpathProfileValueType::LaserPower);
			std::string sLaserSpeedValueName = AMC::CToolpathLayerData::getValueNameByType(LibMCEnv::eToolpathProfileValueType::Speed);

			auto nSegmentCount = pLayerData->getSegmentCount();
			for (uint32_t nSegmentIndex = 0; nSegmentIndex < nSegmentCount; nSegmentIndex++) {
				auto segmentType = pLayerData->getSegmentType(nSegmentIndex);
				auto nPointCount = pLayerData->getSegmentPointCount(nSegmentIndex);

				auto pProfile = pLayerData->getSegmentProfile (nSegmentIndex);
				std::string sProfileName = pProfile->getName();
				double dLaserPower = pProfile->getDoubleValueDef("", sLaserPowerValueName, 0.0);
				double dLaserSpeed = pProfile->getDoubleValueDef("", sLaserSpeedValueName, 0.0);
				uint32_t nColor = ((pProfile->getProfileIndex () + 1) * 12347328) & 0xFFFFFF;
				uint32_t nPartID = pLayerData->getSegmentLocalPartID(nSegmentIndex);
				std::string sPartUUID = pLayerData->getSegmentPartUUID(nSegmentIndex);
				uint32_t nLaserIndex = pLayerData->getSegmentLaserIndex(nSegmentIndex);

				CJSONWriterObject segmentObject(writer);

				if (nPointCount > 0) {

					std::vector<LibMCEnv::sPosition2D> Points;
					Points.resize(nPointCount);
					pLayerData->storePointsToBufferInUnits(nSegmentIndex, Points.data());

					CJSONWriterArray pointArray(writer);

					switch (segmentType) {
					case LibMCEnv::eToolpathSegmentType::Hatch:
						segmentObject.addString(AMC_API_KEY_TYPE, AMC_API_KEY_HATCH);
						break;
					case LibMCEnv::eToolpathSegmentType::Polyline:
						segmentObject.addString(AMC_API_KEY_TYPE, AMC_API_KEY_POLYLINE);
						break;
					}

					for (uint32_t nPointIndex = 0; nPointIndex < nPointCount; nPointIndex++) {

						auto pPoint = &Points[nPointIndex];

						CJSONWriterObject pointObject(writer);
						pointObject.addDouble(AMC_API_KEY_X, pPoint->m_Coordinates[0] * dUnits);
						pointObject.addDouble(AMC_API_KEY_Y, pPoint->m_Coordinates[1] * dUnits);

						pointArray.addObject(pointObject);

					}

					segmentObject.addArray(AMC_API_KEY_POINTS, pointArray);
					segmentObject.addDouble(AMC_API_KEY_LASERPOWER, dLaserPower);
					segmentObject.addDouble(AMC_API_KEY_LASERSPEED, dLaserSpeed);
					segmentObject.addInteger(AMC_API_KEY_PARTID, nPartID);
					segmentObject.addString(AMC_API_KEY_PARTUUID, sPartUUID);
					segmentObject.addString(AMC_API_KEY_PROFILENAME, sProfileName);
					
					segmentObject.addInteger(AMC_API_KEY_COLOR, nColor);
					segmentObject.addInteger(AMC_API_KEY_LASERINDEX, nLaserIndex);

					writeSegmentPowerModulation(writer, segmentObject, pLayerData, pProfile, nSegmentIndex, segmentType, nPointCount, sLaserPowerValueName);

				}

				segmentArray.addObject(segmentObject);

			}
		}
	}


	writer.addArray(AMC_API_KEY_SEGMENTS, segmentArray);
	writer.addArray(AMC_API_KEY_PARTS, partArray);
}


void CAPIHandler_Build::handleListJobsRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& sStatusToQuery, CAPIFormFields& pFormFields)
{	
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	LibMCData::eBuildJobStatus buildStatus = LibMCData::eBuildJobStatus::Created;
	if (sStatusToQuery.empty() || (sStatusToQuery == "validated")) {
		buildStatus = LibMCData::eBuildJobStatus::Validated;
	} 
	else if (sStatusToQuery == "archived") {
		buildStatus = LibMCData::eBuildJobStatus::Archived;
	} else 
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDBUILDJOBSTATUSQUERY, sStatusToQuery);

	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pBuildJobHandler = pDataModel->CreateBuildJobHandler();

	// The head must be read before the rows, so that a change in between is fetched again by the next delta.
	writer.addInteger(AMC_API_KEY_UPLOAD_BUILDJOBBUILDLISTHEADID, pBuildJobHandler->GetBuildListHeadID());

	uint64_t nSinceHeadID = 0;
	bool bIsDelta = pFormFields.getOptionalUint64RequestParameter(AMC_API_KEY_LIST_SINCE, nSinceHeadID);

	LibMCData::PBuildJobIterator pBuildJobIterator;
	if (bIsDelta) {
		writer.addBoolean(AMC_API_KEY_LIST_DELTA, true);
		pBuildJobIterator = pBuildJobHandler->ListJobsChangedSince(nSinceHeadID);
	}
	else {
		pBuildJobIterator = pBuildJobHandler->ListJobsByStatus(buildStatus);
	}

	CJSONWriterArray jobJSONArray(writer);
	CJSONWriterArray removedJSONArray(writer);

	while (pBuildJobIterator->MoveNext()) {
		auto pBuildJob = pBuildJobIterator->GetCurrentJob();

		if (bIsDelta && (pBuildJob->GetStatus() != buildStatus)) {
			removedJSONArray.addString(pBuildJob->GetUUID());
			continue;
		}

		CJSONWriterObject jobJSON(writer);
		jobJSON.addString(AMC_API_KEY_UPLOAD_BUILDJOBUUID, pBuildJob->GetUUID());
		jobJSON.addString(AMC_API_KEY_UPLOAD_BUILDJOBSTORAGESTREAM, pBuildJob->GetStorageStreamUUID());
		jobJSON.addString(AMC_API_KEY_UPLOAD_BUILDJOBNAME, pBuildJob->GetName());
		jobJSON.addInteger(AMC_API_KEY_UPLOAD_BUILDJOBLAYERCOUNT, pBuildJob->GetLayerCount());
		jobJSON.addString(AMC_API_KEY_UPLOAD_BUILDJOBSTATUS, pBuildJob->GetStatusString());
		jobJSON.addInteger(AMC_API_KEY_UPLOAD_BUILDJOBSIZE, pBuildJob->GetStorageStreamSize());
		jobJSON.addInteger(AMC_API_KEY_UPLOAD_BUILDJOBINCREMENTALID, pBuildJob->GetIncrementalID());
		jobJSON.addString(AMC_API_KEY_UPLOAD_BUILDJOBTIMESTAMP, pBuildJob->GetTimeStamp());
		jobJSON.addString(AMC_API_KEY_UPLOAD_BUILDJOBUSER, pBuildJob->GetCreatorName());
		jobJSON.addInteger(AMC_API_KEY_UPLOAD_BUILDJOBEXECUTIONCOUNT, pBuildJob->GetExecutionCount());

		if (pBuildJob->HasThumbnailStream())
			jobJSON.addString(AMC_API_KEY_UPLOAD_ITEMBUILDTHUMBNAIL, pBuildJob->GetThumbnailStreamUUID());

		jobJSONArray.addObject(jobJSON);
		
	}

	writer.addArray(AMC_API_KEY_UPLOAD_BUILDJOBARRAY, jobJSONArray);
	if (bIsDelta)
		writer.addArray(AMC_API_KEY_LIST_REMOVED, removedJSONArray);
}

void CAPIHandler_Build::handleListBuildDataRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& buildUUID)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pBuildJobHandler = pDataModel->CreateBuildJobHandler();
	auto pBuildJob = pBuildJobHandler->RetrieveJob(buildUUID);

	CJSONWriterArray dataJSONArray(writer);

	auto pJobDataIterator = pBuildJob->ListJobData();
	while (pJobDataIterator->MoveNext()) {
		auto pJobData = pJobDataIterator->GetCurrentJobData();
		auto sUUID = pJobData->GetDataUUID();
		auto sDataType = pJobData->GetDataTypeAsString();
		auto sMimeType = pJobData->GetMIMEType();
		auto sName = pJobData->GetName();
		auto sTimeStamp = pJobData->GetTimeStamp();
		auto nSize = pJobData->GetStorageStreamSize();
		auto sSHA2 = pJobData->GetStorageStreamSHA2();

		CJSONWriterObject dataObject (writer);
		dataObject.addString(AMC_API_KEY_UPLOAD_UUID, sUUID);
		dataObject.addString(AMC_API_KEY_UPLOAD_DATATYPE, sDataType);
		dataObject.addString(AMC_API_KEY_UPLOAD_MIMETYPE, sMimeType);
		dataObject.addString(AMC_API_KEY_UPLOAD_NAME, sName);
		dataObject.addString(AMC_API_KEY_UPLOAD_TIMESTAMP, sTimeStamp);
		dataObject.addInteger(AMC_API_KEY_UPLOAD_SIZE, nSize);
		dataObject.addString(AMC_API_KEY_UPLOAD_SHA256, sSHA2);

		dataJSONArray.addObject(dataObject);
	}

	writer.addArray(AMC_API_KEY_UPLOAD_BUILDDATAARRAY, dataJSONArray);

}


void CAPIHandler_Build::writeJobDetailsEx(CJSONWriter& writer, LibMCData::PBuildJob & pBuildJob)
{
	if (pBuildJob.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	writer.addString(AMC_API_KEY_UPLOAD_BUILDJOBUUID, pBuildJob->GetUUID());
	writer.addString(AMC_API_KEY_UPLOAD_BUILDJOBSTORAGESTREAM, pBuildJob->GetStorageStreamUUID());
	writer.addString(AMC_API_KEY_UPLOAD_BUILDJOBNAME, pBuildJob->GetName());
	writer.addInteger(AMC_API_KEY_UPLOAD_BUILDJOBLAYERCOUNT, pBuildJob->GetLayerCount());
	writer.addString(AMC_API_KEY_UPLOAD_BUILDJOBSTATUS, pBuildJob->GetStatusString());
	writer.addInteger(AMC_API_KEY_UPLOAD_BUILDJOBSIZE, pBuildJob->GetStorageStreamSize());

	if (pBuildJob->HasThumbnailStream())
		writer.addString(AMC_API_KEY_UPLOAD_ITEMBUILDTHUMBNAIL, pBuildJob->GetThumbnailStreamUUID());

	auto sStreamUUID = pBuildJob->GetStorageStreamUUID();

	auto pToolpathHandler = m_pSystemState->toolpathHandler();

	auto pToolpath = pToolpathHandler->findToolpathEntity(sStreamUUID, false);
	if (pToolpath == nullptr) {
		pToolpath = pToolpathHandler->loadToolpathEntity(sStreamUUID);
	}

	uint32_t nPartCount = pToolpath->getPartCount();
	auto disabledParts = pToolpathHandler->getDisabledParts(sStreamUUID);

	CJSONWriterArray partJSONArray(writer);
	for (uint32_t nPartIndex = 0; nPartIndex < nPartCount; nPartIndex++) {
		auto pToolpathPart = pToolpath->getPart(nPartIndex);
		auto sPartUUID = AMCCommon::CUtils::normalizeUUIDString(pToolpathPart->getUUID());

		CJSONWriterObject partJSON(writer);
		partJSON.addString(AMC_API_KEY_UPLOAD_BUILDPARTUUID, sPartUUID);
		partJSON.addString(AMC_API_KEY_UPLOAD_BUILDPARTNAME, pToolpathPart->getName());
		partJSON.addString(AMC_API_KEY_UPLOAD_BUILDPARTNUMBER, pToolpathPart->getPartNumber());
		partJSON.addString(AMC_API_KEY_UPLOAD_BUILDPARTGEOMETRYUUID, pToolpathPart->getMeshUUID());
		partJSON.addBool(AMC_API_KEY_PARTDISABLED, disabledParts.find(sPartUUID) != disabledParts.end());

		partJSONArray.addObject(partJSON);

	}

	writer.addArray(AMC_API_KEY_UPLOAD_PARTARRAY, partJSONArray);
	writer.addInteger(AMC_API_KEY_PARTSTATEVERSION, (int64_t)pToolpathHandler->getDisabledPartsVersion(sStreamUUID));

	uint32_t nGlobalLayerThicknessInUnits = 0;
	bool bLayerThicknessIsVariable = false;

	double dUnits = pToolpath->getUnits();

	uint32_t nLayerCount = pToolpath->getLayerCount();
	CJSONWriterArray layerJSONArray(writer);
	for (uint32_t nLayerIndex = 0; nLayerIndex < nLayerCount; nLayerIndex++) {
		uint32_t nZMin = pToolpath->getLayerMinZInUnits(nLayerIndex);
		uint32_t nZMax = pToolpath->getLayerZInUnits(nLayerIndex);

		if (nZMin >= nZMax)
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDTOOLPATHLAYERTHICKNESS, "Invalid toolpath layer thickness in job " + pBuildJob->GetUUID () + ", layer " + std::to_string(nLayerIndex));

		uint32_t nLayerThickness = nZMax - nZMin;
		if (nLayerIndex > 0) {
			if (nGlobalLayerThicknessInUnits != nLayerThickness)
				bLayerThicknessIsVariable = true;
		}
		else {
			nGlobalLayerThicknessInUnits = nLayerThickness;
		}

		CJSONWriterObject layerJSON(writer);
		layerJSON.addInteger(AMC_API_KEY_UPLOAD_BUILDLAYERINDEX, nLayerIndex);
		layerJSON.addDouble(AMC_API_KEY_UPLOAD_BUILDLAYERZMIN, nZMin * dUnits);
		layerJSON.addDouble(AMC_API_KEY_UPLOAD_BUILDLAYERZMAX, nZMax * dUnits);
		layerJSON.addDouble(AMC_API_KEY_UPLOAD_BUILDLAYERTHICKNESS, nLayerThickness * dUnits);

		layerJSONArray.addObject(layerJSON);
	}
	writer.addArray(AMC_API_KEY_UPLOAD_LAYERARRAY, layerJSONArray);

	writer.addBoolean(AMC_API_KEY_UPLOAD_VARIABLELAYERTHICKNESS, bLayerThicknessIsVariable);
	if (!bLayerThicknessIsVariable)
		writer.addDouble(AMC_API_KEY_UPLOAD_BUILDGLOBALLAYERTHICKNESS, nGlobalLayerThicknessInUnits * dUnits);

}

void CAPIHandler_Build::handleBuildJobDetailsRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& buildUUID)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pBuildJobHandler = pDataModel->CreateBuildJobHandler();
	auto pBuildJob = pBuildJobHandler->RetrieveJob(buildUUID);

	writeJobDetailsEx(writer, pBuildJob);

	
}


PAPIResponse CAPIHandler_Build::handleGetBuildDataRequest(PAPIAuth pAuth, const std::string& buildDataUUID)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pBuildJobHandler = pDataModel->CreateBuildJobHandler();
	auto pBuildJob = pBuildJobHandler->FindJobOfData(buildDataUUID);
	auto pBuildJobData = pBuildJob->RetrieveJobData(buildDataUUID);

	auto pStorageStream = pBuildJobData->GetStorageStream();

	auto pResponse = std::make_shared<CAPIFixedBufferResponse>(pBuildJobData->GetMIMEType());
	pStorageStream->GetContent (pResponse->getBuffer());

	return pResponse;
	

}


void CAPIHandler_Build::handleUpdateBuildRequest(CJSONWriter& writer, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth, const std::string& buildUUID)
{
	if (pBodyData == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	CAPIJSONRequest jsonRequest(pBodyData, nBodyDataSize);

	std::string sNewName;
	if (jsonRequest.hasValue (AMC_API_KEY_UPLOAD_NAME)) 
		sNewName = jsonRequest.getRawString(AMC_API_KEY_UPLOAD_NAME, LIBMC_ERROR_INVALIDUPLOADNAME);

	std::string sNewStatus;
	if (jsonRequest.hasValue (AMC_API_KEY_UPLOAD_BUILDJOBSTATUS))
		sNewStatus = jsonRequest.getRawString(AMC_API_KEY_UPLOAD_BUILDJOBSTATUS, LIBMC_ERROR_INVALIDNEWBUILDSTATUS);

	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pBuildJobHandler = pDataModel->CreateBuildJobHandler();
	auto pBuildJob = pBuildJobHandler->RetrieveJob(buildUUID);

	if (!sNewName.empty())
		pBuildJob->ChangeName(sNewName);

	if (!sNewStatus.empty()) {
		if (sNewStatus == "archived") {
			pBuildJob->ArchiveJob();

		} else if (sNewStatus == "validated") {
			pBuildJob->UnArchiveJob();		
		}
		else {
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDNEWBUILDSTATUS, sNewStatus);
		}
		
	}

	writeJobDetailsEx(writer, pBuildJob);
}

PAPIResponse CAPIHandler_Build::handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields & pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	std::string paramUUID;

	auto buildType = parseRequest(sURI, requestType, paramUUID);

	CJSONWriter writer;
	writeJSONHeader(writer, AMC_API_PROTOCOL_BUILD);

	switch (buildType) {
	case APIHandler_BuildType::btListJobs: {
		std::string sStatus = pFormFields.getRequestParameter (AMC_API_KEY_UPLOAD_BUILDJOBSTATUS, false);
		handleListJobsRequest(writer, pAuth, sStatus, pFormFields);
		break;
	}
	case APIHandler_BuildType::btToolpath:
		handleToolpathRequest(writer, pBodyData, nBodyDataSize, pAuth);
		break;

	case APIHandler_BuildType::btListBuildData: 
		handleListBuildDataRequest(writer, pAuth, paramUUID);
		break;

	case APIHandler_BuildType::btGetBuildData:
		return handleGetBuildDataRequest(pAuth, paramUUID);
		break;

	case APIHandler_BuildType::btBuildJobDetails:
		handleBuildJobDetailsRequest(writer, pAuth, paramUUID);
		break;

	case APIHandler_BuildType::btBuildJobUpdate:
		handleUpdateBuildRequest(writer, pBodyData, nBodyDataSize, pAuth, paramUUID);
		break;

	default:
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	}

	return std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, AMC_API_CONTENTTYPE, writer.saveToString());

	return nullptr;
}



		

