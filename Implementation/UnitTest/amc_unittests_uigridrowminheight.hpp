/*++

Copyright (C) 2020 Autodesk Inc.

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
#ifndef __AMCTEST_UNITTEST_UIGRIDROWMINHEIGHT
#define __AMCTEST_UNITTEST_UIGRIDROWMINHEIGHT


#include "amc_unittests.hpp"
#include "amc_ui_gridrowminheight.hpp"

#include "Libraries/PugiXML/pugixml.hpp"


namespace AMCUnitTest {

	class CUnitTestGroup_UIGridRowMinHeight : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "UIGridRowMinHeight";
		}

		void registerTests() override {
			registerTest("MissingAttributeIsNotSet", "A row without the minheight attribute keeps the default behavior of its unit", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIGridRowMinHeight::testMissingAttributeIsNotSet, this));
			registerTest("NonPositiveValuesAreNotSet", "A minimum height of zero or below is ignored without reporting a configuration issue", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIGridRowMinHeight::testNonPositiveValuesAreNotSet, this));
			registerTest("FreeAndAutoRowsAcceptTheValue", "Free and auto rows take the minimum height and keep their own sizing beyond it", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIGridRowMinHeight::testFreeAndAutoRowsAcceptTheValue, this));
			registerTest("FixedRowsReportTheAttributeAsIgnored", "Pixel and point rows report the attribute as ignored instead of applying it", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIGridRowMinHeight::testFixedRowsReportTheAttributeAsIgnored, this));
			registerTest("UnitsAreNormalized", "Row units and minimum height units are matched independent of case and surrounding spaces", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_UIGridRowMinHeight::testUnitsAreNormalized, this));
		}

		void initializeTests() override {
		}

	private:

		static AMC::sGridRowMinHeight readRowMinHeight(const std::string& sAttributes, const std::string& sRowUnit)
		{
			pugi::xml_document document;
			document.load_string(("<row height=\"1\"" + sAttributes + "/>").c_str());

			pugi::xml_node rowNode = document.first_child();
			return AMC::CUIGridRowMinHeight::readFromXML(rowNode, sRowUnit);
		}

		void testMissingAttributeIsNotSet()
		{
			auto minHeight = readRowMinHeight("", "free");
			assertTrue(minHeight.m_bIsSet == false);
			assertTrue(minHeight.m_bIgnoredOnUnit == false);
			assertTrue(minHeight.m_fHeight == 0.0f);
			assertTrue(minHeight.m_sUnit == "");
		}

		void testNonPositiveValuesAreNotSet()
		{
			auto zeroMinHeight = readRowMinHeight(" minheight=\"0\"", "free");
			assertTrue(zeroMinHeight.m_bIsSet == false);
			assertTrue(zeroMinHeight.m_bIgnoredOnUnit == false);

			auto negativeMinHeight = readRowMinHeight(" minheight=\"-12\"", "auto");
			assertTrue(negativeMinHeight.m_bIsSet == false);
			assertTrue(negativeMinHeight.m_bIgnoredOnUnit == false);
		}

		void testFreeAndAutoRowsAcceptTheValue()
		{
			auto freeMinHeight = readRowMinHeight(" minheight=\"90\"", "free");
			assertTrue(freeMinHeight.m_bIsSet == true);
			assertTrue(freeMinHeight.m_bIgnoredOnUnit == false);
			assertTrue(freeMinHeight.m_fHeight == 90.0f);
			assertTrue(freeMinHeight.m_sUnit == "");

			auto autoMinHeight = readRowMinHeight(" minheight=\"90\"", "auto");
			assertTrue(autoMinHeight.m_bIsSet == true);
			assertTrue(autoMinHeight.m_bIgnoredOnUnit == false);
		}

		void testFixedRowsReportTheAttributeAsIgnored()
		{
			auto pointMinHeight = readRowMinHeight(" minheight=\"90\"", "pt");
			assertTrue(pointMinHeight.m_bIsSet == false);
			assertTrue(pointMinHeight.m_bIgnoredOnUnit == true);
			assertTrue(pointMinHeight.m_sUnit == "");

			auto pixelMinHeight = readRowMinHeight(" minheight=\"90\"", "px");
			assertTrue(pixelMinHeight.m_bIsSet == false);
			assertTrue(pixelMinHeight.m_bIgnoredOnUnit == true);
		}

		void testUnitsAreNormalized()
		{
			auto upperCaseRow = readRowMinHeight(" minheight=\"90\"", "FREE");
			assertTrue(upperCaseRow.m_bIsSet == true);

			auto paddedUnit = readRowMinHeight(" minheight=\"90\" minheightunit=\" PX \"", "free");
			assertTrue(paddedUnit.m_bIsSet == true);
			assertTrue(paddedUnit.m_sUnit == "px");

			auto upperCaseUnit = readRowMinHeight(" minheight=\"90\" minheightunit=\"PT\"", "auto");
			assertTrue(upperCaseUnit.m_sUnit == "pt");
		}
	};

}

#endif //__AMCTEST_UNITTEST_UIGRIDROWMINHEIGHT
