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


#ifndef __AMC_STATESIGNALHANDLER
#define __AMC_STATESIGNALHANDLER

#include <memory>
#include <string>
#include <map>
#include <list>
#include <mutex>
#include <unordered_map>

#include "amc_statesignalparameter.hpp"
#include "amc_statesignaltypes.hpp"
#include "amc_parametergroup.hpp"


namespace AMC {

	class CStateSignalHandler;
	typedef std::shared_ptr<CStateSignalHandler> PStateSignalHandler;

	// Do not include StateSignal.hpp anywhere for threadsafety!
	class CStateSignalSlot;
	typedef std::shared_ptr<CStateSignalSlot> PStateSignalSlot;


	class CStateSignalHandler {
	private:
		
		std::map<std::pair <std::string, std::string>, PStateSignalSlot> m_SignalMap;
		std::unordered_map<std::string, PStateSignalSlot> m_SignalUUIDLookupMap;
		std::mutex m_SignalMapMutex;
		std::mutex m_SignalUUIDMapMutex;


	public:

		CStateSignalHandler();
		virtual ~CStateSignalHandler();

		void addSignalDefinition(const std::string & sInstanceName, const std::string & sSignalName, const std::list<CStateSignalParameter> & Parameters, const std::list<CStateSignalParameter> & Results, uint32_t nSignalReactionTimeOutInMS, uint32_t nSignalQueueSize, PParameterGroup pSignalInformationGroup);

		void clearUnhandledSignals(const std::string& sInstanceName, uint64_t nTimestamp);

		void clearUnhandledSignalsOfType(const std::string& sInstanceName, const std::string& sSignalTypeName, uint64_t nTimestamp);

		bool finalizeSignal(const std::string& sUUID);

		bool canTrigger(const std::string& sInstanceName, const std::string& sSignalName);

		bool hasSignalDefinition(const std::string& sInstanceName, const std::string& sSignalName);

		bool findSignalPropertiesByUUID(const std::string& sSignalUUID, std::string & sInstanceName, std::string& sSignalName, std::string& sParameterData);

		AMC::eAMCSignalPhase getSignalPhase (const std::string& sSignalUUID);

		std::string peekSignalMessageFromQueue(const std::string& sInstanceName, const std::string& sSignalName, bool bCheckForReactionTimeout, uint64_t nGlobalTimestamp);

		void checkForReactionTimeouts(uint64_t nGlobalTimestamp);

		bool addNewInQueueSignal(const std::string& sInstanceName, const std::string& sSignalName, const std::string& sSignalUUID, const std::string& sParameterData, uint32_t nResponseTimeOutInMS, uint64_t nTimestamp);

		void changeSignalPhaseToHandled(const std::string& sSignalUUID, const std::string& sResultData, uint64_t nTimestamp);

		void changeSignalPhaseToInProcess(const std::string& sSignalUUID, uint64_t nTimestamp);
		
		void changeSignalPhaseToFailed(const std::string& sSignalUUID, const std::string& sResultData, const std::string & sErrorMessage, uint64_t nTimestamp);

		uint32_t getAvailableSignalQueueEntryCount(const std::string& sInstanceName, const std::string& sSignalName);

		uint32_t getTotalSignalQueueSize(const std::string& sInstanceName, const std::string& sSignalName);

		uint32_t getDefaultReactionTimeout(const std::string& sInstanceName, const std::string& sSignalName);

		uint32_t getReactionTimeout(const std::string& sSignalUUID);

		std::string getResultDataJSON(const std::string& sSignalUUID);

		void populateParameterGroup(const std::string& sInstanceName, const std::string& sSignalName, CParameterGroup * pParameterGroup);

		void populateResultGroup(const std::string& sInstanceName, const std::string& sSignalName, CParameterGroup* pResultGroup);

	};

	
}


#endif //__AMC_STATESIGNALHANDLER

