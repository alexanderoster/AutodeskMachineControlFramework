/*++

Copyright (C) 2025 Autodesk Inc.

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

#ifndef __AMCTEST_UNITTEST_XMLATTRIBUTERANGE
#define __AMCTEST_UNITTEST_XMLATTRIBUTERANGE

#include "amc_unittests.hpp"
#include "libmc_interfaceexception.hpp"
#include "amc_xmldocument.hpp"
#include "libmcenv_xmldocumentattribute.hpp"

#include <functional>
#include <memory>
#include <string>

namespace AMCUnitTest {

	class CUnitTestGroup_XMLAttributeRange : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override
		{
			return "XMLAttributeRange";
		}

		void registerTests() override
		{
			registerTest("IntegerInRange", "GetIntegerValue returns values inside and on the bounds", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLAttributeRange::testIntegerInRange, this));
			registerTest("IntegerOutOfRange", "GetIntegerValue refuses values below or above the bounds with the out-of-range code", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLAttributeRange::testIntegerOutOfRange, this));
			registerTest("DoubleInRange", "GetDoubleValue returns values inside and on the bounds", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLAttributeRange::testDoubleInRange, this));
			registerTest("DoubleOutOfRange", "GetDoubleValue refuses values below or above the bounds with the out-of-range code", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLAttributeRange::testDoubleOutOfRange, this));
			registerTest("UnparseableValue", "Unparseable values keep the invalid-value code", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLAttributeRange::testUnparseableValue, this));
		}

		void initializeTests() override
		{
		}

	private:
		AMC::PXMLDocumentInstance m_pDocument;

		std::unique_ptr<LibMCEnv::Impl::CXMLDocumentAttribute> makeAttribute(const std::string& sValue)
		{
			m_pDocument = std::make_shared<AMC::CXMLDocumentInstance>();
			m_pDocument->createEmptyDocument("root", "http://example.com");
			auto pInstance = m_pDocument->GetRootNode()->AddAttribute(m_pDocument->GetDefaultNamespace(), "value", sValue);
			return std::unique_ptr<LibMCEnv::Impl::CXMLDocumentAttribute>(new LibMCEnv::Impl::CXMLDocumentAttribute(pInstance));
		}

		LibMCResult errorCodeOf(const std::function<void()>& call)
		{
			try {
				call();
			}
			catch (ELibMCInterfaceException& e) {
				return e.getErrorCode();
			}
			return 0;
		}

		void testIntegerInRange()
		{
			assertTrue(makeAttribute("5")->GetIntegerValue(0, 10) == 5);
			assertTrue(makeAttribute("0")->GetIntegerValue(0, 10) == 0);
			assertTrue(makeAttribute("10")->GetIntegerValue(0, 10) == 10);
			assertTrue(makeAttribute("-7")->GetIntegerValue(INT64_MIN, INT64_MAX) == -7);
		}

		void testIntegerOutOfRange()
		{
			auto pBelow = makeAttribute("-1");
			assertTrue(errorCodeOf([&]() { pBelow->GetIntegerValue(0, 10); }) == LIBMCENV_ERROR_INTEGERVALUEATTRIBUTEOUTOFRANGE, "below minimum");
			auto pAbove = makeAttribute("11");
			assertTrue(errorCodeOf([&]() { pAbove->GetIntegerValue(0, 10); }) == LIBMCENV_ERROR_INTEGERVALUEATTRIBUTEOUTOFRANGE, "above maximum");
		}

		void testDoubleInRange()
		{
			assertTrue(makeAttribute("2.5")->GetDoubleValue(0.0, 10.0) == 2.5);
			assertTrue(makeAttribute("0.001")->GetDoubleValue(0.001, 1000.0) == 0.001);
			assertTrue(makeAttribute("1000")->GetDoubleValue(0.001, 1000.0) == 1000.0);
		}

		void testDoubleOutOfRange()
		{
			auto pBelow = makeAttribute("-0.5");
			assertTrue(errorCodeOf([&]() { pBelow->GetDoubleValue(0.0, 10.0); }) == LIBMCENV_ERROR_DOUBLEVALUEATTRIBUTEOUTOFRANGE, "below minimum");
			auto pAbove = makeAttribute("10.5");
			assertTrue(errorCodeOf([&]() { pAbove->GetDoubleValue(0.0, 10.0); }) == LIBMCENV_ERROR_DOUBLEVALUEATTRIBUTEOUTOFRANGE, "above maximum");
		}

		void testUnparseableValue()
		{
			auto pText = makeAttribute("abc");
			assertTrue(errorCodeOf([&]() { pText->GetIntegerValue(0, 10); }) == LIBMCENV_ERROR_INVALIDINTEGERVALUEATTRIBUTE, "integer");
			assertTrue(errorCodeOf([&]() { pText->GetDoubleValue(0.0, 10.0); }) == LIBMCENV_ERROR_INVALIDDOUBLEVALUEATTRIBUTE, "double");
		}

	};

}

#endif // __AMCTEST_UNITTEST_XMLATTRIBUTERANGE
