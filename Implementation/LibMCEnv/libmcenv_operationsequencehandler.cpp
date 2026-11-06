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


Abstract: This is the class definition of COperationSequenceHandler, CDispatchedOperation and COperationSequenceExecution

*/

#include "libmcenv_operationsequencehandler.hpp"
#include "libmcenv_operationsequence.hpp"
#include "libmcenv_operationtypedefinition.hpp"
#include "libmcenv_operationsequenceerrors.hpp"
#include "libmcenv_interfaceexception.hpp"

#include "pugixml.hpp"

using namespace LibMCEnv::Impl;

static std::string operationSequenceLocalName(const std::string& sName)
{
	auto nColon = sName.find(':');
	if (nColon == std::string::npos)
		return sName;
	return sName.substr(nColon + 1);
}


/*************************************************************************************************************************
 Class definition of COperationSequenceHandler
**************************************************************************************************************************/

COperationSequenceHandler::COperationSequenceHandler(AMC::POperationSequenceRegistry pRegistry, bool bAllowRegistration, AMC::PResourcePackage pResourcePackage, AMC::eOperationSequenceOrigin workingCopyOrigin)
	: m_pRegistry(pRegistry), m_bAllowRegistration(bAllowRegistration), m_pResourcePackage(pResourcePackage), m_WorkingCopyOrigin(workingCopyOrigin)
{
	if (pRegistry.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

COperationSequenceHandler::~COperationSequenceHandler()
{
}

IOperationSequence* COperationSequenceHandler::wrapSequence(AMC::POperationSequence pSequence)
{
	if (pSequence.get() == nullptr)
		return nullptr;

	return new COperationSequence(pSequence, m_pRegistry, m_WorkingCopyOrigin);
}

COperationSequence* COperationSequenceHandler::castSequence(IOperationSequence* pSequence)
{
	if (pSequence == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	auto pSequenceInstance = dynamic_cast<COperationSequence*> (pSequence);
	if (pSequenceInstance == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCAST);

	return pSequenceInstance;
}

std::string COperationSequenceHandler::readResource(const std::string& sResourceName)
{
	if (m_pResourcePackage.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_RESOURCEENTRYNOTFOUND, sResourceName);

	if (m_pResourcePackage->findEntryByName(sResourceName, false).get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_RESOURCEENTRYNOTFOUND, sResourceName);

	return m_pResourcePackage->readEntryUTF8String(sResourceName);
}

std::string COperationSequenceHandler::GetStateMachineInstance()
{
	return m_pRegistry->getInstanceName();
}

IOperationTypeDefinition* COperationSequenceHandler::RegisterOperationType(const std::string& sTypeName, const std::string& sTargetState)
{
	if (!m_bAllowRegistration)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_OPERATIONTYPEREGISTRATIONNOTALLOWED, sTypeName);

	auto pType = callOperationSequenceCore([&]() { return m_pRegistry->registerOperationType(sTypeName, sTargetState); });
	return new COperationTypeDefinition(pType, true, m_pResourcePackage);
}

bool COperationSequenceHandler::HasOperationType(const std::string& sTypeName)
{
	return m_pRegistry->hasOperationType(sTypeName);
}

IOperationTypeDefinition* COperationSequenceHandler::FindOperationType(const std::string& sTypeName, const bool bFailIfNotExisting)
{
	auto pType = callOperationSequenceCore([&]() { return m_pRegistry->findOperationType(sTypeName, bFailIfNotExisting); });
	if (pType.get() == nullptr)
		return nullptr;

	return new COperationTypeDefinition(pType, m_bAllowRegistration, m_pResourcePackage);
}

IOperationTypeDefinitionIterator* COperationSequenceHandler::ListOperationTypes()
{
	std::unique_ptr<COperationTypeDefinitionIterator> pIterator(new COperationTypeDefinitionIterator());
	for (auto& pType : m_pRegistry->listOperationTypes())
		pIterator->addType(std::make_shared<COperationTypeDefinition>(pType, m_bAllowRegistration, m_pResourcePackage));
	return pIterator.release();
}

IOperationSequence* COperationSequenceHandler::CreateSequence(const std::string& sSequenceName)
{
	auto pSequence = callOperationSequenceCore([&]() { return m_pRegistry->createSequence(sSequenceName); });
	return wrapSequence(pSequence);
}

IOperationSequence* COperationSequenceHandler::ParseSequenceFromString(const std::string& sXMLString)
{
	auto pSequence = callOperationSequenceCore([&]() { return m_pRegistry->parseSequence(sXMLString, AMC::eOperationSequenceOrigin::String); });
	return wrapSequence(pSequence);
}

IOperationSequence* COperationSequenceHandler::ParseSequenceFromResource(const std::string& sResourceName)
{
	std::string sXMLString = readResource(sResourceName);
	auto pSequence = callOperationSequenceCore([&]() { return m_pRegistry->parseSequence(sXMLString, AMC::eOperationSequenceOrigin::Resource); });
	return wrapSequence(pSequence);
}

IOperationSequence* COperationSequenceHandler::ParseSequenceFromXMLDocument(LibMCEnv::Impl::IXMLDocument* pDocument, const std::string& sNodePath)
{
	if (pDocument == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	std::string sXMLString = pDocument->SaveToString(false);

	pugi::xml_document document;
	pugi::xml_parse_result parseResult = document.load_string(sXMLString.c_str());
	if (!parseResult)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDOPERATIONSEQUENCEXML, parseResult.description());

	pugi::xml_node currentNode = document.document_element();

	std::vector<std::string> pathComponents;
	size_t nStart = 0;
	while (nStart <= sNodePath.length()) {
		size_t nEnd = sNodePath.find('/', nStart);
		if (nEnd == std::string::npos)
			nEnd = sNodePath.length();
		std::string sComponent = sNodePath.substr(nStart, nEnd - nStart);
		if (!sComponent.empty())
			pathComponents.push_back(sComponent);
		nStart = nEnd + 1;
	}

	size_t nComponentIndex = 0;
	if ((!pathComponents.empty()) && (operationSequenceLocalName(currentNode.name()) == pathComponents.front()))
		nComponentIndex = 1;

	for (; nComponentIndex < pathComponents.size(); nComponentIndex++) {
		pugi::xml_node childNode;
		for (pugi::xml_node candidateNode : currentNode.children()) {
			if ((candidateNode.type() == pugi::node_element) && (operationSequenceLocalName(candidateNode.name()) == pathComponents.at(nComponentIndex))) {
				childNode = candidateNode;
				break;
			}
		}

		if (childNode.empty())
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDOPERATIONSEQUENCEXML, "node not found: " + sNodePath);
		currentNode = childNode;
	}

	auto pSequence = callOperationSequenceCore([&]() { return AMC::COperationSequence::parseFromXMLNode(currentNode, AMC::eOperationSequenceOrigin::Document); });
	return wrapSequence(pSequence);
}

IOperationSequenceValidationResult* COperationSequenceHandler::ValidateSequence(IOperationSequence* pSequence)
{
	auto pSequenceInstance = castSequence(pSequence);
	auto pResult = callOperationSequenceCore([&]() { return m_pRegistry->validateSequence(pSequenceInstance->getSequence()); });
	return new COperationSequenceValidationResult(pResult);
}

IOperationSequence* COperationSequenceHandler::PublishSequence(IOperationSequence* pWorkingCopy, const std::string& sUserUUID, const bool bOverwriteNewerRevision)
{
	auto pSequenceInstance = castSequence(pWorkingCopy);
	auto pPublished = callOperationSequenceCore([&]() { return m_pRegistry->publishSequence(pSequenceInstance->getSequence(), sUserUUID, bOverwriteNewerRevision); });
	return wrapSequence(pPublished);
}

IOperationSequence* COperationSequenceHandler::EnsureSequenceFromResource(const std::string& sSequenceName, const std::string& sResourceName)
{
	std::string sXMLString = readResource(sResourceName);
	auto pSequence = callOperationSequenceCore([&]() { return m_pRegistry->ensureSequence(sSequenceName, sResourceName, sXMLString); });
	return wrapSequence(pSequence);
}

bool COperationSequenceHandler::HasSequence(const std::string& sSequenceName)
{
	return m_pRegistry->hasSequence(sSequenceName);
}

IOperationSequence* COperationSequenceHandler::GetSequence(const std::string& sSequenceName)
{
	auto pSequence = callOperationSequenceCore([&]() { return m_pRegistry->getSequence(sSequenceName); });
	return wrapSequence(pSequence);
}

IOperationSequence* COperationSequenceHandler::GetSequenceRevision(const std::string& sSequenceName, const LibMCEnv_uint32 nRevision)
{
	auto pSequence = callOperationSequenceCore([&]() { return m_pRegistry->getSequenceRevision(sSequenceName, nRevision); });
	return wrapSequence(pSequence);
}

IOperationSequenceIterator* COperationSequenceHandler::ListSequences()
{
	std::unique_ptr<COperationSequenceIterator> pIterator(new COperationSequenceIterator());
	for (auto& pSequence : m_pRegistry->listSequences())
		pIterator->addSequence(std::make_shared<COperationSequence>(pSequence, m_pRegistry, m_WorkingCopyOrigin));
	return pIterator.release();
}

IOperationSequenceIterator* COperationSequenceHandler::ListSequenceRevisions(const std::string& sSequenceName)
{
	auto revisions = callOperationSequenceCore([&]() { return m_pRegistry->listSequenceRevisions(sSequenceName); });

	std::unique_ptr<COperationSequenceIterator> pIterator(new COperationSequenceIterator());
	for (auto& pSequence : revisions)
		pIterator->addSequence(std::make_shared<COperationSequence>(pSequence, m_pRegistry, m_WorkingCopyOrigin));
	return pIterator.release();
}

IOperationSequence* COperationSequenceHandler::FindSequenceRevisionByUUID(const std::string& sRevisionUUID, const bool bFailIfNotExisting)
{
	auto pSequence = callOperationSequenceCore([&]() { return m_pRegistry->findSequenceRevisionByUUID(sRevisionUUID, bFailIfNotExisting); });
	return wrapSequence(pSequence);
}

void COperationSequenceHandler::ValidateDispatcherTransitions(const std::string& sDispatcherState)
{
	callOperationSequenceCore([&]() { m_pRegistry->validateDispatcherTransitions(sDispatcherState); });
}

void COperationSequenceHandler::SetStatusParameterGroup(const std::string& sGroupName, const std::string& sValuePrefix)
{
	callOperationSequenceCore([&]() { m_pRegistry->setStatusParameterGroup(sGroupName, sValuePrefix); });
}


/*************************************************************************************************************************
 Class definition of CDispatchedOperation
**************************************************************************************************************************/

CDispatchedOperation::CDispatchedOperation(AMC::PDispatchedOperation pOperation)
	: m_pOperation(pOperation)
{
	if (pOperation.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

CDispatchedOperation::~CDispatchedOperation()
{
}

std::string CDispatchedOperation::GetUUID()
{
	return m_pOperation->getUUID();
}

std::string CDispatchedOperation::GetTypeName()
{
	return m_pOperation->getTypeName();
}

LibMCEnv_uint32 CDispatchedOperation::GetIndex()
{
	return m_pOperation->getIndex();
}

std::string CDispatchedOperation::GetExecutionName()
{
	return m_pOperation->getExecutionName();
}

std::string CDispatchedOperation::GetSequenceName()
{
	return m_pOperation->getSequenceName();
}

LibMCEnv_uint32 CDispatchedOperation::GetRevision()
{
	return m_pOperation->getRevision();
}

std::string CDispatchedOperation::GetComment()
{
	return m_pOperation->getComment();
}

std::string CDispatchedOperation::GetTargetState()
{
	return m_pOperation->getTargetState();
}

std::string CDispatchedOperation::GetReturnState()
{
	return m_pOperation->getReturnState();
}

bool CDispatchedOperation::HasExplicitValue(const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->hasExplicitValue(sName); });
}

std::string CDispatchedOperation::GetStringValue(const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->getStringValue(sName); });
}

LibMCEnv_int64 CDispatchedOperation::GetIntegerValue(const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->getIntegerValue(sName); });
}

LibMCEnv_double CDispatchedOperation::GetDoubleValue(const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->getDoubleValue(sName); });
}

bool CDispatchedOperation::GetBoolValue(const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->getBoolValue(sName); });
}

LibMCEnv_double CDispatchedOperation::ReevaluateDoubleValue(const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->reevaluateDoubleValue(sName); });
}

LibMCEnv_int64 CDispatchedOperation::ReevaluateIntegerValue(const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->reevaluateIntegerValue(sName); });
}

LibMCEnv_uint32 CDispatchedOperation::GetItemCount(const std::string& sListName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->getItemCount(sListName); });
}

std::string CDispatchedOperation::GetItemStringValue(const std::string& sListName, const LibMCEnv_uint32 nIndex, const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->getItemStringValue(sListName, nIndex, sName); });
}

LibMCEnv_int64 CDispatchedOperation::GetItemIntegerValue(const std::string& sListName, const LibMCEnv_uint32 nIndex, const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->getItemIntegerValue(sListName, nIndex, sName); });
}

LibMCEnv_double CDispatchedOperation::GetItemDoubleValue(const std::string& sListName, const LibMCEnv_uint32 nIndex, const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->getItemDoubleValue(sListName, nIndex, sName); });
}

bool CDispatchedOperation::GetItemBoolValue(const std::string& sListName, const LibMCEnv_uint32 nIndex, const std::string& sName)
{
	return callOperationSequenceCore([&]() { return m_pOperation->getItemBoolValue(sListName, nIndex, sName); });
}

void CDispatchedOperation::MarkFailed(const std::string& sMessage)
{
	callOperationSequenceCore([&]() { m_pOperation->markFailed(sMessage); });
}


/*************************************************************************************************************************
 Class definition of COperationSequenceExecution
**************************************************************************************************************************/

COperationSequenceExecution::COperationSequenceExecution(AMC::POperationSequenceExecution pExecution, AMC::POperationSequenceRegistry pRegistry)
	: m_pExecution(pExecution), m_pRegistry(pRegistry)
{
	if ((pExecution.get() == nullptr) || (pRegistry.get() == nullptr))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

COperationSequenceExecution::~COperationSequenceExecution()
{
}

std::string COperationSequenceExecution::GetName()
{
	return m_pExecution->getName();
}

IOperationSequence* COperationSequenceExecution::GetSequence()
{
	return new COperationSequence(m_pExecution->getSequence(), m_pRegistry, AMC::eOperationSequenceOrigin::Code);
}

std::string COperationSequenceExecution::GetBindingGroup()
{
	return m_pExecution->getBindingGroup();
}

void COperationSequenceExecution::SetBindingGroup(const std::string& sGroupName)
{
	callOperationSequenceCore([&]() { m_pExecution->setBindingGroup(sGroupName); });
}

std::string COperationSequenceExecution::GetReturnState()
{
	return m_pExecution->getReturnState();
}

LibMCEnv::eOperationSequenceExecutionStatus COperationSequenceExecution::GetStatus()
{
	return (LibMCEnv::eOperationSequenceExecutionStatus)m_pExecution->getStatus();
}

LibMCEnv_uint32 COperationSequenceExecution::GetNextOperationIndex()
{
	return m_pExecution->getNextOperationIndex();
}

LibMCEnv_uint32 COperationSequenceExecution::GetIterationCount()
{
	return m_pExecution->getIterationCount();
}

IDispatchedOperation* COperationSequenceExecution::DispatchNextOperation()
{
	auto pOperation = callOperationSequenceCore([&]() { return m_pExecution->dispatchNextOperation(); });
	if (pOperation.get() == nullptr)
		return nullptr;

	return new CDispatchedOperation(pOperation);
}

void COperationSequenceExecution::Restart()
{
	callOperationSequenceCore([&]() { m_pExecution->restart(); });
}

void COperationSequenceExecution::Abort(const std::string& sReason)
{
	callOperationSequenceCore([&]() { m_pExecution->abort(sReason); });
}
