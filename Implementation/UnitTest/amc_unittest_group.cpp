/*++

Copyright (C) 2022 Autodesk Inc.

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

#include "amc_unittest_group.hpp"
#include "Common/common_utils.hpp"



using namespace AMCUnitTest;

EUnitTestFailed::EUnitTestFailed(const std::string& sTestName, const std::string& sMessage)
	: std::runtime_error(sMessage)
{

}

CUnitTestGroupTest::CUnitTestGroupTest(const std::string& sTestName, const std::string& sTestDescription, eUnitTestCategory testCategory, std::function<void()> callback)
	: m_sTestName (sTestName), m_sTestDescription (sTestDescription), m_TestCategory (testCategory), m_Callback (callback)
{
	if (sTestName.empty())
		throw std::runtime_error("empty test name");

	if ((testCategory != eUnitTestCategory::utMandatoryPass) && (testCategory != eUnitTestCategory::utOptionalPass) && (testCategory != eUnitTestCategory::utOptionalRunAndPass))
		throw std::runtime_error("invalid unit test category: " + sTestName);

	if (callback == nullptr)
		throw std::runtime_error("invalid unit test callback: " + sTestName);


}

std::string CUnitTestGroupTest::getTestName()
{
	return m_sTestName;
}

std::string CUnitTestGroupTest::getTestDescription()
{
	return m_sTestDescription;
}

eUnitTestCategory CUnitTestGroupTest::getTestCategory()
{
	return m_TestCategory;
}

void CUnitTestGroupTest::runTest()
{
	m_Callback();
}

CUnitTestGroup::CUnitTestGroup()
	: m_pOwner (nullptr)
{

}

CUnitTestGroup::~CUnitTestGroup()
{

}

void CUnitTestGroup::setOwner(CUnitTestOwner* pOwner)
{
	if (pOwner == nullptr)
		throw std::runtime_error("invalid owner for unit test " + getTestGroupName ());

	m_pOwner = pOwner;
}

std::list<std::string> CUnitTestGroup::getTestNames()
{
	std::list<std::string> namesList;
	for (auto iIter : m_Tests)
		namesList.push_back(iIter.first);

	return namesList;
		
}

void CUnitTestGroup::registerTest(const std::string& sTestName, const std::string& sTestDescription, eUnitTestCategory testCategory, std::function<void()> callback)
{
	if (sTestName.empty())
		throw std::runtime_error("unit test has empty name string in group " + getTestGroupName());

	if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString (sTestName))
		throw std::runtime_error("unit test has invalid name string in group " + getTestGroupName() + " : " + sTestName);

	auto iIter = m_Tests.find (sTestName);
	if (iIter != m_Tests.end())
		throw std::runtime_error("duplicate unit test name " + sTestName + " in group " + getTestGroupName());

	m_Tests.insert(std::make_pair(sTestName, CUnitTestGroupTest(sTestName, sTestDescription, testCategory, callback)));

}

void CUnitTestGroup::logWarning(const std::string& sMessage)
{
	if (m_pOwner == nullptr)
		throw std::runtime_error("unit test group " + getTestGroupName() + " has on assigned owner");

	m_pOwner->logTestWarning(sMessage);
}

void CUnitTestGroup::logInfo(const std::string& sMessage)
{
	if (m_pOwner == nullptr)
		throw std::runtime_error("unit test group " + getTestGroupName() + " has on assigned owner");

	m_pOwner->logTestInfo(sMessage);
}


void CUnitTestGroup::assertTrue(bool bValue, const std::string& sContext)
{
	if (!bValue) {
		std::string sMessage = "Condition was not true";
		if (!sContext.empty())
			sMessage += " (" + sContext + ")";

		throw EUnitTestFailed (getTestGroupName () + "/" + m_sRunningTest, sMessage);
	}
}

void CUnitTestGroup::assertFalse(bool bValue, const std::string& sContext)
{
	if (bValue) {
		std::string sMessage = "Condition was not false";
		if (!sContext.empty())
			sMessage += " (" + sContext + ")";

		throw EUnitTestFailed(getTestGroupName() + "/" + m_sRunningTest, sMessage);
	}
}

void CUnitTestGroup::assertAssigned(void* pPtr, const std::string& sContext)
{
	if (pPtr == nullptr) {
		std::string sMessage = "Pointer was null";
		if (!sContext.empty())
			sMessage += " (" + sContext + ")";

		throw EUnitTestFailed(getTestGroupName() + "/" + m_sRunningTest, sMessage);
	}
}

void CUnitTestGroup::assertNull(void* pPtr, const std::string& sContext)
{
	if (pPtr != nullptr) {
		std::string sMessage = "Pointer was not null";
		if (!sContext.empty())
			sMessage += " (" + sContext + ")";

		throw EUnitTestFailed(getTestGroupName() + "/" + m_sRunningTest, sMessage);
	}
}


void CUnitTestGroup::assertIntegerRange(int64_t nValue, int64_t nMin, int64_t nMax, const std::string& sContext)
{
	if ((nValue < nMin) || (nValue > nMax)) {
		std::string sMessage = "Integer value " + std::to_string (nValue) + " was not in range [" + std::to_string (nMin) + ".." + std::to_string (nMax) + "]";
		if (!sContext.empty())
			sMessage += " (" + sContext + ")";

		throw EUnitTestFailed(getTestGroupName() + "/" + m_sRunningTest, sMessage);
	}
}


void CUnitTestGroup::assertDoubleRange(double dValue, double dMin, double dMax, const std::string& sContext)
{
	if ((dValue < dMin) || (dValue > dMax)) {
		std::string sMessage = "Float value " + std::to_string(dValue) + " was not in range [" + std::to_string(dMin) + ".." + std::to_string(dMax) + "]";
		if (!sContext.empty())
			sMessage += " (" + sContext + ")";

		throw EUnitTestFailed(getTestGroupName() + "/" + m_sRunningTest, sMessage);
	}

}

void CUnitTestGroup::runTest(const std::string& sTestName)
{
	auto iIter = m_Tests.find(sTestName);
	if (iIter == m_Tests.end())
		throw std::runtime_error ("unit test " + sTestName + " not found in group " + getTestGroupName ());

	m_sRunningTest = sTestName;
	try {
		iIter->second.runTest();
		m_sRunningTest = "";
	}
	catch (...) {
		m_sRunningTest = "";
		throw;
	}

}