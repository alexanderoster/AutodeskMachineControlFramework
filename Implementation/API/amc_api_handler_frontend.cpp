/*++

Copyright (C) 2025 Autodesk Inc.

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

#include "amc_api_handler_frontend.hpp"
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



CAPIHandler_Frontend::CAPIHandler_Frontend(PSystemState pSystemState)
	: CAPIHandler(pSystemState->getClientHash()), m_pSystemState(pSystemState)
{
	if (pSystemState.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);


}

CAPIHandler_Frontend::~CAPIHandler_Frontend()
{

}

std::string CAPIHandler_Frontend::getBaseURI()
{
	return "api/frontend";
}

APIHandler_FrontendType CAPIHandler_Frontend::parseRequest(const std::string& sURI, const eAPIRequestType requestType, std::string& sParameterUUID, std::string& sAdditionalParameter)
{
	// Leave away base URI
	auto sParameterString = AMCCommon::CUtils::toLowerString(sURI.substr(getBaseURI().length()));
	sParameterUUID = "";
	sAdditionalParameter = "";

	if (requestType == eAPIRequestType::rtGet) {

		if (sURI.empty () || (sURI == "/"))
			return APIHandler_FrontendType::ftStatus;

	}


	if (requestType == eAPIRequestType::rtPost) {


	}

	return APIHandler_FrontendType::ftUnknown;
}


void CAPIHandler_Frontend::checkAuthorizationMode(const std::string& sURI, const eAPIRequestType requestType, bool& bNeedsToBeAuthorized, bool& bCreateNewSession)
{
	bNeedsToBeAuthorized = true;
	bCreateNewSession = false;
	
}

bool CAPIHandler_Frontend::expectsRawBody(const std::string& sURI, const eAPIRequestType requestType)
{
	std::string sParameterUUID;
	std::string sAdditionalParameter;
	auto uiType = parseRequest(sURI, requestType, sParameterUUID, sAdditionalParameter);

	return (uiType == APIHandler_FrontendType::ftTriggerEvent);

}

void CAPIHandler_Frontend::handleStatusRequest(CJSONWriter& writer, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	m_pSystemState->uiHandler()->frontendWriteStatusToJSON(writer, pAuth->getFrontendState().get());
}




PAPIResponse CAPIHandler_Frontend::handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields & pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	std::string sParameterUUID;
	std::string sAdditionalParameter;
	auto uiType = parseRequest(sURI, requestType, sParameterUUID, sAdditionalParameter);

	CJSONWriter writer;
	writeJSONHeader(writer, AMC_API_PROTOCOL_UI);

	switch (uiType) {
	case APIHandler_FrontendType::ftStatus:
		handleStatusRequest(writer, pAuth);
		break;

	default:
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	}

	return std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, AMC_API_CONTENTTYPE, writer.saveToString());
}



		

