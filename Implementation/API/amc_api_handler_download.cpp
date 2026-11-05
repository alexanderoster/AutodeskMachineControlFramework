/*++

Copyright (C) 2026 Autodesk Inc.

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

#include "amc_api_handler_download.hpp"

#include "libmc_interfaceexception.hpp"
#include "libmcdata_dynamic.hpp"
#include "common_utils.hpp"
#include "common_chrono.hpp"
#include "amc_api.hpp"
#include "amc_api_constants.hpp"
#include "amc_streamdownloadregistry.hpp"

using namespace AMC;

CAPIHandler_Download::CAPIHandler_Download(PSystemState pSystemState)
	: CAPIHandler(pSystemState->getClientHash()), m_pSystemState(pSystemState)
{
	if (pSystemState.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
}

CAPIHandler_Download::~CAPIHandler_Download()
{
}

std::string CAPIHandler_Download::getBaseURI()
{
	return "api/download";
}

void CAPIHandler_Download::checkAuthorizationMode(const std::string& sURI, const eAPIRequestType requestType, bool& bNeedsToBeAuthorized, bool& bCreateNewSession)
{
	sURI;
	requestType;

	// Download ticket is unique and validated server-side; auth token not required.
	bNeedsToBeAuthorized = false;
	bCreateNewSession = false;
}

PAPIResponse CAPIHandler_Download::handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields& pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	pFormFields;
	pBodyData;
	nBodyDataSize;

	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	bool bHeadOnly = (requestType == eAPIRequestType::rtHead);
	if ((requestType != eAPIRequestType::rtGet) && (!bHeadOnly))
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	auto sParameterString = sURI.substr(getBaseURI().length());
	if ((sParameterString.length() < 37) || (sParameterString[0] != '/'))
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	std::string sDownloadTicketUUID = AMCCommon::CUtils::normalizeUUIDString(sParameterString.substr(1, 36));

	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pStorage = pDataModel->CreateStorage();
	auto pGlobalChrono = m_pSystemState->getGlobalChronoInstance();
	uint64_t nNow = pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970();

	std::string sStreamUUID;
	std::string sSessionUUID;
	std::string sUserUUID;
	std::string sDownloadFileName;
	uint64_t nCreationTimeStamp = 0;
	bool bRevoked = false;
	pStorage->GetDownloadTicketDetails(sDownloadTicketUUID, sStreamUUID, sDownloadFileName, sSessionUUID, sUserUUID, nCreationTimeStamp, bRevoked);

	// A revoked ticket (cancelled download) or an old link gets 410, so that a browser never resumes it.
	uint64_t nLifetime = (uint64_t)AMC_API_DOWNLOADTICKET_LIFETIME_SECONDS * 1000000ULL;
	if (bRevoked || (nNow > nCreationTimeStamp + nLifetime))
		return CAPI::makeError(AMC_API_HTTP_GONE, LIBMC_ERROR_DOWNLOADTICKETEXPIRED, "download ticket has expired or has been revoked: " + sDownloadTicketUUID);

	if (!pStorage->StreamIsReady(sStreamUUID))
		throw ELibMCInterfaceException(LIBMC_ERROR_STORAGESTREAMNOTFOUND, "download stream not found: " + sStreamUUID);

	// Every download holds its own stream object and file handle; the response streams it in chunks.
	auto pStream = pStorage->RetrieveStream(sStreamUUID);
	auto pResponse = std::make_shared<CAPIStorageStreamResponse>(pStorage, pStream, m_pSystemState->getStreamDownloadRegistryInstance(), pGlobalChrono, bHeadOnly);
	pResponse->setContentDispositionName(sDownloadFileName);

	if (!pResponse->reserveSlot())
		return CAPI::makeError(AMC_API_HTTP_SERVICEUNAVAILABLE, LIBMC_ERROR_TOOMANYDOWNLOADS, "too many concurrent downloads");

	if (!bHeadOnly)
		pResponse->setDownloadUUID(pStorage->LogDownloadStart(sDownloadTicketUUID, pFormFields.getClientAddress(), nNow));

	return pResponse;
}

