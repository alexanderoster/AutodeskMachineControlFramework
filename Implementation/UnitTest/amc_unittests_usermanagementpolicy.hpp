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

#ifndef __AMCTEST_UNITTEST_USERMANAGEMENTPOLICY
#define __AMCTEST_UNITTEST_USERMANAGEMENTPOLICY


#include "amc_unittests.hpp"
#include "amc_accesscontrol.hpp"
#include "amc_accessrole.hpp"
#include "amc_usermanagementpolicy.hpp"

namespace AMCUnitTest {

	class CUnitTestGroup_UserManagementPolicy : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "UserManagementPolicy";
		}

		void registerTests() override {
			registerTest("SecureDefaults", "Without configuration only self service is allowed", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UserManagementPolicy::testSecureDefaults, this));
			registerTest("CapabilityNames", "Capability names round-trip and unknown names throw", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UserManagementPolicy::testCapabilityNames, this));
			registerTest("CapabilityPermissionMapping", "Capabilities follow the mapped permission", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UserManagementPolicy::testCapabilityPermissionMapping, this));
			registerTest("DisabledCapability", "A disabled capability is never granted", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UserManagementPolicy::testDisabledCapability, this));
			registerTest("RoleSubset", "Role subset check used against escalation", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UserManagementPolicy::testRoleSubset, this));
			registerTest("AssignableRoles", "Assignable role list narrows the roles", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UserManagementPolicy::testAssignableRoles, this));
			registerTest("RoleMigration", "Deprecated roles resolve to their replacement", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UserManagementPolicy::testRoleMigration, this));
			registerTest("CyclicRoleMigrationThrows", "Cyclic role migration is detected", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UserManagementPolicy::testCyclicRoleMigrationThrows, this));
			registerTest("Languages", "Language list ignores duplicates and empty identifiers", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UserManagementPolicy::testLanguages, this));
		}

		void initializeTests() override {
		}

	private:

		AMC::CLanguageString makeString(const std::string& s) {
			return AMC::CLanguageString("", s);
		}

		void testSecureDefaults() {
			AMC::CAccessControl control;
			auto pRole = control.addRole("operator", makeString("Operator"), makeString(""));
			auto pPolicy = control.getUserManagementPolicy();

			for (auto capability : AMC::CUserManagementPolicy::getAllCapabilities()) {
				bool bExpected = AMC::CUserManagementPolicy::capabilityIsSelfService(capability);
				assertTrue(pPolicy->roleHasCapability(pRole.get(), capability) == bExpected, AMC::CUserManagementPolicy::capabilityToString(capability));
			}

			assertFalse(pPolicy->roleHasCapability(nullptr, AMC::eUserManagementCapability::ChangeOwnPassword), "No role means no capability");
		}

		void testCapabilityNames() {
			for (auto capability : AMC::CUserManagementPolicy::getAllCapabilities()) {
				std::string sName = AMC::CUserManagementPolicy::capabilityToString(capability);
				assertTrue(AMC::CUserManagementPolicy::stringToCapability(sName) == capability, sName);
			}

			assertTrue(AMC::CUserManagementPolicy::stringToCapability(" ViewUsers ") == AMC::eUserManagementCapability::ViewUsers, "Names are trimmed and case insensitive");

			bool bThrown = false;
			try {
				AMC::CUserManagementPolicy::stringToCapability("becomeadmin");
			}
			catch (...) {
				bThrown = true;
			}
			assertTrue(bThrown, "Unknown capability must throw");
		}

		void testCapabilityPermissionMapping() {
			AMC::CAccessControl control;
			auto pPermission = control.addPermission("perm_users", makeString("Users"), makeString(""));
			auto pAdmin = control.addRole("admin", makeString("Admin"), makeString(""));
			auto pOperator = control.addRole("operator", makeString("Operator"), makeString(""));
			pAdmin->addPermission(pPermission);

			auto pPolicy = control.getUserManagementPolicy();
			pPolicy->setCapability(AMC::eUserManagementCapability::CreateUser, true, "perm_users");

			assertTrue(pPolicy->roleHasCapability(pAdmin.get(), AMC::eUserManagementCapability::CreateUser), "Admin holds the permission");
			assertFalse(pPolicy->roleHasCapability(pOperator.get(), AMC::eUserManagementCapability::CreateUser), "Operator lacks the permission");
			assertFalse(pPolicy->roleHasCapability(pAdmin.get(), AMC::eUserManagementCapability::DeleteUser), "Unconfigured capability stays off");
		}

		void testDisabledCapability() {
			AMC::CAccessControl control;
			auto pRole = control.addRole("operator", makeString("Operator"), makeString(""));
			auto pPolicy = control.getUserManagementPolicy();

			pPolicy->setCapability(AMC::eUserManagementCapability::ChangeOwnPassword, false, "");
			assertFalse(pPolicy->roleHasCapability(pRole.get(), AMC::eUserManagementCapability::ChangeOwnPassword));
		}

		void testRoleSubset() {
			AMC::CAccessControl control;
			auto pRead = control.addPermission("perm_read", makeString("Read"), makeString(""));
			auto pWrite = control.addPermission("perm_write", makeString("Write"), makeString(""));

			auto pObserver = control.addRole("observer", makeString("Observer"), makeString(""));
			auto pOperator = control.addRole("operator", makeString("Operator"), makeString(""));
			auto pGuest = control.addRole("guest", makeString("Guest"), makeString(""));
			pObserver->addPermission(pRead);
			pOperator->addPermission(pRead);
			pOperator->addPermission(pWrite);

			assertTrue(control.roleIsSubsetOf("observer", "operator"), "Observer is below operator");
			assertFalse(control.roleIsSubsetOf("operator", "observer"), "Operator is not below observer");
			assertTrue(control.roleIsSubsetOf("operator", "operator"), "A role is a subset of itself");
			assertTrue(control.roleIsSubsetOf("guest", "observer"), "A role without permissions is below every role");
		}

		void testAssignableRoles() {
			AMC::CUserManagementPolicy policy;
			assertTrue(policy.roleIsInAssignableList("anything"), "Without a list every role passes");

			policy.restrictAssignableRoles({ "observer", "operator" });
			assertTrue(policy.isAssignableRolesRestricted());
			assertTrue(policy.roleIsInAssignableList("operator"));
			assertFalse(policy.roleIsInAssignableList("administrator"));
		}

		void testRoleMigration() {
			AMC::CAccessControl control;
			auto pOld = control.addRole("oldrole", makeString("Old"), makeString(""));
			auto pMiddle = control.addRole("middlerole", makeString("Middle"), makeString(""));
			control.addRole("operator", makeString("Operator"), makeString(""));

			pOld->setMigrateTo("middlerole");
			pMiddle->setMigrateTo("operator");

			assertTrue(pOld->isDeprecated());
			assertTrue(control.resolveMigratedRole("oldrole") == "operator", "Migration chains are followed");
			assertTrue(control.resolveMigratedRole("operator") == "operator", "Active roles stay unchanged");
			assertTrue(control.resolveMigratedRole("unknownrole") == "unknownrole", "Unknown roles stay unchanged");

			bool bThrown = false;
			try {
				pOld->setMigrateTo("oldrole");
			}
			catch (...) {
				bThrown = true;
			}
			assertTrue(bThrown, "A role can not migrate to itself");
		}

		void testCyclicRoleMigrationThrows() {
			AMC::CAccessControl control;
			auto pA = control.addRole("rolea", makeString("A"), makeString(""));
			auto pB = control.addRole("roleb", makeString("B"), makeString(""));
			pA->setMigrateTo("roleb");
			pB->setMigrateTo("rolea");

			bool bThrown = false;
			try {
				control.resolveMigratedRole("rolea");
			}
			catch (...) {
				bThrown = true;
			}
			assertTrue(bThrown, "Cyclic migration must throw");
		}

		void testLanguages() {
			AMC::CUserManagementPolicy policy;
			policy.addLanguage("en", "English");
			policy.addLanguage("de", "");
			policy.addLanguage("en", "Duplicate");
			policy.addLanguage("", "Empty");

			auto languages = policy.getLanguages();
			assertTrue(languages.size() == 2, "Two languages expected");
			assertTrue(languages.at(0).m_sDisplayName == "English");
			assertTrue(languages.at(1).m_sDisplayName == "de", "Missing name falls back to the identifier");
			assertTrue(policy.hasLanguage("de"));
			assertFalse(policy.hasLanguage("fr"));
		}

	};

}

#endif //__AMCTEST_UNITTEST_USERMANAGEMENTPOLICY
