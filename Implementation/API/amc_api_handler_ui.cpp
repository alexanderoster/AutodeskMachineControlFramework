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

#include "amc_api_handler_ui.hpp"
#include "amc_api_jsonrequest.hpp"
#include "amc_ui_handler.hpp"
#include "amc_ui_module_item.hpp"

#define __AMCIMPL_UI_DIALOG
#define __AMCIMPL_UI_PAGE
#define __AMCIMPL_UI_MODULE

#include "amc_ui_page.hpp"
#include "amc_ui_dialog.hpp"
#include "amc_ui_module.hpp"
#include "amc_ui_clientaction.hpp"
#include "amc_meshentity.hpp"
#include "amc_meshhandler.hpp"
#include "amc_dataserieshandler.hpp"
#include "amc_scatterplot.hpp"
#include "amc_toolpathhandler.hpp"

#include "libmc_interfaceexception.hpp"
#include "libmcdata_dynamic.hpp"

#include "common_utils.hpp"
#include "common_chrono.hpp"

#include <cmath>
#include <vector>
#include <memory>
#include <string>
#include <iostream>


using namespace AMC;


class CAPIRenderGeometryResponse : public CAPIFixedFloatBufferResponse {
private:

public:

	CAPIRenderGeometryResponse(CMeshEntity* pMeshEntity)
		: CAPIFixedFloatBufferResponse("application/binary")
	{

		if (pMeshEntity == nullptr)
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

		size_t nFaceCount = pMeshEntity->getFaceCount();
		resizeTo(nFaceCount * 9);

		for (size_t nFaceID = 1; nFaceID <= nFaceCount; nFaceID++) {
			std::array<sMeshEntityNode, 3> Nodes;
			pMeshEntity->getFaceNodes(nFaceID, Nodes.at(0), Nodes.at(1), Nodes.at(2));
			for (uint32_t nNodeIndex = 0; nNodeIndex < 3; nNodeIndex++) {
				for (uint32_t nCoordIndex = 0; nCoordIndex < 3; nCoordIndex++) {
					addFloat(Nodes[nNodeIndex].m_fCoordinates[nCoordIndex]);
				}
			}

		}
	}

};


class CAPIRenderEdgesResponse : public CAPIFixedFloatBufferResponse {
private:

public:

	CAPIRenderEdgesResponse(CMeshEntity* pMeshEntity)
		: CAPIFixedFloatBufferResponse("application/binary")
	{

		if (pMeshEntity == nullptr)
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

		size_t nEdgeCount = pMeshEntity->getEdgeCount();
		resizeTo(nEdgeCount * 6);

		for (size_t nEdgeID = 1; nEdgeID <= nEdgeCount; nEdgeID++) {

			auto& edge = pMeshEntity->getEdge(nEdgeID);

			if (edge.m_nAngleInDegrees > 45) {

				std::array<sMeshEntityNode, 2> Nodes;
				pMeshEntity->getEdgeNodes(nEdgeID, Nodes.at(0), Nodes.at(1));
				for (uint32_t nNodeIndex = 0; nNodeIndex < 2; nNodeIndex++) {
					for (uint32_t nCoordIndex = 0; nCoordIndex < 3; nCoordIndex++) {
						addFloat(Nodes[nNodeIndex].m_fCoordinates[nCoordIndex]);
					}
				}
			}

		}
	}

};


class CAPIRenderPointCloudResponse : public CAPIFixedFloatBufferResponse {
private:

public:

	CAPIRenderPointCloudResponse(AMC::CScatterplot * pScatterplot)
		: CAPIFixedFloatBufferResponse("application/binary")
	{

		
		if (pScatterplot != nullptr) {
			size_t nPointCount = pScatterplot->getEntryCount ();
			resizeTo(nPointCount * 2); 

			auto& entries = pScatterplot->getEntries();

			for (size_t nPointIndex = 0; nPointIndex < nPointCount; nPointIndex++) {

				auto& entry = entries.at(nPointIndex);
				
				addFloat((float)entry.m_dX);
				addFloat((float)entry.m_dY);

			}

		}


	}

};



CAPIHandler_UI::CAPIHandler_UI(PSystemState pSystemState)
	: CAPIHandler(pSystemState->getClientHash()), m_pSystemState(pSystemState)
{
	if (pSystemState.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);


}

CAPIHandler_UI::~CAPIHandler_UI()
{

}

std::string CAPIHandler_UI::getBaseURI()
{
	return "api/ui";
}

APIHandler_UIType CAPIHandler_UI::parseRequest(const std::string& sURI, const eAPIRequestType requestType, std::string& sParameterUUID, std::string& sAdditionalParameter)
{
	// Leave away base URI
	auto sParameterString = AMCCommon::CUtils::toLowerString(sURI.substr(getBaseURI().length()));
	sParameterUUID = "";
	sAdditionalParameter = "";

	if (requestType == eAPIRequestType::rtGet) {

		if ((sParameterString == "/config/") || (sParameterString == "/config")) {
			return APIHandler_UIType::utConfiguration;
		}

		if ((sParameterString == "/state/") || (sParameterString == "/state")) {
			return APIHandler_UIType::utState;
		}

		if (sParameterString.length() == 43) {
			if (sParameterString.substr(0, 7) == "/image/") {
				sParameterUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(7, 36));
				return APIHandler_UIType::utImage;
			}
		}

		if (sParameterString.length() == 46) {
			if (sParameterString.substr(0, 10) == "/download/") {
				sParameterUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(10, 36));
				return APIHandler_UIType::utDownload;
			}
		}

		if (sParameterString.length() == 43) {
			if (sParameterString.substr(0, 7) == "/chart/") {
				sParameterUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(7, 36));
				return APIHandler_UIType::utChart;
			}
		}


		if (sParameterString.length() >= 49) {
			if (sParameterString.substr(0, 13) == "/contentitem/") {
				sParameterUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(13, 36));
				if (sParameterString.length() > 49) {
					if (sParameterString.at(49) == '/') {
						sAdditionalParameter = sParameterString.substr(50);
					}
				}

				return APIHandler_UIType::utContentItem;
			}
		}

		if (sParameterString.length() == 50) {
			if (sParameterString.substr(0, 14) == "/meshgeometry/") {
				sParameterUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(14));
				return APIHandler_UIType::utMeshGeometry;
			}
		}

		if (sParameterString.length() == 47) {
			if (sParameterString.substr(0, 11) == "/meshedges/") {
				sParameterUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(11));
				return APIHandler_UIType::utMeshEdges;
			}
		}

		if (sParameterString.length() == 48) {
			if (sParameterString.substr(0, 12) == "/pointcloud/") {
				sParameterUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(12));
				return APIHandler_UIType::utPointCloud;
			}
		}

	}


	if (requestType == eAPIRequestType::rtPost) {

		if ((sParameterString == "/event/") || (sParameterString == "/event")) {
			return APIHandler_UIType::utEvent;
		}

		if (sParameterString.length() >= 44) {
			if (sParameterString.substr(0, 8) == "/widget/") {
				sParameterUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(8, 36));
				size_t nParameterLength = sParameterString.length();
				if (nParameterLength > 44) {
					if (sParameterString.substr(44, 1) != "/")
						throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDWIDGETREQUEST, "Invalid widget request: " + sParameterString.substr(44));
					
					if (nParameterLength > 45) {
						sAdditionalParameter = sParameterString.substr(45);
						if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString (sAdditionalParameter))
							throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDWIDGETREQUEST, "Invalid widget request: " + sParameterString.substr(44));

					}

				}
				return APIHandler_UIType::utWidgetRequest;
			}
		}

	}

	return APIHandler_UIType::utUnknown;
}


void CAPIHandler_UI::checkAuthorizationMode(const std::string& sURI, const eAPIRequestType requestType, bool& bNeedsToBeAuthorized, bool& bCreateNewSession) 
{
	std::string sParameterUUID;
	std::string sAdditionalParameter;
	auto uiType = parseRequest(sURI, requestType, sParameterUUID, sAdditionalParameter);

	// The Configuration needs to be available pre-login
	// Downloads do not need to be authorized, as they generate download ticket ids that are unique to the session user...
	// Images are dynamically checked in the download, if they are authenticated..
	if ((uiType == APIHandler_UIType::utConfiguration) || (uiType == APIHandler_UIType::utImage) || (uiType == APIHandler_UIType::utDownload)) {
		bNeedsToBeAuthorized = false;
		bCreateNewSession = false;

	}
	else {
		bNeedsToBeAuthorized = true;
		bCreateNewSession = false;
	}
	
}

bool CAPIHandler_UI::expectsRawBody(const std::string& sURI, const eAPIRequestType requestType)
{
	std::string sParameterUUID;
	std::string sAdditionalParameter;
	auto uiType = parseRequest(sURI, requestType, sParameterUUID, sAdditionalParameter);

	return (uiType == APIHandler_UIType::utEvent) || (uiType == APIHandler_UIType::utWidgetRequest);

}

void CAPIHandler_UI::handleConfigurationRequest(CJSONWriter& writer, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	m_pSystemState->uiHandler()->writeConfigurationToJSON(writer, pAuth->getClientVariableHandler ().get());
}

void CAPIHandler_UI::handleStateRequest(CJSONWriter& writer, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	m_pSystemState->uiHandler()->writeStateToJSON(writer, pAuth->getClientVariableHandler().get());
}


PAPIResponse CAPIHandler_UI::handleImageRequest(const std::string& sParameterUUID, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	// First look in resources for UUID
	auto pCoreResourcePackage = m_pSystemState->uiHandler()->getCoreResourcePackage();
	auto pResourceEntry = pCoreResourcePackage->findEntryByUUID(sParameterUUID, false);

	if (pResourceEntry != nullptr) {
		auto apiResponse = std::make_shared<CAPIFixedBufferResponse>(pResourceEntry->getContentType ());
		pCoreResourcePackage->readEntry(pResourceEntry->getName(), apiResponse->getBuffer());

		return apiResponse;
	}

	// Then look in storage for uuid
	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pStorage = pDataModel->CreateStorage();
	if (pStorage->StreamIsImage(sParameterUUID)) {

		auto pStream = pStorage->RetrieveStream(sParameterUUID);
		auto sContentType = pStream->GetMIMEType();

		auto apiResponse = std::make_shared<CAPIFixedBufferResponse>(sContentType);
		pStream->GetContent(apiResponse->getBuffer());

		return apiResponse;		
	}


	// if not found, return 404
	return nullptr;

}


PAPIResponse CAPIHandler_UI::handleDownloadRequest(const std::string& sParameterUUID, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pStorage = pDataModel->CreateStorage();
	auto pGlobalChrono = m_pSystemState->globalChrono();

	std::string sIPAddress;

	std::string sStreamUUID;
	std::string sSessionUUID;
	std::string sUserUUID;
	std::string sDownloadFileName;

	pStorage->RequestDownloadTicket(sParameterUUID, sIPAddress, pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970(), sStreamUUID, sDownloadFileName, sSessionUUID, sUserUUID);

	auto pStream = pStorage->RetrieveStream(sStreamUUID);
	auto sContentType = pStream->GetMIMEType();

	auto apiResponse = std::make_shared<CAPIFixedBufferResponse>(sContentType);
	apiResponse->setContentDispositionName(sDownloadFileName);
	pStream->GetContent(apiResponse->getBuffer());

	return apiResponse;

}


PAPIResponse CAPIHandler_UI::handleChartRequest(const std::string& sParameterUUID, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	auto pDataSeriesHandler = m_pSystemState->getDataSeriesHandlerInstance();
	auto pDataSeries = pDataSeriesHandler->findDataSeries(sParameterUUID, false);

	if (pDataSeries.get() != nullptr) {

		auto apiResponse = std::make_shared<CAPIFixedFloatBufferResponse>("application/binary");
		auto & entries = pDataSeries->getEntries();

		size_t nEntryCount = entries.size();
		apiResponse->resizeTo(nEntryCount * 2);

		for (size_t nIndex = 0; nIndex < nEntryCount; nIndex++) {
			auto & entry = entries.at(nIndex);
			
			apiResponse->addFloat((float) (entry.m_nTimeStampInMicroSeconds * 0.000001));
			apiResponse->addFloat((float) entry.m_dValue);
		}

		return apiResponse;

	}

	// if not found, return 404
	return nullptr;

}


void CAPIHandler_UI::handleContentItemRequest(CJSONWriter& writer, const std::string& sParameterUUID, PAPIAuth pAuth, uint32_t nStateID)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	auto pWidget = m_pSystemState->uiHandler()->findModuleItem (sParameterUUID);
	if (pWidget.get () == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_MODULEITEMNOTFOUND);

	CJSONWriterObject object(writer);
	pWidget->addContentToJSON(writer, object, pAuth->getClientVariableHandler ().get(), nStateID);
	writer.addString(AMC_API_KEY_UI_ITEMUUID, sParameterUUID);
	writer.addObject(AMC_API_KEY_UI_CONTENT, object);
}

void CAPIHandler_UI::handleEventRequest(CJSONWriter& writer, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (pBodyData == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	CAPIJSONRequest apiRequest(pBodyData, nBodyDataSize);
	auto sEventName = apiRequest.getNameString(AMC_API_KEY_UI_EVENTNAME, LIBMC_ERROR_EVENTNAMENOTFOUND);	
	std::string sSenderUUID;
	if (apiRequest.hasValue (AMC_API_KEY_UI_EVENTSENDER))
		sSenderUUID = apiRequest.getUUID(AMC_API_KEY_UI_EVENTSENDER, LIBMC_ERROR_INVALIDEVENTSENDER);

	std::string sFormValueJSON;
	if (apiRequest.hasValue(AMC_API_KEY_UI_FORMVALUEJSON)) {
		sFormValueJSON = apiRequest.getJSONObjectString(AMC_API_KEY_UI_FORMVALUEJSON, LIBMC_ERROR_INVALIDFORMVALUES);
	}
	
	auto pUIHandler = m_pSystemState->uiHandler();

	auto pEventResult = pUIHandler->handleEvent(sEventName, sSenderUUID, sFormValueJSON, "", pAuth);

	CJSONWriterArray contentUpdateNode(writer);

	if (pEventResult.getErrorCode())
	{
		writer.addInteger(AMC_API_KEY_UI_EVENTERRORCODE, pEventResult.getErrorCode ());
		writer.addString(AMC_API_KEY_UI_EVENTERRORMESSAGE, pEventResult.getErrorMessage());
	} 
	else {

		AMC::CJSONWriterArray actionsArray(writer);
		for (auto pAction : pEventResult.getClientActions()) {
			AMC::CJSONWriterObject actionObject(writer);
			pAction->writeToJSON(writer, actionObject);
			actionsArray.addObject(actionObject);
		}
		
		writer.addArray(AMC_API_KEY_UI_EVENTACTIONS, actionsArray);

		
	}

	if (!contentUpdateNode.isEmpty()) {
		writer.addArray(AMC_API_KEY_UI_CONTENTUPDATE, contentUpdateNode);
	}


}


void CAPIHandler_UI::handleWidgetRequest(CJSONWriter& writer, const std::string& sWidgetUUID, const std::string& sRequestType, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (pBodyData == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	CAPIJSONRequest apiRequest(pBodyData, nBodyDataSize);
	auto pUIHandler = m_pSystemState->uiHandler();

	auto pWidget = pUIHandler->findModuleItem(AMCCommon::CUtils::normalizeUUIDString(sWidgetUUID));
	if (pWidget.get () == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_MODULEITEMNOTFOUND, "Widget not found: " + sWidgetUUID);

	pWidget->handleCustomRequest (pAuth, sRequestType, apiRequest, writer, pUIHandler);

}

PAPIResponse CAPIHandler_UI::handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields & pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	std::string sParameterUUID;
	std::string sAdditionalParameter;
	auto uiType = parseRequest(sURI, requestType, sParameterUUID, sAdditionalParameter);

	CJSONWriter writer;
	writeJSONHeader(writer, AMC_API_PROTOCOL_UI);

	switch (uiType) {
	case APIHandler_UIType::utConfiguration:
		handleConfigurationRequest(writer, pAuth);
		break;

	case APIHandler_UIType::utState:
		handleStateRequest(writer, pAuth);
		break;

	case APIHandler_UIType::utContentItem: {
		int64_t nStateID = 0;
		if (!sAdditionalParameter.empty()) {
			nStateID = std::stoi(sAdditionalParameter);

			if ((nStateID < 0) || (nStateID > INT32_MAX))
				throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCONTENTSTATEID, "Invalid content state id: " + sAdditionalParameter);
		}
		handleContentItemRequest(writer, sParameterUUID, pAuth, (uint32_t) nStateID);
		break;
	}

	case APIHandler_UIType::utImage:
		return handleImageRequest(sParameterUUID, pAuth);

	case APIHandler_UIType::utDownload:
		return handleDownloadRequest(sParameterUUID, pAuth);

	case APIHandler_UIType::utMeshGeometry: {
		auto pMeshHandler = m_pSystemState->getMeshHandlerInstance();
		auto pMeshEntity = pMeshHandler->findMeshEntity(sParameterUUID, true);
		return std::make_shared<CAPIRenderGeometryResponse>(pMeshEntity.get());
	}

	case APIHandler_UIType::utMeshEdges: {
		auto pMeshHandler = m_pSystemState->getMeshHandlerInstance();
		auto pMeshEntity = pMeshHandler->findMeshEntity(sParameterUUID, true);
		return std::make_shared<CAPIRenderEdgesResponse>(pMeshEntity.get());
	}

	case APIHandler_UIType::utPointCloud: {
		auto pToolpathHandler = m_pSystemState->getToolpathHandlerInstance();
		auto pScatterplot = pToolpathHandler->restoreScatterplot(sParameterUUID, false);
		return std::make_shared<CAPIRenderPointCloudResponse>(pScatterplot.get ());
	}

	case APIHandler_UIType::utChart:
		return handleChartRequest(sParameterUUID, pAuth);

	case APIHandler_UIType::utEvent:
		handleEventRequest (writer, pBodyData, nBodyDataSize, pAuth);
		break;

	case APIHandler_UIType::utWidgetRequest:
		handleWidgetRequest (writer, sParameterUUID, sAdditionalParameter, pBodyData, nBodyDataSize, pAuth);
		break;

	default:
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	}

	return std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, AMC_API_CONTENTTYPE, writer.saveToString());
}



		

