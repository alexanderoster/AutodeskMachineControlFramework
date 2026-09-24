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

#include "amc_ui_frontenddefinition.hpp"
#include "amc_parametergroup.hpp"
#include "libmc_exceptiontypes.hpp"
#include "common_utils.hpp"

using namespace AMC;

#define AMC_UI_SESSIONVARIABLES_GROUPNAME "session"



CUIFrontendDefinitionAttribute::CUIFrontendDefinitionAttribute(const std::string& sName, eUIFrontendDefinitionAttributeType attributeType)
	: m_sName(sName), m_AttributeType(attributeType)
{
	if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString(sName))
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDFRONTENDATTRIBUTENAME, sName);
}

CUIFrontendDefinitionAttribute::~CUIFrontendDefinitionAttribute()
{

}


std::string CUIFrontendDefinitionAttribute::getName()
{
	return m_sName;
}

eUIFrontendDefinitionAttributeType CUIFrontendDefinitionAttribute::getAttributeType()
{
	return m_AttributeType;
}

std::string CUIFrontendDefinitionAttribute::getSessionReference()
{
	return "";
}

CUIFrontendDefinitionExpressionAttribute::CUIFrontendDefinitionExpressionAttribute(const std::string& sName, eUIFrontendDefinitionAttributeType attributeType, const CUIExpression& valueExpression)
	: CUIFrontendDefinitionAttribute(sName, attributeType), m_ValueExpression(valueExpression)
{
}

CUIFrontendDefinitionExpressionAttribute::~CUIFrontendDefinitionExpressionAttribute()
{

}

void CUIFrontendDefinitionExpressionAttribute::writeToFrontendJSON(CJSONWriter& writer, CJSONWriterObject& attributesObject, CStateMachineData* pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{

	switch (getAttributeType()) {
		case eUIFrontendDefinitionAttributeType::atBoolean: {
			bool bValue = m_ValueExpression.evaluateBoolValue (pStateMachineData, pSessionContext);
			attributesObject.addBool(getName(), bValue);
			break;
		}

		case eUIFrontendDefinitionAttributeType::atString: {
			std::string sValue = m_ValueExpression.evaluateStringValue(pStateMachineData, pSessionContext);
			attributesObject.addString(getName(), sValue);
			break;
		}

		case eUIFrontendDefinitionAttributeType::atNumber: {
			double dValue = m_ValueExpression.evaluateNumberValue(pStateMachineData, pSessionContext);
			attributesObject.addDouble(getName(), dValue);
			break;
		}

		case eUIFrontendDefinitionAttributeType::atInteger: {
			int64_t nValue = m_ValueExpression.evaluateIntegerValue(pStateMachineData, pSessionContext);
			attributesObject.addInteger(getName(), nValue);
			break;
		}

		case eUIFrontendDefinitionAttributeType::atUUID: {
			std::string sValue = m_ValueExpression.evaluateUUIDValue(pStateMachineData, pSessionContext);
			attributesObject.addString(getName(), sValue);
			break;
		}

	}
}

std::string CUIFrontendDefinitionExpressionAttribute::getSessionReference()
{
	return m_ValueExpression.getSessionReference();
}


CUIFrontendDefinitionModuleStore::CUIFrontendDefinitionModuleStore(const std::string& sModuleUUID, const std::string& sModulePath, const std::string& sModuleType)
	: m_sUUID(AMCCommon::CUtils::normalizeUUIDString(sModuleUUID)), m_sPath(sModulePath), m_sModuleType(sModuleType)
{
	if (!AMCCommon::CUtils::stringIsValidAlphanumericPathString (sModulePath))
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDFRONTENDMODULEPATH, sModulePath);

}

CUIFrontendDefinitionModuleStore::~CUIFrontendDefinitionModuleStore()
{

}

PUIFrontendDefinitionAttribute CUIFrontendDefinitionModuleStore::registerValue (const std::string& sName, eUIFrontendDefinitionAttributeType attributeType, const CUIExpression& valueExpression)
{
	if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString(sName))
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDFRONTENDATTRIBUTENAME, sName);

	if (m_Attributes.find(sName) != m_Attributes.end())
		throw ELibMCCustomException(LIBMC_ERROR_DUPLICATEFRONTENDATTRIBUTENAME, sName);

	auto pAttribute = std::make_shared<CUIFrontendDefinitionExpressionAttribute>(sName, attributeType, valueExpression);
	m_Attributes.insert(std::make_pair(sName, pAttribute));

	return pAttribute;
}


std::vector<PUIFrontendDefinitionAttribute> CUIFrontendDefinitionModuleStore::getAttributes()
{
	std::vector<PUIFrontendDefinitionAttribute> attributes;
	for (auto attributePair : m_Attributes) {
		attributes.push_back(attributePair.second);
	}
	return attributes;
}


PUIFrontendDefinitionModuleStore CUIFrontendDefinitionModuleStore::addChildStore(const std::string& sChildUUID, const std::string& sChildPath, const std::string& sChildModuleType)
{
	auto pChildStore = std::make_shared<CUIFrontendDefinitionModuleStore>(sChildUUID, sChildPath, sChildModuleType);
	m_ChildStores.push_back(pChildStore);
	return pChildStore;
}

std::vector<PUIFrontendDefinitionModuleStore> CUIFrontendDefinitionModuleStore::getChildStores()
{
	return m_ChildStores;
}

bool CUIFrontendDefinitionModuleStore::hasChildren()
{
	return !m_ChildStores.empty();
}

std::string CUIFrontendDefinitionModuleStore::getModuleType()
{
	return m_sModuleType;
}

std::string CUIFrontendDefinitionModuleStore::getUUID()
{
	return m_sUUID;
}

void CUIFrontendDefinitionModuleStore::collectSessionReferences(std::vector<std::string>& references)
{
	for (auto& attributePair : m_Attributes) {
		std::string sReference = attributePair.second->getSessionReference();
		if (!sReference.empty())
			references.push_back(sReference);
	}

	for (auto& pChildStore : m_ChildStores)
		pChildStore->collectSessionReferences(references);
}


CUIFrontendDefinition::CUIFrontendDefinition(AMCCommon::PChrono pGlobalChrono)
	: m_pGlobalChrono (pGlobalChrono), m_nSessionVariableBroadcastCounter (0)
{
	if (pGlobalChrono.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	m_pSessionVariableDeclarations = std::make_shared<CParameterGroup>(AMC_UI_SESSIONVARIABLES_GROUPNAME, "Session variables", pGlobalChrono);
	m_pSessionVariableBroadcasts = std::make_shared<CParameterGroup>(AMC_UI_SESSIONVARIABLES_GROUPNAME, "Session variable broadcasts", pGlobalChrono);
}

CUIFrontendDefinition::~CUIFrontendDefinition()
{

}

PUIFrontendDefinitionModuleStore CUIFrontendDefinition::registerModuleStore(const std::string& sModuleUUID, const std::string& sPath, const std::string& sModuleType)
{
	auto pModuleStore = std::make_shared<CUIFrontendDefinitionModuleStore>(sModuleUUID, sPath, sModuleType);
	m_ModuleStores.push_back(pModuleStore);
	return pModuleStore;

}


AMCCommon::PChrono CUIFrontendDefinition::getGlobalChrono()
{
	return m_pGlobalChrono;
}

void CUIFrontendDefinition::addSessionVariable(const std::string& sName, const std::string& sType, const std::string& sDescription, const std::string& sDefaultValue)
{
	if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString(sName))
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDSESSIONVARIABLENAME, sName);

	if (m_pSessionVariableDeclarations->hasParameter(sName))
		throw ELibMCCustomException(LIBMC_ERROR_DUPLICATESESSIONVARIABLE, sName);

	m_pSessionVariableDeclarations->addNewTypedParameter(sName, sType, sDescription, sDefaultValue, "");

	std::lock_guard<std::mutex> lockGuard(m_BroadcastMutex);
	m_pSessionVariableBroadcasts->addNewTypedParameter(sName, sType, sDescription, sDefaultValue, "");
}

bool CUIFrontendDefinition::hasSessionVariable(const std::string& sName)
{
	return m_pSessionVariableDeclarations->hasParameter(sName);
}

PParameterGroup CUIFrontendDefinition::getSessionVariableDeclarations()
{
	return m_pSessionVariableDeclarations;
}

void CUIFrontendDefinition::broadcastSessionVariable(const std::string& sName, const std::string& sValue)
{
	std::lock_guard<std::mutex> lockGuard(m_BroadcastMutex);
	if (!m_pSessionVariableBroadcasts->hasParameter(sName))
		throw ELibMCCustomException(LIBMC_ERROR_SESSIONVARIABLENOTFOUND, sName);

	m_pSessionVariableBroadcasts->setParameterValueByName(sName, sValue);
	m_nSessionVariableBroadcastCounter++;
	m_SessionVariableBroadcastCounters[sName] = m_nSessionVariableBroadcastCounter;
}

void CUIFrontendDefinition::broadcastSessionVariableAsDouble(const std::string& sName, double dValue)
{
	std::lock_guard<std::mutex> lockGuard(m_BroadcastMutex);
	if (!m_pSessionVariableBroadcasts->hasParameter(sName))
		throw ELibMCCustomException(LIBMC_ERROR_SESSIONVARIABLENOTFOUND, sName);

	m_pSessionVariableBroadcasts->setDoubleParameterValueByName(sName, dValue);
	m_nSessionVariableBroadcastCounter++;
	m_SessionVariableBroadcastCounters[sName] = m_nSessionVariableBroadcastCounter;
}

void CUIFrontendDefinition::broadcastSessionVariableAsInteger(const std::string& sName, int64_t nValue)
{
	std::lock_guard<std::mutex> lockGuard(m_BroadcastMutex);
	if (!m_pSessionVariableBroadcasts->hasParameter(sName))
		throw ELibMCCustomException(LIBMC_ERROR_SESSIONVARIABLENOTFOUND, sName);

	m_pSessionVariableBroadcasts->setIntParameterValueByName(sName, nValue);
	m_nSessionVariableBroadcastCounter++;
	m_SessionVariableBroadcastCounters[sName] = m_nSessionVariableBroadcastCounter;
}

void CUIFrontendDefinition::broadcastSessionVariableAsBool(const std::string& sName, bool bValue)
{
	std::lock_guard<std::mutex> lockGuard(m_BroadcastMutex);
	if (!m_pSessionVariableBroadcasts->hasParameter(sName))
		throw ELibMCCustomException(LIBMC_ERROR_SESSIONVARIABLENOTFOUND, sName);

	m_pSessionVariableBroadcasts->setBoolParameterValueByName(sName, bValue);
	m_nSessionVariableBroadcastCounter++;
	m_SessionVariableBroadcastCounters[sName] = m_nSessionVariableBroadcastCounter;
}

uint64_t CUIFrontendDefinition::getSessionVariableBroadcastCounter()
{
	std::lock_guard<std::mutex> lockGuard(m_BroadcastMutex);
	return m_nSessionVariableBroadcastCounter;
}

void CUIFrontendDefinition::getSessionVariableBroadcastsSince(uint64_t nSinceCounter, std::vector<std::pair<std::string, std::string>>& values)
{
	std::lock_guard<std::mutex> lockGuard(m_BroadcastMutex);
	for (auto& counterPair : m_SessionVariableBroadcastCounters) {
		if (counterPair.second > nSinceCounter)
			values.push_back(std::make_pair(counterPair.first, m_pSessionVariableBroadcasts->getParameterValueByName(counterPair.first)));
	}
}

void CUIFrontendDefinition::collectSessionReferences(std::vector<std::string>& references)
{
	for (auto& pModuleStore : m_ModuleStores)
		pModuleStore->collectSessionReferences(references);
}

