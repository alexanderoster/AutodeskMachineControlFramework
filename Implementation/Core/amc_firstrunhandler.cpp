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


#include "amc_firstrunhandler.hpp"
#include "amc_logger.hpp"

#include "common_utils.hpp"
#include "libmc_exceptiontypes.hpp"

#define FIRSTRUN_SETUPCODE_LENGTH 8

// 32 characters without I, O, 0 and 1, so a byte modulo 32 is uniformly distributed.
#define FIRSTRUN_SETUPCODE_ALPHABET "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"

namespace AMC {

	CFirstRunHandler::CFirstRunHandler(PLogger pLogger)
		: m_pLogger(pLogger),
		m_SetupCodeMode(eFirstRunSetupCodeMode::Required),
		m_bIsFirstRun(false),
		m_nFailedAttempts(0),
		m_nBlockedUntilMicroseconds(0)
	{

	}

	CFirstRunHandler::~CFirstRunHandler()
	{

	}

	std::string CFirstRunHandler::generateSetupCode()
	{
		std::string sAlphabet = FIRSTRUN_SETUPCODE_ALPHABET;
		std::string sRandomHex = AMCCommon::CUtils::calculateRandomSHA256String(1);

		std::string sCode;
		for (uint32_t nIndex = 0; nIndex < FIRSTRUN_SETUPCODE_LENGTH; nIndex++) {
			uint32_t nByte = (uint32_t)std::stoul(sRandomHex.substr(nIndex * 2, 2), nullptr, 16);
			sCode.push_back(sAlphabet.at(nByte % sAlphabet.length()));
		}

		return formatSetupCode(sCode);
	}

	std::string CFirstRunHandler::normalizeSetupCode(const std::string& sSetupCode)
	{
		std::string sNormalized;
		for (char c : sSetupCode) {
			if ((c == '-') || (c == ' '))
				continue;
			if ((c >= 'a') && (c <= 'z'))
				c = (char)(c - 'a' + 'A');
			sNormalized.push_back(c);
		}

		return sNormalized;
	}

	std::string CFirstRunHandler::formatSetupCode(const std::string& sNormalizedSetupCode)
	{
		size_t nHalf = sNormalizedSetupCode.length() / 2;
		if (nHalf == 0)
			return sNormalizedSetupCode;

		return sNormalizedSetupCode.substr(0, nHalf) + "-" + sNormalizedSetupCode.substr(nHalf);
	}

	bool CFirstRunHandler::isLoopbackAddress(const std::string& sClientAddress)
	{
		std::string sAddress = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(sClientAddress));

		if (sAddress == "::1")
			return true;
		if (sAddress.rfind("127.", 0) == 0)
			return true;
		if (sAddress.rfind("::ffff:127.", 0) == 0)
			return true;

		return false;
	}

	eFirstRunSetupCodeMode CFirstRunHandler::stringToSetupCodeMode(const std::string& sMode)
	{
		std::string sLowerMode = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(sMode));
		if ((sLowerMode == "required") || sLowerMode.empty())
			return eFirstRunSetupCodeMode::Required;
		if (sLowerMode == "localhost")
			return eFirstRunSetupCodeMode::Localhost;
		if (sLowerMode == "none")
			return eFirstRunSetupCodeMode::None;

		throw ELibMCCustomException(LIBMC_ERROR_INVALIDFIRSTRUNSETUPCODEMODE, sMode);
	}

	std::string CFirstRunHandler::setupCodeModeToString(eFirstRunSetupCodeMode mode)
	{
		switch (mode) {
		case eFirstRunSetupCodeMode::Required: return "required";
		case eFirstRunSetupCodeMode::Localhost: return "localhost";
		case eFirstRunSetupCodeMode::None: return "none";
		default:
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDFIRSTRUNSETUPCODEMODE, std::to_string((uint32_t)mode));
		}
	}

	void CFirstRunHandler::setAdminRoleIdentifier(const std::string& sRoleIdentifier)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_sAdminRoleIdentifier = sRoleIdentifier;
	}

	std::string CFirstRunHandler::getAdminRoleIdentifier()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_sAdminRoleIdentifier;
	}

	bool CFirstRunHandler::isAvailable()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return !m_sAdminRoleIdentifier.empty();
	}

	void CFirstRunHandler::setSetupCodeMode(eFirstRunSetupCodeMode mode)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_SetupCodeMode = mode;
	}

	eFirstRunSetupCodeMode CFirstRunHandler::getSetupCodeMode()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_SetupCodeMode;
	}

	void CFirstRunHandler::setWelcomeTexts(const std::string& sWelcomeTitle, const std::string& sWelcomeText)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_sWelcomeTitle = sWelcomeTitle;
		m_sWelcomeText = sWelcomeText;
	}

	std::string CFirstRunHandler::getWelcomeTitle()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_sWelcomeTitle;
	}

	std::string CFirstRunHandler::getWelcomeText()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_sWelcomeText;
	}

	void CFirstRunHandler::addNextStep(const std::string& sText, const std::string& sPage)
	{
		if (sText.empty())
			return;

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		CFirstRunNextStep nextStep;
		nextStep.m_sText = sText;
		nextStep.m_sPage = sPage;
		m_NextSteps.push_back(nextStep);
	}

	std::vector<CFirstRunNextStep> CFirstRunHandler::getNextSteps()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_NextSteps;
	}

	void CFirstRunHandler::initialize(bool bHasActiveUsers)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		m_bIsFirstRun = !bHasActiveUsers;
		m_sSetupCode.clear();
		m_nFailedAttempts = 0;
		m_nBlockedUntilMicroseconds = 0;

		if (!m_bIsFirstRun)
			return;

		if (m_sAdminRoleIdentifier.empty()) {
			logMessage("No user accounts exist, but no administrator role is known for the first-run setup. Set <firstrun adminrole=\"...\"/> in <accesscontrol>.", true);
			return;
		}

		if (m_SetupCodeMode == eFirstRunSetupCodeMode::None) {
			logMessage("No user accounts exist. Open the web client to create the administrator account. The setup code is disabled, anyone who can reach the server can do this.", true);
			return;
		}

		regenerateSetupCodeNoLock();
		std::string sMessage = "No user accounts exist. Open the web client to create the administrator account. Setup code: " + formatSetupCode(m_sSetupCode);
		if (m_SetupCodeMode == eFirstRunSetupCodeMode::Localhost)
			sMessage += " (not needed on this computer)";
		logMessage(sMessage, true);
	}

	bool CFirstRunHandler::isFirstRun()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_bIsFirstRun;
	}

	bool CFirstRunHandler::setupCodeIsRequiredFor(const std::string& sClientAddress)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		switch (m_SetupCodeMode) {
		case eFirstRunSetupCodeMode::None:
			return false;
		case eFirstRunSetupCodeMode::Localhost:
			return !isLoopbackAddress(sClientAddress);
		default:
			return true;
		}
	}

	void CFirstRunHandler::verifySetupCode(const std::string& sSetupCode, const std::string& sClientAddress, uint64_t nNowMicroseconds)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		if (!m_bIsFirstRun)
			throw ELibMCInterfaceException(LIBMC_ERROR_FIRSTRUNALREADYCOMPLETED);

		if (m_SetupCodeMode == eFirstRunSetupCodeMode::None)
			return;
		if ((m_SetupCodeMode == eFirstRunSetupCodeMode::Localhost) && isLoopbackAddress(sClientAddress))
			return;

		if (nNowMicroseconds < m_nBlockedUntilMicroseconds)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDSETUPCODE, "too many attempts, wait a few seconds");

		std::string sNormalized = normalizeSetupCode(sSetupCode);

		bool bMatches = (!m_sSetupCode.empty()) && (sNormalized.length() == m_sSetupCode.length());
		uint32_t nDifference = 0;
		for (size_t nIndex = 0; nIndex < m_sSetupCode.length(); nIndex++) {
			char cGiven = (nIndex < sNormalized.length()) ? sNormalized.at(nIndex) : 0;
			nDifference |= (uint32_t)(uint8_t)(cGiven ^ m_sSetupCode.at(nIndex));
		}
		if (nDifference != 0)
			bMatches = false;

		if (bMatches)
			return;

		m_nFailedAttempts++;
		m_nBlockedUntilMicroseconds = nNowMicroseconds + FailedAttemptDelayMicroseconds;

		if (m_nFailedAttempts >= MaxFailedSetupCodeAttempts) {
			regenerateSetupCodeNoLock();
			m_nBlockedUntilMicroseconds = nNowMicroseconds + RegeneratedCodeDelayMicroseconds;
			logMessage("First-run: too many wrong setup codes from " + sClientAddress + ". New setup code: " + formatSetupCode(m_sSetupCode), true);
		}
		else {
			logMessage("First-run: wrong setup code from " + sClientAddress, true);
		}

		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDSETUPCODE);
	}

	void CFirstRunHandler::complete(const std::string& sLogin, const std::string& sClientAddress)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		m_bIsFirstRun = false;
		m_sSetupCode.clear();
		m_nFailedAttempts = 0;
		m_nBlockedUntilMicroseconds = 0;

		logMessage("First-run: administrator account '" + sLogin + "' created from " + sClientAddress, false);
	}

	std::string CFirstRunHandler::getCurrentSetupCode()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_sSetupCode;
	}

	void CFirstRunHandler::regenerateSetupCodeNoLock()
	{
		m_sSetupCode = normalizeSetupCode(generateSetupCode());
		m_nFailedAttempts = 0;
	}

	void CFirstRunHandler::logMessage(const std::string& sMessage, bool bIsWarning)
	{
		if (m_pLogger.get() != nullptr)
			m_pLogger->logMessage(sMessage, LOG_SUBSYSTEM_FIRSTRUN, bIsWarning ? eLogLevel::Warning : eLogLevel::Message);
	}

}
