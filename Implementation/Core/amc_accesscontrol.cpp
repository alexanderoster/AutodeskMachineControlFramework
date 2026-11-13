/*++

Copyright (C) 2023 Autodesk Inc.

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


#include "amc_accesscontrol.hpp"
#include "amc_accessrole.hpp"

#include "common_utils.hpp"
#include "libmc_exceptiontypes.hpp"



namespace AMC {


	CAccessControl::CAccessControl()
		: m_pUserManagementPolicy(std::make_shared<CUserManagementPolicy>())
	{

	}

	CAccessControl::~CAccessControl()
	{

	}

	PAccessPermission CAccessControl::addPermission(const std::string& sIdentifier, const CLanguageString& rDisplayName, const CLanguageString& rDescription)
	{

		if (hasPermission(sIdentifier))
			throw ELibMCCustomException(LIBMC_ERROR_DUPLICATEPERMISSIONIDENTIFIER, sIdentifier);

		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);

			auto pResult = std::make_shared<CAccessPermission>(sIdentifier, rDisplayName, rDescription);
			m_Permissions.insert(std::make_pair(pResult->getIdentifier(), pResult));

			return pResult;
		}

	}

	PAccessRole CAccessControl::addRole(const std::string& sIdentifier, const CLanguageString& rDisplayName, const CLanguageString& rDescription)
	{
		if (hasRole(sIdentifier))
			throw ELibMCCustomException(LIBMC_ERROR_DUPLICATEROLEIDENTIFIER, sIdentifier);

		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);

			auto pResult = std::make_shared<CAccessRole>(sIdentifier, rDisplayName, rDescription);
			m_Roles.insert(std::make_pair(pResult->getIdentifier(), pResult));

			return pResult;
		}


	}

	PAccessPermission CAccessControl::findPermission(const std::string& sIdentifier, bool bMustExist)
	{
		auto iIter = m_Permissions.find(sIdentifier);
		if (iIter == m_Permissions.end()) {
			if (bMustExist)
				throw ELibMCCustomException(LIBMC_ERROR_PERMISSIONNOTFOUND, sIdentifier);
			return nullptr;
		}

		return iIter->second;

	}

	PAccessRole CAccessControl::findRole(const std::string& sIdentifier, bool bMustExist)
	{
		auto iIter = m_Roles.find(sIdentifier);
		if (iIter == m_Roles.end()) {
			if (bMustExist)
				throw ELibMCCustomException(LIBMC_ERROR_ROLENOTFOUND, sIdentifier);
			return nullptr;
		}

		return iIter->second;

	}

	bool CAccessControl::hasPermission(const std::string& sIdentifier)
	{
		auto iIter = m_Permissions.find(sIdentifier);
		return iIter != m_Permissions.end();
	}

	bool CAccessControl::hasRole(const std::string& sIdentifier)
	{
		auto iIter = m_Roles.find(sIdentifier);
		return iIter != m_Roles.end();

	}

	void CAccessControl::setDefaultRole(const std::string& sIdentifier)
	{
		PAccessRole pRole = findRole(sIdentifier, true);
		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);
			m_pDefaultRole = pRole;
		}
	}

	void CAccessControl::setToNoAccessControl()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_Permissions.clear();
		m_Roles.clear();

		CLanguageString displayName("", "Default Role");
		CLanguageString description("", "");

		auto pDefaultRole = std::make_shared<CAccessRole>("default", displayName, description);
		m_Roles.insert(std::make_pair(pDefaultRole->getIdentifier(), pDefaultRole));

		m_pDefaultRole = pDefaultRole;
	}

	PAccessRole CAccessControl::getDefaultRole()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		if (m_pDefaultRole.get () == nullptr)
			throw ELibMCInterfaceException(LIBMC_ERROR_DEFAULTROLEHASNOTBEENSET);

		return m_pDefaultRole;
	}


	bool CAccessControl::checkPermissionInRole(const std::string& sRoleIdentifier, const std::string& sPermissionIdentifier)
	{
		if (sRoleIdentifier.empty ())
			throw ELibMCInterfaceException(LIBMC_ERROR_EMPTYROLEIDENTIFIER);
		if (sPermissionIdentifier.empty ())
			throw ELibMCInterfaceException(LIBMC_ERROR_EMPTYPERMISSIONIDENTIFIER);

		PAccessRole pRole = findRole (sRoleIdentifier, true);
		return pRole->hasPermission(sPermissionIdentifier);
	}

	void CAccessControl::getPermissionsForRole(const std::string& sRoleIdentifier, std::set<std::string>& permissionStrings)
	{
		PAccessRole pRole = findRole(sRoleIdentifier, true);
		auto pPermissions = pRole->getPermissions();
		for (auto pPermission : pPermissions)
			permissionStrings.insert(pPermission->getIdentifier ());
	}

	std::vector<PAccessRole> CAccessControl::getRoles()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		std::vector<PAccessRole> roles;
		for (auto iIter : m_Roles)
			roles.push_back(iIter.second);

		return roles;
	}

	std::vector<PAccessPermission> CAccessControl::getPermissions()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		std::vector<PAccessPermission> permissions;
		for (auto iIter : m_Permissions)
			permissions.push_back(iIter.second);

		return permissions;
	}

	std::string CAccessControl::resolveMigratedRole(const std::string& sRoleIdentifier)
	{
		std::string sCurrentIdentifier = sRoleIdentifier;
		std::set<std::string> visitedRoles;

		while (true) {
			PAccessRole pRole = findRole(sCurrentIdentifier, false);
			if (pRole.get() == nullptr)
				return sCurrentIdentifier;

			std::string sMigrateTo = pRole->getMigrateTo();
			if (sMigrateTo.empty())
				return sCurrentIdentifier;

			visitedRoles.insert(sCurrentIdentifier);
			if (visitedRoles.find(sMigrateTo) != visitedRoles.end())
				throw ELibMCCustomException(LIBMC_ERROR_INVALIDACCESSROLEIDENTIFIER, "cyclic role migration: " + sRoleIdentifier);

			sCurrentIdentifier = sMigrateTo;
		}
	}

	bool CAccessControl::roleIsSubsetOf(const std::string& sRoleIdentifier, const std::string& sSuperRoleIdentifier)
	{
		PAccessRole pRole = findRole(sRoleIdentifier, true);
		PAccessRole pSuperRole = findRole(sSuperRoleIdentifier, true);

		for (auto pPermission : pRole->getPermissions()) {
			if (!pSuperRole->hasPermission(pPermission->getIdentifier()))
				return false;
		}

		return true;
	}

	std::string CAccessControl::findRoleWithAllPermissions()
	{
		std::vector<PAccessRole> activeRoles;
		for (auto pRole : getRoles()) {
			if (!pRole->isDeprecated())
				activeRoles.push_back(pRole);
		}

		std::string sFoundIdentifier;
		for (auto pCandidate : activeRoles) {
			bool bHoldsAll = true;
			for (auto pOther : activeRoles) {
				if (!roleIsSubsetOf(pOther->getIdentifier(), pCandidate->getIdentifier())) {
					bHoldsAll = false;
					break;
				}
			}

			if (bHoldsAll) {
				if (!sFoundIdentifier.empty())
					return "";
				sFoundIdentifier = pCandidate->getIdentifier();
			}
		}

		return sFoundIdentifier;
	}

	PUserManagementPolicy CAccessControl::getUserManagementPolicy()
	{
		return m_pUserManagementPolicy;
	}
}


