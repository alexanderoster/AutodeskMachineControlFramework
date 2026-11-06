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

#include "amc_operationsequencehandler.hpp"
#include "amc_operationvalues.hpp"

#include "amc_statemachinedata.hpp"
#include "amc_parameterhandler.hpp"
#include "amc_parametergroup.hpp"
#include "amc_logger.hpp"
#include "amc_ui_formula.hpp"
#include "amc_ui_expression.hpp"

#include "libmc_exceptiontypes.hpp"
#include "common_utils.hpp"

#include <cmath>

namespace AMC {

	static PParameterGroup findBindingGroup(CStateMachineData* pStateMachineData, const std::string& sInstanceName, const std::string& sGroupName)
	{
		if ((pStateMachineData == nullptr) || sGroupName.empty())
			return nullptr;

		auto pParameterHandler = pStateMachineData->getParameterHandler(sInstanceName);
		if (pParameterHandler.get() == nullptr)
			return nullptr;

		return pParameterHandler->findGroup(sGroupName, false);
	}

	static std::string unknownParameterMessage(const std::string& sParameterName, const std::string& sGroupName)
	{
		return "Unknown operation parameter \"$" + sParameterName + "\" (not declared in group \"" + sGroupName + "\")";
	}


	/*************************************************************************************************************************
	 CDispatchedOperation
	**************************************************************************************************************************/

	CDispatchedOperation::CDispatchedOperation(COperationSequenceRegistry* pRegistry, POperationSequenceExecution pExecution, POperation pOperation, POperationTypeDefinition pType, uint32_t nIndex,
		const std::string& sExecutionName, const std::string& sSequenceName, uint32_t nRevision, const std::string& sReturnState, const std::string& sBindingGroup)
		: m_pRegistry(pRegistry),
		m_pExecution(pExecution),
		m_nIndex(nIndex),
		m_sExecutionName(sExecutionName),
		m_sSequenceName(sSequenceName),
		m_nRevision(nRevision),
		m_sReturnState(sReturnState),
		m_sBindingGroup(sBindingGroup)
	{
		LibMCAssertNotNull(pRegistry);
		LibMCAssertNotNull(pOperation.get());
		LibMCAssertNotNull(pType.get());

		m_sUUID = pOperation->getUUID();
		m_sTypeName = pOperation->getTypeName();
		m_sComment = pOperation->getComment();
		m_sTargetState = pType->getTargetState();

		pRegistry->resolveValueList(pType.get(), pOperation->values(), sBindingGroup, m_Values, &m_ValueOrder);

		for (auto& pItemList : pType->getItemLists()) {
			std::string sListName = pItemList->getName();
			std::vector<OperationResolvedValueMap> resolvedItems;

			uint32_t nItemCount = pOperation->getItemCount(sListName);
			for (uint32_t nItemIndex = 0; nItemIndex < nItemCount; nItemIndex++) {
				auto pItem = pOperation->getItem(sListName, nItemIndex);
				OperationResolvedValueMap itemValues;
				pRegistry->resolveValueList(pItemList.get(), pItem->values(), sBindingGroup, itemValues, nullptr);
				resolvedItems.push_back(itemValues);
			}

			m_Items.insert(std::make_pair(sListName, resolvedItems));
		}
	}

	CDispatchedOperation::~CDispatchedOperation()
	{
	}

	const sOperationResolvedValue& CDispatchedOperation::findValue(const std::string& sName) const
	{
		auto iIter = m_Values.find(sName);
		if (iIter == m_Values.end())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONPARAMETERNOTFOUND, m_sTypeName + "." + sName);

		return iIter->second;
	}

	const sOperationResolvedValue& CDispatchedOperation::findItemValue(const std::string& sListName, uint32_t nIndex, const std::string& sName) const
	{
		auto iListIter = m_Items.find(sListName);
		if (iListIter == m_Items.end())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONITEMLISTNOTFOUND, m_sTypeName + "." + sListName);

		if (nIndex >= iListIter->second.size())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONITEMINDEX, m_sTypeName + "." + sListName + "[" + std::to_string(nIndex) + "]");

		auto& itemValues = iListIter->second.at(nIndex);
		auto iIter = itemValues.find(sName);
		if (iIter == itemValues.end())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONPARAMETERNOTFOUND, m_sTypeName + "." + sListName + "." + sName);

		return iIter->second;
	}

	std::string CDispatchedOperation::getUUID() const
	{
		return m_sUUID;
	}

	std::string CDispatchedOperation::getTypeName() const
	{
		return m_sTypeName;
	}

	uint32_t CDispatchedOperation::getIndex() const
	{
		return m_nIndex;
	}

	std::string CDispatchedOperation::getExecutionName() const
	{
		return m_sExecutionName;
	}

	std::string CDispatchedOperation::getSequenceName() const
	{
		return m_sSequenceName;
	}

	uint32_t CDispatchedOperation::getRevision() const
	{
		return m_nRevision;
	}

	std::string CDispatchedOperation::getComment() const
	{
		return m_sComment;
	}

	std::string CDispatchedOperation::getTargetState() const
	{
		return m_sTargetState;
	}

	std::string CDispatchedOperation::getReturnState() const
	{
		return m_sReturnState;
	}

	std::string CDispatchedOperation::getBindingGroup() const
	{
		return m_sBindingGroup;
	}

	bool CDispatchedOperation::hasExplicitValue(const std::string& sName) const
	{
		return findValue(sName).m_bExplicit;
	}

	std::string CDispatchedOperation::valueToString(const sOperationResolvedValue& value)
	{
		return value.m_sStringValue;
	}

	int64_t CDispatchedOperation::valueToInteger(const sOperationResolvedValue& value, const std::string& sName)
	{
		LibMCAssertNotNull(value.m_pDefinition.get());
		if (value.m_pDefinition->isStringLike())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONVALUETYPEMISMATCH, sName + " is a " + COperationParameterDefinition::parameterTypeToString(value.m_pDefinition->getParameterType()) + " parameter");

		return (int64_t)std::llround(value.m_dNumericValue);
	}

	double CDispatchedOperation::valueToDouble(const sOperationResolvedValue& value, const std::string& sName)
	{
		LibMCAssertNotNull(value.m_pDefinition.get());
		if (value.m_pDefinition->isStringLike())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONVALUETYPEMISMATCH, sName + " is a " + COperationParameterDefinition::parameterTypeToString(value.m_pDefinition->getParameterType()) + " parameter");

		return value.m_dNumericValue;
	}

	bool CDispatchedOperation::valueToBool(const sOperationResolvedValue& value, const std::string& sName)
	{
		LibMCAssertNotNull(value.m_pDefinition.get());
		if (!value.m_pDefinition->isStringLike())
			return value.m_bBoolValue;

		std::string sLower = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(value.m_sStringValue));
		if ((sLower == "1") || (sLower == "true"))
			return true;
		if ((sLower == "0") || (sLower == "false") || sLower.empty())
			return false;

		throw ELibMCCustomException(LIBMC_ERROR_OPERATIONVALUETYPEMISMATCH, sName + "=\"" + value.m_sStringValue + "\" is not a bool");
	}

	std::string CDispatchedOperation::getStringValue(const std::string& sName) const
	{
		return valueToString(findValue(sName));
	}

	int64_t CDispatchedOperation::getIntegerValue(const std::string& sName) const
	{
		return valueToInteger(findValue(sName), sName);
	}

	double CDispatchedOperation::getDoubleValue(const std::string& sName) const
	{
		return valueToDouble(findValue(sName), sName);
	}

	bool CDispatchedOperation::getBoolValue(const std::string& sName) const
	{
		return valueToBool(findValue(sName), sName);
	}

	double CDispatchedOperation::reevaluateDoubleValue(const std::string& sName) const
	{
		auto& value = findValue(sName);
		auto resolvedValue = m_pRegistry->resolveValue(value.m_pDefinition, value.m_sRawValue, value.m_bExplicit, m_sBindingGroup);
		return valueToDouble(resolvedValue, sName);
	}

	int64_t CDispatchedOperation::reevaluateIntegerValue(const std::string& sName) const
	{
		auto& value = findValue(sName);
		auto resolvedValue = m_pRegistry->resolveValue(value.m_pDefinition, value.m_sRawValue, value.m_bExplicit, m_sBindingGroup);
		return valueToInteger(resolvedValue, sName);
	}

	uint32_t CDispatchedOperation::getItemCount(const std::string& sListName) const
	{
		auto iListIter = m_Items.find(sListName);
		if (iListIter == m_Items.end())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONITEMLISTNOTFOUND, m_sTypeName + "." + sListName);

		return (uint32_t)iListIter->second.size();
	}

	std::string CDispatchedOperation::getItemStringValue(const std::string& sListName, uint32_t nIndex, const std::string& sName) const
	{
		return valueToString(findItemValue(sListName, nIndex, sName));
	}

	int64_t CDispatchedOperation::getItemIntegerValue(const std::string& sListName, uint32_t nIndex, const std::string& sName) const
	{
		return valueToInteger(findItemValue(sListName, nIndex, sName), sListName + "." + sName);
	}

	double CDispatchedOperation::getItemDoubleValue(const std::string& sListName, uint32_t nIndex, const std::string& sName) const
	{
		return valueToDouble(findItemValue(sListName, nIndex, sName), sListName + "." + sName);
	}

	bool CDispatchedOperation::getItemBoolValue(const std::string& sListName, uint32_t nIndex, const std::string& sName) const
	{
		return valueToBool(findItemValue(sListName, nIndex, sName), sListName + "." + sName);
	}

	const std::vector<std::string>& CDispatchedOperation::getValueOrder() const
	{
		return m_ValueOrder;
	}

	const OperationResolvedValueMap& CDispatchedOperation::getValues() const
	{
		return m_Values;
	}

	void CDispatchedOperation::markFailed(const std::string& sMessage)
	{
		std::string sFullMessage = "[" + m_sSequenceName + " #" + std::to_string(m_nIndex + 1) + "] " + m_sTypeName + " failed: " + sMessage;

		auto pExecution = m_pExecution.lock();
		if (pExecution.get() != nullptr)
			pExecution->markFailed(sFullMessage);
		else
			m_pRegistry->logWarning(sFullMessage);
	}


	/*************************************************************************************************************************
	 COperationSequenceExecution
	**************************************************************************************************************************/

	COperationSequenceExecution::COperationSequenceExecution(COperationSequenceRegistry* pRegistry, const std::string& sName, POperationSequence pSequence, const std::string& sBindingGroup, const std::string& sReturnState)
		: m_pRegistry(pRegistry),
		m_sName(sName),
		m_pSequence(pSequence),
		m_sBindingGroup(sBindingGroup),
		m_sReturnState(sReturnState),
		m_Status(eOperationSequenceExecutionStatus::Running),
		m_nNextOperationIndex(0),
		m_nIterationCount(0),
		m_nCurrentOperationIndex(0)
	{
		LibMCAssertNotNull(pRegistry);
		LibMCAssertNotNull(pSequence.get());
	}

	COperationSequenceExecution::~COperationSequenceExecution()
	{
	}

	std::string COperationSequenceExecution::getName() const
	{
		return m_sName;
	}

	POperationSequence COperationSequenceExecution::getSequence() const
	{
		return m_pSequence;
	}

	std::string COperationSequenceExecution::getBindingGroup() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_sBindingGroup;
	}

	void COperationSequenceExecution::setBindingGroup(const std::string& sBindingGroup)
	{
		if (!m_pRegistry->bindingGroupExists(sBindingGroup))
			throw ELibMCCustomException(LIBMC_ERROR_PARAMETERGROUPNOTFOUND, m_pRegistry->getInstanceName() + "." + sBindingGroup);

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_sBindingGroup = sBindingGroup;
	}

	std::string COperationSequenceExecution::getReturnState() const
	{
		return m_sReturnState;
	}

	eOperationSequenceExecutionStatus COperationSequenceExecution::getStatus() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_Status;
	}

	uint32_t COperationSequenceExecution::getNextOperationIndex() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_nNextOperationIndex;
	}

	uint32_t COperationSequenceExecution::getIterationCount() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_nIterationCount;
	}

	std::string COperationSequenceExecution::getCurrentOperationUUID() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_sCurrentOperationUUID;
	}

	uint32_t COperationSequenceExecution::getCurrentOperationIndex() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_nCurrentOperationIndex;
	}

	PDispatchedOperation COperationSequenceExecution::dispatchNextOperation()
	{
		PDispatchedOperation pDispatchedOperation;
		std::exception_ptr pDispatchError;
		std::string sDispatchError;

		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);

			if (m_Status == eOperationSequenceExecutionStatus::Completed)
				return nullptr;

			if (m_Status != eOperationSequenceExecutionStatus::Running)
				throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCEEXECUTIONNOTRUNNING, m_sName + " is " + statusToString(m_Status));

			uint32_t nOperationCount = m_pSequence->getOperationCount();
			while ((m_nNextOperationIndex < nOperationCount) && (!m_pSequence->getOperation(m_nNextOperationIndex)->isEnabled()))
				m_nNextOperationIndex++;

			if (m_nNextOperationIndex >= nOperationCount) {
				m_Status = eOperationSequenceExecutionStatus::Completed;
			}
			else {
				uint32_t nIndex = m_nNextOperationIndex;
				auto pOperation = m_pSequence->getOperation(nIndex);

				try {
					auto pType = m_pRegistry->findOperationType(pOperation->getTypeName(), true);
					pDispatchedOperation = std::make_shared<CDispatchedOperation>(m_pRegistry, shared_from_this(), pOperation, pType, nIndex,
						m_sName, m_pSequence->getName(), m_pSequence->getRevision(), m_sReturnState, m_sBindingGroup);
				}
				catch (std::exception& E) {
					m_Status = eOperationSequenceExecutionStatus::Failed;
					m_nCurrentOperationIndex = nIndex;
					m_sCurrentOperationUUID = pOperation->getUUID();
					pDispatchError = std::current_exception();
					sDispatchError = "[" + m_pSequence->getName() + " #" + std::to_string(nIndex + 1) + "] " + pOperation->getTypeName() + " could not be dispatched: " + E.what();
				}

				if (m_Status == eOperationSequenceExecutionStatus::Running) {
					m_nNextOperationIndex = nIndex + 1;
					m_nCurrentOperationIndex = nIndex;
					m_sCurrentOperationUUID = pOperation->getUUID();
				}
			}
		}

		if (pDispatchError) {
			m_pRegistry->logWarning(sDispatchError);
			m_pRegistry->mirrorExecutionStatus(this, nullptr);
			std::rethrow_exception(pDispatchError);
		}

		if (pDispatchedOperation.get() == nullptr) {
			m_pRegistry->mirrorExecutionStatus(this, nullptr);
			return nullptr;
		}

		m_pRegistry->setDispatchedOperation(pDispatchedOperation);

		if (!pDispatchedOperation->getComment().empty())
			m_pRegistry->logMessage("[" + m_pSequence->getName() + " #" + std::to_string(pDispatchedOperation->getIndex() + 1) + "] " + pDispatchedOperation->getTypeName() + ": " + pDispatchedOperation->getComment());

		m_pRegistry->mirrorExecutionStatus(this, pDispatchedOperation.get());

		return pDispatchedOperation;
	}

	void COperationSequenceExecution::restart()
	{
		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);
			m_Status = eOperationSequenceExecutionStatus::Running;
			m_nNextOperationIndex = 0;
			m_nIterationCount++;
		}

		m_pRegistry->mirrorExecutionStatus(this, nullptr);
	}

	void COperationSequenceExecution::abort(const std::string& sReason)
	{
		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);
			m_Status = eOperationSequenceExecutionStatus::Aborted;
		}

		m_pRegistry->logMessage("Operation sequence execution " + m_sName + " (" + m_pSequence->getName() + " revision " + std::to_string(m_pSequence->getRevision()) + ") aborted: " + sReason);
		m_pRegistry->mirrorExecutionStatus(this, nullptr);
	}

	void COperationSequenceExecution::markFailed(const std::string& sMessage)
	{
		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);
			m_Status = eOperationSequenceExecutionStatus::Failed;
		}

		m_pRegistry->logWarning(sMessage);
		m_pRegistry->mirrorExecutionStatus(this, nullptr);
	}

	std::string COperationSequenceExecution::statusToString(eOperationSequenceExecutionStatus status)
	{
		switch (status) {
		case eOperationSequenceExecutionStatus::Running: return "running";
		case eOperationSequenceExecutionStatus::Completed: return "completed";
		case eOperationSequenceExecutionStatus::Aborted: return "aborted";
		case eOperationSequenceExecutionStatus::Failed: return "failed";
		default: return "unknown";
		}
	}


	/*************************************************************************************************************************
	 COperationSequenceRegistry
	**************************************************************************************************************************/

	COperationSequenceRegistry::COperationSequenceRegistry(const std::string& sInstanceName, PStateMachineData pStateMachineData, PLogger pLogger, LibMCData::POperationSequenceStore pStore, AMCCommon::PChrono pGlobalChrono)
		: m_sInstanceName(sInstanceName),
		m_pStateMachineData(pStateMachineData),
		m_pLogger(pLogger),
		m_pStore(pStore),
		m_pGlobalChrono(pGlobalChrono)
	{
		LibMCAssertNotNull(pStateMachineData.get());
		LibMCAssertNotNull(pGlobalChrono.get());
	}

	COperationSequenceRegistry::~COperationSequenceRegistry()
	{
	}

	std::string COperationSequenceRegistry::getInstanceName() const
	{
		return m_sInstanceName;
	}

	PStateMachineData COperationSequenceRegistry::getStateMachineData() const
	{
		return m_pStateMachineData;
	}

	void COperationSequenceRegistry::logMessage(const std::string& sMessage)
	{
		if (m_pLogger.get() != nullptr)
			m_pLogger->logMessage(sMessage, m_sInstanceName, eLogLevel::Message);
	}

	void COperationSequenceRegistry::logWarning(const std::string& sMessage)
	{
		if (m_pLogger.get() != nullptr)
			m_pLogger->logMessage(sMessage, m_sInstanceName, eLogLevel::Warning);
	}

	void COperationSequenceRegistry::registerState(const std::string& sStateName, const std::set<std::string>& outStates)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_StateGraph[sStateName] = outStates;
	}

	bool COperationSequenceRegistry::hasState(const std::string& sStateName) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return (m_StateGraph.find(sStateName) != m_StateGraph.end());
	}

	bool COperationSequenceRegistry::hasTransition(const std::string& sFromState, const std::string& sToState) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		auto iIter = m_StateGraph.find(sFromState);
		if (iIter == m_StateGraph.end())
			return false;

		return (iIter->second.find(sToState) != iIter->second.end());
	}

	POperationTypeDefinition COperationSequenceRegistry::registerOperationType(const std::string& sTypeName, const std::string& sTargetState)
	{
		if (!COperationParameterContainer::isValidIdentifier(sTypeName))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONTYPENAME, sTypeName);
		if (!hasState(sTargetState))
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONTARGETSTATENOTFOUND, sTypeName + " -> " + m_sInstanceName + "." + sTargetState);

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		auto iIter = m_OperationTypeMap.find(sTypeName);
		if (iIter != m_OperationTypeMap.end()) {
			if (iIter->second->getTargetState() != sTargetState)
				throw ELibMCCustomException(LIBMC_ERROR_OPERATIONTYPEDEFINITIONMISMATCH, sTypeName + " is registered with target state " + iIter->second->getTargetState());
			return iIter->second;
		}

		auto pType = std::make_shared<COperationTypeDefinition>(sTypeName, sTargetState);
		m_OperationTypes.push_back(pType);
		m_OperationTypeMap.insert(std::make_pair(sTypeName, pType));
		return pType;
	}

	bool COperationSequenceRegistry::hasOperationType(const std::string& sTypeName) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return (m_OperationTypeMap.find(sTypeName) != m_OperationTypeMap.end());
	}

	POperationTypeDefinition COperationSequenceRegistry::findOperationType(const std::string& sTypeName, bool bFailIfNotExisting) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		auto iIter = m_OperationTypeMap.find(sTypeName);
		if (iIter != m_OperationTypeMap.end())
			return iIter->second;

		if (bFailIfNotExisting)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONTYPENOTFOUND, m_sInstanceName + "." + sTypeName);

		return nullptr;
	}

	std::vector<POperationTypeDefinition> COperationSequenceRegistry::listOperationTypes() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_OperationTypes;
	}

	POperationSequence COperationSequenceRegistry::createSequence(const std::string& sSequenceName)
	{
		if (!COperationParameterContainer::isValidIdentifier(sSequenceName))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCENAME, sSequenceName);

		return std::make_shared<COperationSequence>(sSequenceName, eOperationSequenceOrigin::Code);
	}

	POperationSequence COperationSequenceRegistry::parseSequence(const std::string& sXMLString, eOperationSequenceOrigin origin)
	{
		return COperationSequence::parseFromXMLString(sXMLString, origin);
	}

	bool COperationSequenceRegistry::bindingGroupExists(const std::string& sGroupName) const
	{
		return (findBindingGroup(m_pStateMachineData.get(), m_sInstanceName, sGroupName).get() != nullptr);
	}

	std::vector<std::pair<std::string, std::string>> COperationSequenceRegistry::getBindingGroupParameters(const std::string& sGroupName) const
	{
		std::vector<std::pair<std::string, std::string>> parameters;

		auto pGroup = findBindingGroup(m_pStateMachineData.get(), m_sInstanceName, sGroupName);
		if (pGroup.get() == nullptr)
			return parameters;

		uint32_t nCount = pGroup->getParameterCount();
		for (uint32_t nIndex = 0; nIndex < nCount; nIndex++) {
			std::string sName, sDescription, sDefaultValue;
			pGroup->getParameterInfo(nIndex, sName, sDescription, sDefaultValue);
			parameters.push_back(std::make_pair(sName, sDescription));
		}

		return parameters;
	}

	void COperationSequenceRegistry::validateValue(COperationSequenceValidationResult* pResult, const std::string& sOperationUUID, const std::string& sLabel, POperationParameterDefinition pDefinition, const std::string& sRawValue, const std::string& sBindingGroup, bool bBindingGroupExists)
	{
		auto parameterType = pDefinition->getParameterType();
		auto expressionMode = pDefinition->getExpressionMode();

		if (!COperationValues::valueCountsAsExplicit(parameterType, sRawValue))
			return;

		auto valueKind = COperationValues::classifyValue(parameterType, expressionMode, sRawValue);
		auto pGroup = bBindingGroupExists ? findBindingGroup(m_pStateMachineData.get(), m_sInstanceName, sBindingGroup) : nullptr;

		if (pDefinition->isStringLike()) {
			if (valueKind == eOperationValueKind::Reference) {
				std::string sParameterName = sRawValue.substr(1);
				if (!COperationParameterContainer::isValidIdentifier(sParameterName))
					pResult->addError(sOperationUUID, sLabel, sLabel + ": invalid parameter reference \"" + sRawValue + "\"");
				else if ((pGroup.get() != nullptr) && !pGroup->hasParameter(sParameterName))
					pResult->addError(sOperationUUID, sLabel, sLabel + ": " + unknownParameterMessage(sParameterName, sBindingGroup));
				return;
			}

			if ((parameterType == eOperationParameterType::Enum) && !pDefinition->hasEnumOption(sRawValue)) {
				std::string sOptions;
				for (auto& option : pDefinition->getEnumOptions()) {
					if (!sOptions.empty())
						sOptions += ", ";
					sOptions += option.first;
				}
				pResult->addError(sOperationUUID, sLabel, sLabel + ": \"" + sRawValue + "\" is not a valid option (" + sOptions + ")");
			}
			return;
		}

		if (valueKind == eOperationValueKind::Literal) {
			double dValue = 0.0;
			if (COperationValues::tryParseNumber(sRawValue, dValue)) {
				if (pDefinition->hasRange() && (pDefinition->clampValue(dValue) != dValue)) {
					double dMinimum, dMaximum;
					pDefinition->getRange(dMinimum, dMaximum);
					pResult->addWarning(sOperationUUID, sLabel, sLabel + ": " + sRawValue + " is outside of [" + COperationValues::formatDouble(dMinimum) + ", " + COperationValues::formatDouble(dMaximum) + "] and will be clamped");
				}
				return;
			}

			std::string sLower = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(sRawValue));
			if ((parameterType == eOperationParameterType::Bool) && ((sLower == "true") || (sLower == "false")))
				return;

			if (expressionMode == eOperationExpressionMode::LiteralOnly)
				pResult->addError(sOperationUUID, sLabel, sLabel + ": \"" + sRawValue + "\" is not a number, and references or formulas are not allowed");
			else
				pResult->addError(sOperationUUID, sLabel, sLabel + ": \"" + sRawValue + "\" is not a number, and formulas are not allowed");
			return;
		}

		std::vector<std::string> referencedNames;
		std::string sExpanded = COperationValues::expandFormula(sRawValue, m_sInstanceName, sBindingGroup.empty() ? "unbound" : sBindingGroup, referencedNames);
		for (auto& sParameterName : referencedNames) {
			if (sBindingGroup.empty())
				pResult->addError(sOperationUUID, sLabel, sLabel + ": $" + sParameterName + " needs a binding group");
			else if ((pGroup.get() != nullptr) && !pGroup->hasParameter(sParameterName))
				pResult->addError(sOperationUUID, sLabel, sLabel + ": " + unknownParameterMessage(sParameterName, sBindingGroup));
		}

		try {
			CUIFormula formula(sExpanded);
		}
		catch (std::exception& E) {
			pResult->addError(sOperationUUID, sLabel, sLabel + ": formula \"" + sRawValue + "\" is invalid: " + E.what());
		}
	}

	void COperationSequenceRegistry::validateValueList(COperationSequenceValidationResult* pResult, const std::string& sOperationUUID, const std::string& sLabelPrefix, COperationParameterContainer* pContainer, const COperationValueList& values, const std::string& sBindingGroup, bool bBindingGroupExists)
	{
		for (auto& value : values.getValues()) {
			std::string sLabel = sLabelPrefix + value.first;
			auto pDefinition = pContainer->findParameter(value.first, false);
			if (pDefinition.get() == nullptr) {
				pResult->addError(sOperationUUID, sLabel, "Unknown parameter " + sLabel);
				continue;
			}

			validateValue(pResult, sOperationUUID, sLabel, pDefinition, value.second, sBindingGroup, bBindingGroupExists);
		}

		for (auto& pDefinition : pContainer->getParameters()) {
			std::string sLabel = sLabelPrefix + pDefinition->getName();
			std::string sRawValue;
			bool bExplicit = values.getValue(pDefinition->getName(), sRawValue) && COperationValues::valueCountsAsExplicit(pDefinition->getParameterType(), sRawValue);

			if (!bExplicit) {
				if (pDefinition->isRequired())
					pResult->addError(sOperationUUID, sLabel, "Required parameter " + sLabel + " is missing");
				else if ((pDefinition->getParameterType() == eOperationParameterType::Enum) && !pDefinition->hasEnumOption(pDefinition->getDefaultValue()))
					pResult->addError(sOperationUUID, sLabel, sLabel + ": registered default \"" + pDefinition->getDefaultValue() + "\" is not a valid option");
			}
		}
	}

	POperationSequenceValidationResult COperationSequenceRegistry::validateSequence(POperationSequence pSequence)
	{
		LibMCAssertNotNull(pSequence.get());

		auto pResult = std::make_shared<COperationSequenceValidationResult>();

		if (!COperationParameterContainer::isValidIdentifier(pSequence->getName()))
			pResult->addError("", "name", "Invalid sequence name \"" + pSequence->getName() + "\"");

		std::string sBindingGroup = pSequence->getBindingGroup();
		bool bBindingGroupExists = false;
		if (!sBindingGroup.empty()) {
			bBindingGroupExists = bindingGroupExists(sBindingGroup);
			if (!bBindingGroupExists)
				pResult->addError("", "bindinggroup", "Binding group " + sBindingGroup + " does not exist in state machine " + m_sInstanceName);
		}

		std::string sContext = pSequence->getContext();

		for (auto& pOperation : pSequence->getOperations()) {
			std::string sOperationUUID = pOperation->getUUID();
			auto pType = findOperationType(pOperation->getTypeName(), false);
			if (pType.get() == nullptr) {
				pResult->addError(sOperationUUID, "type", "Unknown operation type " + pOperation->getTypeName());
				continue;
			}

			if (!pType->isAllowedInContext(sContext)) {
				std::string sMessage = "Operation type " + pType->getName() + " is only allowed in the contexts " + pType->getAllowedContexts();
				if (sContext.empty())
					pResult->addWarning(sOperationUUID, "type", sMessage + ", but the sequence has no context");
				else
					pResult->addError(sOperationUUID, "type", sMessage + ", not in " + sContext);
			}

			validateValueList(pResult.get(), sOperationUUID, "", pType.get(), pOperation->values(), sBindingGroup, bBindingGroupExists);

			for (auto& sListName : pOperation->getItemListNames()) {
				auto pItemList = pType->findItemList(sListName, false);
				if (pItemList.get() == nullptr) {
					pResult->addError(sOperationUUID, sListName, "Unknown item list " + sListName + " of operation type " + pType->getName());
					continue;
				}

				uint32_t nItemCount = pOperation->getItemCount(sListName);
				if (nItemCount > pItemList->getMaxItemCount())
					pResult->addError(sOperationUUID, sListName, "Item list " + sListName + " has " + std::to_string(nItemCount) + " items, the maximum is " + std::to_string(pItemList->getMaxItemCount()));

				for (uint32_t nItemIndex = 0; nItemIndex < nItemCount; nItemIndex++) {
					auto pItem = pOperation->getItem(sListName, nItemIndex);
					validateValueList(pResult.get(), sOperationUUID, sListName + "[" + std::to_string(nItemIndex) + "].", pItemList.get(), pItem->values(), sBindingGroup, bBindingGroupExists);
				}
			}
		}

		return pResult;
	}

	void COperationSequenceRegistry::logIssues(const std::string& sSequenceName, COperationSequenceValidationResult* pResult)
	{
		for (auto& issue : pResult->getIssues()) {
			std::string sPrefix = (issue.m_Severity == eOperationSequenceIssueSeverity::Error) ? "error" : "warning";
			std::string sOperation = issue.m_sOperationUUID.empty() ? "" : " (operation " + issue.m_sOperationUUID + ")";
			logWarning("Operation sequence " + sSequenceName + " " + sPrefix + sOperation + ": " + issue.m_sMessage);
		}
	}

	POperationSequence COperationSequenceRegistry::publishLocked(POperationSequence pWorkingCopy, const std::string& sUserUUID, eOperationSequenceOrigin origin)
	{
		std::string sSequenceName = pWorkingCopy->getName();

		uint32_t nLatestRevision = 0;
		auto iIter = m_LatestRevisionNumbers.find(sSequenceName);
		if (iIter != m_LatestRevisionNumbers.end())
			nLatestRevision = iIter->second;

		uint32_t nNewRevision = nLatestRevision + 1;
		std::string sRevisionUUID = AMCCommon::CUtils::createUUID();
		uint64_t nTimeStamp = m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970();
		std::string sTimestamp = AMCCommon::CChrono::convertToISO8601TimeUTC(nTimeStamp);
		std::string sNormalizedUserUUID = sUserUUID.empty() ? AMCCommon::CUtils::createEmptyUUID() : AMCCommon::CUtils::normalizeUUIDString(sUserUUID);

		auto pPublished = pWorkingCopy->createPublishedCopy(nNewRevision, sRevisionUUID, origin, sTimestamp, sNormalizedUserUUID);

		if (m_pStore.get() != nullptr) {
			try {
				m_pStore->StoreRevision(sRevisionUUID, m_sInstanceName, sSequenceName, nNewRevision, (uint32_t)origin, pPublished->serializeToXMLString(), sNormalizedUserUUID, nTimeStamp);
			}
			catch (std::exception& E) {
				throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCESTORAGEFAILED, sSequenceName + " revision " + std::to_string(nNewRevision) + ": " + E.what());
			}
		}

		m_LatestRevisionNumbers[sSequenceName] = nNewRevision;
		m_LatestRevisions[sSequenceName] = pPublished;
		cacheRevisionLocked(pPublished);

		logMessage("Published operation sequence " + sSequenceName + " revision " + std::to_string(nNewRevision) + " (origin " + COperationSequence::originToString(origin) + ", user " + sNormalizedUserUUID + ")");

		return pPublished;
	}

	POperationSequence COperationSequenceRegistry::publishSequence(POperationSequence pWorkingCopy, const std::string& sUserUUID, bool bOverwriteNewerRevision)
	{
		LibMCAssertNotNull(pWorkingCopy.get());

		if (pWorkingCopy->isPublished())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCEISPUBLISHED, pWorkingCopy->getName() + ": publish a working copy instead");
		if (!COperationParameterContainer::isValidIdentifier(pWorkingCopy->getName()))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCENAME, pWorkingCopy->getName());

		auto pResult = validateSequence(pWorkingCopy);
		if (!pResult->isValid())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCEVALIDATIONFAILED, pWorkingCopy->getName() + ": " + pResult->getSummary(false));

		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		uint32_t nLatestRevision = 0;
		auto iIter = m_LatestRevisionNumbers.find(pWorkingCopy->getName());
		if (iIter != m_LatestRevisionNumbers.end())
			nLatestRevision = iIter->second;

		if ((!bOverwriteNewerRevision) && (pWorkingCopy->getRevision() != nLatestRevision))
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCEREVISIONCONFLICT, pWorkingCopy->getName() + ": working copy is based on revision " + std::to_string(pWorkingCopy->getRevision()) + ", latest revision is " + std::to_string(nLatestRevision));

		auto origin = pWorkingCopy->getOrigin();
		if (origin == eOperationSequenceOrigin::Unknown)
			origin = eOperationSequenceOrigin::Code;

		return publishLocked(pWorkingCopy, sUserUUID, origin);
	}

	POperationSequence COperationSequenceRegistry::ensureSequence(const std::string& sSequenceName, const std::string& sResourceName, const std::string& sResourceXML)
	{
		if (!COperationParameterContainer::isValidIdentifier(sSequenceName))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCENAME, sSequenceName);

		auto pFactory = COperationSequence::parseFromXMLString(sResourceXML, eOperationSequenceOrigin::Resource);
		pFactory->setName(sSequenceName);

		POperationSequence pStored;
		uint32_t nStoredRevision = 0;
		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);
			m_FactoryDefaults[sSequenceName] = sResourceXML;

			auto iNumberIter = m_LatestRevisionNumbers.find(sSequenceName);
			if (iNumberIter != m_LatestRevisionNumbers.end())
				nStoredRevision = iNumberIter->second;

			auto iIter = m_LatestRevisions.find(sSequenceName);
			if (iIter != m_LatestRevisions.end())
				pStored = iIter->second;
		}

		if (pStored.get() != nullptr) {
			auto pStoredResult = validateSequence(pStored);
			if (pStoredResult->isValid())
				return pStored;

			logWarning("Stored operation sequence " + sSequenceName + " revision " + std::to_string(pStored->getRevision()) + " is invalid against the registered operation types: " + pStoredResult->getSummary(false));
		}
		else if (nStoredRevision > 0) {
			logWarning("Stored operation sequence " + sSequenceName + " revision " + std::to_string(nStoredRevision) + " could not be loaded");
		}

		auto pFactoryResult = validateSequence(pFactory);
		if (!pFactoryResult->isValid()) {
			logIssues(sSequenceName + " (resource " + sResourceName + ")", pFactoryResult.get());
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCEVALIDATIONFAILED, sSequenceName + " (resource " + sResourceName + "): " + pFactoryResult->getSummary(false));
		}

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		auto pPublished = publishLocked(pFactory, "", eOperationSequenceOrigin::Resource);
		if (nStoredRevision > 0)
			logWarning("Operation sequence " + sSequenceName + ": invalid revision " + std::to_string(nStoredRevision) + " was replaced by the factory default from " + sResourceName + " as revision " + std::to_string(pPublished->getRevision()) + ". The old revision is kept in the history.");

		return pPublished;
	}

	bool COperationSequenceRegistry::hasSequence(const std::string& sSequenceName) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return (m_LatestRevisions.find(sSequenceName) != m_LatestRevisions.end());
	}

	POperationSequence COperationSequenceRegistry::getSequence(const std::string& sSequenceName) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		auto iIter = m_LatestRevisions.find(sSequenceName);
		if (iIter == m_LatestRevisions.end())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCENOTFOUND, m_sInstanceName + "." + sSequenceName);

		return iIter->second;
	}

	uint32_t COperationSequenceRegistry::getLatestRevisionNumber(const std::string& sSequenceName) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		auto iIter = m_LatestRevisionNumbers.find(sSequenceName);
		if (iIter == m_LatestRevisionNumbers.end())
			return 0;
		return iIter->second;
	}

	POperationSequence COperationSequenceRegistry::findCachedRevisionLocked(const std::string& sSequenceName, uint32_t nRevision)
	{
		auto iIter = m_RevisionCache.find(sSequenceName);
		if (iIter == m_RevisionCache.end())
			return nullptr;

		auto iRevisionIter = iIter->second.find(nRevision);
		if (iRevisionIter == iIter->second.end())
			return nullptr;

		return iRevisionIter->second;
	}

	void COperationSequenceRegistry::cacheRevisionLocked(POperationSequence pSequence)
	{
		m_RevisionCache[pSequence->getName()][pSequence->getRevision()] = pSequence;
	}

	POperationSequence COperationSequenceRegistry::restoreStoredRevision(LibMCData::POperationSequenceRevision pRevision)
	{
		auto origin = COperationSequence::originFromInteger(pRevision->GetOrigin());
		auto pParsed = COperationSequence::parseFromXMLString(pRevision->GetContent(), origin);
		pParsed->setName(pRevision->GetSequenceName());

		return pParsed->createPublishedCopy(pRevision->GetRevision(), pRevision->GetUUID(), origin, pRevision->GetTimestamp(), pRevision->GetUserUUID());
	}

	POperationSequence COperationSequenceRegistry::getSequenceRevision(const std::string& sSequenceName, uint32_t nRevision)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		auto pCached = findCachedRevisionLocked(sSequenceName, nRevision);
		if (pCached.get() != nullptr)
			return pCached;

		if (m_pStore.get() != nullptr) {
			auto pRevision = m_pStore->FindRevision(m_sInstanceName, sSequenceName, nRevision);
			if (pRevision.get() != nullptr) {
				auto pSequence = restoreStoredRevision(pRevision);
				cacheRevisionLocked(pSequence);
				return pSequence;
			}
		}

		throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCENOTFOUND, m_sInstanceName + "." + sSequenceName + " revision " + std::to_string(nRevision));
	}

	std::vector<POperationSequence> COperationSequenceRegistry::listSequences() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		std::vector<POperationSequence> sequences;
		for (auto& sequence : m_LatestRevisions)
			sequences.push_back(sequence.second);
		return sequences;
	}

	std::vector<POperationSequence> COperationSequenceRegistry::listSequenceRevisions(const std::string& sSequenceName)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		std::vector<POperationSequence> revisions;

		if (m_pStore.get() != nullptr) {
			auto pIterator = m_pStore->ListRevisions(m_sInstanceName, sSequenceName);
			while (pIterator->MoveNext()) {
				auto pRevision = pIterator->GetCurrent();
				auto pSequence = findCachedRevisionLocked(sSequenceName, pRevision->GetRevision());
				if (pSequence.get() == nullptr) {
					try {
						pSequence = restoreStoredRevision(pRevision);
						cacheRevisionLocked(pSequence);
					}
					catch (std::exception& E) {
						logWarning("Operation sequence " + sSequenceName + " revision " + std::to_string(pRevision->GetRevision()) + " could not be loaded: " + E.what());
						continue;
					}
				}
				revisions.push_back(pSequence);
			}
		}
		else {
			auto iIter = m_RevisionCache.find(sSequenceName);
			if (iIter != m_RevisionCache.end()) {
				for (auto iRevisionIter = iIter->second.rbegin(); iRevisionIter != iIter->second.rend(); iRevisionIter++)
					revisions.push_back(iRevisionIter->second);
			}
		}

		if (revisions.empty())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCENOTFOUND, m_sInstanceName + "." + sSequenceName);

		return revisions;
	}

	POperationSequence COperationSequenceRegistry::findSequenceRevisionByUUID(const std::string& sRevisionUUID, bool bFailIfNotExisting)
	{
		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sRevisionUUID);

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		for (auto& sequenceRevisions : m_RevisionCache) {
			for (auto& revision : sequenceRevisions.second)
				if (revision.second->getRevisionUUID() == sNormalizedUUID)
					return revision.second;
		}

		if (m_pStore.get() != nullptr) {
			auto pRevision = m_pStore->FindRevisionByUUID(sNormalizedUUID);
			if ((pRevision.get() != nullptr) && (pRevision->GetStateMachine() == m_sInstanceName)) {
				auto pSequence = restoreStoredRevision(pRevision);
				cacheRevisionLocked(pSequence);
				return pSequence;
			}
		}

		if (bFailIfNotExisting)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCENOTFOUND, "revision " + sNormalizedUUID);

		return nullptr;
	}

	bool COperationSequenceRegistry::hasFactoryDefault(const std::string& sSequenceName) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return (m_FactoryDefaults.find(sSequenceName) != m_FactoryDefaults.end());
	}

	POperationSequence COperationSequenceRegistry::createFactoryDefault(const std::string& sSequenceName)
	{
		std::string sXML;
		uint32_t nLatestRevision = 0;
		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);
			auto iIter = m_FactoryDefaults.find(sSequenceName);
			if (iIter == m_FactoryDefaults.end())
				throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCENOTFOUND, "no factory default for " + m_sInstanceName + "." + sSequenceName);
			sXML = iIter->second;

			auto iNumberIter = m_LatestRevisionNumbers.find(sSequenceName);
			if (iNumberIter != m_LatestRevisionNumbers.end())
				nLatestRevision = iNumberIter->second;
		}

		auto pWorkingCopy = COperationSequence::parseFromXMLString(sXML, eOperationSequenceOrigin::Resource);
		pWorkingCopy->setName(sSequenceName);
		pWorkingCopy->setBaseRevision(nLatestRevision);
		return pWorkingCopy;
	}

	void COperationSequenceRegistry::loadFromStore()
	{
		if (m_pStore.get() == nullptr)
			return;

		auto pIterator = m_pStore->ListLatestRevisions(m_sInstanceName);

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		while (pIterator->MoveNext()) {
			auto pRevision = pIterator->GetCurrent();
			std::string sSequenceName = pRevision->GetSequenceName();
			m_LatestRevisionNumbers[sSequenceName] = pRevision->GetRevision();

			try {
				auto pSequence = restoreStoredRevision(pRevision);
				m_LatestRevisions[sSequenceName] = pSequence;
				cacheRevisionLocked(pSequence);
			}
			catch (std::exception& E) {
				logWarning("Operation sequence " + sSequenceName + " revision " + std::to_string(pRevision->GetRevision()) + " could not be loaded: " + E.what());
			}
		}
	}

	void COperationSequenceRegistry::validateDispatcherTransitions(const std::string& sDispatcherState)
	{
		if (!hasState(sDispatcherState))
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONTARGETSTATENOTFOUND, "dispatcher state " + m_sInstanceName + "." + sDispatcherState);

		std::set<std::string> targetStates;
		for (auto& pType : listOperationTypes())
			targetStates.insert(pType->getTargetState());

		std::string sMissing;
		for (auto& sTargetState : targetStates) {
			if (!hasTransition(sDispatcherState, sTargetState))
				sMissing += (sMissing.empty() ? "" : ", ") + sDispatcherState + " -> " + sTargetState;
			if (!hasTransition(sTargetState, sDispatcherState))
				sMissing += (sMissing.empty() ? "" : ", ") + sTargetState + " -> " + sDispatcherState;
		}

		if (!sMissing.empty())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONDISPATCHERTRANSITIONMISSING, m_sInstanceName + ": missing outstates " + sMissing);
	}

	void COperationSequenceRegistry::setStatusParameterGroup(const std::string& sGroupName, const std::string& sValuePrefix)
	{
		if (!sGroupName.empty() && !bindingGroupExists(sGroupName))
			throw ELibMCCustomException(LIBMC_ERROR_PARAMETERGROUPNOTFOUND, m_sInstanceName + "." + sGroupName);

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_sStatusGroupName = sGroupName;
		m_sStatusValuePrefix = sValuePrefix;
	}

	POperationSequenceExecution COperationSequenceRegistry::startExecution(const std::string& sExecutionName, const std::string& sSequenceName, const std::string& sBindingGroup, const std::string& sReturnState)
	{
		if (!COperationParameterContainer::isValidIdentifier(sExecutionName))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDPARAM, "invalid execution name " + sExecutionName);

		auto pSequence = getSequence(sSequenceName);
		auto pResult = validateSequence(pSequence);
		if (!pResult->isValid())
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCEVALIDATIONFAILED, sSequenceName + " revision " + std::to_string(pSequence->getRevision()) + ": " + pResult->getSummary(false));

		std::string sGroupName = sBindingGroup.empty() ? pSequence->getBindingGroup() : sBindingGroup;
		if (!sGroupName.empty() && !bindingGroupExists(sGroupName))
			throw ELibMCCustomException(LIBMC_ERROR_PARAMETERGROUPNOTFOUND, m_sInstanceName + "." + sGroupName);

		if (!hasState(sReturnState))
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONTARGETSTATENOTFOUND, "return state " + m_sInstanceName + "." + sReturnState);

		POperationSequenceExecution pOldExecution;
		auto pExecution = std::make_shared<COperationSequenceExecution>(this, sExecutionName, pSequence, sGroupName, sReturnState);
		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);
			auto iIter = m_Executions.find(sExecutionName);
			if (iIter != m_Executions.end())
				pOldExecution = iIter->second;
			m_Executions[sExecutionName] = pExecution;
		}

		if ((pOldExecution.get() != nullptr) && (pOldExecution->getStatus() == eOperationSequenceExecutionStatus::Running))
			pOldExecution->abort("replaced by a new execution of " + sSequenceName);

		logMessage("Started operation sequence " + sSequenceName + " revision " + std::to_string(pSequence->getRevision()) + " as execution " + sExecutionName);
		mirrorExecutionStatus(pExecution.get(), nullptr);

		return pExecution;
	}

	POperationSequenceExecution COperationSequenceRegistry::findExecution(const std::string& sExecutionName, bool bFailIfNotExisting) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		auto iIter = m_Executions.find(sExecutionName);
		if (iIter != m_Executions.end())
			return iIter->second;

		if (bFailIfNotExisting)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCEEXECUTIONNOTFOUND, m_sInstanceName + "." + sExecutionName);

		return nullptr;
	}

	bool COperationSequenceRegistry::hasDispatchedOperation() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return (m_pDispatchedOperation.get() != nullptr);
	}

	PDispatchedOperation COperationSequenceRegistry::getDispatchedOperation() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		if (m_pDispatchedOperation.get() == nullptr)
			throw ELibMCCustomException(LIBMC_ERROR_NOOPERATIONDISPATCHED, m_sInstanceName);

		return m_pDispatchedOperation;
	}

	void COperationSequenceRegistry::setDispatchedOperation(PDispatchedOperation pOperation)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_pDispatchedOperation = pOperation;
	}

	sOperationResolvedValue COperationSequenceRegistry::resolveValue(POperationParameterDefinition pDefinition, const std::string& sRawValue, bool bExplicit, const std::string& sBindingGroup)
	{
		LibMCAssertNotNull(pDefinition.get());

		sOperationResolvedValue resolvedValue;
		resolvedValue.m_pDefinition = pDefinition;
		resolvedValue.m_bExplicit = bExplicit;
		resolvedValue.m_sRawValue = sRawValue;

		std::string sName = pDefinition->getName();
		std::string sValue = bExplicit ? sRawValue : pDefinition->getDefaultValue();
		auto parameterType = pDefinition->getParameterType();
		auto expressionMode = pDefinition->getExpressionMode();

		std::string sErrorPrefix = "Operation attribute " + sName + "=\"" + sValue + "\" could not be evaluated: ";

		if (pDefinition->isStringLike()) {
			std::string sResult = sValue;
			if ((expressionMode != eOperationExpressionMode::LiteralOnly) && (!sValue.empty()) && (sValue[0] == '$')) {
				std::string sParameterName = sValue.substr(1);
				auto pGroup = findBindingGroup(m_pStateMachineData.get(), m_sInstanceName, sBindingGroup);
				if ((pGroup.get() == nullptr) || !pGroup->hasParameter(sParameterName))
					throw ELibMCCustomException(LIBMC_ERROR_OPERATIONPARAMETEREVALUATIONFAILED, unknownParameterMessage(sParameterName, sBindingGroup));

				sResult = pGroup->getParameterValueByName(sParameterName);
			}

			if ((parameterType == eOperationParameterType::Enum) && !pDefinition->hasEnumOption(sResult))
				throw ELibMCCustomException(LIBMC_ERROR_OPERATIONPARAMETEREVALUATIONFAILED, sErrorPrefix + "\"" + sResult + "\" is not a valid option");

			std::string sLower = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(sResult));
			resolvedValue.m_sStringValue = sResult;
			resolvedValue.m_bBoolValue = (sLower == "1") || (sLower == "true");
			return resolvedValue;
		}

		double dValue = 0.0;
		std::string sLower = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(sValue));

		if (COperationValues::tryParseNumber(sValue, dValue)) {
			// Literal number.
		}
		else if ((parameterType == eOperationParameterType::Bool) && ((sLower == "true") || (sLower == "false"))) {
			dValue = (sLower == "true") ? 1.0 : 0.0;
		}
		else {
			std::string sParameterName;
			bool bIsSingleReference = COperationValues::isSingleReference(sValue, sParameterName);

			if (expressionMode == eOperationExpressionMode::LiteralOnly)
				throw ELibMCCustomException(LIBMC_ERROR_OPERATIONPARAMETEREVALUATIONFAILED, sErrorPrefix + "not a number");
			if ((expressionMode == eOperationExpressionMode::LiteralOrReference) && !bIsSingleReference)
				throw ELibMCCustomException(LIBMC_ERROR_OPERATIONPARAMETEREVALUATIONFAILED, sErrorPrefix + "formulas are not allowed");

			std::vector<std::string> referencedNames;
			std::string sFormula = COperationValues::expandFormula(sValue, m_sInstanceName, sBindingGroup, referencedNames);

			if (!referencedNames.empty()) {
				auto pGroup = findBindingGroup(m_pStateMachineData.get(), m_sInstanceName, sBindingGroup);
				for (auto& sReferencedName : referencedNames) {
					if ((pGroup.get() == nullptr) || !pGroup->hasParameter(sReferencedName))
						throw ELibMCCustomException(LIBMC_ERROR_OPERATIONPARAMETEREVALUATIONFAILED, unknownParameterMessage(sReferencedName, sBindingGroup));
				}
			}

			std::string sEvaluationError;
			try {
				CUIFormula formula(sFormula);
				CUIExpressionFormulaResolver resolver(m_pStateMachineData.get(), nullptr);
				auto formulaValue = formula.evaluate(&resolver);
				if (parameterType == eOperationParameterType::Bool)
					dValue = formulaValue.toBool() ? 1.0 : 0.0;
				else
					dValue = formulaValue.toDouble();
			}
			catch (std::exception& E) {
				sEvaluationError = E.what();
			}

			if (!sEvaluationError.empty())
				throw ELibMCCustomException(LIBMC_ERROR_OPERATIONPARAMETEREVALUATIONFAILED, sErrorPrefix + sEvaluationError);
		}

		if (parameterType == eOperationParameterType::Bool) {
			resolvedValue.m_bBoolValue = (dValue != 0.0);
			resolvedValue.m_dNumericValue = resolvedValue.m_bBoolValue ? 1.0 : 0.0;
			resolvedValue.m_sStringValue = resolvedValue.m_bBoolValue ? "1" : "0";
			return resolvedValue;
		}

		dValue = pDefinition->clampValue(dValue);
		if (parameterType == eOperationParameterType::Integer) {
			int64_t nValue = (int64_t)std::llround(dValue);
			resolvedValue.m_dNumericValue = (double)nValue;
			resolvedValue.m_sStringValue = std::to_string(nValue);
		}
		else {
			resolvedValue.m_dNumericValue = dValue;
			resolvedValue.m_sStringValue = COperationValues::formatDouble(dValue);
		}
		resolvedValue.m_bBoolValue = (resolvedValue.m_dNumericValue != 0.0);

		return resolvedValue;
	}

	void COperationSequenceRegistry::resolveValueList(COperationParameterContainer* pContainer, const COperationValueList& values, const std::string& sBindingGroup, OperationResolvedValueMap& resolvedValues, std::vector<std::string>* pValueOrder)
	{
		LibMCAssertNotNull(pContainer);

		for (auto& pDefinition : pContainer->getParameters()) {
			std::string sRawValue;
			bool bExplicit = values.getValue(pDefinition->getName(), sRawValue) && COperationValues::valueCountsAsExplicit(pDefinition->getParameterType(), sRawValue);

			resolvedValues[pDefinition->getName()] = resolveValue(pDefinition, sRawValue, bExplicit, sBindingGroup);
			if (pValueOrder != nullptr)
				pValueOrder->push_back(pDefinition->getName());
		}
	}

	void COperationSequenceRegistry::setStatusValue(CParameterGroup* pGroup, const std::string& sName, const std::string& sStringValue, double dNumericValue, bool bIsNumeric)
	{
		if (!pGroup->hasParameter(sName))
			return;

		try {
			double dValue = dNumericValue;
			if (!bIsNumeric) {
				if (!COperationValues::tryParseNumber(sStringValue, dValue)) {
					std::string sLower = AMCCommon::CUtils::toLowerString(sStringValue);
					dValue = (sLower == "true") ? 1.0 : 0.0;
				}
			}

			switch (pGroup->getParameterDataTypeByName(sName)) {
			case eParameterDataType::Integer:
				pGroup->setIntParameterValueByName(sName, (int64_t)std::llround(dValue));
				break;
			case eParameterDataType::Double:
				pGroup->setDoubleParameterValueByName(sName, dValue);
				break;
			case eParameterDataType::Bool:
				pGroup->setBoolParameterValueByName(sName, dValue != 0.0);
				break;
			default:
				pGroup->setParameterValueByName(sName, sStringValue);
				break;
			}
		}
		catch (std::exception& E) {
			logWarning("Operation sequence status " + pGroup->getName() + "." + sName + " could not be written: " + E.what());
		}
	}

	void COperationSequenceRegistry::mirrorExecutionStatus(COperationSequenceExecution* pExecution, CDispatchedOperation* pOperation)
	{
		LibMCAssertNotNull(pExecution);

		std::string sGroupName, sPrefix;
		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);
			sGroupName = m_sStatusGroupName;
			sPrefix = m_sStatusValuePrefix;
		}

		if (sGroupName.empty())
			return;

		auto pGroup = findBindingGroup(m_pStateMachineData.get(), m_sInstanceName, sGroupName);
		if (pGroup.get() == nullptr)
			return;

		auto pSequence = pExecution->getSequence();
		setStatusValue(pGroup.get(), "currentexecution", pExecution->getName(), 0.0, false);
		setStatusValue(pGroup.get(), "currentsequence", pSequence->getName(), 0.0, false);
		setStatusValue(pGroup.get(), "currentrevision", std::to_string(pSequence->getRevision()), (double)pSequence->getRevision(), true);
		setStatusValue(pGroup.get(), "operationcount", std::to_string(pSequence->getOperationCount()), (double)pSequence->getOperationCount(), true);
		setStatusValue(pGroup.get(), "executionstatus", COperationSequenceExecution::statusToString(pExecution->getStatus()), 0.0, false);
		setStatusValue(pGroup.get(), "iteration", std::to_string(pExecution->getIterationCount()), (double)pExecution->getIterationCount(), true);

		if (pOperation != nullptr) {
			uint32_t nCurrentOperation = pOperation->getIndex() + 1;
			setStatusValue(pGroup.get(), "currentoperation", std::to_string(nCurrentOperation), (double)nCurrentOperation, true);
			setStatusValue(pGroup.get(), "currentoperationuuid", pOperation->getUUID(), 0.0, false);
			setStatusValue(pGroup.get(), sPrefix + "type", pOperation->getTypeName(), 0.0, false);

			auto& values = pOperation->getValues();
			for (auto& sName : pOperation->getValueOrder()) {
				auto iIter = values.find(sName);
				if (iIter == values.end())
					continue;

				auto& value = iIter->second;
				setStatusValue(pGroup.get(), sPrefix + sName, value.m_sStringValue, value.m_dNumericValue, !value.m_pDefinition->isStringLike());
			}
		}
	}


	/*************************************************************************************************************************
	 COperationSequenceHandler
	**************************************************************************************************************************/

	COperationSequenceHandler::COperationSequenceHandler(PStateMachineData pStateMachineData, PLogger pLogger, LibMCData::PDataModel pDataModel, AMCCommon::PChrono pGlobalChrono)
		: m_pStateMachineData(pStateMachineData), m_pLogger(pLogger), m_pDataModel(pDataModel), m_pGlobalChrono(pGlobalChrono)
	{
		LibMCAssertNotNull(pStateMachineData.get());
		LibMCAssertNotNull(pGlobalChrono.get());
	}

	COperationSequenceHandler::~COperationSequenceHandler()
	{
	}

	POperationSequenceRegistry COperationSequenceHandler::createRegistry(const std::string& sInstanceName)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		auto iIter = m_Registries.find(sInstanceName);
		if (iIter != m_Registries.end())
			return iIter->second;

		LibMCData::POperationSequenceStore pStore;
		if (m_pDataModel.get() != nullptr)
			pStore = m_pDataModel->CreateOperationSequenceStore();

		auto pRegistry = std::make_shared<COperationSequenceRegistry>(sInstanceName, m_pStateMachineData, m_pLogger, pStore, m_pGlobalChrono);
		m_Registries.insert(std::make_pair(sInstanceName, pRegistry));
		return pRegistry;
	}

	POperationSequenceRegistry COperationSequenceHandler::findRegistry(const std::string& sInstanceName, bool bFailIfNotExisting) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);

		auto iIter = m_Registries.find(sInstanceName);
		if (iIter != m_Registries.end())
			return iIter->second;

		if (bFailIfNotExisting)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCEREGISTRYNOTFOUND, sInstanceName);

		return nullptr;
	}

	std::vector<POperationSequenceRegistry> COperationSequenceHandler::getRegistries() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		std::vector<POperationSequenceRegistry> registries;
		for (auto& registry : m_Registries)
			registries.push_back(registry.second);
		return registries;
	}

}
