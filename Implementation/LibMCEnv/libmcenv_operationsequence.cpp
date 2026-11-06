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

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL AUTODESK INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Abstract: This is the class definition of the operation sequence model classes

*/

#include "libmcenv_operationsequence.hpp"
#include "libmcenv_operationsequenceerrors.hpp"
#include "libmcenv_interfaceexception.hpp"
#include "libmcenv_xmldocument.hpp"

#include "amc_operationvalues.hpp"
#include "amc_xmldocument.hpp"

using namespace LibMCEnv::Impl;

static void checkOperationValueName(const std::string& sName)
{
	if (!AMC::COperationParameterContainer::isValidIdentifier(sName) || AMC::COperationParameterContainer::isReservedAttributeName(sName))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDOPERATIONPARAMETERNAME, sName);
}


/*************************************************************************************************************************
 Class definition of COperationItem
**************************************************************************************************************************/

COperationItem::COperationItem(AMC::POperationItem pItem, AMC::POperationSequence pSequence, uint32_t nIndex)
	: m_pItem(pItem), m_pSequence(pSequence), m_nIndex(nIndex)
{
	if ((pItem.get() == nullptr) || (pSequence.get() == nullptr))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

COperationItem::~COperationItem()
{
}

LibMCEnv_uint32 COperationItem::GetIndex()
{
	return m_nIndex;
}

bool COperationItem::HasValue(const std::string& sName)
{
	return m_pItem->values().hasValue(sName);
}

std::string COperationItem::GetRawValue(const std::string& sName)
{
	std::string sValue;
	m_pItem->values().getValue(sName, sValue);
	return sValue;
}

void COperationItem::SetRawValue(const std::string& sName, const std::string& sValue)
{
	callOperationSequenceCore([&]() { m_pSequence->checkMutable(); });
	checkOperationValueName(sName);
	m_pItem->values().setValue(sName, sValue);
}

void COperationItem::ClearValue(const std::string& sName)
{
	callOperationSequenceCore([&]() { m_pSequence->checkMutable(); });
	m_pItem->values().clearValue(sName);
}


/*************************************************************************************************************************
 Class definition of COperation
**************************************************************************************************************************/

COperation::COperation(AMC::POperation pOperation, AMC::POperationSequence pSequence, AMC::POperationSequenceRegistry pRegistry)
	: m_pOperation(pOperation), m_pSequence(pSequence), m_pRegistry(pRegistry)
{
	if ((pOperation.get() == nullptr) || (pSequence.get() == nullptr) || (pRegistry.get() == nullptr))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

COperation::~COperation()
{
}

AMC::POperation COperation::getOperation()
{
	return m_pOperation;
}

AMC::POperationParameterDefinition COperation::findDefinition(const std::string& sName)
{
	auto pType = m_pRegistry->findOperationType(m_pOperation->getTypeName(), false);
	if (pType.get() == nullptr)
		return nullptr;

	return pType->findParameter(sName, false);
}

std::string COperation::GetUUID()
{
	return m_pOperation->getUUID();
}

LibMCEnv_uint32 COperation::GetIndex()
{
	int64_t nIndex = m_pSequence->findOperationIndex(m_pOperation->getUUID());
	if (nIndex < 0)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_OPERATIONNOTFOUND, m_pOperation->getUUID());

	return (LibMCEnv_uint32)nIndex;
}

std::string COperation::GetTypeName()
{
	return m_pOperation->getTypeName();
}

bool COperation::IsEnabled()
{
	return m_pOperation->isEnabled();
}

void COperation::SetEnabled(const bool bIsEnabled)
{
	callOperationSequenceCore([&]() { m_pSequence->checkMutable(); });
	m_pOperation->setEnabled(bIsEnabled);
}

std::string COperation::GetComment()
{
	return m_pOperation->getComment();
}

void COperation::SetComment(const std::string& sComment)
{
	callOperationSequenceCore([&]() { m_pSequence->checkMutable(); });
	m_pOperation->setComment(sComment);
}

bool COperation::HasValue(const std::string& sName)
{
	return m_pOperation->values().hasValue(sName);
}

std::string COperation::GetRawValue(const std::string& sName)
{
	std::string sValue;
	m_pOperation->values().getValue(sName, sValue);
	return sValue;
}

LibMCEnv::eOperationValueKind COperation::GetValueKind(const std::string& sName)
{
	std::string sValue;
	if (!m_pOperation->values().getValue(sName, sValue))
		return LibMCEnv::eOperationValueKind::Default;

	auto parameterType = AMC::eOperationParameterType::String;
	auto expressionMode = AMC::eOperationExpressionMode::LiteralOrReference;

	auto pDefinition = findDefinition(sName);
	if (pDefinition.get() != nullptr) {
		parameterType = pDefinition->getParameterType();
		expressionMode = pDefinition->getExpressionMode();
	}

	if (!AMC::COperationValues::valueCountsAsExplicit(parameterType, sValue))
		return LibMCEnv::eOperationValueKind::Default;

	return (LibMCEnv::eOperationValueKind)AMC::COperationValues::classifyValue(parameterType, expressionMode, sValue);
}

void COperation::SetRawValue(const std::string& sName, const std::string& sValue)
{
	callOperationSequenceCore([&]() { m_pSequence->checkMutable(); });
	checkOperationValueName(sName);
	m_pOperation->values().setValue(sName, sValue);
}

void COperation::ClearValue(const std::string& sName)
{
	callOperationSequenceCore([&]() { m_pSequence->checkMutable(); });
	m_pOperation->values().clearValue(sName);
}

LibMCEnv_uint32 COperation::GetItemCount(const std::string& sListName)
{
	return m_pOperation->getItemCount(sListName);
}

IOperationItem* COperation::GetItem(const std::string& sListName, const LibMCEnv_uint32 nIndex)
{
	auto pItem = callOperationSequenceCore([&]() { return m_pOperation->getItem(sListName, nIndex); });
	return new COperationItem(pItem, m_pSequence, nIndex);
}

IOperationItem* COperation::AddItem(const std::string& sListName)
{
	callOperationSequenceCore([&]() { m_pSequence->checkMutable(); });

	auto pType = m_pRegistry->findOperationType(m_pOperation->getTypeName(), false);
	if (pType.get() != nullptr) {
		auto pItemList = pType->findItemList(sListName, false);
		if (pItemList.get() == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_OPERATIONITEMLISTNOTFOUND, m_pOperation->getTypeName() + "." + sListName);
		if (m_pOperation->getItemCount(sListName) >= pItemList->getMaxItemCount())
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_OPERATIONITEMLISTFULL, m_pOperation->getTypeName() + "." + sListName);
	}
	else {
		checkOperationValueName(sListName);
	}

	uint32_t nIndex = m_pOperation->getItemCount(sListName);
	auto pItem = m_pOperation->addItem(sListName);
	return new COperationItem(pItem, m_pSequence, nIndex);
}

void COperation::RemoveItem(const std::string& sListName, const LibMCEnv_uint32 nIndex)
{
	callOperationSequenceCore([&]() {
		m_pSequence->checkMutable();
		m_pOperation->removeItem(sListName, nIndex);
	});
}


/*************************************************************************************************************************
 Class definition of COperationIterator
**************************************************************************************************************************/

COperationIterator::COperationIterator(AMC::POperationSequence pSequence, AMC::POperationSequenceRegistry pRegistry)
	: m_pSequence(pSequence), m_pRegistry(pRegistry)
{
	if ((pSequence.get() == nullptr) || (pRegistry.get() == nullptr))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	for (auto& pOperation : pSequence->getOperations())
		m_List.push_back(std::make_shared<COperation>(pOperation, pSequence, pRegistry));
}

COperationIterator::~COperationIterator()
{
}

IOperation* COperationIterator::GetCurrent()
{
	if ((m_nCurrentIndex < 0) || (m_nCurrentIndex >= (int32_t)m_List.size()))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDITERATOR);

	auto pOperation = std::dynamic_pointer_cast<COperation> (m_List[m_nCurrentIndex]);
	if (pOperation.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCAST);

	return new COperation(pOperation->getOperation(), m_pSequence, m_pRegistry);
}

IIterator* COperationIterator::Clone()
{
	return new COperationIterator(m_pSequence, m_pRegistry);
}


/*************************************************************************************************************************
 Class definition of COperationSequenceValidationResult
**************************************************************************************************************************/

COperationSequenceValidationResult::COperationSequenceValidationResult(AMC::POperationSequenceValidationResult pResult)
	: m_pResult(pResult)
{
	if (pResult.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

COperationSequenceValidationResult::~COperationSequenceValidationResult()
{
}

bool COperationSequenceValidationResult::IsValid()
{
	return m_pResult->isValid();
}

LibMCEnv_uint32 COperationSequenceValidationResult::GetIssueCount()
{
	return m_pResult->getIssueCount();
}

void COperationSequenceValidationResult::GetIssue(const LibMCEnv_uint32 nIndex, LibMCEnv::eOperationSequenceIssueSeverity& eSeverity, std::string& sOperationUUID, std::string& sParameterName, std::string& sMessage)
{
	auto& issue = callOperationSequenceCore([&]() -> const AMC::sOperationSequenceIssue& { return m_pResult->getIssue(nIndex); });
	eSeverity = (LibMCEnv::eOperationSequenceIssueSeverity)issue.m_Severity;
	sOperationUUID = issue.m_sOperationUUID;
	sParameterName = issue.m_sParameterName;
	sMessage = issue.m_sMessage;
}


/*************************************************************************************************************************
 Class definition of COperationSequence
**************************************************************************************************************************/

COperationSequence::COperationSequence(AMC::POperationSequence pSequence, AMC::POperationSequenceRegistry pRegistry, AMC::eOperationSequenceOrigin workingCopyOrigin)
	: m_pSequence(pSequence), m_pRegistry(pRegistry), m_WorkingCopyOrigin(workingCopyOrigin)
{
	if ((pSequence.get() == nullptr) || (pRegistry.get() == nullptr))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

COperationSequence::~COperationSequence()
{
}

AMC::POperationSequence COperationSequence::getSequence()
{
	return m_pSequence;
}

AMC::POperationSequenceRegistry COperationSequence::getRegistry()
{
	return m_pRegistry;
}

AMC::eOperationSequenceOrigin COperationSequence::getWorkingCopyOrigin()
{
	return m_WorkingCopyOrigin;
}

std::string COperationSequence::GetName()
{
	return m_pSequence->getName();
}

std::string COperationSequence::GetDisplayName()
{
	return m_pSequence->getDisplayName();
}

void COperationSequence::SetDisplayName(const std::string& sDisplayName)
{
	callOperationSequenceCore([&]() { m_pSequence->setDisplayName(sDisplayName); });
}

std::string COperationSequence::GetDescription()
{
	return m_pSequence->getDescription();
}

void COperationSequence::SetDescription(const std::string& sDescription)
{
	callOperationSequenceCore([&]() { m_pSequence->setDescription(sDescription); });
}

std::string COperationSequence::GetContext()
{
	return m_pSequence->getContext();
}

void COperationSequence::SetContext(const std::string& sContext)
{
	callOperationSequenceCore([&]() { m_pSequence->setContext(sContext); });
}

std::string COperationSequence::GetBindingGroup()
{
	return m_pSequence->getBindingGroup();
}

void COperationSequence::SetBindingGroup(const std::string& sGroupName)
{
	callOperationSequenceCore([&]() { m_pSequence->setBindingGroup(sGroupName); });
}

bool COperationSequence::IsPublished()
{
	return m_pSequence->isPublished();
}

LibMCEnv_uint32 COperationSequence::GetRevision()
{
	return m_pSequence->getRevision();
}

std::string COperationSequence::GetRevisionUUID()
{
	return m_pSequence->getRevisionUUID();
}

bool COperationSequence::IsValid()
{
	return callOperationSequenceCore([&]() { return m_pRegistry->validateSequence(m_pSequence)->isValid(); });
}

IOperationSequenceValidationResult* COperationSequence::GetValidationResult()
{
	auto pResult = callOperationSequenceCore([&]() { return m_pRegistry->validateSequence(m_pSequence); });
	return new COperationSequenceValidationResult(pResult);
}

LibMCEnv::eOperationSequenceOrigin COperationSequence::GetOrigin()
{
	return (LibMCEnv::eOperationSequenceOrigin)m_pSequence->getOrigin();
}

std::string COperationSequence::GetTimestamp()
{
	return m_pSequence->getTimestamp();
}

std::string COperationSequence::GetUserUUID()
{
	return m_pSequence->getUserUUID();
}

LibMCEnv_uint32 COperationSequence::GetOperationCount()
{
	return m_pSequence->getOperationCount();
}

IOperation* COperationSequence::GetOperation(const LibMCEnv_uint32 nIndex)
{
	auto pOperation = callOperationSequenceCore([&]() { return m_pSequence->getOperation(nIndex); });
	return new COperation(pOperation, m_pSequence, m_pRegistry);
}

IOperation* COperationSequence::FindOperation(const std::string& sUUID, const bool bFailIfNotExisting)
{
	auto pOperation = callOperationSequenceCore([&]() { return m_pSequence->findOperation(sUUID); });
	if (pOperation.get() == nullptr) {
		if (bFailIfNotExisting)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_OPERATIONNOTFOUND, sUUID);
		return nullptr;
	}

	return new COperation(pOperation, m_pSequence, m_pRegistry);
}

IOperationIterator* COperationSequence::ListOperations()
{
	return new COperationIterator(m_pSequence, m_pRegistry);
}

IOperation* COperationSequence::AddOperation(const std::string& sTypeName)
{
	return InsertOperation(m_pSequence->getOperationCount(), sTypeName);
}

IOperation* COperationSequence::InsertOperation(const LibMCEnv_uint32 nIndex, const std::string& sTypeName)
{
	auto pOperation = callOperationSequenceCore([&]() {
		m_pSequence->checkMutable();
		m_pRegistry->findOperationType(sTypeName, true);
		return m_pSequence->insertOperation(nIndex, sTypeName, "");
	});

	return new COperation(pOperation, m_pSequence, m_pRegistry);
}

void COperationSequence::RemoveOperation(const std::string& sUUID)
{
	callOperationSequenceCore([&]() { m_pSequence->removeOperation(sUUID); });
}

void COperationSequence::MoveOperation(const std::string& sUUID, const LibMCEnv_uint32 nNewIndex)
{
	callOperationSequenceCore([&]() { m_pSequence->moveOperation(sUUID, nNewIndex); });
}

IOperationSequence* COperationSequence::CreateWorkingCopy()
{
	auto pWorkingCopy = callOperationSequenceCore([&]() { return m_pSequence->createWorkingCopy(m_WorkingCopyOrigin); });
	return new COperationSequence(pWorkingCopy, m_pRegistry, m_WorkingCopyOrigin);
}

std::string COperationSequence::SerializeToXMLString()
{
	return callOperationSequenceCore([&]() { return m_pSequence->serializeToXMLString(); });
}

LibMCEnv::Impl::IXMLDocument* COperationSequence::SerializeToXMLDocument()
{
	std::string sXMLString = SerializeToXMLString();

	auto pDocument = std::make_shared<AMC::CXMLDocumentInstance>();
	pDocument->parseXMLString(sXMLString);

	return new CXMLDocument(pDocument);
}


/*************************************************************************************************************************
 Class definition of COperationSequenceIterator
**************************************************************************************************************************/

COperationSequenceIterator::COperationSequenceIterator()
{
}

COperationSequenceIterator::~COperationSequenceIterator()
{
}

IOperationSequence* COperationSequenceIterator::GetCurrent()
{
	if ((m_nCurrentIndex < 0) || (m_nCurrentIndex >= (int32_t)m_List.size()))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDITERATOR);

	auto pSequence = std::dynamic_pointer_cast<COperationSequence> (m_List[m_nCurrentIndex]);
	if (pSequence.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCAST);

	return new COperationSequence(pSequence->getSequence(), pSequence->getRegistry(), pSequence->getWorkingCopyOrigin());
}

IIterator* COperationSequenceIterator::Clone()
{
	std::unique_ptr<COperationSequenceIterator> pNewIterator(new COperationSequenceIterator());
	for (auto pBase : m_List)
		pNewIterator->m_List.push_back(pBase);
	return pNewIterator.release();
}

void COperationSequenceIterator::addSequence(std::shared_ptr<COperationSequence> pSequence)
{
	if (pSequence.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
	m_List.push_back(pSequence);
}
