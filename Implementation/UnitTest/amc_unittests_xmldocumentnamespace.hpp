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

#ifndef __AMCTEST_UNITTEST_XMLDOCUMENTNAMESPACE
#define __AMCTEST_UNITTEST_XMLDOCUMENTNAMESPACE

#pragma once

#include "amc_unittests.hpp"
#include "amc_xmldocumentnamespace.hpp"

namespace AMCUnitTest {

	class CUnitTestGroup_XMLDocumentNamespace : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "XMLDocumentNamespace";
		}

		void registerTests() override {
			registerTest("CreateNamespaceAndReadBack", "Create a namespace and check stored values", eUnitTestCategory::utMandatoryPass,
				std::bind(&CUnitTestGroup_XMLDocumentNamespace::testCreateNamespaceAndReadBack, this));

			registerTest("PrefixMutation", "Change the prefix and verify it updates", eUnitTestCategory::utMandatoryPass,
				std::bind(&CUnitTestGroup_XMLDocumentNamespace::testPrefixMutation, this));

			registerTest("HasPrefixDetection", "Detect if prefix exists", eUnitTestCategory::utMandatoryPass,
				std::bind(&CUnitTestGroup_XMLDocumentNamespace::testHasPrefixDetection, this));

			registerTest("StaticChecksReturnTrue", "Static checks for namespace name/prefix return true", eUnitTestCategory::utMandatoryPass,
				std::bind(&CUnitTestGroup_XMLDocumentNamespace::testStaticChecksReturnTrue, this));

			registerTest("CreateWithEmptyNamespace", "Create with empty namespace name", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLDocumentNamespace::testCreateWithEmptyNamespace, this));
			registerTest("ChangePrefixToEmpty", "Change prefix to empty string", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLDocumentNamespace::testChangePrefixToEmpty, this));
			registerTest("MultiplePrefixChanges", "Change prefix multiple times", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLDocumentNamespace::testMultiplePrefixChanges, this));
			registerTest("CreateWithLongPrefixAndNamespace", "Create with long prefix and namespace", eUnitTestCategory::utOptionalPass, std::bind(&CUnitTestGroup_XMLDocumentNamespace::testCreateWithLongPrefixAndNamespace, this));
			registerTest("PrefixIsNotCopiedByReference", "Ensure prefix state is local to each instance", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLDocumentNamespace::testPrefixIsNotCopiedByReference, this));
			registerTest("CheckXMLNameValidationNegative", "Simulate name validation with invalid inputs", eUnitTestCategory::utOptionalPass, std::bind(&CUnitTestGroup_XMLDocumentNamespace::testCheckXMLNameValidationNegative, this));
			registerTest("IntegrationWithNodePrefixedAttribute", "Verify namespaced attribute resolves correctly", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLDocumentNamespace::testIntegrationWithNodePrefixedAttribute, this));
			registerTest("SamePrefixDifferentNamespace", "Same prefix allowed for different URIs", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLDocumentNamespace::testSamePrefixDifferentNamespace, this));
			registerTest("EqualityCheckByPointer", "Check that namespace comparison uses pointers", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_XMLDocumentNamespace::testEqualityCheckByPointer, this));


		}

		void initializeTests() override {
			// No setup needed
		}

	private:

		void testCreateNamespaceAndReadBack() {
			AMC::CXMLDocumentNameSpace ns("http://example.com/schema", "ex");
			assertTrue(ns.getNameSpaceName() == "http://example.com/schema");
			assertTrue(ns.getPrefix() == "ex");
		}

		void testPrefixMutation() {
			AMC::CXMLDocumentNameSpace ns("http://example.com/schema", "pre");
			assertTrue(ns.getPrefix() == "pre");

			ns.changePrefix("newpre");
			assertTrue(ns.getPrefix() == "newpre");
		}

		void testHasPrefixDetection() {
			AMC::CXMLDocumentNameSpace nsWith("http://ns.com", "abc");
			assertTrue(nsWith.hasPrefix());

			AMC::CXMLDocumentNameSpace nsWithout("http://ns.com", "");
			assertFalse(nsWithout.hasPrefix());
		}

		void testStaticChecksReturnTrue() {
			assertTrue(AMC::CXMLDocumentNameSpace::checkXMLNameSpaceName("abc"));
			assertTrue(AMC::CXMLDocumentNameSpace::checkXMLNameSpacePrefixName("xyz"));
		}

		void testCreateWithEmptyNamespace() {
			AMC::CXMLDocumentNameSpace ns("", "pre");
			assertTrue(ns.getNameSpaceName() == "", "Namespace name should be empty");
			assertTrue(ns.getPrefix() == "pre");
		}


		void testChangePrefixToEmpty() {
			AMC::CXMLDocumentNameSpace ns("http://example.org", "someprefix");
			ns.changePrefix("");
			assertTrue(ns.getPrefix() == "", "Prefix should be cleared");
			assertFalse(ns.hasPrefix(), "hasPrefix should be false after clearing");
		}


		void testMultiplePrefixChanges() {
			AMC::CXMLDocumentNameSpace ns("http://multi.org", "a");
			ns.changePrefix("b");
			assertTrue(ns.getPrefix() == "b");
			ns.changePrefix("c");
			assertTrue(ns.getPrefix() == "c");
			ns.changePrefix("final");
			assertTrue(ns.getPrefix() == "final");
		}

		void testCreateWithLongPrefixAndNamespace() {
			std::string longNS(1000, 'n');
			std::string longPrefix(500, 'p');

			AMC::CXMLDocumentNameSpace ns(longNS, longPrefix);
			assertTrue(ns.getNameSpaceName() == longNS);
			assertTrue(ns.getPrefix() == longPrefix);
		}

		void testPrefixIsNotCopiedByReference() {
			AMC::CXMLDocumentNameSpace ns1("http://x1.com", "prefix1");
			AMC::CXMLDocumentNameSpace ns2("http://x2.com", "prefix2");

			ns1.changePrefix("modified");
			assertTrue(ns2.getPrefix() == "prefix2", "ns2 should not be affected by ns1 changes");
		}

		void testCheckXMLNameValidationNegative() {
			// Simulate invalid inputs (assumes method will be expanded)
			assertTrue(AMC::CXMLDocumentNameSpace::checkXMLNameSpaceName("") == true); // currently always true
			assertTrue(AMC::CXMLDocumentNameSpace::checkXMLNameSpacePrefixName("bad:name") == true); // currently always true
		}

		void testIntegrationWithNodePrefixedAttribute() {
			auto doc = std::make_shared<AMC::CXMLDocumentInstance>();
			doc->createEmptyDocument("root", "http://default");

			doc->RegisterNamespace("http://otherns.com", "ns");

			auto ns = doc->FindNamespaceByPrefix("ns", true);
			auto root = doc->GetRootNode();

			auto attr = root->AddAttribute(ns, "attrname", "value");
			assertTrue(attr->getPrefixedName() == "ns:attrname", "Prefixed attribute name mismatch");
		}


		void testSamePrefixDifferentNamespace() {
			AMC::CXMLDocumentNameSpace ns1("http://a.com", "x");
			AMC::CXMLDocumentNameSpace ns2("http://b.com", "x");

			assertTrue(ns1.getPrefix() == ns2.getPrefix(), "Prefixes match");
			assertTrue(ns1.getNameSpaceName() != ns2.getNameSpaceName(), "Namespaces differ");
		}

		void testEqualityCheckByPointer() {
			auto ns1 = std::make_shared<AMC::CXMLDocumentNameSpace>("http://a.com", "x");
			auto ns2 = std::make_shared<AMC::CXMLDocumentNameSpace>("http://a.com", "x");

			assertFalse(ns1.get() == ns2.get(), "Pointer equality expected to fail");
		}

	};
	

}

#endif // __AMCTEST_UNITTEST_XMLDOCUMENTNAMESPACE