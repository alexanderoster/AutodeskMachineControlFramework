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


#include "amc_api_handler_users.hpp"
#include "amc_api_jsonrequest.hpp"
#include "amc_accesscontrol.hpp"
#include "amc_userinformation.hpp"

#include "libmc_interfaceexception.hpp"
#include "libmc_exceptiontypes.hpp"

#include "common_utils.hpp"
#include "common_chrono.hpp"

#include <string>

#define USERS_BASEURI "api/users"

using namespace AMC;

static std::vector<std::string> splitURIPath(const std::string& sPath)
{
	std::vector<std::string> segments;
	std::string sCurrent;
	for (char c : sPath) {
		if (c == '/') {
			if (!sCurrent.empty())
				segments.push_back(sCurrent);
			sCurrent.clear();
		}
		else {
			sCurrent.push_back(c);
		}
	}
	if (!sCurrent.empty())
		segments.push_back(sCurrent);

	return segments;
}

static std::string displayNameOrIdentifier(const CLanguageString& displayName, const std::string& sIdentifier)
{
	std::string sDisplayName = displayName.getCustomValue();
	if (sDisplayName.empty())
		return sIdentifier;
	return sDisplayName;
}

CAPIHandler_Users::CAPIHandler_Users(PSystemState pSystemState, PUserManagementService pService)
	: CAPIHandler(pSystemState->getClientHash()), m_pSystemState(pSystemState), m_pService(pService)
{
	if (pSystemState.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (pService.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
}

CAPIHandler_Users::~CAPIHandler_Users()
{

}

std::string CAPIHandler_Users::getBaseURI()
{
	return USERS_BASEURI;
}

bool CAPIHandler_Users::expectsRawBody(const std::string& sURI, const eAPIRequestType requestType)
{
	return (requestType == eAPIRequestType::rtPost);
}

APIHandler_UsersType CAPIHandler_Users::parseRequest(const std::string& sURI, const eAPIRequestType requestType, std::string& sUserUUID)
{
	sUserUUID = "";

	std::string sBaseURI = USERS_BASEURI;
	if (sURI.length() < sBaseURI.length())
		return APIHandler_UsersType::utUnknown;

	std::string sParameterString = AMCCommon::CUtils::toLowerString(sURI.substr(sBaseURI.length()));
	auto nQueryPosition = sParameterString.find('?');
	if (nQueryPosition != std::string::npos)
		sParameterString = sParameterString.substr(0, nQueryPosition);

	if ((!sParameterString.empty()) && (sParameterString.at(0) != '/'))
		return APIHandler_UsersType::utUnknown;

	auto segments = splitURIPath(sParameterString);
	size_t nCount = segments.size();

	bool bFirstIsUUID = (nCount > 0) && AMCCommon::CUtils::stringIsUUIDString(segments.at(0));
	if (bFirstIsUUID)
		sUserUUID = AMCCommon::CUtils::normalizeUUIDString(segments.at(0));

	if (requestType == eAPIRequestType::rtGet) {
		if (nCount == 0)
			return APIHandler_UsersType::utList;
		if ((nCount == 1) && (segments.at(0) == "meta"))
			return APIHandler_UsersType::utMeta;
		if ((nCount == 1) && (segments.at(0) == "me"))
			return APIHandler_UsersType::utGetOwnUser;
		if ((nCount == 1) && bFirstIsUUID)
			return APIHandler_UsersType::utGetUser;
		if ((nCount == 2) && bFirstIsUUID && (segments.at(1) == "sessions"))
			return APIHandler_UsersType::utGetSessions;
	}

	if (requestType == eAPIRequestType::rtPost) {
		if (nCount == 0)
			return APIHandler_UsersType::utCreateUser;
		if ((nCount == 1) && (segments.at(0) == "me"))
			return APIHandler_UsersType::utUpdateOwnProfile;
		if ((nCount == 2) && (segments.at(0) == "me") && (segments.at(1) == "passwordchallenge"))
			return APIHandler_UsersType::utPasswordChallenge;
		if ((nCount == 2) && (segments.at(0) == "me") && (segments.at(1) == "password"))
			return APIHandler_UsersType::utChangeOwnPassword;
		if ((nCount == 1) && bFirstIsUUID)
			return APIHandler_UsersType::utUpdateUser;
		if ((nCount == 2) && bFirstIsUUID && (segments.at(1) == "password"))
			return APIHandler_UsersType::utResetPassword;
		if ((nCount == 2) && bFirstIsUUID && (segments.at(1) == "delete"))
			return APIHandler_UsersType::utDeleteUser;
		if ((nCount == 3) && bFirstIsUUID && (segments.at(1) == "sessions") && (segments.at(2) == "revoke"))
			return APIHandler_UsersType::utRevokeSessions;
	}

	return APIHandler_UsersType::utUnknown;
}

void CAPIHandler_Users::writeUser(CJSONWriterObject& userObject, const CUserManagementUser& user, PUserInformation pCaller, const std::vector<CAPIUserSessionInfo>& sessions)
{
	auto pRole = m_pSystemState->accessControl()->findRole(user.m_sRoleIdentifier, false);

	uint64_t nLastActivity = 0;
	for (auto& session : sessions) {
		if (session.m_nLastActivityMicroseconds > nLastActivity)
			nLastActivity = session.m_nLastActivityMicroseconds;
	}

	// Salt and password hash are never part of a response.
	userObject.addString(AMC_API_KEY_USERS_UUID, user.m_sUUID);
	userObject.addString(AMC_API_KEY_USERS_LOGIN, user.m_sLogin);
	userObject.addString(AMC_API_KEY_USERS_DESCRIPTION, user.m_sDescription);
	userObject.addString(AMC_API_KEY_USERS_ROLE, user.m_sRoleIdentifier);
	userObject.addString(AMC_API_KEY_USERS_ROLENAME, (pRole.get() != nullptr) ? displayNameOrIdentifier(pRole->getDisplayName(), pRole->getIdentifier()) : user.m_sRoleIdentifier);
	userObject.addString(AMC_API_KEY_USERS_LANGUAGE, user.m_sLanguageIdentifier);
	userObject.addBool(AMC_API_KEY_USERS_ISSELF, m_pService->userIsSelf(pCaller, user.m_sUUID));
	userObject.addBool(AMC_API_KEY_USERS_MANAGEABLE, m_pService->targetIsManageable(pCaller, user));
	userObject.addBool(AMC_API_KEY_USERS_ONLINE, !sessions.empty());
	userObject.addInteger(AMC_API_KEY_USERS_SESSIONCOUNT, (int64_t)sessions.size());
	userObject.addInteger(AMC_API_KEY_USERS_LASTACTIVITY, (int64_t)nLastActivity);
}

void CAPIHandler_Users::handleMetaRequest(CJSONWriter& writer, PAPIAuth pAuth)
{
	auto pCaller = pAuth->getUserInformation();
	auto pAccessControl = m_pSystemState->accessControl();
	auto pPolicy = pAccessControl->getUserManagementPolicy();

	CJSONWriterObject capabilitiesObject(writer);
	for (auto capability : CUserManagementPolicy::getAllCapabilities())
		capabilitiesObject.addBool(CUserManagementPolicy::capabilityToString(capability), m_pService->hasCapability(pCaller, capability));
	writer.addObject(AMC_API_KEY_USERS_CAPABILITIES, capabilitiesObject);

	std::string sDefaultRole = pAccessControl->getDefaultRole()->getIdentifier();

	CJSONWriterArray rolesArray(writer);
	for (auto pRole : pAccessControl->getRoles()) {
		CJSONWriterObject roleObject(writer);
		roleObject.addString(AMC_API_KEY_USERS_IDENTIFIER, pRole->getIdentifier());
		roleObject.addString(AMC_API_KEY_USERS_DISPLAYNAME, displayNameOrIdentifier(pRole->getDisplayName(), pRole->getIdentifier()));
		roleObject.addString(AMC_API_KEY_USERS_DESCRIPTION, pRole->getDescription().getCustomValue());

		CJSONWriterArray rolePermissionsArray(writer);
		for (auto pPermission : pRole->getPermissions())
			rolePermissionsArray.addString(pPermission->getIdentifier());
		roleObject.addArray(AMC_API_KEY_USERS_PERMISSIONS, rolePermissionsArray);

		roleObject.addBool(AMC_API_KEY_USERS_ASSIGNABLE, m_pService->roleIsAssignableBy(pCaller, pRole->getIdentifier()));
		roleObject.addBool(AMC_API_KEY_USERS_ISDEFAULT, pRole->getIdentifier() == sDefaultRole);
		roleObject.addBool(AMC_API_KEY_USERS_DEPRECATED, pRole->isDeprecated());
		rolesArray.addObject(roleObject);
	}
	writer.addArray(AMC_API_KEY_USERS_ROLES, rolesArray);

	CJSONWriterArray permissionsArray(writer);
	for (auto pPermission : pAccessControl->getPermissions()) {
		CJSONWriterObject permissionObject(writer);
		permissionObject.addString(AMC_API_KEY_USERS_IDENTIFIER, pPermission->getIdentifier());
		permissionObject.addString(AMC_API_KEY_USERS_DISPLAYNAME, displayNameOrIdentifier(pPermission->getDisplayName(), pPermission->getIdentifier()));
		permissionObject.addString(AMC_API_KEY_USERS_DESCRIPTION, pPermission->getDescription().getCustomValue());
		permissionsArray.addObject(permissionObject);
	}
	writer.addArray(AMC_API_KEY_USERS_PERMISSIONS, permissionsArray);

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

	CJSONWriterObject selfObject(writer);
	selfObject.addString(AMC_API_KEY_USERS_UUID, AMCCommon::CUtils::normalizeUUIDString(pCaller->getUUID()));
	selfObject.addString(AMC_API_KEY_USERS_LOGIN, pCaller->getLogin());
	selfObject.addString(AMC_API_KEY_USERS_ROLE, pCaller->getRoleIdentifier());
	writer.addObject(AMC_API_KEY_USERS_SELF, selfObject);

	bool bShowDefaultTestUser = m_pService->hasCapability(pCaller, eUserManagementCapability::ViewUsers) && m_pService->defaultTestUserPresent();
	writer.addBoolean(AMC_API_KEY_USERS_DEFAULTTESTUSER, bShowDefaultTestUser);
}

void CAPIHandler_Users::handleListRequest(CJSONWriter& writer, PAPIAuth pAuth)
{
	auto pCaller = pAuth->getUserInformation();
	auto users = m_pService->listUsers(pCaller);
	auto sessionMap = m_pService->getSessionsOfAllUsers(pCaller);

	CJSONWriterArray usersArray(writer);
	for (auto& user : users) {
		CJSONWriterObject userObject(writer);
		auto iSessions = sessionMap.find(user.m_sUUID);
		if (iSessions != sessionMap.end())
			writeUser(userObject, user, pCaller, iSessions->second);
		else
			writeUser(userObject, user, pCaller, std::vector<CAPIUserSessionInfo>());
		usersArray.addObject(userObject);
	}
	writer.addArray(AMC_API_KEY_USERS_USERS, usersArray);
	writer.addInteger(AMC_API_KEY_USERS_SERVERTIME, (int64_t)m_pSystemState->globalChrono()->getUTCTimeStampInMicrosecondsSince1970());
}

void CAPIHandler_Users::handleGetUserRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& sUserUUID)
{
	auto pCaller = pAuth->getUserInformation();
	auto user = m_pService->getUser(pCaller, sUserUUID);

	std::vector<CAPIUserSessionInfo> sessions;
	if (m_pService->userIsSelf(pCaller, user.m_sUUID)) {
		sessions = m_pService->getSessionsOfUser(pCaller, user.m_sUUID);
	}
	else {
		auto sessionMap = m_pService->getSessionsOfAllUsers(pCaller);
		auto iSessions = sessionMap.find(user.m_sUUID);
		if (iSessions != sessionMap.end())
			sessions = iSessions->second;
	}

	CJSONWriterObject userObject(writer);
	writeUser(userObject, user, pCaller, sessions);
	writer.addObject(AMC_API_KEY_USERS_USER, userObject);
	writer.addInteger(AMC_API_KEY_USERS_SERVERTIME, (int64_t)m_pSystemState->globalChrono()->getUTCTimeStampInMicrosecondsSince1970());
}

void CAPIHandler_Users::handleGetSessionsRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& sUserUUID)
{
	auto pCaller = pAuth->getUserInformation();
	auto sessions = m_pService->getSessionsOfUser(pCaller, sUserUUID);

	// Session UUIDs are bearer secrets and are not exposed.
	CJSONWriterArray sessionsArray(writer);
	for (auto& session : sessions) {
		CJSONWriterObject sessionObject(writer);
		sessionObject.addInteger(AMC_API_KEY_USERS_LASTACTIVITY, (int64_t)session.m_nLastActivityMicroseconds);
		sessionObject.addBool(AMC_API_KEY_USERS_ISSELF, session.m_sSessionUUID == pAuth->getSessionUUID());
		sessionsArray.addObject(sessionObject);
	}
	writer.addArray(AMC_API_KEY_USERS_SESSIONS, sessionsArray);
	writer.addInteger(AMC_API_KEY_USERS_SERVERTIME, (int64_t)m_pSystemState->globalChrono()->getUTCTimeStampInMicrosecondsSince1970());
}

void CAPIHandler_Users::handleCreateUserRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth)
{
	auto pCaller = pAuth->getUserInformation();

	std::string sLogin = request.getNameString(AMC_API_KEY_USERS_LOGIN, LIBMC_ERROR_INVALIDUSERNAME);
	std::string sDescription = request.hasValue(AMC_API_KEY_USERS_DESCRIPTION) ? request.getRawString(AMC_API_KEY_USERS_DESCRIPTION, LIBMC_ERROR_INVALIDUSERDESCRIPTION) : "";
	std::string sRole = request.hasValue(AMC_API_KEY_USERS_ROLE) ? request.getRawString(AMC_API_KEY_USERS_ROLE, LIBMC_ERROR_ROLENOTFOUND) : "";
	std::string sLanguage = request.hasValue(AMC_API_KEY_USERS_LANGUAGE) ? request.getRawString(AMC_API_KEY_USERS_LANGUAGE, LIBMC_ERROR_INVALIDLANGUAGEIDENTIFIER) : "";
	std::string sSalt = request.getSHA256(AMC_API_KEY_USERS_SALT, LIBMC_ERROR_INVALIDPASSWORD);
	std::string sPasswordHash = request.getSHA256(AMC_API_KEY_USERS_PASSWORDHASH, LIBMC_ERROR_INVALIDPASSWORD);

	std::string sUUID = m_pService->createUser(pCaller, sLogin, sDescription, sRole, sLanguage, sSalt, sPasswordHash);

	writer.addString(AMC_API_KEY_USERS_UUID, sUUID);
	writer.addBoolean(AMC_API_KEY_USERS_SUCCESS, true);
}

void CAPIHandler_Users::handleUpdateUserRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth, const std::string& sUserUUID)
{
	auto pCaller = pAuth->getUserInformation();

	bool bUpdateDescription = request.hasValue(AMC_API_KEY_USERS_DESCRIPTION);
	bool bUpdateLanguage = request.hasValue(AMC_API_KEY_USERS_LANGUAGE);
	bool bUpdateRole = request.hasValue(AMC_API_KEY_USERS_ROLE);

	std::string sDescription = bUpdateDescription ? request.getRawString(AMC_API_KEY_USERS_DESCRIPTION, LIBMC_ERROR_INVALIDUSERDESCRIPTION) : "";
	std::string sLanguage = bUpdateLanguage ? request.getRawString(AMC_API_KEY_USERS_LANGUAGE, LIBMC_ERROR_INVALIDLANGUAGEIDENTIFIER) : "";
	std::string sRole = bUpdateRole ? request.getRawString(AMC_API_KEY_USERS_ROLE, LIBMC_ERROR_ROLENOTFOUND) : "";

	m_pService->updateUser(pCaller, sUserUUID, bUpdateDescription, sDescription, bUpdateLanguage, sLanguage, bUpdateRole, sRole);

	writer.addBoolean(AMC_API_KEY_USERS_SUCCESS, true);
}

void CAPIHandler_Users::handleResetPasswordRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth, const std::string& sUserUUID)
{
	auto pCaller = pAuth->getUserInformation();

	std::string sSalt = request.getSHA256(AMC_API_KEY_USERS_SALT, LIBMC_ERROR_INVALIDPASSWORD);
	std::string sPasswordHash = request.getSHA256(AMC_API_KEY_USERS_PASSWORDHASH, LIBMC_ERROR_INVALIDPASSWORD);

	m_pService->resetPassword(pCaller, sUserUUID, sSalt, sPasswordHash);

	writer.addBoolean(AMC_API_KEY_USERS_SUCCESS, true);
}

void CAPIHandler_Users::handleDeleteUserRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& sUserUUID)
{
	m_pService->deleteUser(pAuth->getUserInformation(), sUserUUID);

	writer.addBoolean(AMC_API_KEY_USERS_SUCCESS, true);
}

void CAPIHandler_Users::handleRevokeSessionsRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& sUserUUID)
{
	uint32_t nRevoked = m_pService->revokeSessions(pAuth->getUserInformation(), pAuth->getSessionUUID(), sUserUUID);

	writer.addInteger(AMC_API_KEY_USERS_REVOKED, nRevoked);
	writer.addBoolean(AMC_API_KEY_USERS_SUCCESS, true);
}

void CAPIHandler_Users::handleUpdateOwnProfileRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth)
{
	auto pCaller = pAuth->getUserInformation();

	// The role can never be changed through the own profile.
	bool bUpdateDescription = request.hasValue(AMC_API_KEY_USERS_DESCRIPTION);
	bool bUpdateLanguage = request.hasValue(AMC_API_KEY_USERS_LANGUAGE);

	std::string sDescription = bUpdateDescription ? request.getRawString(AMC_API_KEY_USERS_DESCRIPTION, LIBMC_ERROR_INVALIDUSERDESCRIPTION) : "";
	std::string sLanguage = bUpdateLanguage ? request.getRawString(AMC_API_KEY_USERS_LANGUAGE, LIBMC_ERROR_INVALIDLANGUAGEIDENTIFIER) : "";

	m_pService->updateOwnProfile(pCaller, bUpdateDescription, sDescription, bUpdateLanguage, sLanguage);

	writer.addBoolean(AMC_API_KEY_USERS_SUCCESS, true);
}

void CAPIHandler_Users::handlePasswordChallengeRequest(CJSONWriter& writer, PAPIAuth pAuth)
{
	auto challenge = m_pService->createPasswordChallenge(pAuth->getUserInformation(), pAuth->getSessionUUID());

	writer.addString(AMC_API_KEY_USERS_CHALLENGEUUID, challenge.m_sChallengeUUID);
	writer.addString(AMC_API_KEY_USERS_SALT, challenge.m_sSalt);
	writer.addString(AMC_API_KEY_USERS_NONCE, challenge.m_sNonce);
}

void CAPIHandler_Users::handleChangeOwnPasswordRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth)
{
	std::string sChallengeUUID = request.getUUID(AMC_API_KEY_USERS_CHALLENGEUUID, LIBMC_ERROR_PASSWORDCHALLENGENOTFOUND);
	std::string sProof = request.getSHA256(AMC_API_KEY_USERS_PROOF, LIBMC_ERROR_INVALIDPASSWORDPROOF);
	std::string sSalt = request.getSHA256(AMC_API_KEY_USERS_SALT, LIBMC_ERROR_INVALIDPASSWORD);
	std::string sPasswordHash = request.getSHA256(AMC_API_KEY_USERS_PASSWORDHASH, LIBMC_ERROR_INVALIDPASSWORD);

	m_pService->changeOwnPassword(pAuth->getUserInformation(), pAuth->getSessionUUID(), sChallengeUUID, sProof, sSalt, sPasswordHash);

	writer.addBoolean(AMC_API_KEY_USERS_SUCCESS, true);
}

PAPIResponse CAPIHandler_Users::handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields& pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (!pAuth->userIsAuthorized())
		throw ELibMCInterfaceException(LIBMC_ERROR_SESSIONNOTAUTHORIZED);

	std::string sUserUUID;
	auto requestSubType = parseRequest(sURI, requestType, sUserUUID);
	if (requestSubType == APIHandler_UsersType::utUnknown)
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDPARAM, "unknown users request: " + sURI);

	// Requests without arguments may come with an empty body.
	std::string sEmptyBody = "{}";
	const uint8_t* pRequestData = pBodyData;
	size_t nRequestSize = nBodyDataSize;
	if ((pRequestData == nullptr) || (nRequestSize == 0)) {
		pRequestData = (const uint8_t*)sEmptyBody.c_str();
		nRequestSize = sEmptyBody.length();
	}

	CJSONWriter writer;
	writeJSONHeader(writer, AMC_API_PROTOCOL_USERS);

	switch (requestSubType) {
	case APIHandler_UsersType::utMeta:
		handleMetaRequest(writer, pAuth);
		break;
	case APIHandler_UsersType::utList:
		handleListRequest(writer, pAuth);
		break;
	case APIHandler_UsersType::utGetUser:
		handleGetUserRequest(writer, pAuth, sUserUUID);
		break;
	case APIHandler_UsersType::utGetOwnUser:
		handleGetUserRequest(writer, pAuth, pAuth->getUserUUID());
		break;
	case APIHandler_UsersType::utGetSessions:
		handleGetSessionsRequest(writer, pAuth, sUserUUID);
		break;
	case APIHandler_UsersType::utCreateUser: {
		CAPIJSONRequest request(pRequestData, nRequestSize);
		handleCreateUserRequest(writer, request, pAuth);
		break;
	}
	case APIHandler_UsersType::utUpdateUser: {
		CAPIJSONRequest request(pRequestData, nRequestSize);
		handleUpdateUserRequest(writer, request, pAuth, sUserUUID);
		break;
	}
	case APIHandler_UsersType::utResetPassword: {
		CAPIJSONRequest request(pRequestData, nRequestSize);
		handleResetPasswordRequest(writer, request, pAuth, sUserUUID);
		break;
	}
	case APIHandler_UsersType::utDeleteUser:
		handleDeleteUserRequest(writer, pAuth, sUserUUID);
		break;
	case APIHandler_UsersType::utRevokeSessions:
		handleRevokeSessionsRequest(writer, pAuth, sUserUUID);
		break;
	case APIHandler_UsersType::utUpdateOwnProfile: {
		CAPIJSONRequest request(pRequestData, nRequestSize);
		handleUpdateOwnProfileRequest(writer, request, pAuth);
		break;
	}
	case APIHandler_UsersType::utPasswordChallenge:
		handlePasswordChallengeRequest(writer, pAuth);
		break;
	case APIHandler_UsersType::utChangeOwnPassword: {
		CAPIJSONRequest request(pRequestData, nRequestSize);
		handleChangeOwnPasswordRequest(writer, request, pAuth);
		break;
	}
	default:
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	}

	return std::make_shared<CAPIStringResponse>(AMC_API_HTTP_SUCCESS, AMC_API_CONTENTTYPE, writer.saveToString());
}
