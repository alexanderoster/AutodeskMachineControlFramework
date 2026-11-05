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

#ifndef __AMCTEST_UNITTEST_EVENTFORMVALUES
#define __AMCTEST_UNITTEST_EVENTFORMVALUES


#include "amc_unittests.hpp"
#include "amc_ui_eventformvalues.hpp"
#include "amc_ui_expression.hpp"
// The test drives a real form edit's poll path, so it opts into the protected UI module headers.
#ifndef __AMCIMPL_UI_MODULE
#define __AMCIMPL_UI_MODULE
#endif
#include "amc_ui_module_contentitem_form.hpp"
#include "amc_statemachinedata.hpp"
#include "amc_parameterhandler.hpp"
#include "amc_parametergroup.hpp"
#include "common_chrono.hpp"


namespace AMCUnitTest {

	// A value posted with an event must survive a content poll that resyncs a
	// sync:value edit between the payload write and the event handler's read.
	class CUnitTestGroup_EventFormValues : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "EventFormValues";
		}

		void registerTests() override {
			registerTest("PostedValueSurvivesPoll", "A posted sync:value edit value survives a concurrent content poll", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_EventFormValues::testPostedValueSurvivesPoll, this));
			registerTest("OtherPropertiesAreLive", "Only the posted value is read from the snapshot", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_EventFormValues::testOtherPropertiesAreLive, this));
			registerTest("UnpostedPathsAreLive", "Paths that were not posted read the client variables", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_EventFormValues::testUnpostedPathsAreLive, this));
			registerTest("WriteDropsSnapshot", "Setting the value during the event replaces the posted value", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_EventFormValues::testWriteDropsSnapshot, this));
		}

		void initializeTests() override {
		}

	private:

		struct sEditFixture {
			AMC::PStateMachineData m_pStateMachineData;
			AMC::PParameterGroup m_pMachineGroup;
			AMC::PParameterHandler m_pClientVariables;
			AMC::PUIModule_ContentFormEdit m_pEdit;
			std::string m_sEditPath;
		};

		AMC::CUIExpression createExpression(const std::string& sValue, bool bSync)
		{
			AMC::CUIExpression expression;
			if (bSync)
				expression.setSyncValue(sValue);
			else
				expression.setFixedValue(sValue);
			return expression;
		}

		// An editable <edit name="laserpower" value="sync:main.ui.laserpower"/> in form "main.start.form",
		// with its client variables populated as for a session.
		sEditFixture createFixture()
		{
			auto pChrono = std::make_shared<AMCCommon::CChrono>();
			auto pChangeCounter = std::make_shared<AMC::CFrontendChangeCounter>();

			sEditFixture fixture;

			auto pMainHandler = std::make_shared<AMC::CParameterHandler>("main", pChrono, pChangeCounter);
			fixture.m_pMachineGroup = pMainHandler->addGroup("ui", "UI");
			fixture.m_pMachineGroup->addNewIntParameter("laserpower", "Laser power", 700);

			fixture.m_pStateMachineData = std::make_shared<AMC::CStateMachineData>();
			fixture.m_pStateMachineData->registerParameterHandler("main", pMainHandler, pChrono);

			fixture.m_pEdit = std::make_shared<AMC::CUIModule_ContentFormEdit>("laserpower", "main.start.form",
				createExpression("Laser Power", false), createExpression("main.ui.laserpower", true),
				createExpression("", false), createExpression("", false), "", fixture.m_pStateMachineData);
			fixture.m_sEditPath = fixture.m_pEdit->getElementPath();

			fixture.m_pClientVariables = std::make_shared<AMC::CParameterHandler>("client", pChrono, pChangeCounter);
			fixture.m_pEdit->populateClientVariables(fixture.m_pClientVariables.get());

			return fixture;
		}

		// What CUIHandler::handleEvent does for each posted form value.
		void postValue(sEditFixture& fixture, AMC::CUIEventFormValues& eventFormValues, const std::string& sValue)
		{
			auto pGroup = fixture.m_pClientVariables->findGroup(fixture.m_sEditPath, true);
			pGroup->setParameterValueByName("value", sValue);
			eventFormValues.capturePostedValue(fixture.m_sEditPath, pGroup, sValue);
		}

		// What a content poll of the form does to the edit.
		void poll(sEditFixture& fixture)
		{
			fixture.m_pEdit->syncClientVariables(fixture.m_pClientVariables.get());
		}

		void testPostedValueSurvivesPoll()
		{
			auto fixture = createFixture();
			AMC::CUIEventFormValues eventFormValues(std::make_shared<AMCCommon::CChrono>());

			postValue(fixture, eventFormValues, "310");
			poll(fixture);

			assertTrue(fixture.m_pClientVariables->findGroup(fixture.m_sEditPath, true)->getParameterValueByName("value") == "700", "the poll resyncs the shared client variable (the race this group covers)");

			auto pReadGroup = eventFormValues.groupForRead(fixture.m_pClientVariables.get(), fixture.m_sEditPath, "value");
			assertTrue(pReadGroup->getParameterValueByName("value") == "310", "the handler reads the posted value");
			assertTrue(pReadGroup->getIntParameterValueByName("value") == 310, "the posted value converts like the client variable");
		}

		void testOtherPropertiesAreLive()
		{
			auto fixture = createFixture();
			AMC::CUIEventFormValues eventFormValues(std::make_shared<AMCCommon::CChrono>());

			postValue(fixture, eventFormValues, "310");
			fixture.m_pClientVariables->findGroup(fixture.m_sEditPath, true)->setParameterValueByName("caption", "Changed");

			auto pReadGroup = eventFormValues.groupForRead(fixture.m_pClientVariables.get(), fixture.m_sEditPath, "caption");
			assertTrue(pReadGroup->getParameterValueByName("caption") == "Changed");
		}

		void testUnpostedPathsAreLive()
		{
			auto fixture = createFixture();
			AMC::CUIEventFormValues eventFormValues(std::make_shared<AMCCommon::CChrono>());

			poll(fixture);
			auto pReadGroup = eventFormValues.groupForRead(fixture.m_pClientVariables.get(), fixture.m_sEditPath, "value");
			assertTrue(pReadGroup->getParameterValueByName("value") == "700");
		}

		void testWriteDropsSnapshot()
		{
			auto fixture = createFixture();
			AMC::CUIEventFormValues eventFormValues(std::make_shared<AMCCommon::CChrono>());

			postValue(fixture, eventFormValues, "310");

			auto pWriteGroup = eventFormValues.groupForWrite(fixture.m_pClientVariables.get(), fixture.m_sEditPath, "value");
			pWriteGroup->setParameterValueByName("value", "500");

			auto pReadGroup = eventFormValues.groupForRead(fixture.m_pClientVariables.get(), fixture.m_sEditPath, "value");
			assertTrue(pReadGroup->getParameterValueByName("value") == "500", "a value set by the handler is read back");
		}

	};

}

#endif //__AMCTEST_UNITTEST_EVENTFORMVALUES
