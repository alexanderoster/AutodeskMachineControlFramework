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

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS' AND
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

#include "amc_api_handler_datatable.hpp"
#include "amc_api_constants.hpp"
#include "amc_datacapturereader.hpp"
#include "amc_datacapturequery.hpp"
#include "amc_jsonwriter.hpp"

#include "libmc_interfaceexception.hpp"
#include "libmcdata_dynamic.hpp"
#include "common_utils.hpp"
#include "common_chrono.hpp"

#include <iomanip>
#include <sstream>

using namespace AMC;

#define AMC_API_PROTOCOL_DATATABLE "com.autodesk.machinecontrol.datatable"

#define DATATABLE_UUIDLENGTH 36

namespace
{
	std::string dataCaptureStatusToString(LibMCData::eDataCaptureStatus eStatus)
	{
		switch (eStatus) {
		case LibMCData::eDataCaptureStatus::Recording: return "recording";
		case LibMCData::eDataCaptureStatus::Finished: return "finished";
		case LibMCData::eDataCaptureStatus::Aborted: return "aborted";
		default: return "unknown";
		}
	}

	// Parses a row number or count. Anything but decimal digits is an error: a typo must not
	// silently turn into a request for the whole capture.
	uint64_t parseUnsignedParameter(const std::string& sName, const std::string& sValue)
	{
		if (sValue.empty() || (sValue.length() > 19) || (sValue.find_first_not_of("0123456789") != std::string::npos))
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM, "invalid value for parameter " + sName + ": " + sValue);

		return std::stoull(sValue);
	}

	std::vector<std::string> splitCommaSeparated(const std::string& sValue)
	{
		std::vector<std::string> result;
		std::stringstream stream(sValue);
		std::string sItem;
		while (std::getline(stream, sItem, ','))
			result.push_back(AMCCommon::CUtils::trimString(sItem));

		return result;
	}
}

CAPIHandler_DataTable::CAPIHandler_DataTable(PSystemState pSystemState)
	: CAPIHandler(pSystemState->getClientHash()), m_pSystemState(pSystemState)
{
	if (pSystemState.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
}

CAPIHandler_DataTable::~CAPIHandler_DataTable()
{
}

std::string CAPIHandler_DataTable::getBaseURI()
{
	return "api/datatable";
}

PAPIResponse CAPIHandler_DataTable::handleExecutionListRequest(const std::string& sExecutionUUID)
{
	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pBuildJobHandler = pDataModel->CreateBuildJobHandler();
	auto pExecution = pBuildJobHandler->RetrieveJobExecution(AMCCommon::CUtils::normalizeUUIDString(sExecutionUUID));

	CJSONWriter writer;
	writeJSONHeader(writer, AMC_API_PROTOCOL_DATATABLE);
	writer.addString("executionuuid", pExecution->GetExecutionUUID());

	CJSONWriterArray capturesArray(writer);

	uint32_t nCaptureCount = pExecution->GetDataCaptureCount();
	for (uint32_t nIndex = 0; nIndex < nCaptureCount; nIndex++) {
		auto pManifest = pExecution->RetrieveDataCaptureByIndex(nIndex);

		CJSONWriterObject captureObject(writer);
		captureObject.addString("uuid", pManifest->GetUUID());
		captureObject.addString("identifier", pManifest->GetIdentifier());
		captureObject.addString("name", pManifest->GetName());
		captureObject.addString("status", dataCaptureStatusToString(pManifest->GetStatus()));
		captureObject.addString("timestamp", pManifest->GetTimeStamp());
		captureObject.addInteger("rowcount", (int64_t)pManifest->GetRowCount());
		captureObject.addInteger("columncount", (int64_t)pManifest->GetColumnCount());
		captureObject.addInteger("segmentcount", (int64_t)pManifest->GetSegmentCount());
		captureObject.addInteger("storedsize", (int64_t)pManifest->GetStoredSizeInBytes());
		capturesArray.addObject(captureObject);
	}

	writer.addArray("captures", capturesArray);
	return std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, AMC_API_CONTENTTYPE, writer.saveToString());
}

PAPIResponse CAPIHandler_DataTable::handleInfoRequest(const std::string& sCaptureUUID)
{
	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pManifest = pDataModel->CreateBuildJobHandler()->RetrieveDataCapture(sCaptureUUID);

	auto pReader = std::make_shared<CDataCaptureReader>(pDataModel, pManifest);
	CDataCaptureQuery query(pReader);

	CJSONWriter writer;
	writeJSONHeader(writer, AMC_API_PROTOCOL_DATATABLE);
	writer.addString("status", dataCaptureStatusToString(pManifest->GetStatus()));
	writer.addString("executionuuid", pManifest->GetExecutionUUID());
	writer.addString("timestamp", pManifest->GetTimeStamp());
	writer.addInteger("maxvaluecount", DATACAPTUREQUERY_MAXVALUECOUNT);
	writer.addInteger("maxtargetpoints", DATACAPTUREQUERY_MAXTARGETPOINTS);
	query.writeInfo(writer);

	return std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, AMC_API_CONTENTTYPE, writer.saveToString());
}

PAPIResponse CAPIHandler_DataTable::handleDataRequest(const std::string& sCaptureUUID, CAPIFormFields& pFormFields)
{
	sDataCaptureQuery query;

	if (pFormFields.hasRequestParameter("columns"))
		query.m_ColumnIdentifiers = splitCommaSeparated(pFormFields.getRequestParameter("columns", false));

	if (pFormFields.hasRequestParameter("fromrow")) {
		query.m_bHasFromRow = true;
		query.m_nFromRow = parseUnsignedParameter("fromrow", pFormFields.getRequestParameter("fromrow", false));
	}

	if (pFormFields.hasRequestParameter("rowcount")) {
		query.m_bHasRowCount = true;
		query.m_nRowCount = parseUnsignedParameter("rowcount", pFormFields.getRequestParameter("rowcount", false));
	}

	if (pFormFields.hasRequestParameter("downsample"))
		query.m_Downsample = CDataCaptureQuery::parseDownsample(pFormFields.getRequestParameter("downsample", false));

	// count is the name of this parameter in the journal envelope request.
	std::string sTargetPointsName = pFormFields.hasRequestParameter("targetpoints") ? "targetpoints" : "count";
	if (pFormFields.hasRequestParameter(sTargetPointsName)) {
		uint64_t nTargetPoints = parseUnsignedParameter(sTargetPointsName, pFormFields.getRequestParameter(sTargetPointsName, false));
		if (nTargetPoints > DATACAPTUREQUERY_MAXTARGETPOINTS)
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM, sTargetPointsName + " must be between 1 and " + std::to_string(DATACAPTUREQUERY_MAXTARGETPOINTS));
		query.m_nTargetPoints = (uint32_t)nTargetPoints;
	}

	std::string sFormat = "json";
	if (pFormFields.hasRequestParameter("format"))
		sFormat = AMCCommon::CUtils::toLowerString(pFormFields.getRequestParameter("format", false));

	if ((sFormat != "json") && (sFormat != "csv") && (sFormat != "bin") && (sFormat != "bin32"))
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM, "invalid format: " + sFormat + " (expected json, csv, bin or bin32)");

	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pManifest = pDataModel->CreateBuildJobHandler()->RetrieveDataCapture(sCaptureUUID);

	auto pReader = std::make_shared<CDataCaptureReader>(pDataModel, pManifest);
	CDataCaptureQuery captureQuery(pReader);

	if (sFormat == "csv") {
		auto pResponse = std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, "text/csv", captureQuery.buildCSV(query));
		pResponse->setContentDispositionName(pManifest->GetIdentifier() + ".csv");
		return pResponse;
	}

	if ((sFormat == "bin") || (sFormat == "bin32")) {
		auto pResponse = std::make_shared<CAPIFixedBufferResponse>("application/octet-stream");
		captureQuery.buildBinary(query, (sFormat == "bin32"), pResponse->getBuffer());
		return pResponse;
	}

	// The data arrays are written directly, because values may be NaN and a capture may hold 64 bit integers.
	std::string sJSON = "{\"" AMC_API_KEY_PROTOCOL "\":\"" AMC_API_PROTOCOL_DATATABLE "\",\"" AMC_API_KEY_VERSION "\":\"" AMC_API_PROTOCOL_VERSION "\",";
	captureQuery.appendJSONMembers(query, sJSON);
	sJSON += "}";

	return std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, AMC_API_CONTENTTYPE, sJSON);
}

PAPIResponse CAPIHandler_DataTable::handleSegmentTicketRequest(const std::string& sCaptureUUID, const std::string& sSegmentIndex, PAPIAuth pAuth)
{
	uint64_t nSegmentIndex = parseUnsignedParameter("segment index", sSegmentIndex);

	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pManifest = pDataModel->CreateBuildJobHandler()->RetrieveDataCapture(sCaptureUUID);

	if (nSegmentIndex >= pManifest->GetSegmentCount())
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM, "invalid segment index: " + sSegmentIndex);

	std::string sStreamUUID;
	uint64_t nFirstRow = 0;
	uint64_t nRowCount = 0;
	uint64_t nSizeInBytes = 0;
	bool bIsPurged = false;
	pManifest->GetSegmentInformation((uint32_t)nSegmentIndex, sStreamUUID, nFirstRow, nRowCount, nSizeInBytes, bIsPurged);

	if (bIsPurged)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM, "segment " + sSegmentIndex + " has been deleted by retention");

	std::stringstream sFileName;
	sFileName << pManifest->GetIdentifier() << "_segment" << std::setw(4) << std::setfill('0') << nSegmentIndex << ".datatable";

	// The segment is downloaded through the ticket flow of the download handler.
	std::string sTicketUUID = pAuth->createStreamDownloadTicket(sStreamUUID, sFileName.str());

	auto pStorage = pDataModel->CreateStorage();
	pStorage->CreateDownloadTicket(sTicketUUID, sStreamUUID, sFileName.str(), pAuth->getSessionUUID(), pAuth->getUserUUID(), m_pSystemState->globalChrono()->getUTCTimeStampInMicrosecondsSince1970());

	CJSONWriter writer;
	writeJSONHeader(writer, AMC_API_PROTOCOL_DATATABLE);
	writer.addString("ticketuuid", sTicketUUID);
	writer.addString("filename", sFileName.str());
	writer.addString("url", "api/download/" + sTicketUUID);
	writer.addInteger("segmentindex", (int64_t)nSegmentIndex);
	writer.addInteger("firstrow", (int64_t)nFirstRow);
	writer.addInteger("rowcount", (int64_t)nRowCount);
	writer.addInteger("size", (int64_t)nSizeInBytes);

	return std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, AMC_API_CONTENTTYPE, writer.saveToString());
}

PAPIResponse CAPIHandler_DataTable::handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields& pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	pBodyData;
	nBodyDataSize;

	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (requestType != eAPIRequestType::rtGet)
		return nullptr;

	auto sParameterString = sURI.substr(getBaseURI().length());

	// Normalize any trailing slash
	if (!sParameterString.empty() && (sParameterString.back() == '/'))
		sParameterString = sParameterString.substr(0, sParameterString.length() - 1);

	// GET api/datatable/execution/{executionuuid}
	const std::string sExecutionPrefix = "/execution/";
	if (sParameterString.substr(0, sExecutionPrefix.length()) == sExecutionPrefix) {
		std::string sExecutionUUID = sParameterString.substr(sExecutionPrefix.length());
		if (sExecutionUUID.length() != DATATABLE_UUIDLENGTH)
			return nullptr;

		return handleExecutionListRequest(sExecutionUUID);
	}

	if ((sParameterString.length() < DATATABLE_UUIDLENGTH + 1) || (sParameterString[0] != '/'))
		return nullptr;

	std::string sCaptureUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(1, DATATABLE_UUIDLENGTH));
	std::string sSubPath = sParameterString.substr(DATATABLE_UUIDLENGTH + 1);

	// GET api/datatable/{captureuuid}
	if (sSubPath.empty())
		return handleDataRequest(sCaptureUUID, pFormFields);

	// GET api/datatable/{captureuuid}/info
	if (sSubPath == "/info")
		return handleInfoRequest(sCaptureUUID);

	// GET api/datatable/{captureuuid}/segment/{index}
	const std::string sSegmentPrefix = "/segment/";
	if (sSubPath.substr(0, sSegmentPrefix.length()) == sSegmentPrefix)
		return handleSegmentTicketRequest(sCaptureUUID, sSubPath.substr(sSegmentPrefix.length()), pAuth);

	return nullptr;
}
