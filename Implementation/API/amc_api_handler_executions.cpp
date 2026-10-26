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

#define __AMCIMPL_API_CONSTANTS

#include "amc_api_handler_executions.hpp"
#include "amc_api_executionlistentry.hpp"
#include "amc_api_constants.hpp"
#include "libmc_interfaceexception.hpp"
#include "libmcdata_dynamic.hpp"
#include "common_utils.hpp"

#include <vector>
#include <memory>
#include <string>

using namespace AMC;

CAPIHandler_Executions::CAPIHandler_Executions(PSystemState pSystemState)
	: CAPIHandler(pSystemState->getClientHash()), m_pSystemState(pSystemState)
{
	if (pSystemState.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
}

CAPIHandler_Executions::~CAPIHandler_Executions()
{
}

std::string CAPIHandler_Executions::getBaseURI()
{
	return "api/executions";
}

void CAPIHandler_Executions::handleListExecutionsRequest(CJSONWriter& writer, CAPIFormFields& pFormFields)
{
	auto pDataModel = m_pSystemState->getDataModelInstance();
	auto pBuildJobHandler = pDataModel->CreateBuildJobHandler();
	auto pGlobalChrono = m_pSystemState->globalChrono();

	// The head must be read before the rows, so that a change in between is fetched again by the next delta.
	writer.addInteger(AMC_API_KEY_EXECUTIONS_HEADID, (int64_t) pBuildJobHandler->GetExecutionListHeadID());

	uint64_t nSinceHeadID = 0;
	LibMCData::PBuildJobExecutionIterator pExecutionIterator;
	if (pFormFields.getOptionalUint64RequestParameter(AMC_API_KEY_LIST_SINCE, nSinceHeadID)) {
		writer.addBoolean(AMC_API_KEY_LIST_DELTA, true);
		pExecutionIterator = pBuildJobHandler->ListJobExecutionsChangedSince(nSinceHeadID);
	}
	else {
		pExecutionIterator = pBuildJobHandler->ListJobExecutions("", "", "");
	}

	CJSONWriterArray executionArray(writer);

	// Every entry is written from the values the list query loaded, so the list costs one query
	// regardless of its length. One timer value for all entries keeps their durations consistent.
	uint64_t nGlobalTimer = pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970();
	while (pExecutionIterator->MoveNext()) {
		CJSONWriterObject entryObject(writer);
		writeExecutionListEntryToJSON(entryObject, pExecutionIterator->GetCurrentJobExecution(), nGlobalTimer);
		executionArray.addObject(entryObject);
	}

	writer.addArray("executions", executionArray);
}

PAPIResponse CAPIHandler_Executions::handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields& pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	if (requestType == eAPIRequestType::rtGet) {
		CJSONWriter writer;
		writeJSONHeader(writer, AMC_API_PROTOCOL_BUILD);
		handleListExecutionsRequest(writer, pFormFields);
		return std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, AMC_API_CONTENTTYPE, writer.saveToString());
	}

	throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
}
