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


#ifndef __AMC_OPERATIONSEQUENCEHANDLER
#define __AMC_OPERATIONSEQUENCEHANDLER

#include "amc_operationtypes.hpp"
#include "amc_operationsequence.hpp"

#include "common_chrono.hpp"
#include "libmcdata_dynamic.hpp"

namespace AMC {

	amcDeclareDependingClass(CStateMachineData, PStateMachineData);
	amcDeclareDependingClass(CLogger, PLogger);
	amcDeclareDependingClass(CParameterGroup, PParameterGroup);

	class COperationSequenceRegistry;
	class COperationSequenceExecution;
	class CDispatchedOperation;
	class COperationSequenceHandler;

	typedef std::shared_ptr<COperationSequenceRegistry> POperationSequenceRegistry;
	typedef std::shared_ptr<COperationSequenceExecution> POperationSequenceExecution;
	typedef std::shared_ptr<CDispatchedOperation> PDispatchedOperation;
	typedef std::shared_ptr<COperationSequenceHandler> POperationSequenceHandler;

	typedef struct _sOperationResolvedValue {
		POperationParameterDefinition m_pDefinition;
		bool m_bExplicit = false;
		std::string m_sRawValue;
		std::string m_sStringValue;
		double m_dNumericValue = 0.0;
		bool m_bBoolValue = false;
	} sOperationResolvedValue;

	typedef std::map<std::string, sOperationResolvedValue> OperationResolvedValueMap;

	// Snapshot of the operation that is being executed. All values are resolved, defaulted and clamped at dispatch.
	class CDispatchedOperation {
	private:
		COperationSequenceRegistry* m_pRegistry;
		std::weak_ptr<COperationSequenceExecution> m_pExecution;

		std::string m_sUUID;
		std::string m_sTypeName;
		uint32_t m_nIndex;
		std::string m_sExecutionName;
		std::string m_sSequenceName;
		uint32_t m_nRevision;
		std::string m_sComment;
		std::string m_sTargetState;
		std::string m_sReturnState;
		std::string m_sBindingGroup;

		std::vector<std::string> m_ValueOrder;
		OperationResolvedValueMap m_Values;
		std::map<std::string, std::vector<OperationResolvedValueMap>> m_Items;

		const sOperationResolvedValue& findValue(const std::string& sName) const;
		const sOperationResolvedValue& findItemValue(const std::string& sListName, uint32_t nIndex, const std::string& sName) const;

	public:

		CDispatchedOperation(COperationSequenceRegistry* pRegistry, POperationSequenceExecution pExecution, POperation pOperation, POperationTypeDefinition pType, uint32_t nIndex,
			const std::string& sExecutionName, const std::string& sSequenceName, uint32_t nRevision, const std::string& sReturnState, const std::string& sBindingGroup);

		virtual ~CDispatchedOperation();

		std::string getUUID() const;
		std::string getTypeName() const;
		uint32_t getIndex() const;
		std::string getExecutionName() const;
		std::string getSequenceName() const;
		uint32_t getRevision() const;
		std::string getComment() const;
		std::string getTargetState() const;
		std::string getReturnState() const;
		std::string getBindingGroup() const;

		bool hasExplicitValue(const std::string& sName) const;

		std::string getStringValue(const std::string& sName) const;
		int64_t getIntegerValue(const std::string& sName) const;
		double getDoubleValue(const std::string& sName) const;
		bool getBoolValue(const std::string& sName) const;

		double reevaluateDoubleValue(const std::string& sName) const;
		int64_t reevaluateIntegerValue(const std::string& sName) const;

		uint32_t getItemCount(const std::string& sListName) const;
		std::string getItemStringValue(const std::string& sListName, uint32_t nIndex, const std::string& sName) const;
		int64_t getItemIntegerValue(const std::string& sListName, uint32_t nIndex, const std::string& sName) const;
		double getItemDoubleValue(const std::string& sListName, uint32_t nIndex, const std::string& sName) const;
		bool getItemBoolValue(const std::string& sListName, uint32_t nIndex, const std::string& sName) const;

		const std::vector<std::string>& getValueOrder() const;
		const OperationResolvedValueMap& getValues() const;

		void markFailed(const std::string& sMessage);

		static std::string valueToString(const sOperationResolvedValue& value);
		static int64_t valueToInteger(const sOperationResolvedValue& value, const std::string& sName);
		static double valueToDouble(const sOperationResolvedValue& value, const std::string& sName);
		static bool valueToBool(const sOperationResolvedValue& value, const std::string& sName);
	};

	// A named cursor over a pinned sequence revision.
	class COperationSequenceExecution : public std::enable_shared_from_this<COperationSequenceExecution> {
	private:
		COperationSequenceRegistry* m_pRegistry;

		std::string m_sName;
		POperationSequence m_pSequence;
		std::string m_sBindingGroup;
		std::string m_sReturnState;

		eOperationSequenceExecutionStatus m_Status;
		uint32_t m_nNextOperationIndex;
		uint32_t m_nIterationCount;
		std::string m_sCurrentOperationUUID;
		uint32_t m_nCurrentOperationIndex;

		mutable std::mutex m_Mutex;

	public:

		COperationSequenceExecution(COperationSequenceRegistry* pRegistry, const std::string& sName, POperationSequence pSequence, const std::string& sBindingGroup, const std::string& sReturnState);

		virtual ~COperationSequenceExecution();

		std::string getName() const;
		POperationSequence getSequence() const;

		std::string getBindingGroup() const;
		void setBindingGroup(const std::string& sBindingGroup);

		std::string getReturnState() const;

		eOperationSequenceExecutionStatus getStatus() const;
		uint32_t getNextOperationIndex() const;
		uint32_t getIterationCount() const;

		// UUID and 0-based index of the operation that was dispatched last. Empty if none.
		std::string getCurrentOperationUUID() const;
		uint32_t getCurrentOperationIndex() const;

		PDispatchedOperation dispatchNextOperation();
		void restart();
		void abort(const std::string& sReason);
		void markFailed(const std::string& sMessage);

		static std::string statusToString(eOperationSequenceExecutionStatus status);
	};

	// Operation type and sequence registry of one state machine instance.
	class COperationSequenceRegistry {
	private:
		std::string m_sInstanceName;
		PStateMachineData m_pStateMachineData;
		PLogger m_pLogger;
		LibMCData::POperationSequenceStore m_pStore;
		AMCCommon::PChrono m_pGlobalChrono;

		mutable std::mutex m_Mutex;

		std::map<std::string, std::set<std::string>> m_StateGraph;

		std::vector<POperationTypeDefinition> m_OperationTypes;
		std::map<std::string, POperationTypeDefinition> m_OperationTypeMap;

		std::map<std::string, uint32_t> m_LatestRevisionNumbers;
		std::map<std::string, POperationSequence> m_LatestRevisions;
		std::map<std::string, std::map<uint32_t, POperationSequence>> m_RevisionCache;
		std::map<std::string, std::string> m_FactoryDefaults;

		std::string m_sStatusGroupName;
		std::string m_sStatusValuePrefix;

		std::map<std::string, POperationSequenceExecution> m_Executions;
		PDispatchedOperation m_pDispatchedOperation;

		POperationSequence publishLocked(POperationSequence pWorkingCopy, const std::string& sUserUUID, eOperationSequenceOrigin origin);
		POperationSequence restoreStoredRevision(LibMCData::POperationSequenceRevision pRevision);
		POperationSequence findCachedRevisionLocked(const std::string& sSequenceName, uint32_t nRevision);
		void cacheRevisionLocked(POperationSequence pSequence);

		void validateValue(COperationSequenceValidationResult* pResult, const std::string& sOperationUUID, const std::string& sLabel, POperationParameterDefinition pDefinition, const std::string& sRawValue, const std::string& sBindingGroup, bool bBindingGroupExists);
		void validateValueList(COperationSequenceValidationResult* pResult, const std::string& sOperationUUID, const std::string& sLabelPrefix, COperationParameterContainer* pContainer, const COperationValueList& values, const std::string& sBindingGroup, bool bBindingGroupExists);

		void logIssues(const std::string& sSequenceName, COperationSequenceValidationResult* pResult);

		void setStatusValue(CParameterGroup* pGroup, const std::string& sName, const std::string& sStringValue, double dNumericValue, bool bIsNumeric);

	public:

		COperationSequenceRegistry(const std::string& sInstanceName, PStateMachineData pStateMachineData, PLogger pLogger, LibMCData::POperationSequenceStore pStore, AMCCommon::PChrono pGlobalChrono);

		virtual ~COperationSequenceRegistry();

		std::string getInstanceName() const;

		PStateMachineData getStateMachineData() const;

		void logMessage(const std::string& sMessage);
		void logWarning(const std::string& sMessage);

		// State graph of the state machine, used to check target states and transitions.
		void registerState(const std::string& sStateName, const std::set<std::string>& outStates);
		bool hasState(const std::string& sStateName) const;
		bool hasTransition(const std::string& sFromState, const std::string& sToState) const;

		POperationTypeDefinition registerOperationType(const std::string& sTypeName, const std::string& sTargetState);
		bool hasOperationType(const std::string& sTypeName) const;
		POperationTypeDefinition findOperationType(const std::string& sTypeName, bool bFailIfNotExisting) const;
		std::vector<POperationTypeDefinition> listOperationTypes() const;

		POperationSequence createSequence(const std::string& sSequenceName);
		POperationSequence parseSequence(const std::string& sXMLString, eOperationSequenceOrigin origin);
		POperationSequenceValidationResult validateSequence(POperationSequence pSequence);

		POperationSequence publishSequence(POperationSequence pWorkingCopy, const std::string& sUserUUID, bool bOverwriteNewerRevision);
		POperationSequence ensureSequence(const std::string& sSequenceName, const std::string& sResourceName, const std::string& sResourceXML);

		bool hasSequence(const std::string& sSequenceName) const;
		POperationSequence getSequence(const std::string& sSequenceName) const;
		POperationSequence getSequenceRevision(const std::string& sSequenceName, uint32_t nRevision);
		std::vector<POperationSequence> listSequences() const;
		std::vector<POperationSequence> listSequenceRevisions(const std::string& sSequenceName);
		POperationSequence findSequenceRevisionByUUID(const std::string& sRevisionUUID, bool bFailIfNotExisting);
		uint32_t getLatestRevisionNumber(const std::string& sSequenceName) const;

		bool hasFactoryDefault(const std::string& sSequenceName) const;
		POperationSequence createFactoryDefault(const std::string& sSequenceName);

		// Loads the latest revision of every stored sequence. Parsing needs no registered types.
		void loadFromStore();

		void validateDispatcherTransitions(const std::string& sDispatcherState);

		void setStatusParameterGroup(const std::string& sGroupName, const std::string& sValuePrefix);

		POperationSequenceExecution startExecution(const std::string& sExecutionName, const std::string& sSequenceName, const std::string& sBindingGroup, const std::string& sReturnState);
		POperationSequenceExecution findExecution(const std::string& sExecutionName, bool bFailIfNotExisting) const;

		bool hasDispatchedOperation() const;
		PDispatchedOperation getDispatchedOperation() const;
		void setDispatchedOperation(PDispatchedOperation pOperation);

		bool bindingGroupExists(const std::string& sGroupName) const;
		std::vector<std::pair<std::string, std::string>> getBindingGroupParameters(const std::string& sGroupName) const;

		// Resolves a raw value of a parameter against a binding group. Throws OPERATIONPARAMETEREVALUATIONFAILED.
		sOperationResolvedValue resolveValue(POperationParameterDefinition pDefinition, const std::string& sRawValue, bool bExplicit, const std::string& sBindingGroup);
		void resolveValueList(COperationParameterContainer* pContainer, const COperationValueList& values, const std::string& sBindingGroup, OperationResolvedValueMap& resolvedValues, std::vector<std::string>* pValueOrder);

		void mirrorExecutionStatus(COperationSequenceExecution* pExecution, CDispatchedOperation* pOperation);
	};

	// System wide owner of the registries of all state machine instances.
	class COperationSequenceHandler {
	private:
		PStateMachineData m_pStateMachineData;
		PLogger m_pLogger;
		LibMCData::PDataModel m_pDataModel;
		AMCCommon::PChrono m_pGlobalChrono;

		mutable std::mutex m_Mutex;
		std::map<std::string, POperationSequenceRegistry> m_Registries;

	public:

		COperationSequenceHandler(PStateMachineData pStateMachineData, PLogger pLogger, LibMCData::PDataModel pDataModel, AMCCommon::PChrono pGlobalChrono);

		virtual ~COperationSequenceHandler();

		POperationSequenceRegistry createRegistry(const std::string& sInstanceName);

		POperationSequenceRegistry findRegistry(const std::string& sInstanceName, bool bFailIfNotExisting) const;

		std::vector<POperationSequenceRegistry> getRegistries() const;
	};

}

#endif //__AMC_OPERATIONSEQUENCEHANDLER
