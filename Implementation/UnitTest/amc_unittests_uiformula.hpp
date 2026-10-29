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

#ifndef __AMCTEST_UNITTEST_UIFORMULA
#define __AMCTEST_UNITTEST_UIFORMULA


#include "amc_unittests.hpp"
#include "amc_ui_formula.hpp"
#include "amc_ui_expression.hpp"
#include "amc_ui_frontenddefinition.hpp"
#include "amc_ui_frontendstate.hpp"
#include "amc_statemachinedata.hpp"
#include "amc_parameterhandler.hpp"
#include "amc_parametergroup.hpp"
#include "amc_userinformation.hpp"
#include "amc_accesscontrol.hpp"
#include "amc_accessrole.hpp"
#include "common_chrono.hpp"
#include "common_utils.hpp"

#include <cmath>
#include <vector>


namespace AMCUnitTest {

	class CUnitTestGroup_UIFormula : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "UIFormula";
		}

		void registerTests() override {
			registerTest("Arithmetic", "Formula literals and arithmetic keep integer and double types", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFormula::testArithmetic, this));
			registerTest("LogicAndComparison", "Logic operators, keywords, comparisons and short-circuit evaluation", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFormula::testLogicAndComparison, this));
			registerTest("Functions", "Whitelisted functions", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFormula::testFunctions, this));
			registerTest("SyntaxErrors", "Invalid formulas are rejected when they are compiled", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFormula::testSyntaxErrors, this));
			registerTest("References", "Formulas over several parameters and state machines", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFormula::testReferences, this));
			registerTest("SingleReferenceCompatibility", "Single references keep the established evaluation path", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFormula::testSingleReferenceCompatibility, this));
			registerTest("Validation", "Load-time validation of formula references", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFormula::testValidation, this));
			registerTest("FailSoft", "Runtime errors return neutral values and are reported once", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFormula::testFailSoft, this));
			registerTest("SessionReferences", "Formulas with several session references are session scoped", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFormula::testSessionReferences, this));
		}

		void initializeTests() override {
		}

	private:

		struct sFormulaFixture {
			AMC::PStateMachineData m_pStateMachineData;
			AMC::PParameterGroup m_pUIGroup;
			AMC::PParameterGroup m_pJobInfoGroup;
		};

		sFormulaFixture createFixture()
		{
			auto pChrono = std::make_shared<AMCCommon::CChrono>();
			auto pChangeCounter = std::make_shared<AMC::CFrontendChangeCounter>();

			sFormulaFixture fixture;

			auto pMainHandler = std::make_shared<AMC::CParameterHandler>("main", pChrono, pChangeCounter);
			fixture.m_pUIGroup = pMainHandler->addGroup("ui", "UI");
			fixture.m_pUIGroup->addNewBoolParameter("main_haserror", "Main error", false);
			fixture.m_pUIGroup->addNewBoolParameter("laser_haserror", "Laser error", false);
			fixture.m_pJobInfoGroup = pMainHandler->addGroup("jobinfo", "Job information");
			fixture.m_pJobInfoGroup->addNewIntParameter("layercount", "Layer count", 0);
			fixture.m_pJobInfoGroup->addNewIntParameter("currentlayer", "Current layer", 0);
			fixture.m_pJobInfoGroup->addNewDoubleParameter("currentheight", "Current height", 0.0, 0.001);
			fixture.m_pJobInfoGroup->addNewStringParameter("jobname", "Job name", "Job");

			auto pLaserHandler = std::make_shared<AMC::CParameterHandler>("laser", pChrono, pChangeCounter);
			auto pStatusGroup = pLaserHandler->addGroup("status", "Status");
			pStatusGroup->addNewBoolParameter("laserok", "Laser OK", true);

			fixture.m_pStateMachineData = std::make_shared<AMC::CStateMachineData>();
			fixture.m_pStateMachineData->registerParameterHandler("main", pMainHandler, pChrono);
			fixture.m_pStateMachineData->registerParameterHandler("laser", pLaserHandler, pChrono);
			fixture.m_pStateMachineData->setInstanceStateName("main", "idle");
			fixture.m_pStateMachineData->setInstanceStateName("laser", "ready");

			return fixture;
		}

		AMC::CUIExpression createSyncExpression(const std::string& sExpression)
		{
			AMC::CUIExpression expression;
			expression.setSyncValue(sExpression);
			return expression;
		}

		AMC::CUIFormulaValue evaluate(const std::string& sFormula)
		{
			AMC::CUIFormula formula(sFormula);
			return formula.evaluate(nullptr);
		}

		template <typename F> bool throwsException(F function)
		{
			try {
				function();
			}
			catch (...) {
				return true;
			}
			return false;
		}

		bool isNear(double dValue, double dExpected)
		{
			return std::fabs(dValue - dExpected) < 1.0e-9;
		}

		void testArithmetic()
		{
			auto sum = evaluate("1 + 2 * 3");
			assertTrue(sum.getType() == AMC::eUIFormulaValueType::Integer);
			assertTrue(sum.toInteger() == 7);

			assertTrue(evaluate("(1 + 2) * 3").toInteger() == 9);
			assertTrue(evaluate("-2 + 5").toInteger() == 3);
			assertTrue(evaluate("7 % 3").toInteger() == 1);
			assertTrue(evaluate("- -4").toInteger() == 4);

			auto quotient = evaluate("10 / 4");
			assertTrue(quotient.getType() == AMC::eUIFormulaValueType::Double);
			assertTrue(isNear(quotient.toDouble(), 2.5));
			assertTrue(evaluate("100 * 3 / 4").toString() == "75", "integral doubles are written without decimals");

			assertTrue(isNear(evaluate("1.5e2").toDouble(), 150.0));
			assertTrue(isNear(evaluate(".5 + 0.25").toDouble(), 0.75));
			assertTrue(evaluate("9223372036854775807 + 1").getType() == AMC::eUIFormulaValueType::Double, "integer overflow falls back to double");
			assertTrue(evaluate("'3' + 4").toInteger() == 7, "numeric strings are numbers in arithmetic");
		}

		void testLogicAndComparison()
		{
			assertTrue(evaluate("true and not false").toBool());
			assertTrue(evaluate("true && !false").toBool());
			assertTrue(evaluate("false or true").toBool());
			assertFalse(evaluate("false || false").toBool());
			assertTrue(evaluate("not 1 == 2").toBool(), "not binds looser than comparisons");

			assertTrue(evaluate("1 < 2 and 2 <= 2 and 3 > 2 and 3 >= 3").toBool());
			assertTrue(evaluate("1 == 1.0").toBool());
			assertTrue(evaluate("'idle' == 'idle'").toBool());
			assertTrue(evaluate("'idle' != 'ready'").toBool());
			assertTrue(evaluate("\"x\" == 'x'").toBool(), "both quote styles are strings");
			assertTrue(evaluate("'it''s' == \"it's\"").toBool(), "doubled quotes escape the quote");
			assertTrue(evaluate("'10' == 10").toBool());
			assertFalse(evaluate("'abc' == 1").toBool());
			assertTrue(evaluate("'abc' != 1").toBool());
			assertTrue(evaluate("'a' < 'b'").toBool());

			assertFalse(evaluate("false and 1 / 0 > 0").toBool(), "and short-circuits");
			assertTrue(evaluate("true or 1 / 0 > 0").toBool(), "or short-circuits");
			assertTrue(throwsException([&]() { evaluate("'abc' < 1"); }), "ordering a non-numeric string against a number is a runtime error");
		}

		void testFunctions()
		{
			assertTrue(evaluate("if(1 > 0, 'yes', 'no')").toString() == "yes");
			assertTrue(evaluate("if(0, 1 / 0, 5)").toInteger() == 5, "if evaluates only the chosen branch");
			assertTrue(evaluate("between(5, 1, 10)").toBool());
			assertFalse(evaluate("between(11, 1, 10)").toBool());
			assertTrue(evaluate("coalesce('', 'fallback')").toString() == "fallback");
			assertTrue(evaluate("coalesce('first', 1 / 0)").toString() == "first", "coalesce stops at the first non-empty value");

			auto minimum = evaluate("min(3, 1, 2)");
			assertTrue(minimum.getType() == AMC::eUIFormulaValueType::Integer);
			assertTrue(minimum.toInteger() == 1);
			assertTrue(isNear(evaluate("max(1, 2.5, 2)").toDouble(), 2.5));
			assertTrue(evaluate("clamp(15, 0, 10)").toInteger() == 10);
			assertTrue(evaluate("clamp(-5, 0, 10)").toInteger() == 0);
			assertTrue(evaluate("abs(-3)").toInteger() == 3);

			assertTrue(evaluate("round(2.5)").toInteger() == 3);
			assertTrue(evaluate("round(2.5)").getType() == AMC::eUIFormulaValueType::Integer);
			assertTrue(isNear(evaluate("round(1.2345, 2)").toDouble(), 1.23));
			assertTrue(evaluate("floor(2.7)").toInteger() == 2);
			assertTrue(evaluate("ceil(2.1)").toInteger() == 3);

			assertTrue(evaluate("concat('Layer ', 3, ' / ', 10)").toString() == "Layer 3 / 10");
			assertTrue(evaluate("concat(true, false)").toString() == "10", "bools are written like boolean parameters");
			assertTrue(evaluate("fixed(3.14159, 2)").toString() == "3.14");
			assertTrue(evaluate("fixed(-0.001, 2)").toString() == "0.00", "fixed does not produce a negative zero");
			assertTrue(evaluate("fixed(2, 1)").toString() == "2.0");
			assertTrue(evaluate("upper('Idle')").toString() == "IDLE");
			assertTrue(evaluate("lower('Idle')").toString() == "idle");
			assertTrue(evaluate("len(concat('\xC3\xA4', 'b'))").toInteger() == 2, "len counts UTF-8 code points");
			assertTrue(evaluate("contains('fatalerror', 'error')").toBool());
			assertTrue(evaluate("startswith('fatalerror', 'fatal')").toBool());
			assertFalse(evaluate("startswith('error', 'fatal')").toBool());

			assertTrue(throwsException([&]() { evaluate("round(1.5, 16)"); }), "decimals are limited");
			assertTrue(throwsException([&]() { evaluate("clamp(1, 10, 0)"); }), "clamp bounds must be ordered");
		}

		void testSyntaxErrors()
		{
			std::vector<std::string> invalidFormulas = {
				"",
				"   ",
				"1 +",
				"(1 + 2",
				"1 2",
				"main.ui",
				"main.ui.flag.extra",
				"main..ui",
				"main.ui.",
				"main.$group.flag",
				"foo",
				"unknown(1)",
				"if(1, 2)",
				"min()",
				"1 = 2",
				"main.ui.a & main.ui.b",
				"main.ui.a | main.ui.b",
				"1 < 2 < 3",
				"'unterminated",
				"2abc",
				"1.2.3",
				"#"
			};

			for (auto& sFormula : invalidFormulas)
				assertTrue(throwsException([&]() { AMC::CUIFormula formula(sFormula); }), "formula must be rejected: " + sFormula);

			std::string sDeepFormula;
			for (uint32_t nIndex = 0; nIndex < 100; nIndex++)
				sDeepFormula += "(";
			sDeepFormula += "1";
			for (uint32_t nIndex = 0; nIndex < 100; nIndex++)
				sDeepFormula += ")";
			assertTrue(throwsException([&]() { AMC::CUIFormula formula(sDeepFormula); }), "deep nesting must be rejected");

			assertTrue(throwsException([&]() { AMC::CUIFormula formula(std::string(AMC_UI_FORMULA_MAXLENGTH + 1, '1')); }), "long formulas must be rejected");
			assertTrue(throwsException([&]() { createSyncExpression("main.ui.a +"); }), "CUIExpression rejects invalid formulas");
		}

		void testReferences()
		{
			auto fixture = createFixture();
			auto pData = fixture.m_pStateMachineData.get();

			auto errorExpression = createSyncExpression("main.ui.main_haserror or main.ui.laser_haserror");
			assertTrue(errorExpression.isFormula());
			assertFalse(errorExpression.evaluateBoolValue(pData));
			fixture.m_pUIGroup->setBoolParameterValueByName("laser_haserror", true);
			assertTrue(errorExpression.evaluateBoolValue(pData));

			assertTrue(createSyncExpression("main.$state == 'idle' and laser.$state == 'ready'").evaluateBoolValue(pData), "formulas may reference several state machines");
			assertTrue(createSyncExpression("laser.status.laserok and not main.ui.main_haserror").evaluateBoolValue(pData));

			auto progressExpression = createSyncExpression("if(main.jobinfo.layercount > 0, 100 * main.jobinfo.currentlayer / main.jobinfo.layercount, 0)");
			assertTrue(isNear(progressExpression.evaluateNumberValue(pData), 0.0));
			fixture.m_pJobInfoGroup->setIntParameterValueByName("layercount", 4);
			fixture.m_pJobInfoGroup->setIntParameterValueByName("currentlayer", 1);
			assertTrue(isNear(progressExpression.evaluateNumberValue(pData), 25.0));
			assertTrue(progressExpression.evaluateIntegerValue(pData) == 25);
			assertTrue(progressExpression.evaluateStringValue(pData) == "25");

			auto layerExpression = createSyncExpression("concat(main.jobinfo.jobname, ': ', main.jobinfo.currentlayer, ' / ', main.jobinfo.layercount)");
			assertTrue(layerExpression.evaluateStringValue(pData) == "Job: 1 / 4");

			fixture.m_pJobInfoGroup->setDoubleParameterValueByName("currentheight", 1.25);
			assertTrue(createSyncExpression("concat(fixed(main.jobinfo.currentheight, 2), ' mm')").evaluateStringValue(pData) == "1.25 mm");

			pugi::xml_document document;
			assertTrue(document.load_string("<item sync:value=\"main.jobinfo.currentlayer / main.jobinfo.layercount * 100\" format:value=\"%.1f %%\" />"));
			AMC::CUIExpression formattedExpression(document.child("item"), "value");
			assertTrue(formattedExpression.isFormula());
			assertTrue(formattedExpression.evaluateStringValue(pData) == "25.0 %", "format: applies to formula results");

			auto references = AMC::CUIFormula("main.jobinfo.currentlayer + main.jobinfo.currentlayer + laser.$state").getReferences();
			assertTrue(references.size() == 2, "references are deduplicated");
			assertTrue(references.at(0).m_Type == AMC::eUIFormulaReferenceType::Parameter);
			assertTrue(references.at(0).m_sGroupName == "jobinfo");
			assertTrue(references.at(1).m_Type == AMC::eUIFormulaReferenceType::StateName);
			assertTrue(references.at(1).m_sInstanceName == "laser");
		}

		void testSingleReferenceCompatibility()
		{
			auto fixture = createFixture();
			auto pData = fixture.m_pStateMachineData.get();

			auto plainExpression = createSyncExpression("main.ui.main_haserror");
			assertFalse(plainExpression.isFormula());
			assertFalse(plainExpression.evaluateBoolValue(pData));

			auto invertedExpression = createSyncExpression("!main.ui.main_haserror");
			assertFalse(invertedExpression.isFormula());
			assertTrue(invertedExpression.evaluateBoolValue(pData));

			auto keywordExpression = createSyncExpression("not main.ui.main_haserror");
			assertFalse(keywordExpression.isFormula(), "a negated single reference uses the established path");
			assertTrue(keywordExpression.evaluateBoolValue(pData));

			auto stateExpression = createSyncExpression(" main.$state ");
			assertFalse(stateExpression.isFormula());
			assertTrue(stateExpression.evaluateStringValue(pData) == "idle");

			assertTrue(createSyncExpression("main.jobinfo.jobname").evaluateStringValue(pData) == "Job");
			assertTrue(createSyncExpression("!!main.ui.main_haserror").isFormula());
			assertFalse(createSyncExpression("!!main.ui.main_haserror").evaluateBoolValue(pData));
		}

		void testValidation()
		{
			auto fixture = createFixture();
			auto pData = fixture.m_pStateMachineData.get();

			createSyncExpression("main.ui.main_haserror or laser.status.laserok").checkExpressionSyntax(pData);
			createSyncExpression("laser.$state == 'ready' or session.flag").checkExpressionSyntax(pData);

			assertTrue(throwsException([&]() { createSyncExpression("main.ui.unknown or main.ui.main_haserror").checkExpressionSyntax(pData); }), "unknown parameter must be rejected");
			assertTrue(throwsException([&]() { createSyncExpression("main.unknown.flag or main.ui.main_haserror").checkExpressionSyntax(pData); }), "unknown group must be rejected");
			assertTrue(throwsException([&]() { createSyncExpression("unknown.$state == 'idle'").checkExpressionSyntax(pData); }), "unknown state machine must be rejected");
		}

		void testFailSoft()
		{
			auto fixture = createFixture();
			auto pData = fixture.m_pStateMachineData.get();

			std::vector<std::string> warnings;
			fixture.m_pStateMachineData->setWarningHandler([&warnings](const std::string& sMessage) {
				warnings.push_back(sMessage);
			});

			auto divisionExpression = createSyncExpression("100 / main.jobinfo.layercount");
			assertTrue(isNear(divisionExpression.evaluateNumberValue(pData), 0.0));
			assertTrue(divisionExpression.evaluateIntegerValue(pData) == 0);
			assertTrue(divisionExpression.evaluateStringValue(pData).empty());
			assertTrue(warnings.size() == 1, "a failing formula is reported once");

			auto typeExpression = createSyncExpression("main.jobinfo.jobname * 2");
			assertFalse(typeExpression.evaluateBoolValue(pData));
			assertTrue(warnings.size() == 2, "each failing formula is reported");

			fixture.m_pJobInfoGroup->setIntParameterValueByName("layercount", 4);
			assertTrue(isNear(divisionExpression.evaluateNumberValue(pData), 25.0), "a formula recovers when its inputs become valid");
		}

		void testSessionReferences()
		{
			auto fixture = createFixture();
			auto pData = fixture.m_pStateMachineData.get();

			auto pDefinition = std::make_shared<AMC::CUIFrontendDefinition>(std::make_shared<AMCCommon::CChrono>(), std::make_shared<AMC::CFrontendChangeCounter>());
			pDefinition->addSessionVariable("flag", "bool", "Flag", "0");
			pDefinition->addSessionVariable("counter", "int", "Counter", "5");
			pDefinition->addSessionVariable("caption", "string", "Caption", "Hello");

			auto mixedExpression = createSyncExpression("session.flag and not main.ui.main_haserror");
			assertTrue(mixedExpression.hasSessionReferences());
			assertTrue(mixedExpression.getSessionReference() == "session.flag");

			auto multiExpression = createSyncExpression("concat(session.caption, ' ', $user.login, ' ', session.caption)");
			std::vector<std::string> references;
			multiExpression.getSessionReferences(references);
			assertTrue(references.size() == 2);
			assertTrue(references.at(0) == "session.caption");
			assertTrue(references.at(1) == "$user.login");

			assertFalse(createSyncExpression("main.ui.main_haserror or laser.status.laserok").hasSessionReferences());
			assertTrue(createSyncExpression("!session.flag").hasSessionReferences());

			AMC::CUIFrontendState session(pDefinition);
			assertFalse(mixedExpression.evaluateBoolValue(pData, &session));
			session.setSessionVariableAsBool("flag", true);
			assertTrue(mixedExpression.evaluateBoolValue(pData, &session));

			auto counterExpression = createSyncExpression("session.counter + 1");
			assertTrue(counterExpression.evaluateIntegerValue(pData, &session) == 6, "session variables keep their declared type");
			assertTrue(counterExpression.evaluateIntegerValue(pData) == 1, "session references are empty without a session context");

			auto pAccessControl = std::make_shared<AMC::CAccessControl>();
			auto pPermission = pAccessControl->addPermission("perm_edit", AMC::CLanguageString("", "Edit"), AMC::CLanguageString("", ""));
			auto pRole = pAccessControl->addRole("operator", AMC::CLanguageString("", "Operator"), AMC::CLanguageString("", ""));
			pRole->addPermission(pPermission);
			auto pUserInformation = std::make_shared<AMC::CUserInformation>(AMCCommon::CUtils::createUUID(), "jdoe", "John Doe", "operator", "en");
			session.setSessionIdentity(AMCCommon::CUtils::createUUID(), pUserInformation, pAccessControl);

			assertTrue(multiExpression.evaluateStringValue(pData, &session) == "Hello jdoe Hello");
			assertTrue(createSyncExpression("$user.permission.perm_edit and session.flag").evaluateBoolValue(pData, &session));
		}

	};

}

#endif // __AMCTEST_UNITTEST_UIFORMULA
