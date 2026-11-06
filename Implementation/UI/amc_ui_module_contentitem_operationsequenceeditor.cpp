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

#define __AMCIMPL_UI_MODULE
#define __AMCIMPL_API_CONSTANTS

#include "amc_ui_module_contentitem_operationsequenceeditor.hpp"
#include "amc_ui_module.hpp"
#include "amc_ui_interfaces.hpp"
#include "amc_api_jsonrequest.hpp"
#include "amc_api_auth.hpp"
#include "amc_api_constants.hpp"
#include "amc_accesscontrol.hpp"
#include "amc_userinformation.hpp"
#include "amc_resourcepackage.hpp"
#include "amc_parameterhandler.hpp"
#include "amc_parametergroup.hpp"
#include "amc_operationvalues.hpp"
#include "amc_ui_frontendstate.hpp"

#include "libmc_exceptiontypes.hpp"
#include "libmcdata_dynamic.hpp"
#include "common_utils.hpp"

#include "RapidJSON/writer.h"
#include "RapidJSON/stringbuffer.h"

#define AMC_UI_OPERATIONSEQUENCEEDITOR_SAVEDSEQUENCE "savedsequence"
#define AMC_UI_OPERATIONSEQUENCEEDITOR_SAVEDREVISION "savedrevision"

using namespace AMC;

static std::vector<std::string> splitOperationSequenceList(const std::string& sList)
{
	std::vector<std::string> entries;
	size_t nStart = 0;
	while (nStart <= sList.length()) {
		size_t nEnd = sList.find(',', nStart);
		if (nEnd == std::string::npos)
			nEnd = sList.length();
		std::string sEntry = AMCCommon::CUtils::trimString(sList.substr(nStart, nEnd - nStart));
		if (!sEntry.empty())
			entries.push_back(sEntry);
		nStart = nEnd + 1;
	}
	return entries;
}

static std::string jsonValueToRawString(const rapidjson::Value& value, const std::string& sContext)
{
	if (value.IsString())
		return value.GetString();
	if (value.IsBool())
		return value.GetBool() ? "1" : "0";
	if (value.IsInt64())
		return std::to_string(value.GetInt64());
	if (value.IsUint64())
		return std::to_string(value.GetUint64());
	if (value.IsNumber())
		return COperationValues::formatDouble(value.GetDouble());
	if (value.IsNull())
		return "";

	throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, sContext + " must be a string");
}

static std::string jsonMemberString(const rapidjson::Value& objectValue, const char* pszName)
{
	auto iIter = objectValue.FindMember(pszName);
	if (iIter == objectValue.MemberEnd())
		return "";
	return jsonValueToRawString(iIter->value, pszName);
}

static void parseJSONValueList(const rapidjson::Value& valuesObject, COperationValueList& values, const std::string& sContext)
{
	if (!valuesObject.IsObject())
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, sContext + " must be an object");

	for (auto iIter = valuesObject.MemberBegin(); iIter != valuesObject.MemberEnd(); iIter++) {
		std::string sName = iIter->name.GetString();
		if (!COperationParameterContainer::isValidIdentifier(sName) || COperationParameterContainer::isReservedAttributeName(sName))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, sContext + ": invalid parameter name \"" + sName + "\"");

		values.setValue(sName, jsonValueToRawString(iIter->value, sContext + "." + sName));
	}
}

static void writeValueListToJSON(CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName, const COperationValueList& values)
{
	CJSONWriterObject valuesObject(writer);
	for (auto& value : values.getValues())
		valuesObject.addString(value.first, value.second);
	object.addObject(sName, valuesObject);
}

static void writeRevisionSummaryToJSON(CJSONWriterObject& object, COperationSequence* pSequence)
{
	object.addInteger("revision", pSequence->getRevision());
	object.addString("uuid", pSequence->getRevisionUUID());
	object.addString("timestamp", pSequence->getTimestamp());
	object.addString("user", pSequence->getUserUUID());
	object.addString("origin", COperationSequence::originToString(pSequence->getOrigin()));
}


PUIModule_ContentOperationSequenceEditor CUIModule_ContentOperationSequenceEditor::makeFromXML(const pugi::xml_node& xmlNode, const std::string& sItemName, const std::string& sModulePath, PUIModuleEnvironment pUIModuleEnvironment)
{
	LibMCAssertNotNull(pUIModuleEnvironment);

	auto statemachineAttrib = xmlNode.attribute("statemachine");
	std::string sStateMachineName = statemachineAttrib.as_string();
	if (sStateMachineName.empty())
		throw ELibMCCustomException(LIBMC_ERROR_MISSINGOPERATIONSEQUENCEEDITORSTATEMACHINE, sItemName);

	auto pItem = std::make_shared<CUIModule_ContentOperationSequenceEditor>(AMCCommon::CUtils::createUUID(), sItemName, sModulePath, pUIModuleEnvironment);
	pItem->setStateMachineName(sStateMachineName);
	pItem->setSequenceFilter(splitOperationSequenceList(xmlNode.attribute("sequences").as_string()));
	pItem->setEditPermission(xmlNode.attribute("editpermission").as_string());
	pItem->setHighlightExecution(xmlNode.attribute("highlightexecution").as_string());
	pItem->setSaveEvent(xmlNode.attribute("saveevent").as_string());

	return pItem;
}

CUIModule_ContentOperationSequenceEditor::CUIModule_ContentOperationSequenceEditor(const std::string& sUUID, const std::string& sItemName, const std::string& sModulePath, PUIModuleEnvironment pUIModuleEnvironment)
	: CUIModule_ContentItem(sUUID, sItemName, sModulePath)
{
	LibMCAssertNotNull(pUIModuleEnvironment);

	m_pOperationSequenceHandler = pUIModuleEnvironment->operationSequenceHandler();
	m_pAccessControl = pUIModuleEnvironment->accessControl();
	m_pResourcePackage = pUIModuleEnvironment->resourcePackage();
	m_pDataModel = pUIModuleEnvironment->dataModel();

	LibMCAssertNotNull(m_pOperationSequenceHandler.get());
	LibMCAssertNotNull(m_pAccessControl.get());
}

CUIModule_ContentOperationSequenceEditor::~CUIModule_ContentOperationSequenceEditor()
{
}

void CUIModule_ContentOperationSequenceEditor::setStateMachineName(const std::string& sStateMachineName)
{
	m_sStateMachineName = sStateMachineName;
}

void CUIModule_ContentOperationSequenceEditor::setSequenceFilter(const std::vector<std::string>& sequenceFilter)
{
	m_SequenceFilter = sequenceFilter;
}

void CUIModule_ContentOperationSequenceEditor::setEditPermission(const std::string& sEditPermission)
{
	m_sEditPermission = sEditPermission;
}

void CUIModule_ContentOperationSequenceEditor::setHighlightExecution(const std::string& sHighlightExecution)
{
	m_sHighlightExecution = sHighlightExecution;
}

void CUIModule_ContentOperationSequenceEditor::setSaveEvent(const std::string& sSaveEvent)
{
	m_sSaveEvent = sSaveEvent;
}

POperationSequenceRegistry CUIModule_ContentOperationSequenceEditor::getRegistry()
{
	return m_pOperationSequenceHandler->findRegistry(m_sStateMachineName, true);
}

bool CUIModule_ContentOperationSequenceEditor::sequenceIsVisible(const std::string& sSequenceName)
{
	if (m_SequenceFilter.empty())
		return true;

	for (auto& sEntry : m_SequenceFilter)
		if (sEntry == sSequenceName)
			return true;

	return false;
}

void CUIModule_ContentOperationSequenceEditor::checkSequenceIsVisible(const std::string& sSequenceName)
{
	if (!sequenceIsVisible(sSequenceName))
		throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCENOTFOUND, m_sStateMachineName + "." + sSequenceName);
}

bool CUIModule_ContentOperationSequenceEditor::userCanEdit(PAPIAuth pAuth)
{
	if (pAuth.get() == nullptr)
		return false;

	auto pUserInformation = pAuth->getUserInformation();
	if (pUserInformation.get() == nullptr)
		return false;

	if (m_sEditPermission.empty())
		return true;

	return m_pAccessControl->checkPermissionInRole(pUserInformation->getRoleIdentifier(), m_sEditPermission);
}

std::string CUIModule_ContentOperationSequenceEditor::resolveUserName(const std::string& sUserUUID)
{
	if ((m_pDataModel.get() == nullptr) || sUserUUID.empty() || (sUserUUID == AMCCommon::CUtils::createEmptyUUID()))
		return "";

	try {
		auto pLoginHandler = m_pDataModel->CreateLoginHandler();
		return pLoginHandler->GetUsernameByUUID(sUserUUID);
	}
	catch (std::exception&) {
		return "";
	}
}

std::string CUIModule_ContentOperationSequenceEditor::resolveIconResource(const std::string& sResourceName)
{
	if (sResourceName.empty() || (m_pResourcePackage.get() == nullptr))
		return "";

	auto pEntry = m_pResourcePackage->findEntryByName(sResourceName, false);
	if (pEntry.get() == nullptr)
		return "";

	return pEntry->getUUID();
}

void CUIModule_ContentOperationSequenceEditor::populateClientVariables(CParameterHandler* pClientVariableHandler)
{
	LibMCAssertNotNull(pClientVariableHandler);
	auto pGroup = pClientVariableHandler->addGroup(getItemPath(), "operation sequence editor");
	pGroup->addNewStringParameter(AMC_UI_OPERATIONSEQUENCEEDITOR_SAVEDSEQUENCE, "Name of the sequence saved last", "");
	pGroup->addNewIntParameter(AMC_UI_OPERATIONSEQUENCEEDITOR_SAVEDREVISION, "Revision saved last", 0);
}

std::string CUIModule_ContentOperationSequenceEditor::findElementPathByUUID(const std::string& sUUID)
{
	if (sUUID == m_sUUID)
		return getItemPath();
	return "";
}

std::string CUIModule_ContentOperationSequenceEditor::getItemType()
{
	return "operationsequenceeditor";
}

void CUIModule_ContentOperationSequenceEditor::registerFrontendAttributes()
{
	CUIExpression stateMachineExpression;
	stateMachineExpression.setFixedValue(m_sStateMachineName);
	registerItemStringAttribute("statemachine", stateMachineExpression);

	registerItemProviderAttribute("catalog", eUIFrontendDefinitionAttributeType::atArray, eUIFrontendDefinitionAttributeScope::asGlobal,
		[this](CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName, CStateMachineData* pStateMachineData, CUIFrontendState* pFrontendState) {
			writeCatalogToJSON(writer, object, sName);
		});

	registerItemProviderAttribute("sequences", eUIFrontendDefinitionAttributeType::atArray, eUIFrontendDefinitionAttributeScope::asGlobal,
		[this](CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName, CStateMachineData* pStateMachineData, CUIFrontendState* pFrontendState) {
			writeSequencesToJSON(writer, object, sName);
		});

	registerItemProviderAttribute("references", eUIFrontendDefinitionAttributeType::atObject, eUIFrontendDefinitionAttributeScope::asGlobal,
		[this](CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName, CStateMachineData* pStateMachineData, CUIFrontendState* pFrontendState) {
			writeReferencesToJSON(writer, object, sName);
		});

	registerItemProviderAttribute("activeoperation", eUIFrontendDefinitionAttributeType::atString, eUIFrontendDefinitionAttributeScope::asGlobal,
		[this](CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName, CStateMachineData* pStateMachineData, CUIFrontendState* pFrontendState) {
			std::string sOperationUUID;
			auto pRegistry = m_pOperationSequenceHandler->findRegistry(m_sStateMachineName, false);
			if ((pRegistry.get() != nullptr) && !m_sHighlightExecution.empty()) {
				auto pExecution = pRegistry->findExecution(m_sHighlightExecution, false);
				if ((pExecution.get() != nullptr) && (pExecution->getStatus() == eOperationSequenceExecutionStatus::Running))
					sOperationUUID = pExecution->getCurrentOperationUUID();
			}
			object.addString(sName, sOperationUUID);
		});

	registerItemProviderAttribute("activesequence", eUIFrontendDefinitionAttributeType::atString, eUIFrontendDefinitionAttributeScope::asGlobal,
		[this](CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName, CStateMachineData* pStateMachineData, CUIFrontendState* pFrontendState) {
			std::string sSequenceName;
			auto pRegistry = m_pOperationSequenceHandler->findRegistry(m_sStateMachineName, false);
			if ((pRegistry.get() != nullptr) && !m_sHighlightExecution.empty()) {
				auto pExecution = pRegistry->findExecution(m_sHighlightExecution, false);
				if ((pExecution.get() != nullptr) && (pExecution->getStatus() == eOperationSequenceExecutionStatus::Running))
					sSequenceName = pExecution->getSequence()->getName();
			}
			object.addString(sName, sSequenceName);
		});

	registerItemProviderAttribute("activerevision", eUIFrontendDefinitionAttributeType::atInteger, eUIFrontendDefinitionAttributeScope::asGlobal,
		[this](CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName, CStateMachineData* pStateMachineData, CUIFrontendState* pFrontendState) {
			int64_t nRevision = 0;
			auto pRegistry = m_pOperationSequenceHandler->findRegistry(m_sStateMachineName, false);
			if ((pRegistry.get() != nullptr) && !m_sHighlightExecution.empty()) {
				auto pExecution = pRegistry->findExecution(m_sHighlightExecution, false);
				if ((pExecution.get() != nullptr) && (pExecution->getStatus() == eOperationSequenceExecutionStatus::Running))
					nRevision = pExecution->getSequence()->getRevision();
			}
			object.addInteger(sName, nRevision);
		});

	registerItemProviderAttribute("canedit", eUIFrontendDefinitionAttributeType::atBoolean, eUIFrontendDefinitionAttributeScope::asSession,
		[this](CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName, CStateMachineData* pStateMachineData, CUIFrontendState* pFrontendState) {
			bool bCanEdit = true;
			if (!m_sEditPermission.empty())
				bCanEdit = (pFrontendState != nullptr) && pFrontendState->evaluateSessionReferenceAsBool("$user.permission." + m_sEditPermission);
			object.addBool(sName, bCanEdit);
		});
}

void CUIModule_ContentOperationSequenceEditor::writeParameterDefinitionsToJSON(CJSONWriter& writer, CJSONWriterObject& object, COperationParameterContainer* pContainer)
{
	CJSONWriterArray parametersArray(writer);
	for (auto& pDefinition : pContainer->getParameters()) {
		CJSONWriterObject parameterObject(writer);
		parameterObject.addString("name", pDefinition->getName());
		parameterObject.addString("displayname", pDefinition->getDisplayName());
		if (!pDefinition->getDescription().empty())
			parameterObject.addString("description", pDefinition->getDescription());
		parameterObject.addString("type", COperationParameterDefinition::parameterTypeToString(pDefinition->getParameterType()));
		if (!pDefinition->getUnit().empty())
			parameterObject.addString("unit", pDefinition->getUnit());
		parameterObject.addString("default", pDefinition->getDefaultValue());

		if (pDefinition->hasRange()) {
			double dMinimum, dMaximum;
			pDefinition->getRange(dMinimum, dMaximum);
			parameterObject.addDouble("min", dMinimum);
			parameterObject.addDouble("max", dMaximum);
		}

		parameterObject.addString("expression", COperationParameterDefinition::expressionModeToString(pDefinition->getExpressionMode()));
		parameterObject.addBool("required", pDefinition->isRequired());

		if (pDefinition->getParameterType() == eOperationParameterType::Enum) {
			CJSONWriterArray optionsArray(writer);
			for (auto& option : pDefinition->getEnumOptions()) {
				CJSONWriterObject optionObject(writer);
				optionObject.addString("value", option.first);
				optionObject.addString("displayname", option.second);
				optionsArray.addObject(optionObject);
			}
			parameterObject.addArray("options", optionsArray);
		}

		parametersArray.addObject(parameterObject);
	}
	object.addArray("parameters", parametersArray);
}

void CUIModule_ContentOperationSequenceEditor::writeCatalogToJSON(CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName)
{
	CJSONWriterArray catalogArray(writer);

	auto pRegistry = m_pOperationSequenceHandler->findRegistry(m_sStateMachineName, false);
	if (pRegistry.get() != nullptr) {
		for (auto& pType : pRegistry->listOperationTypes()) {
			CJSONWriterObject typeObject(writer);
			typeObject.addString("type", pType->getName());
			typeObject.addString("displayname", pType->getDisplayName());
			if (!pType->getDescription().empty())
				typeObject.addString("description", pType->getDescription());
			typeObject.addString("category", pType->getCategory());

			std::string sIconUUID = resolveIconResource(pType->getIconResource());
			if (!sIconUUID.empty())
				typeObject.addString("icon", sIconUUID);

			CJSONWriterArray contextsArray(writer);
			for (auto& sContext : pType->getAllowedContextSet())
				contextsArray.addString(sContext);
			typeObject.addArray("contexts", contextsArray);

			writeParameterDefinitionsToJSON(writer, typeObject, pType.get());

			CJSONWriterArray itemListsArray(writer);
			for (auto& pItemList : pType->getItemLists()) {
				CJSONWriterObject itemListObject(writer);
				itemListObject.addString("name", pItemList->getName());
				itemListObject.addString("displayname", pItemList->getDisplayName());
				itemListObject.addInteger("maxcount", pItemList->getMaxItemCount());
				writeParameterDefinitionsToJSON(writer, itemListObject, pItemList.get());
				itemListsArray.addObject(itemListObject);
			}
			typeObject.addArray("itemlists", itemListsArray);

			catalogArray.addObject(typeObject);
		}
	}

	object.addArray(sName, catalogArray);
}

void CUIModule_ContentOperationSequenceEditor::writeSequencesToJSON(CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName)
{
	CJSONWriterArray sequencesArray(writer);

	auto pRegistry = m_pOperationSequenceHandler->findRegistry(m_sStateMachineName, false);
	if (pRegistry.get() != nullptr) {
		for (auto& pSequence : pRegistry->listSequences()) {
			if (!sequenceIsVisible(pSequence->getName()))
				continue;

			CJSONWriterObject sequenceObject(writer);
			sequenceObject.addString("name", pSequence->getName());
			sequenceObject.addString("displayname", pSequence->getDisplayName());
			sequenceObject.addString("context", pSequence->getContext());
			writeRevisionSummaryToJSON(sequenceObject, pSequence.get());
			sequenceObject.addString("username", resolveUserName(pSequence->getUserUUID()));
			sequenceObject.addBool("hasdefault", pRegistry->hasFactoryDefault(pSequence->getName()));
			sequencesArray.addObject(sequenceObject);
		}
	}

	object.addArray(sName, sequencesArray);
}

void CUIModule_ContentOperationSequenceEditor::writeReferencesToJSON(CJSONWriter& writer, CJSONWriterObject& object, const std::string& sName)
{
	CJSONWriterObject referencesObject(writer);

	auto pRegistry = m_pOperationSequenceHandler->findRegistry(m_sStateMachineName, false);
	if (pRegistry.get() != nullptr) {
		for (auto& pSequence : pRegistry->listSequences()) {
			if (!sequenceIsVisible(pSequence->getName()))
				continue;

			CJSONWriterArray parametersArray(writer);
			for (auto& parameter : pRegistry->getBindingGroupParameters(pSequence->getBindingGroup())) {
				CJSONWriterObject parameterObject(writer);
				parameterObject.addString("name", parameter.first);
				parameterObject.addString("description", parameter.second);
				parametersArray.addObject(parameterObject);
			}
			referencesObject.addArray(pSequence->getName(), parametersArray);
		}
	}

	object.addObject(sName, referencesObject);
}

void CUIModule_ContentOperationSequenceEditor::writeValidationResultToJSON(CJSONWriter& writer, CJSONWriterObject& object, COperationSequenceValidationResult* pResult)
{
	object.addBool("valid", pResult->isValid());

	CJSONWriterArray issuesArray(writer);
	for (auto& issue : pResult->getIssues()) {
		CJSONWriterObject issueObject(writer);
		issueObject.addString("severity", (issue.m_Severity == eOperationSequenceIssueSeverity::Error) ? "error" : "warning");
		issueObject.addString("operation", issue.m_sOperationUUID);
		issueObject.addString("parameter", issue.m_sParameterName);
		issueObject.addString("message", issue.m_sMessage);
		issuesArray.addObject(issueObject);
	}
	object.addArray("issues", issuesArray);
}

void CUIModule_ContentOperationSequenceEditor::writeSequenceToJSON(CJSONWriter& writer, CJSONWriterObject& object, COperationSequence* pSequence)
{
	LibMCAssertNotNull(pSequence);

	object.addString("name", pSequence->getName());
	object.addString("displayname", pSequence->getDisplayName());
	object.addString("description", pSequence->getDescription());
	object.addString("context", pSequence->getContext());
	object.addString("bindinggroup", pSequence->getBindingGroup());
	object.addBool("published", pSequence->isPublished());
	writeRevisionSummaryToJSON(object, pSequence);

	CJSONWriterArray operationsArray(writer);
	for (auto& pOperation : pSequence->getOperations()) {
		CJSONWriterObject operationObject(writer);
		operationObject.addString("uuid", pOperation->getUUID());
		operationObject.addString("type", pOperation->getTypeName());
		operationObject.addBool("enabled", pOperation->isEnabled());
		operationObject.addString("comment", pOperation->getComment());
		writeValueListToJSON(writer, operationObject, "values", pOperation->values());

		CJSONWriterObject itemsObject(writer);
		for (auto& sListName : pOperation->getItemListNames()) {
			CJSONWriterArray itemsArray(writer);
			uint32_t nItemCount = pOperation->getItemCount(sListName);
			for (uint32_t nItemIndex = 0; nItemIndex < nItemCount; nItemIndex++) {
				CJSONWriterObject itemObject(writer);
				for (auto& value : pOperation->getItem(sListName, nItemIndex)->values().getValues())
					itemObject.addString(value.first, value.second);
				itemsArray.addObject(itemObject);
			}
			itemsObject.addArray(sListName, itemsArray);
		}
		operationObject.addObject("items", itemsObject);

		operationsArray.addObject(operationObject);
	}
	object.addArray("operations", operationsArray);
}

POperationSequence CUIModule_ContentOperationSequenceEditor::parseSequenceFromJSON(const rapidjson::Value& sequenceValue)
{
	if (!sequenceValue.IsObject())
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, "sequence must be an object");

	std::string sName = jsonMemberString(sequenceValue, "name");
	if (!COperationParameterContainer::isValidIdentifier(sName))
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCENAME, sName);

	auto pSequence = std::make_shared<COperationSequence>(sName, eOperationSequenceOrigin::Editor);
	pSequence->setDisplayName(jsonMemberString(sequenceValue, "displayname"));
	pSequence->setDescription(jsonMemberString(sequenceValue, "description"));
	pSequence->setContext(jsonMemberString(sequenceValue, "context"));
	pSequence->setBindingGroup(jsonMemberString(sequenceValue, "bindinggroup"));

	auto iOperationsIter = sequenceValue.FindMember("operations");
	if (iOperationsIter == sequenceValue.MemberEnd())
		return pSequence;
	if (!iOperationsIter->value.IsArray())
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, "operations must be an array");

	for (auto& operationValue : iOperationsIter->value.GetArray()) {
		std::string sContext = "operation " + std::to_string(pSequence->getOperationCount() + 1);
		if (!operationValue.IsObject())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, sContext + " must be an object");

		std::string sType = jsonMemberString(operationValue, "type");
		if (sType.empty())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, sContext + " has no type");

		std::string sUUID = jsonMemberString(operationValue, "uuid");
		if (!sUUID.empty() && !AMCCommon::CUtils::stringIsUUIDString(sUUID))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, sContext + " has an invalid uuid");

		auto pOperation = pSequence->insertOperation(pSequence->getOperationCount(), sType, sUUID);

		auto iEnabledIter = operationValue.FindMember("enabled");
		if ((iEnabledIter != operationValue.MemberEnd()) && iEnabledIter->value.IsBool())
			pOperation->setEnabled(iEnabledIter->value.GetBool());

		pOperation->setComment(jsonMemberString(operationValue, "comment"));

		auto iValuesIter = operationValue.FindMember("values");
		if (iValuesIter != operationValue.MemberEnd())
			parseJSONValueList(iValuesIter->value, pOperation->values(), sContext);

		auto iItemsIter = operationValue.FindMember("items");
		if (iItemsIter != operationValue.MemberEnd()) {
			if (!iItemsIter->value.IsObject())
				throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, sContext + ": items must be an object");

			for (auto iListIter = iItemsIter->value.MemberBegin(); iListIter != iItemsIter->value.MemberEnd(); iListIter++) {
				std::string sListName = iListIter->name.GetString();
				if (!COperationParameterContainer::isValidIdentifier(sListName))
					throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, sContext + ": invalid item list name \"" + sListName + "\"");
				if (!iListIter->value.IsArray())
					throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, sContext + "." + sListName + " must be an array");

				for (auto& itemValue : iListIter->value.GetArray()) {
					auto pItem = pOperation->addItem(sListName);
					parseJSONValueList(itemValue, pItem->values(), sContext + "." + sListName);
				}
			}
		}
	}

	return pSequence;
}

POperationSequence CUIModule_ContentOperationSequenceEditor::parseSequenceRequest(const CAPIJSONRequest& requestData)
{
	std::string sSequenceJSON = requestData.getJSONObjectString("sequence", LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON);

	rapidjson::Document document;
	document.Parse(sSequenceJSON.c_str());
	if (document.HasParseError())
		throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON, "could not parse sequence");

	auto pSequence = parseSequenceFromJSON(document);
	checkSequenceIsVisible(pSequence->getName());

	if (requestData.hasValue("baserevision"))
		pSequence->setBaseRevision((uint32_t)requestData.getUint64("baserevision", 0, UINT32_MAX, LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON));

	return pSequence;
}

void CUIModule_ContentOperationSequenceEditor::handleCustomRequest(PAPIAuth pAuth, const std::string& requestType, const CAPIJSONRequest& requestData, CJSONWriter& response, CUIModule_UIEventHandler* pEventHandler)
{
	if (pAuth.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	auto pRegistry = getRegistry();

	if (requestType == "loadsequence") {
		std::string sName = requestData.getRawString("name", LIBMC_ERROR_INVALIDOPERATIONSEQUENCENAME);
		checkSequenceIsVisible(sName);

		POperationSequence pSequence;
		if (requestData.hasValue("revision"))
			pSequence = pRegistry->getSequenceRevision(sName, (uint32_t)requestData.getUint64("revision", 1, UINT32_MAX, LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON));
		else
			pSequence = pRegistry->getSequence(sName);

		CJSONWriterObject sequenceObject(response);
		writeSequenceToJSON(response, sequenceObject, pSequence.get());
		response.addObject("sequence", sequenceObject);
		response.addInteger("latestrevision", pRegistry->getLatestRevisionNumber(sName));
		response.addString("username", resolveUserName(pSequence->getUserUUID()));

		CJSONWriterObject validationObject(response);
		writeValidationResultToJSON(response, validationObject, pRegistry->validateSequence(pSequence).get());
		response.addObject("validation", validationObject);
		return;
	}

	if (requestType == "listrevisions") {
		std::string sName = requestData.getRawString("name", LIBMC_ERROR_INVALIDOPERATIONSEQUENCENAME);
		checkSequenceIsVisible(sName);

		CJSONWriterArray revisionsArray(response);
		for (auto& pRevision : pRegistry->listSequenceRevisions(sName)) {
			CJSONWriterObject revisionObject(response);
			writeRevisionSummaryToJSON(revisionObject, pRevision.get());
			revisionObject.addString("username", resolveUserName(pRevision->getUserUUID()));
			revisionsArray.addObject(revisionObject);
		}
		response.addArray("revisions", revisionsArray);
		return;
	}

	if (requestType == "validatesequence") {
		auto pSequence = parseSequenceRequest(requestData);

		CJSONWriterObject validationObject(response);
		writeValidationResultToJSON(response, validationObject, pRegistry->validateSequence(pSequence).get());
		response.addObject("validation", validationObject);
		return;
	}

	if (requestType == "savesequence") {
		if (!userCanEdit(pAuth))
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCEEDITNOTPERMITTED, m_sStateMachineName);

		auto pSequence = parseSequenceRequest(requestData);

		auto pResult = pRegistry->validateSequence(pSequence);
		if (!pResult->isValid()) {
			response.addString("status", "invalid");
			CJSONWriterObject validationObject(response);
			writeValidationResultToJSON(response, validationObject, pResult.get());
			response.addObject("validation", validationObject);
			return;
		}

		uint32_t nLatestRevision = pRegistry->getLatestRevisionNumber(pSequence->getName());
		if (pSequence->getRevision() != nLatestRevision) {
			response.addString("status", "conflict");
			response.addInteger("latestrevision", nLatestRevision);
			return;
		}

		std::string sUserUUID;
		auto pUserInformation = pAuth->getUserInformation();
		if (pUserInformation.get() != nullptr)
			sUserUUID = pUserInformation->getUUID();

		POperationSequence pPublished;
		try {
			pPublished = pRegistry->publishSequence(pSequence, sUserUUID, false);
		}
		catch (ELibMCInterfaceException& E) {
			if (E.getErrorCode() != LIBMC_ERROR_OPERATIONSEQUENCEREVISIONCONFLICT)
				throw;

			response.addString("status", "conflict");
			response.addInteger("latestrevision", pRegistry->getLatestRevisionNumber(pSequence->getName()));
			return;
		}

		response.addString("status", "saved");
		CJSONWriterObject sequenceObject(response);
		writeSequenceToJSON(response, sequenceObject, pPublished.get());
		response.addObject("sequence", sequenceObject);

		auto pClientVariableHandler = pAuth->getLegacyParameterHandler(true);
		if (pClientVariableHandler != nullptr) {
			auto pGroup = pClientVariableHandler->findGroup(getItemPath(), false);
			if (pGroup.get() != nullptr) {
				pGroup->setParameterValueByName(AMC_UI_OPERATIONSEQUENCEEDITOR_SAVEDSEQUENCE, pPublished->getName());
				pGroup->setIntParameterValueByName(AMC_UI_OPERATIONSEQUENCEEDITOR_SAVEDREVISION, pPublished->getRevision());
			}
		}

		if (!m_sSaveEvent.empty() && (pEventHandler != nullptr)) {
			CJSONWriter parameterWriter;
			parameterWriter.addString(AMC_UI_OPERATIONSEQUENCEEDITOR_SAVEDSEQUENCE, pPublished->getName());
			parameterWriter.addString(AMC_UI_OPERATIONSEQUENCEEDITOR_SAVEDREVISION, std::to_string(pPublished->getRevision()));
			pEventHandler->handleEvent(m_sSaveEvent, m_sUUID, "", parameterWriter.saveToString(), pAuth);
		}
		return;
	}

	if (requestType == "resetsequence") {
		std::string sName = requestData.getRawString("name", LIBMC_ERROR_INVALIDOPERATIONSEQUENCENAME);
		checkSequenceIsVisible(sName);

		auto pWorkingCopy = pRegistry->createFactoryDefault(sName);

		CJSONWriterObject sequenceObject(response);
		writeSequenceToJSON(response, sequenceObject, pWorkingCopy.get());
		response.addObject("sequence", sequenceObject);

		CJSONWriterObject validationObject(response);
		writeValidationResultToJSON(response, validationObject, pRegistry->validateSequence(pWorkingCopy).get());
		response.addObject("validation", validationObject);
		return;
	}

	if (requestType == "exportsequence") {
		std::string sName = requestData.getRawString("name", LIBMC_ERROR_INVALIDOPERATIONSEQUENCENAME);
		checkSequenceIsVisible(sName);

		POperationSequence pSequence;
		if (requestData.hasValue("revision"))
			pSequence = pRegistry->getSequenceRevision(sName, (uint32_t)requestData.getUint64("revision", 1, UINT32_MAX, LIBMC_ERROR_INVALIDOPERATIONSEQUENCEJSON));
		else
			pSequence = pRegistry->getSequence(sName);

		response.addString("xml", pSequence->serializeToXMLString());
		return;
	}

	if (requestType == "importsequence") {
		std::string sXML = requestData.getRawString("xml", LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML);
		auto pWorkingCopy = pRegistry->parseSequence(sXML, eOperationSequenceOrigin::Editor);

		if (requestData.hasValue("name")) {
			std::string sName = requestData.getRawString("name", LIBMC_ERROR_INVALIDOPERATIONSEQUENCENAME);
			if (!sName.empty())
				pWorkingCopy->setName(sName);
		}
		checkSequenceIsVisible(pWorkingCopy->getName());
		pWorkingCopy->setBaseRevision(pRegistry->getLatestRevisionNumber(pWorkingCopy->getName()));

		CJSONWriterObject sequenceObject(response);
		writeSequenceToJSON(response, sequenceObject, pWorkingCopy.get());
		response.addObject("sequence", sequenceObject);

		CJSONWriterObject validationObject(response);
		writeValidationResultToJSON(response, validationObject, pRegistry->validateSequence(pWorkingCopy).get());
		response.addObject("validation", validationObject);
		return;
	}

	throw ELibMCCustomException(LIBMC_ERROR_CUSTOMREQUESTNOTSUPPORTED, requestType);
}
