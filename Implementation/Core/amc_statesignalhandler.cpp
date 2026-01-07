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


#include "amc_statesignalhandler.hpp"

#include "amc_statesignal.hpp"

#include "libmc_exceptiontypes.hpp"

#include "common_utils.hpp"

namespace AMC {
	
	
	CStateSignalHandler::CStateSignalHandler()
	{
	}
	

	CStateSignalHandler::~CStateSignalHandler()
	{
	}
	
	void CStateSignalHandler::addSignalDefinition(const std::string& sInstanceName, const std::string& sSignalName, const std::list<CStateSignalParameter>& Parameters, const std::list<CStateSignalParameter>& Results, uint32_t nSignalReactionTimeOutInMS, uint32_t nSignalQueueSize, PParameterGroup pSignalInformationGroup)
	{
		std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);

		if (sSignalName.empty ())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDSIGNALNAME, sSignalName);

		auto iter = m_SignalMap.find(std::make_pair (sInstanceName, sSignalName));
		if (iter != m_SignalMap.end())
			throw ELibMCCustomException(LIBMC_ERROR_DUPLICATESIGNAL, sInstanceName + "/" + sSignalName);

		auto pSignal = std::make_shared<CStateSignalSlot>(sInstanceName, sSignalName, Parameters, Results, nSignalReactionTimeOutInMS, nSignalQueueSize, pSignalInformationGroup);
		m_SignalMap.insert(std::make_pair(std::make_pair (sInstanceName, sSignalName), pSignal));
	}

	bool CStateSignalHandler::addNewInQueueSignal(const std::string& sInstanceName, const std::string& sSignalName, const std::string& sSignalUUID, const std::string& sParameterData, uint32_t nResponseTimeOutInMS, uint64_t nTimestamp)
	{		

		AMC::PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);
			auto iter = m_SignalMap.find(std::make_pair(sInstanceName, sSignalName));
			if (iter == m_SignalMap.end())
				throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "addNewInQueueSignal: " + sInstanceName + "/" + sSignalName);

			pSlot = iter->second;
		}

		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sSignalUUID);
		
		std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);

		auto iUUIDIter = m_SignalUUIDLookupMap.find(sNormalizedUUID);
		if (iUUIDIter != m_SignalUUIDLookupMap.end())
			throw ELibMCCustomException(LIBMC_ERROR_SIGNALALREADYTRIGGERED, sNormalizedUUID);


		if (pSlot->addNewInQueueSignalInternal (sNormalizedUUID, sParameterData, nResponseTimeOutInMS, nTimestamp)) {
			m_SignalUUIDLookupMap.insert(std::make_pair(sNormalizedUUID, pSlot));
			return true;
		}

		return false;
	}


	bool CStateSignalHandler::hasSignalDefinition(const std::string& sInstanceName, const std::string& sSignalName)
	{
		std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);
		auto iter = m_SignalMap.find(std::make_pair(sInstanceName, sSignalName));
		return (iter != m_SignalMap.end());
	}



	void CStateSignalHandler::clearUnhandledSignals(const std::string& sInstanceName, uint64_t nTimestamp)
	{
		std::vector<AMC::PStateSignalSlot> slotList;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);
			slotList.reserve(m_SignalMap.size());

			for (auto it = m_SignalMap.begin(); it != m_SignalMap.end(); it++) {
				// Check if the first element of the key matches
				if (it->first.first == sInstanceName) {
					slotList.push_back(it->second);					
				}
			}
		}

		std::vector<std::string> clearedUUIDs;
		for (auto pSlot : slotList) {
			pSlot->clearQueueInternal(clearedUUIDs, nTimestamp);
		}

		if (!clearedUUIDs.empty ())
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);

			for (auto& sUUID : clearedUUIDs)
				m_SignalUUIDLookupMap.erase(sUUID);
		}

	}

	void CStateSignalHandler::clearUnhandledSignalsOfType(const std::string& sInstanceName, const std::string& sSignalTypeName, uint64_t nTimestamp)
	{

		std::vector<AMC::PStateSignalSlot> slotList;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);
			for (auto it = m_SignalMap.begin(); it != m_SignalMap.end(); it++) {
				// Check if the first element of the key matches
				if (it->first.first == sInstanceName) {
					if (it->second->getNameInternal() == sSignalTypeName) {
						slotList.push_back(it->second);						
					}
				}
			}
		}

		std::vector<std::string> clearedUUIDs;

		for (auto pSlot : slotList) {
			pSlot->clearQueueInternal(clearedUUIDs, nTimestamp);
		}

		if (!clearedUUIDs.empty())
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);

			for (auto& sUUID : clearedUUIDs)
				m_SignalUUIDLookupMap.erase(sUUID);
		}

	}


	bool CStateSignalHandler::finalizeSignal(const std::string& sUUID) {

		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);

		PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);
			auto iIter = m_SignalUUIDLookupMap.find(sNormalizedUUID);
			if (iIter == m_SignalUUIDLookupMap.end())
				return false;

			pSlot = iIter->second;
			m_SignalUUIDLookupMap.erase(iIter);
		}

		return pSlot->eraseMessage(sNormalizedUUID);
	}

	bool CStateSignalHandler::canTrigger(const std::string& sInstanceName, const std::string& sSignalName)
	{
		AMC::PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);
			auto iter = m_SignalMap.find(std::make_pair(sInstanceName, sSignalName));
			if (iter == m_SignalMap.end())
				throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "canTrigger: " + sInstanceName + "/" + sSignalName);

			pSlot = iter->second;
		}

		return !pSlot->queueIsFull();
	}


	void CStateSignalHandler::changeSignalPhaseToHandled(const std::string& sSignalUUID, const std::string& sResultData, uint64_t nTimestamp)
	{
		std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);

		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sSignalUUID);

		auto iter = m_SignalUUIDLookupMap.find(sNormalizedUUID);
		if (iter == m_SignalUUIDLookupMap.end())
			throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "signal not found while changing phase to handled (" + sNormalizedUUID + ")");

		iter->second->changeSignalPhaseToHandledInternal(sNormalizedUUID, sResultData, nTimestamp);
	}

	void CStateSignalHandler::changeSignalPhaseToInProcess(const std::string& sSignalUUID, uint64_t nTimestamp)
	{
		std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);

		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sSignalUUID);

		auto iter = m_SignalUUIDLookupMap.find(sNormalizedUUID);
		if (iter == m_SignalUUIDLookupMap.end())
			throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "signal not found while changing phase to inprocess (" + sNormalizedUUID + ")");

		iter->second->changeSignalPhaseToInProcessInternal (sNormalizedUUID, nTimestamp);
	}

	void CStateSignalHandler::changeSignalPhaseToFailed(const std::string& sSignalUUID, const std::string& sResultData, const std::string& sErrorMessage, uint64_t nTimestamp)
	{
		std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);

		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sSignalUUID);

		auto iter = m_SignalUUIDLookupMap.find(sNormalizedUUID);
		if (iter == m_SignalUUIDLookupMap.end())
			throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "signal not found while changing phase to failed (" + sNormalizedUUID + ")");

		iter->second->changeSignalPhaseToInFailedInternal(sNormalizedUUID, sResultData, sErrorMessage, nTimestamp);
	}

	AMC::eAMCSignalPhase CStateSignalHandler::getSignalPhase(const std::string& sSignalUUID)
	{
		std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);

		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sSignalUUID);

		auto iter = m_SignalUUIDLookupMap.find(sNormalizedUUID);
		if (iter == m_SignalUUIDLookupMap.end())
			throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "signal not found while getting signal phase (" + sNormalizedUUID + ")");

		return iter->second->getSignalPhaseInternal (sNormalizedUUID);
	}

	std::string CStateSignalHandler::peekSignalMessageFromQueue(const std::string& sInstanceName, const std::string& sSignalName, bool bCheckForTimeouts, uint64_t nGlobalTimestamp)
	{
		AMC::PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);

			auto iter = m_SignalMap.find(std::make_pair(sInstanceName, sSignalName));
			if (iter == m_SignalMap.end())
				throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "peekSignalMessageFromQueue: " + sInstanceName + "/" + sSignalName);

			pSlot = iter->second;
		}



		return pSlot->peekMessageFromQueueInternal(bCheckForTimeouts, nGlobalTimestamp);

	}


	void CStateSignalHandler::checkForReactionTimeouts(uint64_t nGlobalTimestamp)
	{
		std::list<AMC::PStateSignalSlot> slots;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);
			for (auto iIter : m_SignalMap)
				slots.push_back(iIter.second);
		}

		for (auto pSlot : slots) {
			pSlot->checkForReactionTimeouts(nGlobalTimestamp);
		}
	}

	uint32_t CStateSignalHandler::getAvailableSignalQueueEntryCount(const std::string& sInstanceName, const std::string& sSignalName)
	{
		AMC::PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);

			auto iter = m_SignalMap.find(std::make_pair(sInstanceName, sSignalName));
			if (iter == m_SignalMap.end())
				throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "getAvailableSignalQueueEntryCount:" + sInstanceName + "/" + sSignalName);

			pSlot = iter->second;
		}

		return pSlot->getAvailableSignalQueueEntriesInternal ();

	}

	uint32_t CStateSignalHandler::getTotalSignalQueueSize(const std::string& sInstanceName, const std::string& sSignalName)
	{
		AMC::PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);

			auto iter = m_SignalMap.find(std::make_pair(sInstanceName, sSignalName));
			if (iter == m_SignalMap.end())
				throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "getTotalSignalQueueSize: " + sInstanceName + "/" + sSignalName);
			pSlot = iter->second;

		}

		return pSlot->getTotalSignalQueueSizeInternal();

	}

	uint32_t CStateSignalHandler::getDefaultReactionTimeout(const std::string& sInstanceName, const std::string& sSignalName)
	{
		AMC::PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);

			auto iter = m_SignalMap.find(std::make_pair(sInstanceName, sSignalName));
			if (iter == m_SignalMap.end())
				throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "getDefaultReactionTimeout: " + sInstanceName + "/" + sSignalName);

			pSlot = iter->second;
		}

		return pSlot->getDefaultReactionTimeoutInternal();


	}

	uint32_t CStateSignalHandler::getReactionTimeout(const std::string& sSignalUUID)
	{
		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sSignalUUID);

		AMC::PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);

			auto iter = m_SignalUUIDLookupMap.find(sNormalizedUUID);
			if (iter == m_SignalUUIDLookupMap.end())
				throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "signal not found while getting reaction timeout (" + sNormalizedUUID + ")");

			pSlot = iter->second;


			return pSlot->getReactionTimeoutInternal(sNormalizedUUID);

		}

	}

	std::string CStateSignalHandler::getResultDataJSON(const std::string& sSignalUUID)
	{
		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sSignalUUID);

		AMC::PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);

			auto iter = m_SignalUUIDLookupMap.find(sNormalizedUUID);
			if (iter == m_SignalUUIDLookupMap.end())
				throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "signal not found while getting result data JSON (" + sNormalizedUUID + ")");

			pSlot = iter->second;


			return pSlot->getResultDataJSONInternal(sNormalizedUUID);
		}

	}

	bool CStateSignalHandler::findSignalPropertiesByUUID(const std::string& sSignalUUID, std::string& sInstanceName, std::string& sSignalName, std::string& sParameterData)
	{
		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sSignalUUID);

		AMC::PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalUUIDMapMutex);

			auto iter = m_SignalUUIDLookupMap.find(sNormalizedUUID);
			if (iter != m_SignalUUIDLookupMap.end()) {
				pSlot = iter->second;
			}



			if (pSlot.get() != nullptr) {
				sInstanceName = pSlot->getInstanceNameInternal();
				sSignalName = pSlot->getNameInternal();
				sParameterData = pSlot->getParameterDataJSONInternal(sNormalizedUUID);
				return true;
			}

			return false;

		}

	}


	void CStateSignalHandler::populateParameterGroup(const std::string& sInstanceName, const std::string& sSignalName, CParameterGroup* pParameterGroup)
	{
		LibMCAssertNotNull(pParameterGroup);

		AMC::PStateSignalSlot pSlot;
		{

			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);

			auto iter = m_SignalMap.find(std::make_pair(sInstanceName, sSignalName));
			if (iter == m_SignalMap.end())
				throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "populateParameterGroup: " + sInstanceName + "/" + sSignalName);
			pSlot = iter->second;
		}

		pSlot->populateParameterGroup(pParameterGroup);
	}

	void CStateSignalHandler::populateResultGroup(const std::string& sInstanceName, const std::string& sSignalName, CParameterGroup* pResultGroup)
	{

		LibMCAssertNotNull(pResultGroup);
		AMC::PStateSignalSlot pSlot;
		{
			std::lock_guard<std::mutex> lockGuard(m_SignalMapMutex);

			auto iter = m_SignalMap.find(std::make_pair(sInstanceName, sSignalName));
			if (iter == m_SignalMap.end())
				throw ELibMCCustomException(LIBMC_ERROR_SIGNALNOTFOUND, "populateResultGroup: " + sInstanceName + "/" + sSignalName);

			pSlot = iter->second;
		}


		pSlot->populateResultGroup(pResultGroup);
	}

}



