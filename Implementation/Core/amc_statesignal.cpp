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


#include "amc_statesignal.hpp"
#include "common_utils.hpp"
#include "libmc_exceptiontypes.hpp"

#include <iterator>

namespace AMC {
	

	CStateSignalMessage::CStateSignalMessage(const std::string& sUUID, uint32_t nReactionTimeoutInMS, AMC::eAMCSignalPhase initialPhase, uint64_t nCreationTimestamp)
		: m_sUUID(AMCCommon::CUtils::normalizeUUIDString(sUUID)),
		m_nReactionTimeoutInMS(nReactionTimeoutInMS),
		m_MessagePhase (initialPhase),
		m_nCreationTimestamp (nCreationTimestamp),
		m_nMicrosecondsUntilInProcess (0),
		m_nMicrosecondsUntilHandledOrFailed (0),
		m_nMicrosecondsUntilCleared (0)

	{

	}

	CStateSignalMessage::~CStateSignalMessage()
	{

	}

	std::string CStateSignalMessage::getUUID() const
	{
		return m_sUUID;
	}

	void CStateSignalMessage::setPhase(AMC::eAMCSignalPhase messagePhase, uint64_t nGlobalTimeStamp)
	{
		if (nGlobalTimeStamp < m_nCreationTimestamp)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDTIMESTAMP, "setPhase: Global timestamp is earlier than creation timestamp");

		uint64_t nElapsedMicroseconds = (nGlobalTimeStamp - m_nCreationTimestamp);
		if (nElapsedMicroseconds > UINT32_MAX)
			nElapsedMicroseconds = UINT32_MAX;

		switch (messagePhase) {
			case AMC::eAMCSignalPhase::InProcess:
				m_nMicrosecondsUntilInProcess = (uint32_t)nElapsedMicroseconds;
				break;

			case AMC::eAMCSignalPhase::Handled:
			case AMC::eAMCSignalPhase::Failed:
			case AMC::eAMCSignalPhase::TimedOut:
				m_nMicrosecondsUntilHandledOrFailed = (uint32_t)nElapsedMicroseconds;
				break;

			case AMC::eAMCSignalPhase::Cleared:
				m_nMicrosecondsUntilCleared = (uint32_t)nElapsedMicroseconds;
				break;
		}


		m_MessagePhase = messagePhase;
	}

	AMC::eAMCSignalPhase CStateSignalMessage::getPhase() const
	{
		return m_MessagePhase;
	}

	uint32_t CStateSignalMessage::getReactionTimeoutInMS() const
	{
		return m_nReactionTimeoutInMS;
	}

	bool CStateSignalMessage::hadReactionTimeout(uint64_t nGlobalTimestamp)
	{
		uint64_t nTimeoutTimestamp = m_nCreationTimestamp + m_nReactionTimeoutInMS * 1000;
		return (nGlobalTimestamp >= nTimeoutTimestamp);
	}

	std::string CStateSignalMessage::getResultDataJSON() const
	{
		return m_sResultDataJSON;
	}

	std::string CStateSignalMessage::getParameterDataJSON() const
	{
		return m_sParameterDataJSON;
	}

	std::string CStateSignalMessage::getErrorMessage() const
	{
		return m_sErrorMessage;
	}

	void CStateSignalMessage::setErrorMessage(const std::string & sErrorMessage) 
	{
		m_sErrorMessage = sErrorMessage;
	}


	void CStateSignalMessage::setResultDataJSON(const std::string& sResultDataJSON)
	{
		m_sResultDataJSON = sResultDataJSON;
	}

	void CStateSignalMessage::setParameterDataJSON(const std::string& sParameterDataJSON)
	{
		m_sParameterDataJSON = sParameterDataJSON;
	}


	CStateSignalSlot::CStateSignalSlot(const std::string& sInstanceName, const std::string& sName, const std::vector<CStateSignalParameter>& Parameters, const std::vector<CStateSignalParameter>& Results, uint32_t nSignalDefaultReactionTimeOutInMS, uint32_t nSignalQueueSize, PParameterGroup pSignalInformationGroup)
		: m_sInstanceName (sInstanceName), 
		m_sName (sName),  
		m_ParameterDefinitions(Parameters), 
		m_ResultDefinitions(Results),
		m_nSignalDefaultReactionTimeOutInMS(nSignalDefaultReactionTimeOutInMS),
		m_nSignalQueueSize (nSignalQueueSize),
		m_pSignalInformationGroup (pSignalInformationGroup),
		m_nTriggerCount (0),
		m_nHandledCount (0),
		m_nFailedCount (0),
		m_nTimedOutCount (0),
		m_nMaxReactionTime (0),
		m_nMaxSuccessTime (0)

	{

		if (pSignalInformationGroup.get() != nullptr) {
			pSignalInformationGroup->addNewIntParameter("triggered_" + m_sName, m_sName + " was triggered", 0);
			pSignalInformationGroup->addNewIntParameter("handled_" + m_sName, m_sName + " was handled", 0);
			pSignalInformationGroup->addNewIntParameter("failed_" + m_sName, m_sName + " has failed", 0);
			pSignalInformationGroup->addNewIntParameter("timeout_" + m_sName, m_sName + " timed out", 0);
			pSignalInformationGroup->addNewIntParameter("reactiontime_" + m_sName, m_sName + " max reaction time (microseconds)", 0);
			pSignalInformationGroup->addNewIntParameter("successtime_" + m_sName, m_sName + " max success time (microseconds)", 0);
		}

	}
	
	CStateSignalSlot::~CStateSignalSlot()
	{
	}

	std::string CStateSignalSlot::getNameInternal() const
	{
		return m_sName;
	}
	
	std::string CStateSignalSlot::getInstanceNameInternal() const
	{
		return m_sInstanceName;
	}

	CStateSignalMessage *CStateSignalSlot::getMessageByUUIDNoMutex(const std::string& sSignalUUID)
	{

		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sSignalUUID);
		auto it = m_MessageMap.find(sNormalizedUUID);
		if (it == m_MessageMap.end()) 
			throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "getMessageByUUIDNoMutex: Signal UUID not found: " + sNormalizedUUID);
		
		return it->second.get ();
	}

	void CStateSignalSlot::increaseTriggerCount()
	{
		m_nTriggerCount++;
		if (m_pSignalInformationGroup.get() != nullptr) {
			m_pSignalInformationGroup->setIntParameterValueByName("triggered_" + m_sName, (int64_t)m_nTriggerCount);
		}

	}

	void CStateSignalSlot::increaseHandledCount()
	{
		m_nHandledCount++;
		if (m_pSignalInformationGroup.get() != nullptr) {
			m_pSignalInformationGroup->setIntParameterValueByName("handled_" + m_sName, (int64_t)m_nHandledCount);
		}

	}

	void CStateSignalSlot::increaseFailedCount()
	{
		m_nFailedCount++;
		if (m_pSignalInformationGroup.get() != nullptr) {
			m_pSignalInformationGroup->setIntParameterValueByName("failed_" + m_sName, (int64_t)m_nFailedCount);
		}

	}

	void CStateSignalSlot::increaseTimeoutCount()
	{
		m_nTimedOutCount++;
		if (m_pSignalInformationGroup.get() != nullptr) {
			m_pSignalInformationGroup->setIntParameterValueByName("timeout_" + m_sName, (int64_t)m_nTimedOutCount);
		}

	}


	bool CStateSignalSlot::queueIsFullNoMutex()
	{
		return (m_Queue.size() >= m_nSignalQueueSize);
	}

	bool CStateSignalSlot::queueIsFull()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return queueIsFullNoMutex();
	}

	uint32_t CStateSignalSlot::getAvailableSignalQueueEntriesInternal()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		if (m_Queue.size() < m_nSignalQueueSize)
			return m_nSignalQueueSize - (uint32_t)m_Queue.size();
		else
			return 0;
	}

	uint32_t CStateSignalSlot::getTotalSignalQueueSizeInternal()
	{
		return m_nSignalQueueSize;
	}


	bool CStateSignalSlot::eraseMessage(const std::string& sUUID)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		if (auto iQueueIterator = m_QueueMap.find(sUUID); iQueueIterator != m_QueueMap.end()) {
			m_Queue.erase(iQueueIterator->second);
			m_QueueMap.erase(iQueueIterator);
		}
		m_InProcess.erase(sUUID);
		m_Handled.erase(sUUID);
		m_Failed.erase(sUUID);
		m_TimedOut.erase(sUUID);
		m_Cleared.erase(sUUID);

		return m_MessageMap.erase(sUUID) > 0;

	}

	size_t CStateSignalSlot::clearQueueInternal(std::vector<std::string>& clearedUUIDs, uint64_t nTimeStamp)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		size_t nCount = 0;

		while (m_Queue.size() > 0) {
			auto pMessage = m_Queue.front();
			std::string sUUID = pMessage->getUUID();
			m_Queue.pop_front();
			m_QueueMap.erase(sUUID);

			pMessage->setPhase(AMC::eAMCSignalPhase::Cleared, nTimeStamp);
			m_Cleared.insert(sUUID);

			m_MessageMap.erase(sUUID);
			clearedUUIDs.push_back(sUUID);

			nCount++;
		}

		return nCount;

	}

	PStateSignalMessage CStateSignalSlot::addNewInQueueSignalInternal(const std::string& sSignalUUID, const std::string& sParameterData, uint32_t nReactionTimeoutInMS, uint64_t nTimeStamp)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sSignalUUID);
		if (queueIsFullNoMutex()) {
			// Queue is full, cannot add new signal
			return false;
		}

		auto iIterator = m_MessageMap.find(sNormalizedUUID);
		if (iIterator != m_MessageMap.end()) {
			// Signal already exists, cannot add again
			return false;
		}

		auto pMessage = std::make_shared<CStateSignalMessage>(sNormalizedUUID, nReactionTimeoutInMS, eAMCSignalPhase::InQueue, nTimeStamp);
		m_Queue.push_back(pMessage);
		m_QueueMap.insert(std::make_pair(sNormalizedUUID, std::prev(m_Queue.end())));
		m_MessageMap.insert(std::make_pair (sNormalizedUUID, pMessage));

		increaseTriggerCount();

		pMessage->setParameterDataJSON(sParameterData);

		return pMessage;
	}

	bool CStateSignalSlot::changeSignalPhaseToInProcessInternal(const std::string& sSignalUUID, uint64_t nTimeStamp)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		auto pMessage = getMessageByUUIDNoMutex(sSignalUUID);
		AMC::eAMCSignalPhase messagePhase = pMessage->getPhase();
		std::string sUUID = pMessage->getUUID();

		if (messagePhase == eAMCSignalPhase::InQueue) {

			pMessage->setPhase(eAMCSignalPhase::InProcess, nTimeStamp);
			m_InProcess.insert(sUUID);

			auto iQueueIter = m_QueueMap.find(sUUID);
			if (iQueueIter == m_QueueMap.end()) 
				return false;
			m_Queue.erase(iQueueIter->second);
			m_QueueMap.erase(iQueueIter);
			
			return true;
		}

		return false;		

	}

	bool CStateSignalSlot::changeSignalPhaseToHandledInternal(const std::string & sSignalUUID, const std::string& sResultData, uint64_t nTimeStamp)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		auto pMessage = getMessageByUUIDNoMutex(sSignalUUID);
		AMC::eAMCSignalPhase messagePhase = pMessage->getPhase();
		std::string sUUID = pMessage->getUUID();

		if (messagePhase == eAMCSignalPhase::InQueue) {

			auto iQueueIter = m_QueueMap.find(sUUID);
			if (iQueueIter == m_QueueMap.end())
				return false;
			m_Queue.erase(iQueueIter->second);
			m_QueueMap.erase(iQueueIter);

			pMessage->setResultDataJSON(sResultData);
			pMessage->setPhase(eAMCSignalPhase::Handled, nTimeStamp);
			m_Handled.insert(sUUID);

			increaseHandledCount();

			return true;
		}

		if (messagePhase == eAMCSignalPhase::InProcess) {
			pMessage->setResultDataJSON(sResultData);
			pMessage->setPhase(eAMCSignalPhase::Handled, nTimeStamp);
			m_Handled.insert(sUUID);
			m_InProcess.erase(sUUID);

			increaseHandledCount();

			return true;
		}

		return false;

	}

	bool CStateSignalSlot::changeSignalPhaseToInFailedInternal(const std::string& sSignalUUID, const std::string& sResultData, const std::string& sErrorMessage, uint64_t nTimeStamp)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		auto pMessage = getMessageByUUIDNoMutex(sSignalUUID);
		AMC::eAMCSignalPhase messagePhase = pMessage->getPhase();
		std::string sUUID = pMessage->getUUID();

		if (messagePhase == eAMCSignalPhase::InQueue) {
			auto iQueueIter = m_QueueMap.find(sUUID);
			if (iQueueIter == m_QueueMap.end())
				return false;
			m_Queue.erase(iQueueIter->second);
			m_QueueMap.erase(iQueueIter);
			
			pMessage->setResultDataJSON(sResultData);
			pMessage->setPhase(eAMCSignalPhase::Failed, nTimeStamp);
			pMessage->setErrorMessage(sErrorMessage);
			m_Failed.insert(sUUID);

			increaseFailedCount();

			return true;
		}

		if (messagePhase == eAMCSignalPhase::InProcess) {
			pMessage->setPhase(eAMCSignalPhase::Failed, nTimeStamp);
			pMessage->setResultDataJSON(sResultData);
			pMessage->setErrorMessage(sErrorMessage);
			m_Failed.insert(sUUID);
			m_InProcess.erase(sUUID);

			increaseFailedCount();

			return true;
		}

		return false;

	}

	AMC::eAMCSignalPhase CStateSignalSlot::getSignalPhaseInternal(const std::string& sSignalUUID)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		auto pMessage = getMessageByUUIDNoMutex(sSignalUUID);
		return pMessage->getPhase();
	}


	
	uint32_t CStateSignalSlot::getDefaultReactionTimeoutInternal()
	{
		return m_nSignalDefaultReactionTimeOutInMS;
	}

	uint32_t CStateSignalSlot::getReactionTimeoutInternal(const std::string& sSignalUUID)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		auto pMessage = getMessageByUUIDNoMutex(sSignalUUID);

		return pMessage->getReactionTimeoutInMS();

	}

	void CStateSignalSlot::checkForReactionTimeoutsNoMutex(uint64_t nGlobalTimestamp)
	{
		auto it = m_Queue.begin();
		while (it != m_Queue.end()) {
			auto pMessage = (*it);
			if (pMessage->hadReactionTimeout(nGlobalTimestamp)) {
				// Signal has timed out
				std::string sUUID = pMessage->getUUID();
				pMessage->setPhase(eAMCSignalPhase::TimedOut, nGlobalTimestamp);
				m_TimedOut.insert(sUUID);

				it = m_Queue.erase(it);
				m_QueueMap.erase(sUUID);
				m_MessageMap.erase(sUUID);

				increaseTimeoutCount();

			}
			else {
				++it;
			}
		}


	}

	void CStateSignalSlot::checkForReactionTimeouts(uint64_t nGlobalTimestamp)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		checkForReactionTimeoutsNoMutex(nGlobalTimestamp);
	}


	std::string CStateSignalSlot::peekMessageFromQueueInternal(bool bCheckForReactionTimeout, uint64_t nGlobalTimestamp)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		if (bCheckForReactionTimeout) {
			checkForReactionTimeoutsNoMutex(nGlobalTimestamp);
		}

		if (m_Queue.empty())
			return "";

		return m_Queue.front()->getUUID();
	}


	std::string CStateSignalSlot::getResultDataJSONInternal(const std::string& sSignalUUID)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		auto pMessage = getMessageByUUIDNoMutex(sSignalUUID);

		return pMessage->getResultDataJSON();

	}

	std::string CStateSignalSlot::getParameterDataJSONInternal(const std::string& sSignalUUID)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		auto pMessage = getMessageByUUIDNoMutex(sSignalUUID);

		return pMessage->getParameterDataJSON();

	}





	void CStateSignalSlot::populateParameterGroup(CParameterGroup* pParameterGroup)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		LibMCAssertNotNull(pParameterGroup);

		for (auto & Definition : m_ParameterDefinitions) {
			std::string sType = Definition.getType();
			std::string sName = Definition.getName();
			pParameterGroup->addNewTypedParameter(sName, sType, "", "", "");			
		}

	}

	void CStateSignalSlot::populateResultGroup(CParameterGroup* pResultGroup)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		LibMCAssertNotNull(pResultGroup);

		for (auto & Definition : m_ResultDefinitions) {
			std::string sType = Definition.getType();
			std::string sName = Definition.getName();
			pResultGroup->addNewTypedParameter(sName, sType, "", "", "");
		}
	}


}


