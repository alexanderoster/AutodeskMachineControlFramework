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


#ifndef __AMC_FIRSTRUNHANDLER
#define __AMC_FIRSTRUNHANDLER

#include <memory>
#include <string>
#include <vector>
#include <mutex>

#define LOG_SUBSYSTEM_FIRSTRUN "firstrun"

namespace AMC {

	class CLogger;
	typedef std::shared_ptr<CLogger> PLogger;

	class CFirstRunHandler;
	typedef std::shared_ptr<CFirstRunHandler> PFirstRunHandler;

	enum class eFirstRunSetupCodeMode : uint32_t {
		Required = 1,
		Localhost = 2,
		None = 3
	};

	class CFirstRunNextStep {
	public:
		std::string m_sText;
		std::string m_sPage;
	};

	// The machine is in first-run mode while no active user exists. In that mode the
	// first administrator account can be created without login, protected by a one-time
	// setup code that is only shown in the server console and log.
	class CFirstRunHandler {
	private:

		std::mutex m_Mutex;

		PLogger m_pLogger;

		std::string m_sAdminRoleIdentifier;
		eFirstRunSetupCodeMode m_SetupCodeMode;
		std::string m_sWelcomeTitle;
		std::string m_sWelcomeText;
		std::vector<CFirstRunNextStep> m_NextSteps;

		bool m_bIsFirstRun;

		// Normalized form, without separator.
		std::string m_sSetupCode;
		uint32_t m_nFailedAttempts;
		uint64_t m_nBlockedUntilMicroseconds;

		void regenerateSetupCodeNoLock();

		void logMessage(const std::string& sMessage, bool bIsWarning);

	public:

		static const uint32_t MaxFailedSetupCodeAttempts = 5;
		static const uint64_t FailedAttemptDelayMicroseconds = 1000000ULL;
		static const uint64_t RegeneratedCodeDelayMicroseconds = 10000000ULL;

		CFirstRunHandler(PLogger pLogger);

		virtual ~CFirstRunHandler();

		// Returns a random code of the form XXXX-XXXX from an alphabet without look-alike characters.
		static std::string generateSetupCode();

		static std::string normalizeSetupCode(const std::string& sSetupCode);

		static std::string formatSetupCode(const std::string& sNormalizedSetupCode);

		static bool isLoopbackAddress(const std::string& sClientAddress);

		static eFirstRunSetupCodeMode stringToSetupCodeMode(const std::string& sMode);

		static std::string setupCodeModeToString(eFirstRunSetupCodeMode mode);

		void setAdminRoleIdentifier(const std::string& sRoleIdentifier);
		std::string getAdminRoleIdentifier();
		bool isAvailable();

		void setSetupCodeMode(eFirstRunSetupCodeMode mode);
		eFirstRunSetupCodeMode getSetupCodeMode();

		void setWelcomeTexts(const std::string& sWelcomeTitle, const std::string& sWelcomeText);
		std::string getWelcomeTitle();
		std::string getWelcomeText();

		void addNextStep(const std::string& sText, const std::string& sPage);
		std::vector<CFirstRunNextStep> getNextSteps();

		// Called once after the configuration has been loaded.
		void initialize(bool bHasActiveUsers);

		bool isFirstRun();

		bool setupCodeIsRequiredFor(const std::string& sClientAddress);

		// Throws INVALIDSETUPCODE on mismatch. Repeated failures delay further attempts,
		// and after MaxFailedSetupCodeAttempts the code is replaced by a new one.
		void verifySetupCode(const std::string& sSetupCode, const std::string& sClientAddress, uint64_t nNowMicroseconds);

		void complete(const std::string& sLogin, const std::string& sClientAddress);

		// Only for the console log and for tests. MUST NOT be returned by any API.
		std::string getCurrentSetupCode();

	};

}


#endif //__AMC_FIRSTRUNHANDLER
