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

#ifndef __AMCTEST_UNITTEST_GROUP
#define __AMCTEST_UNITTEST_GROUP


#include <string>
#include <memory>
#include <map>
#include <cstdint>
#include <stdexcept>

#include <string>
#include <functional>
#include <stdexcept>
#include <map>
#include <list>

namespace AMCUnitTest {

	
	enum class eUnitTestCategory : uint32_t {
		utUnknown = 0,
		utMandatoryPass = 1, // Must run and pass
		utOptionalPass = 2, // Must run but not necessarily pass
		utOptionalRunAndPass = 3 // Extended test to run in an extended version
		
	};
	
	class CUnitTestGroupTest {
	private:
		std::string m_sTestName;
		std::string m_sTestDescription;
		eUnitTestCategory m_TestCategory;
		std::function<void()> m_Callback;

	public:
		CUnitTestGroupTest(const std::string& sTestName, const std::string& sTestDescription, eUnitTestCategory testCategory, std::function<void()> callback);

		std::string getTestName();
		std::string getTestDescription();
		eUnitTestCategory getTestCategory();
		
		void runTest();

	};

	class CUnitTestOwner {
	public:
		virtual void logTestWarning(const std::string& sMessage) = 0;

		virtual void logTestInfo(const std::string& sMessage) = 0;

	};
	
	
	class EUnitTestFailed : public std::runtime_error {
		
		public:
			EUnitTestFailed (const std::string& sTestName, const std::string & sMessage);
		
	};



	class CUnitTestGroup {	
	private:
		
		CUnitTestOwner* m_pOwner;

		std::string m_sRunningTest;

		std::map<std::string, CUnitTestGroupTest> m_Tests;
		
	protected:

		// registers a test to the testing framework
		void registerTest (const std::string & sTestName, const std::string & sTestDescription, eUnitTestCategory testCategory, std::function<void()> callback);

		// logs a warning to the test report and increases the warning counter
		void logWarning (const std::string & sMessage);

		// logs an info string to the test report
		void logInfo (const std::string & sMessage);

		// asserts if something is true
		void assertTrue (bool bValue, const std::string & sContext = "");		

		// asserts if something is false
		void assertFalse (bool bValue, const std::string & sContext = "");		

		// asserts that a pointer is not null
		void assertAssigned (void * pPtr, const std::string & sContext = "");		

		// asserts that a pointer is null
		void assertNull(void* pPtr, const std::string& sContext = "");

		// asserts that a value is in an Integer Range (Boundaries included)
		void assertIntegerRange (int64_t nValue, int64_t nMin, int64_t nMax, const std::string & sContext = "");		

		// asserts that a value is in an Integer Range (Boundaries included)
		void assertDoubleRange (double dValue, double dMin, double dMax, const std::string & sContext = "");		

	public:
		
		CUnitTestGroup ();

		virtual ~CUnitTestGroup();

		// Sets the owner of the test group
		void setOwner(CUnitTestOwner* pOwner);

		// Lists all tests in the group
		std::list<std::string> getTestNames();
		
		// Returns a test group name
		virtual std::string getTestGroupName () = 0;

		// Registers all test for that group
		virtual void registerTests () = 0;

		// Initializes all the tests and prepares custom memory allocations
		virtual void initializeTests () = 0;
					
		// Runs a specific test
		// Throw exceptions to fail the test
		void runTest (const std::string & sTestName);
		
		
			
	};

	typedef std::shared_ptr<CUnitTestGroup> PUnitTestGroup;
	
	
	
}

#endif //__AMCTEST_UNITTEST_GROUP