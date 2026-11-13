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


#ifndef __AMC_API_USERMANAGEMENTSERVICE
#define __AMC_API_USERMANAGEMENTSERVICE

#include "header_protection.hpp"
#include "amc_systemstate.hpp"
#include "amc_api_sessionhandler.hpp"
#include "amc_usermanagementpolicy.hpp"
#include "amc_userinformation.hpp"

#include <mutex>
#include <map>
#include <string>
#include <vector>

namespace AMC {

	amcDeclareDependingClass(CUserManagementService, PUserManagementService);
	amcDeclareDependingClass(CAccessRole, PAccessRole);

	class CUserManagementUser {
	public:
		std::string m_sUUID;
		std::string m_sLogin;
		std::string m_sDescription;
		std::string m_sRoleIdentifier;
		std::string m_sLanguageIdentifier;
	};

	class CUserManagementPasswordChallenge {
	public:
		std::string m_sChallengeUUID;
		std::string m_sSessionUUID;
		std::string m_sUserUUID;
		std::string m_sNonce;
		std::string m_sSalt;
		uint64_t m_nExpiryTimeInMicroseconds;
	};

	// Enforces the user management rules. Every operation is checked against the
	// capabilities of the calling user, so the REST API can not be used to escalate rights.
	class CUserManagementService {
	private:

		PSystemState m_pSystemState;
		PAPISessionHandler m_pSessionHandler;

		// Serializes all write operations, so the checks and the change are atomic.
		std::mutex m_WriteMutex;

		std::mutex m_ChallengeMutex;
		std::map<std::string, CUserManagementPasswordChallenge> m_PasswordChallenges;

		PUserManagementPolicy getPolicy();

		PAccessRole getCallerRole(PUserInformation pCaller);

		std::string resolveUserRole(const std::string& sStoredRoleIdentifier);

		void requireCapability(PUserInformation pCaller, eUserManagementCapability capability);

		void requireNotSelf(PUserInformation pCaller, const std::string& sTargetUUID, const std::string& sOperation);

		void requireManageableTarget(PUserInformation pCaller, const CUserManagementUser& target);

		void requireAssignableRole(PUserInformation pCaller, const std::string& sRoleIdentifier);

		void requireManagersRemain(const std::string& sTargetUUID, const std::string& sNewRoleIdentifier);

		bool roleHasGuardPermission(const std::string& sRoleIdentifier, const std::string& sGuardPermission);

		CUserManagementUser readUser(const std::string& sUUID);

		std::string validateLogin(const std::string& sLogin);
		std::string validateDescription(const std::string& sDescription);
		std::string validateLanguage(const std::string& sLanguageIdentifier);

		void cleanupPasswordChallenges(uint64_t nNow);

		void logChange(PUserInformation pCaller, const std::string& sMessage);

	public:

		CUserManagementService(PSystemState pSystemState, PAPISessionHandler pSessionHandler);

		virtual ~CUserManagementService();

		bool hasCapability(PUserInformation pCaller, eUserManagementCapability capability);

		bool roleIsAssignableBy(PUserInformation pCaller, const std::string& sRoleIdentifier);

		bool userIsSelf(PUserInformation pCaller, const std::string& sTargetUUID);

		bool targetIsManageable(PUserInformation pCaller, const CUserManagementUser& target);

		// True if the account test/test created by older versions still exists with its default password.
		bool defaultTestUserPresent();

		std::vector<CUserManagementUser> listUsers(PUserInformation pCaller);

		CUserManagementUser getUser(PUserInformation pCaller, const std::string& sUUID);

		CUserManagementUser getOwnUser(PUserInformation pCaller);

		std::map<std::string, std::vector<CAPIUserSessionInfo>> getSessionsOfAllUsers(PUserInformation pCaller);

		std::vector<CAPIUserSessionInfo> getSessionsOfUser(PUserInformation pCaller, const std::string& sUUID);

		std::string createUser(PUserInformation pCaller, const std::string& sLogin, const std::string& sDescription, const std::string& sRoleIdentifier, const std::string& sLanguageIdentifier, const std::string& sSalt, const std::string& sPasswordHash);

		// Creates the first account without a calling user. Fails with FIRSTRUNALREADYCOMPLETED as soon as any active user exists.
		std::string createFirstAdministrator(const std::string& sLogin, const std::string& sDescription, const std::string& sRoleIdentifier, const std::string& sLanguageIdentifier, const std::string& sSalt, const std::string& sPasswordHash);

		void updateUser(PUserInformation pCaller, const std::string& sUUID, bool bUpdateDescription, const std::string& sDescription, bool bUpdateLanguage, const std::string& sLanguageIdentifier, bool bUpdateRole, const std::string& sRoleIdentifier);

		void resetPassword(PUserInformation pCaller, const std::string& sUUID, const std::string& sSalt, const std::string& sPasswordHash);

		void deleteUser(PUserInformation pCaller, const std::string& sUUID);

		uint32_t revokeSessions(PUserInformation pCaller, const std::string& sCallerSessionUUID, const std::string& sUUID);

		void updateOwnProfile(PUserInformation pCaller, bool bUpdateDescription, const std::string& sDescription, bool bUpdateLanguage, const std::string& sLanguageIdentifier);

		CUserManagementPasswordChallenge createPasswordChallenge(PUserInformation pCaller, const std::string& sCallerSessionUUID);

		void changeOwnPassword(PUserInformation pCaller, const std::string& sCallerSessionUUID, const std::string& sChallengeUUID, const std::string& sProof, const std::string& sNewSalt, const std::string& sNewPasswordHash);

	};

}


#endif //__AMC_API_USERMANAGEMENTSERVICE

