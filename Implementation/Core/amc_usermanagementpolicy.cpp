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


#include "amc_usermanagementpolicy.hpp"
#include "amc_accessrole.hpp"

#include "common_utils.hpp"
#include "libmc_exceptiontypes.hpp"


namespace AMC {

	CUserManagementCapabilityGrant::CUserManagementCapabilityGrant()
		: m_bEnabled(false)
	{

	}

	CUserManagementCapabilityGrant::CUserManagementCapabilityGrant(bool bEnabled, const std::string& sPermission)
		: m_bEnabled(bEnabled), m_sPermission(sPermission)
	{

	}

	CUserManagementPolicy::CUserManagementPolicy()
		: m_nMinimumManagers(1),
		m_bRestrictAssignableRoles(false),
		m_nPasswordMinimumLength(8),
		m_bPasswordRequiresDigit(false),
		m_bPasswordRequiresLetter(false)
	{
		// Secure default: everything that acts on other users is off, self service is open to any logged in user.
		for (auto capability : getAllCapabilities())
			m_Grants.insert(std::make_pair(capability, CUserManagementCapabilityGrant(capabilityIsSelfService(capability), "")));
	}

	CUserManagementPolicy::~CUserManagementPolicy()
	{

	}

	std::vector<eUserManagementCapability> CUserManagementPolicy::getAllCapabilities()
	{
		return {
			eUserManagementCapability::ViewUsers,
			eUserManagementCapability::CreateUser,
			eUserManagementCapability::EditDetails,
			eUserManagementCapability::AssignRole,
			eUserManagementCapability::ResetPassword,
			eUserManagementCapability::DeleteUser,
			eUserManagementCapability::ManageSessions,
			eUserManagementCapability::ChangeOwnPassword,
			eUserManagementCapability::EditOwnProfile
		};
	}

	std::string CUserManagementPolicy::capabilityToString(eUserManagementCapability capability)
	{
		switch (capability) {
		case eUserManagementCapability::ViewUsers: return "viewusers";
		case eUserManagementCapability::CreateUser: return "createuser";
		case eUserManagementCapability::EditDetails: return "editdetails";
		case eUserManagementCapability::AssignRole: return "assignrole";
		case eUserManagementCapability::ResetPassword: return "resetpassword";
		case eUserManagementCapability::DeleteUser: return "deleteuser";
		case eUserManagementCapability::ManageSessions: return "managesessions";
		case eUserManagementCapability::ChangeOwnPassword: return "changeownpassword";
		case eUserManagementCapability::EditOwnProfile: return "editownprofile";
		default:
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDUSERMANAGEMENTCAPABILITY, std::to_string((uint32_t)capability));
		}
	}

	eUserManagementCapability CUserManagementPolicy::stringToCapability(const std::string& sCapability)
	{
		std::string sLowerCase = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(sCapability));
		for (auto capability : getAllCapabilities()) {
			if (capabilityToString(capability) == sLowerCase)
				return capability;
		}

		throw ELibMCCustomException(LIBMC_ERROR_INVALIDUSERMANAGEMENTCAPABILITY, sCapability);
	}

	bool CUserManagementPolicy::capabilityIsSelfService(eUserManagementCapability capability)
	{
		return (capability == eUserManagementCapability::ChangeOwnPassword) || (capability == eUserManagementCapability::EditOwnProfile);
	}

	void CUserManagementPolicy::setCapability(eUserManagementCapability capability, bool bEnabled, const std::string& sPermission)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_Grants[capability] = CUserManagementCapabilityGrant(bEnabled, sPermission);
	}

	CUserManagementCapabilityGrant CUserManagementPolicy::getCapability(eUserManagementCapability capability)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		auto iIter = m_Grants.find(capability);
		if (iIter == m_Grants.end())
			return CUserManagementCapabilityGrant();

		return iIter->second;
	}

	bool CUserManagementPolicy::roleHasCapability(CAccessRole* pRole, eUserManagementCapability capability)
	{
		if (pRole == nullptr)
			return false;

		auto grant = getCapability(capability);
		if (!grant.m_bEnabled)
			return false;

		if (grant.m_sPermission.empty())
			return true;

		return pRole->hasPermission(grant.m_sPermission);
	}

	void CUserManagementPolicy::setGuardPermission(const std::string& sPermission)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_sGuardPermission = sPermission;
	}

	std::string CUserManagementPolicy::getGuardPermission()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_sGuardPermission;
	}

	void CUserManagementPolicy::setMinimumManagers(uint32_t nMinimumManagers)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_nMinimumManagers = nMinimumManagers;
	}

	uint32_t CUserManagementPolicy::getMinimumManagers()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_nMinimumManagers;
	}

	void CUserManagementPolicy::restrictAssignableRoles(const std::set<std::string>& roleIdentifiers)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_bRestrictAssignableRoles = true;
		m_AssignableRoles = roleIdentifiers;
	}

	bool CUserManagementPolicy::isAssignableRolesRestricted()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_bRestrictAssignableRoles;
	}

	bool CUserManagementPolicy::roleIsInAssignableList(const std::string& sRoleIdentifier)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		if (!m_bRestrictAssignableRoles)
			return true;

		return m_AssignableRoles.find(sRoleIdentifier) != m_AssignableRoles.end();
	}

	void CUserManagementPolicy::setPasswordPolicy(uint32_t nMinimumLength, bool bRequiresDigit, bool bRequiresLetter)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_nPasswordMinimumLength = nMinimumLength;
		m_bPasswordRequiresDigit = bRequiresDigit;
		m_bPasswordRequiresLetter = bRequiresLetter;
	}

	uint32_t CUserManagementPolicy::getPasswordMinimumLength()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_nPasswordMinimumLength;
	}

	bool CUserManagementPolicy::passwordRequiresDigit()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_bPasswordRequiresDigit;
	}

	bool CUserManagementPolicy::passwordRequiresLetter()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_bPasswordRequiresLetter;
	}

	void CUserManagementPolicy::addLanguage(const std::string& sIdentifier, const std::string& sDisplayName)
	{
		if (sIdentifier.empty())
			return;

		if (hasLanguage(sIdentifier))
			return;

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		CUserManagementLanguage language;
		language.m_sIdentifier = sIdentifier;
		language.m_sDisplayName = sDisplayName.empty() ? sIdentifier : sDisplayName;
		m_Languages.push_back(language);
	}

	std::vector<CUserManagementLanguage> CUserManagementPolicy::getLanguages()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_Languages;
	}

	bool CUserManagementPolicy::hasLanguage(const std::string& sIdentifier)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		for (auto& language : m_Languages) {
			if (language.m_sIdentifier == sIdentifier)
				return true;
		}

		return false;
	}

}

