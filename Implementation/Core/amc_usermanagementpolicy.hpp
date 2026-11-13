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


#ifndef __AMC_USERMANAGEMENTPOLICY
#define __AMC_USERMANAGEMENTPOLICY

#include <memory>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <mutex>

namespace AMC {

	class CAccessRole;

	class CUserManagementPolicy;
	typedef std::shared_ptr<CUserManagementPolicy> PUserManagementPolicy;

	enum class eUserManagementCapability : uint32_t {
		ViewUsers = 1,
		CreateUser = 2,
		EditDetails = 3,
		AssignRole = 4,
		ResetPassword = 5,
		DeleteUser = 6,
		ManageSessions = 7,
		ChangeOwnPassword = 8,
		EditOwnProfile = 9
	};

	class CUserManagementCapabilityGrant {
	public:
		bool m_bEnabled;

		// Empty permission means: any logged in user.
		std::string m_sPermission;

		CUserManagementCapabilityGrant();
		CUserManagementCapabilityGrant(bool bEnabled, const std::string& sPermission);
	};

	class CUserManagementLanguage {
	public:
		std::string m_sIdentifier;
		std::string m_sDisplayName;
	};

	class CUserManagementPolicy {
	private:

		std::mutex m_Mutex;

		std::map<eUserManagementCapability, CUserManagementCapabilityGrant> m_Grants;

		std::string m_sGuardPermission;
		uint32_t m_nMinimumManagers;

		bool m_bRestrictAssignableRoles;
		std::set<std::string> m_AssignableRoles;

		uint32_t m_nPasswordMinimumLength;
		bool m_bPasswordRequiresDigit;
		bool m_bPasswordRequiresLetter;

		std::vector<CUserManagementLanguage> m_Languages;

	public:

		CUserManagementPolicy();

		virtual ~CUserManagementPolicy();

		static std::vector<eUserManagementCapability> getAllCapabilities();

		static std::string capabilityToString(eUserManagementCapability capability);

		static eUserManagementCapability stringToCapability(const std::string& sCapability);

		static bool capabilityIsSelfService(eUserManagementCapability capability);

		void setCapability(eUserManagementCapability capability, bool bEnabled, const std::string& sPermission);

		CUserManagementCapabilityGrant getCapability(eUserManagementCapability capability);

		bool roleHasCapability(CAccessRole* pRole, eUserManagementCapability capability);

		void setGuardPermission(const std::string& sPermission);

		std::string getGuardPermission();

		void setMinimumManagers(uint32_t nMinimumManagers);

		uint32_t getMinimumManagers();

		void restrictAssignableRoles(const std::set<std::string>& roleIdentifiers);

		bool isAssignableRolesRestricted();

		bool roleIsInAssignableList(const std::string& sRoleIdentifier);

		void setPasswordPolicy(uint32_t nMinimumLength, bool bRequiresDigit, bool bRequiresLetter);

		uint32_t getPasswordMinimumLength();

		bool passwordRequiresDigit();

		bool passwordRequiresLetter();

		void addLanguage(const std::string& sIdentifier, const std::string& sDisplayName);

		std::vector<CUserManagementLanguage> getLanguages();

		bool hasLanguage(const std::string& sIdentifier);

	};

}


#endif //__AMC_USERMANAGEMENTPOLICY

