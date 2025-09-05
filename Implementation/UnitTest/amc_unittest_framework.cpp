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


#include "amc_unittest_framework.hpp"


using namespace AMCUnitTest;


CUnitTestFramework::CUnitTestFramework(PUnitTestIO pIO)
	: m_pIO (pIO)
{
	if (pIO == nullptr)
		throw std::runtime_error ("Invalid UnitTest IO parameter");
}

CUnitTestFramework::~CUnitTestFramework()
{

}

void CUnitTestFramework::registerTestGroup(PUnitTestGroup pTestGroup)
{
	if (pTestGroup.get() == nullptr)
		throw std::runtime_error("invalid param: UnitTestGroup");

	std::string sGroupName = pTestGroup->getTestGroupName();
	auto iIterator = m_TestGroupMap.find(sGroupName);
	if (iIterator != m_TestGroupMap.end())
		throw std::runtime_error("UnitTestGroup with name " + sGroupName + " already registered");

	m_TestGroupMap.insert(std::make_pair (sGroupName, pTestGroup));
	m_TestGroupList.push_back(pTestGroup);

	pTestGroup->setOwner(this);
	pTestGroup->registerTests();

}

uint32_t CUnitTestFramework::countTests()
{
	uint32_t nCount = 0;
	for (auto pTestGroup : m_TestGroupList) {
		auto testNames = pTestGroup->getTestNames();
		nCount += static_cast<uint32_t>(testNames.size());
	}
	return nCount;
}


uint32_t CUnitTestFramework::runAllTests()
{
	uint32_t nSucessfulTests = 0;
	uint32_t nTestCount = countTests();
	uint32_t nTestIndex = 0;

	for (auto pTestGroup : m_TestGroupList) {

		//m_pIO->logMessageString("Running test group: " + pTestGroup->getTestGroupName());
		pTestGroup->initializeTests();

		auto testNames = pTestGroup->getTestNames();
		for (auto testName : testNames) {
			try {
				nTestIndex++;
				
				pTestGroup->runTest(testName);

				m_pIO->writeTestResult(pTestGroup->getTestGroupName(), testName, nTestIndex, nTestCount, true);
				nSucessfulTests++;
			}
			catch (const std::exception& e) {
				m_pIO->writeTestError(e.what());
				m_pIO->writeTestResult(pTestGroup->getTestGroupName(), testName, nTestIndex, nTestCount, false);
			}
		}



	}
	
	if (nTestCount > 0)
		m_pIO->logMessageString("Success rate " + std::to_string (nSucessfulTests * 100 / nTestCount) + "% (" + std::to_string (nSucessfulTests) + " of " + std::to_string (nTestCount) + ")");

	return nSucessfulTests;
}

void CUnitTestFramework::logTestWarning(const std::string& sMessage)
{
	m_pIO->writeTestWarning(sMessage);
}

void CUnitTestFramework::logTestInfo(const std::string& sMessage)
{
	m_pIO->writeTestInfo(sMessage);
}

