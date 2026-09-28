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

#ifndef __AMCTEST_UNITTEST_UIFRONTEND
#define __AMCTEST_UNITTEST_UIFRONTEND

// The page headers are protected for implementation files; the tests use them white-box.
#define __AMCIMPL_UI_MODULE
#define __AMCIMPL_UI_PAGE

#include "amc_unittests.hpp"
#include "amc_ui_frontenddefinition.hpp"
#include "amc_ui_frontendstate.hpp"
#include "amc_ui_page.hpp"
#include "amc_parametergroup.hpp"
#include "common_chrono.hpp"
#include "common_utils.hpp"

#include "RapidJSON/document.h"


namespace AMCUnitTest {

	// Pages require an event handler; the tests never trigger events.
	class CUnitTestUIEventHandler : public AMC::CUIModule_UIEventHandler {
	public:

		void ensureUIEventExists(const std::string& sEventName) override
		{
		}

		AMC::CUIHandleEventResponse handleEvent(const std::string& sEventName, const std::string& sSenderUUID, const std::string& sEventFormPayloadJSON, const std::string& sEventParameterJSON, AMC::PAPIAuth pAPIAuth) override
		{
			throw std::runtime_error("unit test event handler does not handle events: " + sEventName);
		}
	};

	class CUnitTestGroup_UIFrontend : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "UIFrontend";
		}

		void registerTests() override {
			registerTest("DefinitionValues", "Frontend attributes report definition values without client variables", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFrontend::testDefinitionValues, this));
			registerTest("ClientVariableValues", "Frontend attributes report values set through the client variables", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFrontend::testClientVariableValues, this));
			registerTest("SynchronizedAttributes", "Attributes bound to state machine data are flagged as synchronized", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFrontend::testSynchronizedAttributes, this));
			registerTest("PageShowEvent", "Frontend page status contains the show event", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIFrontend::testPageShowEvent, this));
		}

		void initializeTests() override {
		}

	private:

		static AMC::CUIExpression fixedExpression(const std::string& sValue)
		{
			AMC::CUIExpression expression;
			expression.setFixedValue(sValue);
			return expression;
		}

		static AMC::PUIFrontendDefinition createDefinition()
		{
			return std::make_shared<AMC::CUIFrontendDefinition>(std::make_shared<AMCCommon::CChrono>());
		}

		// Attributes of a form entity, one per value type.
		static AMC::PUIFrontendDefinitionModuleStore createEntityStore(AMC::PUIFrontendDefinition pDefinition, const std::string& sPath)
		{
			auto pStore = pDefinition->registerModuleStore(AMCCommon::CUtils::createUUID(), sPath, "edit");
			pStore->registerValue("caption", AMC::eUIFrontendDefinitionAttributeType::atString, fixedExpression("PLC IP Address"));
			pStore->registerValue("value", AMC::eUIFrontendDefinitionAttributeType::atString, fixedExpression("0"));
			pStore->registerValue("count", AMC::eUIFrontendDefinitionAttributeType::atInteger, fixedExpression("0"));
			pStore->registerValue("ratio", AMC::eUIFrontendDefinitionAttributeType::atNumber, fixedExpression("0"));
			pStore->registerValue("visible", AMC::eUIFrontendDefinitionAttributeType::atBoolean, fixedExpression("1"));
			pStore->registerValue("readonly", AMC::eUIFrontendDefinitionAttributeType::atBoolean, fixedExpression("0"));
			return pStore;
		}

		void writeAttributes(AMC::CUIFrontendState& frontendState, AMC::CUIFrontendDefinitionModuleStore* pStore, rapidjson::Document& document)
		{
			AMC::CJSONWriter writer;
			AMC::CJSONWriterObject attributesObject(writer);
			frontendState.writeModuleAttributesToJSON(writer, attributesObject, pStore, nullptr);
			writer.addObject("attributes", attributesObject);

			document.Parse(writer.saveToString().c_str());
			assertTrue(document.IsObject() && document.HasMember("attributes"), "attributes JSON");
		}

		void writePageStatus(AMC::CUIPage& page, AMC::CUIFrontendState& frontendState, rapidjson::Document& document)
		{
			AMC::CJSONWriter writer;
			AMC::CJSONWriterObject pageObject(writer);
			page.frontendWritePageStatusToJSON(writer, pageObject, &frontendState, nullptr);
			writer.addObject("page", pageObject);

			document.Parse(writer.saveToString().c_str());
			assertTrue(document.IsObject() && document.HasMember("page"), "page JSON");
		}

		void testDefinitionValues()
		{
			auto pDefinition = createDefinition();
			auto pStore = createEntityStore(pDefinition, "systemsettings.plcipaddress.plcipaddress.plcipaddress");
			AMC::CUIFrontendState frontendState(pDefinition);

			rapidjson::Document document;
			writeAttributes(frontendState, pStore.get(), document);
			const auto& attributes = document["attributes"];

			assertTrue(attributes["caption"].GetString() == std::string("PLC IP Address"), "caption");
			assertTrue(attributes["value"].GetString() == std::string("0"), "value");
			assertTrue(attributes["count"].GetInt64() == 0, "count");
			assertDoubleRange(attributes["ratio"].GetDouble(), 0.0, 0.0, "ratio");
			assertTrue(attributes["visible"].GetBool(), "visible");
			assertFalse(attributes["readonly"].GetBool(), "readonly");
		}

		void testClientVariableValues()
		{
			std::string sPath = "systemsettings.plcipaddress.plcipaddress.plcipaddress";
			auto pDefinition = createDefinition();
			auto pStore = createEntityStore(pDefinition, sPath);
			auto pOtherStore = createEntityStore(pDefinition, "systemsettings.plcmask.plcmask.plcmask");
			AMC::CUIFrontendState frontendState(pDefinition);

			// Client variables as populateClientVariables creates them, changed like UIEnvironment::SetUIProperty does.
			auto pGroup = frontendState.getLegacyParameterHandler()->addGroup(sPath, "form entity edit");
			pGroup->addNewStringParameter("value", "edit value", "0");
			pGroup->addNewStringParameter("count", "combo value", "0");
			pGroup->addNewDoubleParameter("ratio", "ratio", 0.0, 0.001);
			pGroup->addNewBoolParameter("visible", "visibility", true);
			pGroup->setParameterValueByName("value", "192.168.2.2");
			pGroup->setIntParameterValueByName("count", 2);
			pGroup->setDoubleParameterValueByName("ratio", 1007.0);
			pGroup->setBoolParameterValueByName("visible", false);

			rapidjson::Document document;
			writeAttributes(frontendState, pStore.get(), document);
			const auto& attributes = document["attributes"];

			assertTrue(attributes["value"].GetString() == std::string("192.168.2.2"), "string client value");
			assertTrue(attributes["count"].GetInt64() == 2, "integer client value");
			assertDoubleRange(attributes["ratio"].GetDouble(), 1006.999, 1007.001, "number client value");
			assertFalse(attributes["visible"].GetBool(), "boolean client value");

			// Attributes without a client variable keep their definition value.
			assertTrue(attributes["caption"].GetString() == std::string("PLC IP Address"), "caption without client variable");
			assertFalse(attributes["readonly"].GetBool(), "readonly without client variable");

			// Client variables of one element must not change other elements.
			rapidjson::Document otherDocument;
			writeAttributes(frontendState, pOtherStore.get(), otherDocument);
			assertTrue(otherDocument["attributes"]["value"].GetString() == std::string("0"), "other element value");
			assertTrue(otherDocument["attributes"]["visible"].GetBool(), "other element visibility");
		}

		void testSynchronizedAttributes()
		{
			auto pDefinition = createDefinition();
			auto pStore = pDefinition->registerModuleStore(AMCCommon::CUtils::createUUID(), "systemsettings.scanner2", "content");

			AMC::CUIExpression syncExpression;
			syncExpression.setSyncValue("main.jobinfo.isactive");

			auto pSyncAttribute = pStore->registerValue("visible", AMC::eUIFrontendDefinitionAttributeType::atBoolean, syncExpression);
			auto pFixedAttribute = pStore->registerValue("caption", AMC::eUIFrontendDefinitionAttributeType::atString, fixedExpression("Scanner2"));

			assertTrue(pSyncAttribute->isSynchronized(), "sync expression");
			assertFalse(pFixedAttribute->isSynchronized(), "fixed expression");
		}

		void testPageShowEvent()
		{
			CUnitTestUIEventHandler eventHandler;
			auto pDefinition = createDefinition();
			AMC::CUIFrontendState frontendState(pDefinition);

			AMC::CUIPage pageWithShowEvent("systemsettings", &eventHandler, fixedExpression(""), fixedExpression("System Settings"), fixedExpression(""), "onshowsystemsettings");
			AMC::CUIPage pageWithoutShowEvent("buildlist", &eventHandler, fixedExpression(""), fixedExpression("Build Library"), fixedExpression(""), "");

			rapidjson::Document document;
			writePageStatus(pageWithShowEvent, frontendState, document);
			assertTrue(document["page"]["name"].GetString() == std::string("systemsettings"), "page name");
			assertTrue(document["page"].HasMember("showevent"), "page with show event");
			assertTrue(document["page"]["showevent"].GetString() == std::string("onshowsystemsettings"), "show event name");

			rapidjson::Document documentWithoutShowEvent;
			writePageStatus(pageWithoutShowEvent, frontendState, documentWithoutShowEvent);
			assertFalse(documentWithoutShowEvent["page"].HasMember("showevent"), "page without show event");
		}
	};

}

#endif // __AMCTEST_UNITTEST_UIFRONTEND
