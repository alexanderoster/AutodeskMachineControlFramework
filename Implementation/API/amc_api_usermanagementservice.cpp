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


#include "amc_api_usermanagementservice.hpp"
#include "amc_accesscontrol.hpp"
#include "amc_logger.hpp"

#include "libmc_exceptiontypes.hpp"
#include "libmcdata_dynamic.hpp"

#include "common_utils.hpp"
#include "common_chrono.hpp"

#include <cctype>

#define USERMANAGEMENT_LOGSUBSYSTEM "users"
#define USERMANAGEMENT_MAXLOGINLENGTH 64
#define USERMANAGEMENT_MAXDESCRIPTIONLENGTH 256
#define USERMANAGEMENT_MAXLANGUAGELENGTH 64
#define USERMANAGEMENT_PASSWORDCHALLENGE_LIFETIME_MICROSECONDS (60ULL * 1000000ULL)
#define USERMANAGEMENT_MAXPASSWORDCHALLENGES 1024

using namespace AMC;

static bool constantTimeEquals(const std::string& sA, const std::string& sB)
{
	if (sA.length() != sB.length())
		return false;

	unsigned char nDifference = 0;
	for (size_t nIndex = 0; nIndex < sA.length(); nIndex++)
		nDifference |= (unsigned char)(sA[nIndex] ^ sB[nIndex]);

	return nDifference == 0;
}

CUserManagementService::CUserManagementService(PSystemState pSystemState, PAPISessionHandler pSessionHandler)
	: m_pSystemState(pSystemState), m_pSessionHandler(pSessionHandler)
{
	if (pSystemState.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (pSessionHandler.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
}

CUserManagementService::~CUserManagementService()
{

}

PUserManagementPolicy CUserManagementService::getPolicy()
{
	return m_pSystemState->accessControl()->getUserManagementPolicy();
}

std::string CUserManagementService::resolveUserRole(const std::string& sStoredRoleIdentifier)
{
	auto pAccessControl = m_pSystemState->accessControl();
	if (sStoredRoleIdentifier.empty())
		return pAccessControl->getDefaultRole()->getIdentifier();

	return pAccessControl->resolveMigratedRole(sStoredRoleIdentifier);
}

PAccessRole CUserManagementService::getCallerRole(PUserInformation pCaller)
{
	if (pCaller.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	return m_pSystemState->accessControl()->findRole(resolveUserRole(pCaller->getRoleIdentifier()), false);
}

bool CUserManagementService::hasCapability(PUserInformation pCaller, eUserManagementCapability capability)
{
	auto pRole = getCallerRole(pCaller);
	return getPolicy()->roleHasCapability(pRole.get(), capability);
}

void CUserManagementService::requireCapability(PUserInformation pCaller, eUserManagementCapability capability)
{
	if (!hasCapability(pCaller, capability))
		throw ELibMCCustomException(LIBMC_ERROR_USERMANAGEMENTNOTPERMITTED, CUserManagementPolicy::capabilityToString(capability));
}

bool CUserManagementService::userIsSelf(PUserInformation pCaller, const std::string& sTargetUUID)
{
	if (pCaller.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	return AMCCommon::CUtils::normalizeUUIDString(pCaller->getUUID()) == AMCCommon::CUtils::normalizeUUIDString(sTargetUUID);
}

void CUserManagementService::requireNotSelf(PUserInformation pCaller, const std::string& sTargetUUID, const std::string& sOperation)
{
	if (userIsSelf(pCaller, sTargetUUID))
		throw ELibMCCustomException(LIBMC_ERROR_CANNOTMODIFYOWNUSER, sOperation);
}

bool CUserManagementService::roleIsAssignableBy(PUserInformation pCaller, const std::string& sRoleIdentifier)
{
	auto pAccessControl = m_pSystemState->accessControl();

	auto pCallerRole = getCallerRole(pCaller);
	if (pCallerRole.get() == nullptr)
		return false;

	auto pRole = pAccessControl->findRole(sRoleIdentifier, false);
	if (pRole.get() == nullptr)
		return false;

	if (pRole->isDeprecated())
		return false;

	if (!getPolicy()->roleIsInAssignableList(pRole->getIdentifier()))
		return false;

	return pAccessControl->roleIsSubsetOf(pRole->getIdentifier(), pCallerRole->getIdentifier());
}

void CUserManagementService::requireAssignableRole(PUserInformation pCaller, const std::string& sRoleIdentifier)
{
	if (!roleIsAssignableBy(pCaller, sRoleIdentifier))
		throw ELibMCCustomException(LIBMC_ERROR_ROLENOTASSIGNABLE, sRoleIdentifier);
}

bool CUserManagementService::targetIsManageable(PUserInformation pCaller, const CUserManagementUser& target)
{
	// Users with rights the caller does not have can not be changed by the caller.
	auto pAccessControl = m_pSystemState->accessControl();
	auto pCallerRole = getCallerRole(pCaller);
	if (pCallerRole.get() == nullptr)
		return false;

	auto pTargetRole = pAccessControl->findRole(target.m_sRoleIdentifier, false);
	if (pTargetRole.get() == nullptr)
		return true;

	return pAccessControl->roleIsSubsetOf(pTargetRole->getIdentifier(), pCallerRole->getIdentifier());
}

void CUserManagementService::requireManageableTarget(PUserInformation pCaller, const CUserManagementUser& target)
{
	if (!targetIsManageable(pCaller, target))
		throw ELibMCCustomException(LIBMC_ERROR_ROLENOTASSIGNABLE, "user " + target.m_sLogin + " has rights beyond your own");
}

bool CUserManagementService::roleHasGuardPermission(const std::string& sRoleIdentifier, const std::string& sGuardPermission)
{
	auto pRole = m_pSystemState->accessControl()->findRole(sRoleIdentifier, false);
	if (pRole.get() == nullptr)
		return false;

	return pRole->hasPermission(sGuardPermission);
}

void CUserManagementService::requireManagersRemain(const std::string& sTargetUUID, const std::string& sNewRoleIdentifier)
{
	auto pPolicy = getPolicy();
	std::string sGuardPermission = pPolicy->getGuardPermission();
	uint32_t nMinimumManagers = pPolicy->getMinimumManagers();
	if (sGuardPermission.empty() || (nMinimumManagers == 0))
		return;

	std::string sNormalizedTargetUUID = AMCCommon::CUtils::normalizeUUIDString(sTargetUUID);

	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();
	auto pUserList = pLoginHandler->GetActiveUsers();

	bool bTargetIsManager = false;
	uint32_t nManagersAfterChange = 0;

	uint32_t nCount = pUserList->Count();
	for (uint32_t nIndex = 0; nIndex < nCount; nIndex++) {
		std::string sLogin, sUUID, sDescription, sRole, sLanguage;
		pUserList->GetUserProperties(nIndex, sLogin, sUUID, sDescription, sRole, sLanguage);

		std::string sResolvedRole = resolveUserRole(sRole);
		bool bIsTarget = (AMCCommon::CUtils::normalizeUUIDString(sUUID) == sNormalizedTargetUUID);

		if (bIsTarget) {
			bTargetIsManager = roleHasGuardPermission(sResolvedRole, sGuardPermission);
			// An empty new role means the user is removed.
			if ((!sNewRoleIdentifier.empty()) && roleHasGuardPermission(sNewRoleIdentifier, sGuardPermission))
				nManagersAfterChange++;
		}
		else {
			if (roleHasGuardPermission(sResolvedRole, sGuardPermission))
				nManagersAfterChange++;
		}
	}

	if (bTargetIsManager && (nManagersAfterChange < nMinimumManagers))
		throw ELibMCCustomException(LIBMC_ERROR_LASTUSERMANAGER, "at least " + std::to_string(nMinimumManagers) + " user(s) must keep the permission " + sGuardPermission);
}

CUserManagementUser CUserManagementService::readUser(const std::string& sUUID)
{
	if (!AMCCommon::CUtils::stringIsUUIDString(sUUID))
		throw ELibMCCustomException(LIBMC_ERROR_USERNOTFOUND, sUUID);

	std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);

	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();
	if (!pLoginHandler->UserUUIDExists(sNormalizedUUID))
		throw ELibMCCustomException(LIBMC_ERROR_USERNOTFOUND, sNormalizedUUID);

	CUserManagementUser user;
	user.m_sUUID = sNormalizedUUID;
	std::string sStoredRole;
	pLoginHandler->GetUserPropertiesByUUID(sNormalizedUUID, user.m_sLogin, user.m_sDescription, sStoredRole, user.m_sLanguageIdentifier);
	user.m_sRoleIdentifier = resolveUserRole(sStoredRole);

	return user;
}

std::string CUserManagementService::validateLogin(const std::string& sLogin)
{
	std::string sTrimmedLogin = AMCCommon::CUtils::trimString(sLogin);
	if (sTrimmedLogin.empty())
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDUSERNAME);
	if (sTrimmedLogin.length() > USERMANAGEMENT_MAXLOGINLENGTH)
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDUSERNAME, "login is too long");

	if (!isalnum((unsigned char)sTrimmedLogin.at(0)))
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDUSERNAME, "login must start with a letter or digit");

	for (char c : sTrimmedLogin) {
		bool bIsAllowed = isalnum((unsigned char)c) || (c == '_') || (c == '-') || (c == '.') || (c == '@');
		if (!bIsAllowed)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDUSERNAME, "login may only contain letters, digits and _ - . @");
	}

	return sTrimmedLogin;
}

std::string CUserManagementService::validateDescription(const std::string& sDescription)
{
	if (sDescription.length() > USERMANAGEMENT_MAXDESCRIPTIONLENGTH)
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDUSERDESCRIPTION, "description is too long");
	if (!AMCCommon::CUtils::UTF8StringIsValid(sDescription))
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDUSERDESCRIPTION, "description is not valid UTF8");
	for (char c : sDescription) {
		if ((unsigned char)c < 32)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDUSERDESCRIPTION, "description contains control characters");
	}

	return AMCCommon::CUtils::trimString(sDescription);
}

std::string CUserManagementService::validateLanguage(const std::string& sLanguageIdentifier)
{
	if (sLanguageIdentifier.empty())
		return sLanguageIdentifier;

	auto pPolicy = getPolicy();
	if (!pPolicy->getLanguages().empty()) {
		if (!pPolicy->hasLanguage(sLanguageIdentifier))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDLANGUAGEIDENTIFIER, sLanguageIdentifier);
	}
	else {
		if ((sLanguageIdentifier.length() > USERMANAGEMENT_MAXLANGUAGELENGTH) || (!AMCCommon::CUtils::stringIsValidAlphanumericNameString(sLanguageIdentifier)))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDLANGUAGEIDENTIFIER, sLanguageIdentifier);
	}

	return sLanguageIdentifier;
}

void CUserManagementService::logChange(PUserInformation pCaller, const std::string& sMessage)
{
	m_pSystemState->logger()->logMessage("user " + pCaller->getLogin() + " " + sMessage, USERMANAGEMENT_LOGSUBSYSTEM, eLogLevel::Message);
}

std::vector<CUserManagementUser> CUserManagementService::listUsers(PUserInformation pCaller)
{
	requireCapability(pCaller, eUserManagementCapability::ViewUsers);

	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();
	auto pUserList = pLoginHandler->GetActiveUsers();

	std::vector<CUserManagementUser> users;
	uint32_t nCount = pUserList->Count();
	for (uint32_t nIndex = 0; nIndex < nCount; nIndex++) {
		CUserManagementUser user;
		std::string sStoredRole;
		pUserList->GetUserProperties(nIndex, user.m_sLogin, user.m_sUUID, user.m_sDescription, sStoredRole, user.m_sLanguageIdentifier);
		user.m_sUUID = AMCCommon::CUtils::normalizeUUIDString(user.m_sUUID);
		user.m_sRoleIdentifier = resolveUserRole(sStoredRole);
		users.push_back(user);
	}

	return users;
}

CUserManagementUser CUserManagementService::getUser(PUserInformation pCaller, const std::string& sUUID)
{
	if (!userIsSelf(pCaller, sUUID))
		requireCapability(pCaller, eUserManagementCapability::ViewUsers);

	return readUser(sUUID);
}

CUserManagementUser CUserManagementService::getOwnUser(PUserInformation pCaller)
{
	if (pCaller.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	return readUser(pCaller->getUUID());
}

std::map<std::string, std::vector<CAPIUserSessionInfo>> CUserManagementService::getSessionsOfAllUsers(PUserInformation pCaller)
{
	requireCapability(pCaller, eUserManagementCapability::ViewUsers);

	return m_pSessionHandler->getSessionsOfAllUsers();
}

std::vector<CAPIUserSessionInfo> CUserManagementService::getSessionsOfUser(PUserInformation pCaller, const std::string& sUUID)
{
	if (!userIsSelf(pCaller, sUUID))
		requireCapability(pCaller, eUserManagementCapability::ManageSessions);

	auto user = readUser(sUUID);
	return m_pSessionHandler->getSessionsOfUser(user.m_sUUID);
}

std::string CUserManagementService::createUser(PUserInformation pCaller, const std::string& sLogin, const std::string& sDescription, const std::string& sRoleIdentifier, const std::string& sLanguageIdentifier, const std::string& sSalt, const std::string& sPasswordHash)
{
	requireCapability(pCaller, eUserManagementCapability::CreateUser);

	std::string sValidLogin = validateLogin(sLogin);
	std::string sValidDescription = validateDescription(sDescription);
	std::string sValidLanguage = validateLanguage(sLanguageIdentifier);
	std::string sNormalizedSalt = AMCCommon::CUtils::normalizeSHA256String(sSalt);
	std::string sNormalizedHash = AMCCommon::CUtils::normalizeSHA256String(sPasswordHash);

	std::string sDefaultRole = m_pSystemState->accessControl()->getDefaultRole()->getIdentifier();
	std::string sRole = sRoleIdentifier.empty() ? sDefaultRole : sRoleIdentifier;
	if (sRole != sDefaultRole)
		requireCapability(pCaller, eUserManagementCapability::AssignRole);
	requireAssignableRole(pCaller, sRole);

	std::lock_guard<std::mutex> lockGuard(m_WriteMutex);

	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();
	if (pLoginHandler->UserExists(sValidLogin))
		throw ELibMCCustomException(LIBMC_ERROR_USERALREADYEXISTS, sValidLogin);

	std::string sUUID = pLoginHandler->CreateUser(sValidLogin, sRole, sNormalizedSalt, sNormalizedHash, sValidDescription);
	if (!sValidLanguage.empty())
		pLoginHandler->SetUserLanguageByUUID(sUUID, sValidLanguage);

	logChange(pCaller, "created user " + sValidLogin + " with role " + sRole);

	return AMCCommon::CUtils::normalizeUUIDString(sUUID);
}

bool CUserManagementService::defaultTestUserPresent()
{
	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();
	if (!pLoginHandler->UserExists("test"))
		return false;

	std::string sSalt;
	std::string sHashedPassword;
	pLoginHandler->GetUserDetails("test", sSalt, sHashedPassword);
	std::string sDefaultHash = AMCCommon::CUtils::calculateSHA256FromString(sSalt + "test");

	return AMCCommon::CUtils::normalizeSHA256String(sDefaultHash) == AMCCommon::CUtils::normalizeSHA256String(sHashedPassword);
}

std::string CUserManagementService::createFirstAdministrator(const std::string& sLogin, const std::string& sDescription, const std::string& sRoleIdentifier, const std::string& sLanguageIdentifier, const std::string& sSalt, const std::string& sPasswordHash)
{
	std::string sValidLogin = validateLogin(sLogin);
	std::string sValidDescription = validateDescription(sDescription);
	std::string sValidLanguage = validateLanguage(sLanguageIdentifier);
	std::string sNormalizedSalt = AMCCommon::CUtils::normalizeSHA256String(sSalt);
	std::string sNormalizedHash = AMCCommon::CUtils::normalizeSHA256String(sPasswordHash);

	auto pRole = m_pSystemState->accessControl()->findRole(sRoleIdentifier, false);
	if ((pRole.get() == nullptr) || pRole->isDeprecated())
		throw ELibMCCustomException(LIBMC_ERROR_FIRSTRUNADMINROLEINVALID, sRoleIdentifier);

	std::lock_guard<std::mutex> lockGuard(m_WriteMutex);

	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();
	if (pLoginHandler->GetActiveUsers()->Count() > 0)
		throw ELibMCInterfaceException(LIBMC_ERROR_FIRSTRUNALREADYCOMPLETED);

	std::string sUUID = pLoginHandler->CreateUser(sValidLogin, pRole->getIdentifier(), sNormalizedSalt, sNormalizedHash, sValidDescription);
	if (!sValidLanguage.empty())
		pLoginHandler->SetUserLanguageByUUID(sUUID, sValidLanguage);

	m_pSystemState->logger()->logMessage("first-run setup created administrator " + sValidLogin + " with role " + pRole->getIdentifier(), USERMANAGEMENT_LOGSUBSYSTEM, eLogLevel::Message);

	return AMCCommon::CUtils::normalizeUUIDString(sUUID);
}

void CUserManagementService::updateUser(PUserInformation pCaller, const std::string& sUUID, bool bUpdateDescription, const std::string& sDescription, bool bUpdateLanguage, const std::string& sLanguageIdentifier, bool bUpdateRole, const std::string& sRoleIdentifier)
{
	bool bIsSelf = userIsSelf(pCaller, sUUID);

	std::lock_guard<std::mutex> lockGuard(m_WriteMutex);

	auto target = readUser(sUUID);

	if (bUpdateDescription || bUpdateLanguage) {
		if (bIsSelf) {
			if (!hasCapability(pCaller, eUserManagementCapability::EditDetails))
				requireCapability(pCaller, eUserManagementCapability::EditOwnProfile);
		}
		else {
			requireCapability(pCaller, eUserManagementCapability::EditDetails);
			requireManageableTarget(pCaller, target);
		}
	}

	bool bRoleChanges = bUpdateRole && (sRoleIdentifier != target.m_sRoleIdentifier);
	if (bRoleChanges) {
		requireCapability(pCaller, eUserManagementCapability::AssignRole);
		requireNotSelf(pCaller, target.m_sUUID, "change own role");
		requireManageableTarget(pCaller, target);
		requireAssignableRole(pCaller, sRoleIdentifier);
		requireManagersRemain(target.m_sUUID, sRoleIdentifier);
	}

	std::string sValidDescription = bUpdateDescription ? validateDescription(sDescription) : "";
	std::string sValidLanguage = bUpdateLanguage ? validateLanguage(sLanguageIdentifier) : "";

	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();

	if (bUpdateDescription && (sValidDescription != target.m_sDescription))
		pLoginHandler->SetUserDescriptionByUUID(target.m_sUUID, sValidDescription);

	if (bUpdateLanguage && (sValidLanguage != target.m_sLanguageIdentifier))
		pLoginHandler->SetUserLanguageByUUID(target.m_sUUID, sValidLanguage);

	if (bRoleChanges) {
		pLoginHandler->SetUserRoleByUUID(target.m_sUUID, sRoleIdentifier);
		// The role is copied into the session at login, so the user has to log in again.
		m_pSessionHandler->deactivateSessionsOfUser(target.m_sUUID, "");
		logChange(pCaller, "changed role of " + target.m_sLogin + ": " + target.m_sRoleIdentifier + " -> " + sRoleIdentifier);
	}

	if (bUpdateDescription || bUpdateLanguage)
		logChange(pCaller, "updated details of " + target.m_sLogin);
}

void CUserManagementService::resetPassword(PUserInformation pCaller, const std::string& sUUID, const std::string& sSalt, const std::string& sPasswordHash)
{
	requireCapability(pCaller, eUserManagementCapability::ResetPassword);
	requireNotSelf(pCaller, sUUID, "reset own password");

	std::string sNormalizedSalt = AMCCommon::CUtils::normalizeSHA256String(sSalt);
	std::string sNormalizedHash = AMCCommon::CUtils::normalizeSHA256String(sPasswordHash);

	std::lock_guard<std::mutex> lockGuard(m_WriteMutex);

	auto target = readUser(sUUID);
	requireManageableTarget(pCaller, target);

	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();
	pLoginHandler->SetUserPasswordByUUID(target.m_sUUID, sNormalizedSalt, sNormalizedHash);

	m_pSessionHandler->deactivateSessionsOfUser(target.m_sUUID, "");

	logChange(pCaller, "reset the password of " + target.m_sLogin);
}

void CUserManagementService::deleteUser(PUserInformation pCaller, const std::string& sUUID)
{
	requireCapability(pCaller, eUserManagementCapability::DeleteUser);
	requireNotSelf(pCaller, sUUID, "delete own user");

	std::lock_guard<std::mutex> lockGuard(m_WriteMutex);

	auto target = readUser(sUUID);
	requireManageableTarget(pCaller, target);
	requireManagersRemain(target.m_sUUID, "");

	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();
	pLoginHandler->DeactivateUserByUUID(target.m_sUUID);

	m_pSessionHandler->deactivateSessionsOfUser(target.m_sUUID, "");

	logChange(pCaller, "deleted user " + target.m_sLogin);
}

uint32_t CUserManagementService::revokeSessions(PUserInformation pCaller, const std::string& sCallerSessionUUID, const std::string& sUUID)
{
	bool bIsSelf = userIsSelf(pCaller, sUUID);

	auto target = readUser(sUUID);
	if (!bIsSelf) {
		requireCapability(pCaller, eUserManagementCapability::ManageSessions);
		requireManageableTarget(pCaller, target);
	}

	// Revoking the own sessions keeps the current one alive.
	uint32_t nCount = m_pSessionHandler->deactivateSessionsOfUser(target.m_sUUID, bIsSelf ? sCallerSessionUUID : "");

	logChange(pCaller, "ended " + std::to_string(nCount) + " session(s) of " + target.m_sLogin);

	return nCount;
}

void CUserManagementService::updateOwnProfile(PUserInformation pCaller, bool bUpdateDescription, const std::string& sDescription, bool bUpdateLanguage, const std::string& sLanguageIdentifier)
{
	requireCapability(pCaller, eUserManagementCapability::EditOwnProfile);

	updateUser(pCaller, pCaller->getUUID(), bUpdateDescription, sDescription, bUpdateLanguage, sLanguageIdentifier, false, "");
}

void CUserManagementService::cleanupPasswordChallenges(uint64_t nNow)
{
	for (auto iIter = m_PasswordChallenges.begin(); iIter != m_PasswordChallenges.end(); ) {
		if (iIter->second.m_nExpiryTimeInMicroseconds < nNow)
			iIter = m_PasswordChallenges.erase(iIter);
		else
			iIter++;
	}
}

CUserManagementPasswordChallenge CUserManagementService::createPasswordChallenge(PUserInformation pCaller, const std::string& sCallerSessionUUID)
{
	requireCapability(pCaller, eUserManagementCapability::ChangeOwnPassword);

	auto user = readUser(pCaller->getUUID());

	std::string sSalt;
	std::string sHashedPassword;
	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();
	pLoginHandler->GetUserDetails(user.m_sLogin, sSalt, sHashedPassword);

	uint64_t nNow = m_pSystemState->globalChrono()->getUTCTimeStampInMicrosecondsSince1970();

	CUserManagementPasswordChallenge challenge;
	challenge.m_sChallengeUUID = AMCCommon::CUtils::createUUID();
	challenge.m_sSessionUUID = sCallerSessionUUID;
	challenge.m_sUserUUID = user.m_sUUID;
	challenge.m_sNonce = AMCCommon::CUtils::calculateSHA256FromString(AMCCommon::CUtils::createUUIDV4() + AMCCommon::CUtils::createUUIDV4() + std::to_string(nNow));
	challenge.m_sSalt = sSalt;
	challenge.m_nExpiryTimeInMicroseconds = nNow + USERMANAGEMENT_PASSWORDCHALLENGE_LIFETIME_MICROSECONDS;

	std::lock_guard<std::mutex> lockGuard(m_ChallengeMutex);
	cleanupPasswordChallenges(nNow);

	// Only one open challenge per session.
	for (auto iIter = m_PasswordChallenges.begin(); iIter != m_PasswordChallenges.end(); ) {
		if (iIter->second.m_sSessionUUID == sCallerSessionUUID)
			iIter = m_PasswordChallenges.erase(iIter);
		else
			iIter++;
	}

	if (m_PasswordChallenges.size() >= USERMANAGEMENT_MAXPASSWORDCHALLENGES)
		throw ELibMCCustomException(LIBMC_ERROR_PASSWORDCHALLENGENOTFOUND, "too many open password challenges");

	m_PasswordChallenges.insert(std::make_pair(challenge.m_sChallengeUUID, challenge));

	return challenge;
}

void CUserManagementService::changeOwnPassword(PUserInformation pCaller, const std::string& sCallerSessionUUID, const std::string& sChallengeUUID, const std::string& sProof, const std::string& sNewSalt, const std::string& sNewPasswordHash)
{
	requireCapability(pCaller, eUserManagementCapability::ChangeOwnPassword);

	std::string sNormalizedNewSalt = AMCCommon::CUtils::normalizeSHA256String(sNewSalt);
	std::string sNormalizedNewHash = AMCCommon::CUtils::normalizeSHA256String(sNewPasswordHash);
	std::string sNormalizedProof = AMCCommon::CUtils::normalizeSHA256String(sProof);

	CUserManagementPasswordChallenge challenge;
	{
		// A challenge can only be used once, whatever the outcome.
		std::lock_guard<std::mutex> lockGuard(m_ChallengeMutex);
		uint64_t nNow = m_pSystemState->globalChrono()->getUTCTimeStampInMicrosecondsSince1970();
		cleanupPasswordChallenges(nNow);

		auto iIter = m_PasswordChallenges.find(AMCCommon::CUtils::normalizeUUIDString(sChallengeUUID));
		if (iIter == m_PasswordChallenges.end())
			throw ELibMCInterfaceException(LIBMC_ERROR_PASSWORDCHALLENGENOTFOUND);

		challenge = iIter->second;
		m_PasswordChallenges.erase(iIter);
	}

	if ((challenge.m_sSessionUUID != sCallerSessionUUID) || (!userIsSelf(pCaller, challenge.m_sUserUUID)))
		throw ELibMCInterfaceException(LIBMC_ERROR_PASSWORDCHALLENGENOTFOUND);

	std::lock_guard<std::mutex> lockGuard(m_WriteMutex);

	auto user = readUser(challenge.m_sUserUUID);

	std::string sSalt;
	std::string sHashedPassword;
	auto pLoginHandler = m_pSystemState->getDataModelInstance()->CreateLoginHandler();
	pLoginHandler->GetUserDetails(user.m_sLogin, sSalt, sHashedPassword);

	std::string sExpectedProof = AMCCommon::CUtils::calculateSHA256FromString(challenge.m_sNonce + AMCCommon::CUtils::normalizeSHA256String(sHashedPassword));
	if (!constantTimeEquals(sExpectedProof, sNormalizedProof)) {
		logChange(pCaller, "failed to change the own password: wrong current password");
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPASSWORDPROOF);
	}

	pLoginHandler->SetUserPasswordByUUID(user.m_sUUID, sNormalizedNewSalt, sNormalizedNewHash);

	m_pSessionHandler->deactivateSessionsOfUser(user.m_sUUID, sCallerSessionUUID);

	logChange(pCaller, "changed the own password");
}
