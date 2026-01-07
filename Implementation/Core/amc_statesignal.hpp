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


#ifndef __AMC_STATESIGNAL
#define __AMC_STATESIGNAL

#include "amc_statesignalparameter.hpp"
#include "amc_statesignaltypes.hpp"
#include "amc_parametergroup.hpp"

#include <memory>
#include <string>
#include <map>
#include <unordered_map>
#include <deque>
#include <set>
#include <vector>
#include <list>

#include <queue>
#include <thread>

namespace AMC {

	class CStateSignalSlot;
	typedef std::shared_ptr<CStateSignalSlot> PStateSignalSlot;

	class CStateSignalMessage;
	typedef std::shared_ptr<CStateSignalMessage> PStateSignalMessage;

	class CStateSignalMessage
	{
		private:

			std::string m_sUUID;

			AMC::eAMCSignalPhase m_MessagePhase;

			uint32_t m_nReactionTimeoutInMS;

			std::string m_sResultDataJSON;

			std::string m_sParameterDataJSON;

			std::string m_sErrorMessage;

			uint64_t m_nCreationTimestamp;
			uint32_t m_nMicrosecondsUntilInProcess; 
			uint32_t m_nMicrosecondsUntilHandledOrFailed;
			uint32_t m_nMicrosecondsUntilCleared;

		public:

			CStateSignalMessage(const std::string & sUUID, uint32_t nReactionTimeoutInMS, AMC::eAMCSignalPhase initialPhase, uint64_t nCreationTimestamp);

			virtual ~CStateSignalMessage();

			std::string getUUID() const;

			void setPhase (AMC::eAMCSignalPhase messagePhase, uint64_t nGlobalTimeStamp);

			AMC::eAMCSignalPhase getPhase() const;

			uint32_t getReactionTimeoutInMS() const;

			std::string getResultDataJSON() const;

			std::string getParameterDataJSON() const;

			std::string getErrorMessage() const;

			void setErrorMessage(const std::string & sErrorMessage);

			void setResultDataJSON(const std::string& sResultDataJSON);

			void setParameterDataJSON(const std::string& sParameterDataJSON);

			bool hadReactionTimeout(uint64_t nGlobalTimestamp);

	};


	class CStateSignalSlot {
	private:
		std::string m_sInstanceName;
		std::string m_sName;

		std::vector <CStateSignalParameter> m_ParameterDefinitions;
		std::vector <CStateSignalParameter> m_ResultDefinitions;

		uint32_t m_nSignalDefaultReactionTimeOutInMS;
		uint32_t m_nSignalQueueSize;

		std::map<std::string, PStateSignalMessage> m_MessageMap;

		std::list<PStateSignalMessage> m_Queue;
		std::unordered_map<std::string, std::list<PStateSignalMessage>::iterator> m_QueueMap;

		std::set<std::string> m_InProcess;
		std::set<std::string> m_Handled;
		std::set<std::string> m_Failed;
		std::set<std::string> m_TimedOut;
		std::set<std::string> m_Cleared;
		std::deque<std::string> m_Archived;

		uint64_t m_nTriggerCount;
		uint64_t m_nHandledCount;
		uint64_t m_nFailedCount;
		uint64_t m_nTimedOutCount;
		uint64_t m_nMaxReactionTime;
		uint64_t m_nMaxSuccessTime;

		void increaseTriggerCount();
		void increaseHandledCount();
		void increaseFailedCount();
		void increaseTimeoutCount();

		PParameterGroup m_pSignalInformationGroup;

		std::mutex m_Mutex;

		CStateSignalMessage* getMessageByUUIDNoMutex (const std::string& sSignalUUID);
		bool queueIsFullNoMutex();


		void checkForReactionTimeoutsNoMutex(uint64_t nGlobalTimestamp);

	public:

		CStateSignalSlot(const std::string & sInstanceName, const std::string& sName, const std::vector<CStateSignalParameter>& Parameters, const std::vector<CStateSignalParameter>& Results, uint32_t nSignalDefaultReactionTimeOutInMS, uint32_t nSignalQueueSize, PParameterGroup pSignalInformationGroup);
		virtual ~CStateSignalSlot();

		std::string getNameInternal() const;
		std::string getInstanceNameInternal() const;

		bool queueIsFull();
		size_t clearQueueInternal(std::vector<std::string>& clearedUUIDs, uint64_t nTimeStamp);
		bool eraseMessage(const std::string& sUUID);

		PStateSignalMessage addNewInQueueSignalInternal(const std::string& sSignalUUID, const std::string& sParameterData, uint32_t nReactionTimeoutInMS, uint64_t nTimeStamp);
		bool changeSignalPhaseToHandledInternal(const std::string& sSignalUUID, const std::string& sResultData, uint64_t nTimeStamp);
		bool changeSignalPhaseToInFailedInternal(const std::string& sSignalUUID, const std::string& sResultData, const std::string& sErrorMessage, uint64_t nTimeStamp);
		bool changeSignalPhaseToInProcessInternal(const std::string& sSignalUUID, uint64_t nTimeStamp);
		AMC::eAMCSignalPhase getSignalPhaseInternal(const std::string& sSignalUUID);

		uint32_t getAvailableSignalQueueEntriesInternal ();

		uint32_t getTotalSignalQueueSizeInternal();

		uint32_t getDefaultReactionTimeoutInternal();

		uint32_t getReactionTimeoutInternal(const std::string& sSignalUUID);

		void checkForReactionTimeouts(uint64_t nGlobalTimestamp);

		std::string peekMessageFromQueueInternal(bool bCheckForReactionTimeout, uint64_t nGlobalTimestamp);
		
		std::string getResultDataJSONInternal(const std::string& sSignalUUID);
		std::string getParameterDataJSONInternal(const std::string& sSignalUUID);

		void populateParameterGroup(CParameterGroup* pParameterGroup);
		void populateResultGroup(CParameterGroup* pResultGroup);

	};

	
}


#endif //__AMC_STATESIGNAL

