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


#include "amc_api_handler_firstrun.hpp"
#include "amc_api_jsonrequest.hpp"
#include "amc_accesscontrol.hpp"
#include "amc_accessrole.hpp"
#include "amc_firstrunhandler.hpp"

#include "libmc_interfaceexception.hpp"
#include "libmc_exceptiontypes.hpp"

#include "common_utils.hpp"
#include "common_chrono.hpp"

#define FIRSTRUN_BASEURI "api/firstrun"

using namespace AMC;

static std::string firstRunDisplayName(const CLanguageString& displayName, const std::string& sIdentifier)
{
	std::string sDisplayName = displayName.getCustomValue();
	if (sDisplayName.empty())
		return sIdentifier;
	return sDisplayName;
}

CAPIHandler_FirstRun::CAPIHandler_FirstRun(PSystemState pSystemState, PUserManagementService pService)
	: CAPIHandler(pSystemState->getClientHash()), m_pSystemState(pSystemState), m_pService(pService)
{
	if (pSystemState.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (pService.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
}

CAPIHandler_FirstRun::~CAPIHandler_FirstRun()
{

}

std::string CAPIHandler_FirstRun::getBaseURI()
{
	return FIRSTRUN_BASEURI;
}

void CAPIHandler_FirstRun::checkAuthorizationMode(const std::string& sURI, const eAPIRequestType requestType, bool& bNeedsToBeAuthorized, bool& bCreateNewSession)
{
	// While no user exists, nobody can log in.
	bNeedsToBeAuthorized = false;
	bCreateNewSession = false;
}

bool CAPIHandler_FirstRun::expectsRawBody(const std::string& sURI, const eAPIRequestType requestType)
{
	return (requestType == eAPIRequestType::rtPost);
}

APIHandler_FirstRunType CAPIHandler_FirstRun::parseRequest(const std::string& sURI, const eAPIRequestType requestType)
{
	std::string sBaseURI = FIRSTRUN_BASEURI;
	if (sURI.length() < sBaseURI.length())
		return APIHandler_FirstRunType::frtUnknown;

	std::string sParameterString = AMCCommon::CUtils::toLowerString(sURI.substr(sBaseURI.length()));
	auto nQueryPosition = sParameterString.find('?');
	if (nQueryPosition != std::string::npos)
		sParameterString = sParameterString.substr(0, nQueryPosition);

	if ((requestType == eAPIRequestType::rtGet) && ((sParameterString == "") || (sParameterString == "/")))
		return APIHandler_FirstRunType::frtInformation;

	if ((requestType == eAPIRequestType::rtPost) && ((sParameterString == "/admin") || (sParameterString == "/admin/")))
		return APIHandler_FirstRunType::frtCreateAdministrator;

	return APIHandler_FirstRunType::frtUnknown;
}

void CAPIHandler_FirstRun::handleInformationRequest(CJSONWriter& writer, PAPIAuth pAuth)
{
	auto pFirstRunHandler = m_pSystemState->getFirstRunHandlerInstance();

	bool bIsFirstRun = pFirstRunHandler->isFirstRun();
	writer.addBoolean(AMC_API_KEY_FIRSTRUN_FIRSTRUN, bIsFirstRun);

	// Outside of first-run mode nothing else is revealed.
	if (!bIsFirstRun)
		return;

	auto pAccessControl = m_pSystemState->accessControl();
	auto pPolicy = pAccessControl->getUserManagementPolicy();

	std::string sAdminRole = pFirstRunHandler->getAdminRoleIdentifier();
	PAccessRole pAdminRole;
	if (!sAdminRole.empty())
		pAdminRole = pAccessControl->findRole(sAdminRole, false);

	writer.addBoolean(AMC_API_KEY_FIRSTRUN_AVAILABLE, pAdminRole.get() != nullptr);
	writer.addBoolean(AMC_API_KEY_FIRSTRUN_SETUPCODEREQUIRED, pFirstRunHandler->setupCodeIsRequiredFor(pAuth->getClientAddress()));
	writer.addString(AMC_API_KEY_FIRSTRUN_WELCOMETITLE, pFirstRunHandler->getWelcomeTitle());
	writer.addString(AMC_API_KEY_FIRSTRUN_WELCOMETEXT, pFirstRunHandler->getWelcomeText());
	writer.addString(AMC_API_KEY_FIRSTRUN_GITHASH, m_pSystemState->getGitHash());

	CJSONWriterArray nextStepsArray(writer);
	for (auto& nextStep : pFirstRunHandler->getNextSteps()) {
		CJSONWriterObject nextStepObject(writer);
		nextStepObject.addString(AMC_API_KEY_FIRSTRUN_TEXT, nextStep.m_sText);
		nextStepObject.addString(AMC_API_KEY_FIRSTRUN_PAGE, nextStep.m_sPage);
		nextStepsArray.addObject(nextStepObject);
	}
	writer.addArray(AMC_API_KEY_FIRSTRUN_NEXTSTEPS, nextStepsArray);

	if (pAdminRole.get() != nullptr) {
		CJSONWriterObject roleObject(writer);
		roleObject.addString(AMC_API_KEY_USERS_IDENTIFIER, pAdminRole->getIdentifier());
		roleObject.addString(AMC_API_KEY_USERS_DISPLAYNAME, firstRunDisplayName(pAdminRole->getDisplayName(), pAdminRole->getIdentifier()));
		roleObject.addString(AMC_API_KEY_USERS_DESCRIPTION, pAdminRole->getDescription().getCustomValue());

		CJSONWriterArray permissionsArray(writer);
		for (auto pPermission : pAdminRole->getPermissions()) {
			CJSONWriterObject permissionObject(writer);
			permissionObject.addString(AMC_API_KEY_USERS_IDENTIFIER, pPermission->getIdentifier());
			permissionObject.addString(AMC_API_KEY_USERS_DISPLAYNAME, firstRunDisplayName(pPermission->getDisplayName(), pPermission->getIdentifier()));
			permissionsArray.addObject(permissionObject);
		}
		roleObject.addArray(AMC_API_KEY_USERS_PERMISSIONS, permissionsArray);

		writer.addObject(AMC_API_KEY_FIRSTRUN_ADMINROLE, roleObject);
	}

	CJSONWriterArray languagesArray(writer);
	for (auto& language : pPolicy->getLanguages()) {
		CJSONWriterObject languageObject(writer);
		languageObject.addString(AMC_API_KEY_USERS_IDENTIFIER, language.m_sIdentifier);
		languageObject.addString(AMC_API_KEY_USERS_DISPLAYNAME, language.m_sDisplayName);
		languagesArray.addObject(languageObject);
	}
	writer.addArray(AMC_API_KEY_USERS_LANGUAGES, languagesArray);

	CJSONWriterObject passwordPolicyObject(writer);
	passwordPolicyObject.addInteger(AMC_API_KEY_USERS_MINIMUMLENGTH, pPolicy->getPasswordMinimumLength());
	passwordPolicyObject.addBool(AMC_API_KEY_USERS_REQUIREDIGIT, pPolicy->passwordRequiresDigit());
	passwordPolicyObject.addBool(AMC_API_KEY_USERS_REQUIRELETTER, pPolicy->passwordRequiresLetter());
	writer.addObject(AMC_API_KEY_USERS_PASSWORDPOLICY, passwordPolicyObject);
}

void CAPIHandler_FirstRun::handleCreateAdministratorRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth)
{
	auto pFirstRunHandler = m_pSystemState->getFirstRunHandlerInstance();
	std::string sClientAddress = pAuth->getClientAddress();

	if (!pFirstRunHandler->isFirstRun())
		throw ELibMCInterfaceException(LIBMC_ERROR_FIRSTRUNALREADYCOMPLETED);

	std::string sAdminRole = pFirstRunHandler->getAdminRoleIdentifier();
	if (sAdminRole.empty())
		throw ELibMCInterfaceException(LIBMC_ERROR_FIRSTRUNNOTAVAILABLE);

	std::string sSetupCode = request.hasValue(AMC_API_KEY_FIRSTRUN_SETUPCODE) ? request.getRawString(AMC_API_KEY_FIRSTRUN_SETUPCODE, LIBMC_ERROR_INVALIDSETUPCODE) : "";
	pFirstRunHandler->verifySetupCode(sSetupCode, sClientAddress, m_pSystemState->globalChrono()->getUTCTimeStampInMicrosecondsSince1970());

	std::string sLogin = request.getRawString(AMC_API_KEY_USERS_LOGIN, LIBMC_ERROR_INVALIDUSERNAME);
	std::string sDescription = request.hasValue(AMC_API_KEY_USERS_DESCRIPTION) ? request.getRawString(AMC_API_KEY_USERS_DESCRIPTION, LIBMC_ERROR_INVALIDUSERDESCRIPTION) : "";
	std::string sLanguage = request.hasValue(AMC_API_KEY_USERS_LANGUAGE) ? request.getRawString(AMC_API_KEY_USERS_LANGUAGE, LIBMC_ERROR_INVALIDLANGUAGEIDENTIFIER) : "";
	std::string sSalt = request.getSHA256(AMC_API_KEY_USERS_SALT, LIBMC_ERROR_INVALIDPASSWORD);
	std::string sPasswordHash = request.getSHA256(AMC_API_KEY_USERS_PASSWORDHASH, LIBMC_ERROR_INVALIDPASSWORD);

	std::string sUserUUID = m_pService->createFirstAdministrator(sLogin, sDescription, sAdminRole, sLanguage, sSalt, sPasswordHash);

	pFirstRunHandler->complete(sLogin, sClientAddress);

	writer.addString(AMC_API_KEY_FIRSTRUN_USERUUID, sUserUUID);
	writer.addBoolean(AMC_API_KEY_USERS_SUCCESS, true);
}

PAPIResponse CAPIHandler_FirstRun::handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields& pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	auto requestSubType = parseRequest(sURI, requestType);
	if (requestSubType == APIHandler_FirstRunType::frtUnknown)
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDPARAM, "unknown first-run request: " + sURI);

	CJSONWriter writer;
	writeJSONHeader(writer, AMC_API_PROTOCOL_FIRSTRUN);

	switch (requestSubType) {
	case APIHandler_FirstRunType::frtInformation:
		handleInformationRequest(writer, pAuth);
		break;

	case APIHandler_FirstRunType::frtCreateAdministrator: {
		if ((pBodyData == nullptr) || (nBodyDataSize == 0))
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

		CAPIJSONRequest request(pBodyData, nBodyDataSize);
		handleCreateAdministratorRequest(writer, request, pAuth);
		break;
	}

	default:
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	}

	return std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, AMC_API_CONTENTTYPE, writer.saveToString());
}
