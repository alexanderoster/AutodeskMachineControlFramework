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

#ifndef __AMCTEST_UNITTEST_FIRSTRUN
#define __AMCTEST_UNITTEST_FIRSTRUN


#include "amc_unittests.hpp"
#include "amc_accesscontrol.hpp"
#include "amc_accessrole.hpp"
#include "amc_firstrunhandler.hpp"
#include "common_utils.hpp"
#include "libmc_exceptiontypes.hpp"

#include <set>

namespace AMCUnitTest {

	class CUnitTestGroup_FirstRun : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "FirstRun";
		}

		void registerTests() override {
			registerTest("SetupCodeFormat", "Setup codes have the form XXXX-XXXX without look-alike characters", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testSetupCodeFormat, this));
			registerTest("NormalizeSetupCode", "Separators and case are ignored", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testNormalizeSetupCode, this));
			registerTest("LoopbackAddress", "Loopback address detection", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testLoopbackAddress, this));
			registerTest("SetupCodeModes", "Setup code mode names round-trip and unknown names throw", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testSetupCodeModes, this));
			registerTest("NotFirstRun", "With active users no setup code exists and verification fails", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testNotFirstRun, this));
			registerTest("VerifySetupCode", "Correct code passes, wrong code fails", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testVerifySetupCode, this));
			registerTest("FailedAttemptDelay", "A wrong code blocks further attempts for a moment", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testFailedAttemptDelay, this));
			registerTest("CodeRegeneration", "Too many wrong codes replace the setup code", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testCodeRegeneration, this));
			registerTest("LocalhostMode", "Localhost mode needs the code only for remote clients", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testLocalhostMode, this));
			registerTest("NoneMode", "None mode needs no code", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testNoneMode, this));
			registerTest("NoAdminRole", "Without an administrator role the setup is unavailable", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testNoAdminRole, this));
			registerTest("Complete", "Completing the setup ends first-run mode", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testComplete, this));
			registerTest("NextSteps", "Next steps keep their order and skip empty texts", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testNextSteps, this));
			registerTest("FindRoleWithAllPermissions", "The administrator role is found automatically", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testFindRoleWithAllPermissions, this));
			registerTest("FindRoleAmbiguous", "No role is picked if the choice is ambiguous", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_FirstRun::testFindRoleAmbiguous, this));
		}

		void initializeTests() override {
		}

	private:

		static const uint64_t Second = 1000000ULL;

		AMC::CLanguageString makeString(const std::string& s) {
			return AMC::CLanguageString("", s);
		}

		// Returns the error code of the thrown exception, or 0 if the call succeeded.
		uint32_t verifyErrorCode(AMC::CFirstRunHandler& handler, const std::string& sCode, const std::string& sAddress, uint64_t nNow) {
			try {
				handler.verifySetupCode(sCode, sAddress, nNow);
				return 0;
			}
			catch (ELibMCInterfaceException& e) {
				return (uint32_t)e.getErrorCode();
			}
		}

		std::shared_ptr<AMC::CFirstRunHandler> createFirstRunHandler(AMC::eFirstRunSetupCodeMode mode) {
			auto pHandler = std::make_shared<AMC::CFirstRunHandler>(nullptr);
			pHandler->setAdminRoleIdentifier("administrator");
			pHandler->setSetupCodeMode(mode);
			pHandler->initialize(false);
			return pHandler;
		}

		void testSetupCodeFormat() {
			std::string sAlphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
			std::set<std::string> codes;

			for (uint32_t nIndex = 0; nIndex < 50; nIndex++) {
				std::string sCode = AMC::CFirstRunHandler::generateSetupCode();
				assertTrue(sCode.length() == 9, "Code must have 9 characters: " + sCode);
				assertTrue(sCode.at(4) == '-', "Code must have a separator in the middle: " + sCode);

				std::string sNormalized = AMC::CFirstRunHandler::normalizeSetupCode(sCode);
				assertTrue(sNormalized.length() == 8, "Normalized code must have 8 characters: " + sCode);
				for (char c : sNormalized)
					assertTrue(sAlphabet.find(c) != std::string::npos, "Unexpected character in code: " + sCode);

				codes.insert(sNormalized);
			}

			assertTrue(codes.size() > 45, "Codes must be random");
		}

		void testNormalizeSetupCode() {
			assertTrue(AMC::CFirstRunHandler::normalizeSetupCode("abcd-efgh") == "ABCDEFGH");
			assertTrue(AMC::CFirstRunHandler::normalizeSetupCode(" AB CD-EF GH ") == "ABCDEFGH");
			assertTrue(AMC::CFirstRunHandler::normalizeSetupCode("") == "");
			assertTrue(AMC::CFirstRunHandler::formatSetupCode("ABCDEFGH") == "ABCD-EFGH");
			assertTrue(AMC::CFirstRunHandler::formatSetupCode("") == "");
		}

		void testLoopbackAddress() {
			assertTrue(AMC::CFirstRunHandler::isLoopbackAddress("127.0.0.1"));
			assertTrue(AMC::CFirstRunHandler::isLoopbackAddress("127.1.2.3"));
			assertTrue(AMC::CFirstRunHandler::isLoopbackAddress("::1"));
			assertTrue(AMC::CFirstRunHandler::isLoopbackAddress(" ::FFFF:127.0.0.1 "));
			assertFalse(AMC::CFirstRunHandler::isLoopbackAddress("192.168.1.10"));
			assertFalse(AMC::CFirstRunHandler::isLoopbackAddress("10.127.0.1"));
			assertFalse(AMC::CFirstRunHandler::isLoopbackAddress("::ffff:192.168.1.10"));
			assertFalse(AMC::CFirstRunHandler::isLoopbackAddress(""), "Unknown addresses are remote");
		}

		void testSetupCodeModes() {
			assertTrue(AMC::CFirstRunHandler::stringToSetupCodeMode("") == AMC::eFirstRunSetupCodeMode::Required, "Default is required");
			assertTrue(AMC::CFirstRunHandler::stringToSetupCodeMode(" Localhost ") == AMC::eFirstRunSetupCodeMode::Localhost);

			for (auto mode : { AMC::eFirstRunSetupCodeMode::Required, AMC::eFirstRunSetupCodeMode::Localhost, AMC::eFirstRunSetupCodeMode::None }) {
				std::string sName = AMC::CFirstRunHandler::setupCodeModeToString(mode);
				assertTrue(AMC::CFirstRunHandler::stringToSetupCodeMode(sName) == mode, sName);
			}

			bool bThrown = false;
			try {
				AMC::CFirstRunHandler::stringToSetupCodeMode("optional");
			}
			catch (ELibMCInterfaceException& e) {
				bThrown = (e.getErrorCode() == LIBMC_ERROR_INVALIDFIRSTRUNSETUPCODEMODE);
			}
			assertTrue(bThrown, "Unknown setup code mode must throw");
		}

		void testNotFirstRun() {
			AMC::CFirstRunHandler handler(nullptr);
			handler.setAdminRoleIdentifier("administrator");
			handler.initialize(true);

			assertFalse(handler.isFirstRun());
			assertTrue(handler.getCurrentSetupCode().empty(), "No setup code outside of first-run mode");
			assertTrue(verifyErrorCode(handler, "", "127.0.0.1", 0) == LIBMC_ERROR_FIRSTRUNALREADYCOMPLETED);
		}

		void testVerifySetupCode() {
			auto pHandler = createFirstRunHandler(AMC::eFirstRunSetupCodeMode::Required);
			assertTrue(pHandler->isFirstRun());
			assertTrue(pHandler->isAvailable());

			std::string sCode = pHandler->getCurrentSetupCode();
			assertTrue(sCode.length() == 8, "A setup code is generated on initialization");
			assertTrue(pHandler->setupCodeIsRequiredFor("127.0.0.1"), "Required mode also applies to this computer");

			assertTrue(verifyErrorCode(*pHandler, "", "192.168.1.10", 0) == LIBMC_ERROR_INVALIDSETUPCODE, "Empty code");
			assertTrue(verifyErrorCode(*pHandler, sCode.substr(0, 7), "192.168.1.10", 2 * Second) == LIBMC_ERROR_INVALIDSETUPCODE, "Truncated code");
			assertTrue(verifyErrorCode(*pHandler, sCode + "A", "192.168.1.10", 4 * Second) == LIBMC_ERROR_INVALIDSETUPCODE, "Extended code");

			std::string sFormattedLowerCase = AMCCommon::CUtils::toLowerString(AMC::CFirstRunHandler::formatSetupCode(sCode));
			assertTrue(verifyErrorCode(*pHandler, sFormattedLowerCase, "192.168.1.10", 6 * Second) == 0, "Correct code in any spelling passes");
			assertTrue(pHandler->isFirstRun(), "Verification alone does not end first-run mode");
		}

		void testFailedAttemptDelay() {
			auto pHandler = createFirstRunHandler(AMC::eFirstRunSetupCodeMode::Required);
			std::string sCode = pHandler->getCurrentSetupCode();

			assertTrue(verifyErrorCode(*pHandler, "WRONGCODE", "192.168.1.10", 10 * Second) == LIBMC_ERROR_INVALIDSETUPCODE);
			assertTrue(verifyErrorCode(*pHandler, sCode, "192.168.1.10", 10 * Second + Second / 2) == LIBMC_ERROR_INVALIDSETUPCODE, "Even the correct code is rejected while blocked");
			assertTrue(verifyErrorCode(*pHandler, sCode, "192.168.1.10", 12 * Second) == 0, "The correct code passes after the delay");
		}

		void testCodeRegeneration() {
			auto pHandler = createFirstRunHandler(AMC::eFirstRunSetupCodeMode::Required);
			std::string sOldCode = pHandler->getCurrentSetupCode();

			uint64_t nNow = 100 * Second;
			for (uint32_t nAttempt = 0; nAttempt < AMC::CFirstRunHandler::MaxFailedSetupCodeAttempts; nAttempt++) {
				assertTrue(pHandler->getCurrentSetupCode() == sOldCode, "Code must not change before the limit");
				assertTrue(verifyErrorCode(*pHandler, "WRONGCODE", "192.168.1.10", nNow) == LIBMC_ERROR_INVALIDSETUPCODE);
				nNow += 2 * Second;
			}

			std::string sNewCode = pHandler->getCurrentSetupCode();
			assertTrue(sNewCode.length() == 8);
			assertTrue(sNewCode != sOldCode, "The code is replaced after too many failures");

			assertTrue(verifyErrorCode(*pHandler, sNewCode, "192.168.1.10", nNow) == LIBMC_ERROR_INVALIDSETUPCODE, "Longer block after regeneration");

			nNow += AMC::CFirstRunHandler::RegeneratedCodeDelayMicroseconds;
			assertTrue(verifyErrorCode(*pHandler, sOldCode, "192.168.1.10", nNow) == LIBMC_ERROR_INVALIDSETUPCODE, "The old code is no longer valid");

			nNow += 2 * Second;
			assertTrue(verifyErrorCode(*pHandler, sNewCode, "192.168.1.10", nNow) == 0, "The new code passes");
		}

		void testLocalhostMode() {
			auto pHandler = createFirstRunHandler(AMC::eFirstRunSetupCodeMode::Localhost);
			std::string sCode = pHandler->getCurrentSetupCode();
			assertTrue(sCode.length() == 8, "Remote clients still need a code");

			assertFalse(pHandler->setupCodeIsRequiredFor("127.0.0.1"));
			assertFalse(pHandler->setupCodeIsRequiredFor("::1"));
			assertTrue(pHandler->setupCodeIsRequiredFor("192.168.1.10"));

			assertTrue(verifyErrorCode(*pHandler, "", "127.0.0.1", 0) == 0, "This computer needs no code");
			assertTrue(verifyErrorCode(*pHandler, "", "192.168.1.10", 0) == LIBMC_ERROR_INVALIDSETUPCODE, "Remote clients need the code");
			assertTrue(verifyErrorCode(*pHandler, sCode, "192.168.1.10", 2 * Second) == 0);
		}

		void testNoneMode() {
			auto pHandler = createFirstRunHandler(AMC::eFirstRunSetupCodeMode::None);
			assertTrue(pHandler->getCurrentSetupCode().empty(), "No code is generated");
			assertFalse(pHandler->setupCodeIsRequiredFor("192.168.1.10"));
			assertTrue(verifyErrorCode(*pHandler, "", "192.168.1.10", 0) == 0);
		}

		void testNoAdminRole() {
			AMC::CFirstRunHandler handler(nullptr);
			handler.initialize(false);

			assertTrue(handler.isFirstRun());
			assertFalse(handler.isAvailable());
			assertTrue(handler.getCurrentSetupCode().empty(), "No code without administrator role");
			assertTrue(verifyErrorCode(handler, "", "192.168.1.10", 0) == LIBMC_ERROR_INVALIDSETUPCODE);
		}

		void testComplete() {
			auto pHandler = createFirstRunHandler(AMC::eFirstRunSetupCodeMode::Required);
			std::string sCode = pHandler->getCurrentSetupCode();

			pHandler->complete("admin", "192.168.1.10");

			assertFalse(pHandler->isFirstRun());
			assertTrue(pHandler->getCurrentSetupCode().empty(), "The code is discarded");
			assertTrue(verifyErrorCode(*pHandler, sCode, "192.168.1.10", 0) == LIBMC_ERROR_FIRSTRUNALREADYCOMPLETED, "The code can not be used twice");
		}

		void testNextSteps() {
			AMC::CFirstRunHandler handler(nullptr);
			handler.addNextStep("Create operator accounts", "users");
			handler.addNextStep("", "ignored");
			handler.addNextStep("Read the manual", "");

			auto nextSteps = handler.getNextSteps();
			assertTrue(nextSteps.size() == 2);
			assertTrue(nextSteps.at(0).m_sText == "Create operator accounts");
			assertTrue(nextSteps.at(0).m_sPage == "users");
			assertTrue(nextSteps.at(1).m_sPage.empty());
		}

		void testFindRoleWithAllPermissions() {
			AMC::CAccessControl control;
			assertTrue(control.findRoleWithAllPermissions().empty(), "No roles, no administrator");

			auto pView = control.addPermission("permission_view", makeString("View"), makeString(""));
			auto pUsers = control.addPermission("permission_user_management", makeString("Users"), makeString(""));

			auto pObserver = control.addRole("observer", makeString("Observer"), makeString(""));
			pObserver->addPermission(pView);

			auto pAdministrator = control.addRole("administrator", makeString("Administrator"), makeString(""));
			pAdministrator->addPermission(pView);
			pAdministrator->addPermission(pUsers);

			assertTrue(control.findRoleWithAllPermissions() == "administrator");

			auto pOld = control.addRole("oldadmin", makeString("Old"), makeString(""));
			pOld->addPermission(pView);
			pOld->addPermission(pUsers);
			pOld->setMigrateTo("administrator");
			assertTrue(control.findRoleWithAllPermissions() == "administrator", "Deprecated roles are ignored");
		}

		void testFindRoleAmbiguous() {
			AMC::CAccessControl control;
			auto pView = control.addPermission("permission_view", makeString("View"), makeString(""));
			auto pUsers = control.addPermission("permission_user_management", makeString("Users"), makeString(""));

			auto pViewer = control.addRole("viewer", makeString("Viewer"), makeString(""));
			pViewer->addPermission(pView);
			auto pManager = control.addRole("manager", makeString("Manager"), makeString(""));
			pManager->addPermission(pUsers);
			assertTrue(control.findRoleWithAllPermissions().empty(), "No role holds all permissions");

			pManager->addPermission(pView);
			auto pSecondAdmin = control.addRole("secondadmin", makeString("Second"), makeString(""));
			pSecondAdmin->addPermission(pView);
			pSecondAdmin->addPermission(pUsers);
			assertTrue(control.findRoleWithAllPermissions().empty(), "Two equal administrator roles are ambiguous");
		}

	};

}

#endif //__AMCTEST_UNITTEST_FIRSTRUN
