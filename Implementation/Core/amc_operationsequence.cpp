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

#include "amc_operationsequence.hpp"

#include "libmc_exceptiontypes.hpp"
#include "common_utils.hpp"

#include <sstream>
#include <algorithm>

namespace AMC {

	/*************************************************************************************************************************
	 COperationSequenceValidationResult
	**************************************************************************************************************************/

	COperationSequenceValidationResult::COperationSequenceValidationResult()
		: m_nErrorCount(0)
	{
	}

	void COperationSequenceValidationResult::addIssue(eOperationSequenceIssueSeverity severity, const std::string& sOperationUUID, const std::string& sParameterName, const std::string& sMessage)
	{
		sOperationSequenceIssue issue;
		issue.m_Severity = severity;
		issue.m_sOperationUUID = sOperationUUID;
		issue.m_sParameterName = sParameterName;
		issue.m_sMessage = sMessage;
		m_Issues.push_back(issue);

		if (severity == eOperationSequenceIssueSeverity::Error)
			m_nErrorCount++;
	}

	void COperationSequenceValidationResult::addError(const std::string& sOperationUUID, const std::string& sParameterName, const std::string& sMessage)
	{
		addIssue(eOperationSequenceIssueSeverity::Error, sOperationUUID, sParameterName, sMessage);
	}

	void COperationSequenceValidationResult::addWarning(const std::string& sOperationUUID, const std::string& sParameterName, const std::string& sMessage)
	{
		addIssue(eOperationSequenceIssueSeverity::Warning, sOperationUUID, sParameterName, sMessage);
	}

	bool COperationSequenceValidationResult::isValid() const
	{
		return (m_nErrorCount == 0);
	}

	uint32_t COperationSequenceValidationResult::getIssueCount() const
	{
		return (uint32_t)m_Issues.size();
	}

	uint32_t COperationSequenceValidationResult::getErrorCount() const
	{
		return m_nErrorCount;
	}

	const sOperationSequenceIssue& COperationSequenceValidationResult::getIssue(uint32_t nIndex) const
	{
		if (nIndex >= m_Issues.size())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEISSUEINDEX, std::to_string(nIndex));

		return m_Issues.at(nIndex);
	}

	const std::vector<sOperationSequenceIssue>& COperationSequenceValidationResult::getIssues() const
	{
		return m_Issues;
	}

	std::string COperationSequenceValidationResult::getSummary(bool bIncludeWarnings) const
	{
		std::string sSummary;
		for (auto& issue : m_Issues) {
			bool bIsError = (issue.m_Severity == eOperationSequenceIssueSeverity::Error);
			if (!bIsError && !bIncludeWarnings)
				continue;

			if (!sSummary.empty())
				sSummary += "; ";
			sSummary += bIsError ? "error: " : "warning: ";
			sSummary += issue.m_sMessage;
		}
		return sSummary;
	}


	/*************************************************************************************************************************
	 COperationValueList
	**************************************************************************************************************************/

	bool COperationValueList::hasValue(const std::string& sName) const
	{
		for (auto& value : m_Values)
			if (value.first == sName)
				return true;
		return false;
	}

	bool COperationValueList::getValue(const std::string& sName, std::string& sValue) const
	{
		for (auto& value : m_Values) {
			if (value.first == sName) {
				sValue = value.second;
				return true;
			}
		}
		return false;
	}

	void COperationValueList::setValue(const std::string& sName, const std::string& sValue)
	{
		for (auto& value : m_Values) {
			if (value.first == sName) {
				value.second = sValue;
				return;
			}
		}
		m_Values.push_back(std::make_pair(sName, sValue));
	}

	bool COperationValueList::clearValue(const std::string& sName)
	{
		for (auto iIter = m_Values.begin(); iIter != m_Values.end(); iIter++) {
			if (iIter->first == sName) {
				m_Values.erase(iIter);
				return true;
			}
		}
		return false;
	}

	const std::vector<std::pair<std::string, std::string>>& COperationValueList::getValues() const
	{
		return m_Values;
	}


	/*************************************************************************************************************************
	 COperationItem
	**************************************************************************************************************************/

	COperationValueList& COperationItem::values()
	{
		return m_Values;
	}

	const COperationValueList& COperationItem::values() const
	{
		return m_Values;
	}


	/*************************************************************************************************************************
	 COperation
	**************************************************************************************************************************/

	COperation::COperation(const std::string& sUUID, const std::string& sTypeName)
		: m_sUUID(AMCCommon::CUtils::normalizeUUIDString(sUUID)), m_sTypeName(sTypeName), m_bEnabled(true)
	{
	}

	POperation COperation::clone() const
	{
		auto pClone = std::make_shared<COperation>(m_sUUID, m_sTypeName);
		pClone->m_bEnabled = m_bEnabled;
		pClone->m_sComment = m_sComment;
		pClone->m_Values = m_Values;
		pClone->m_ItemListOrder = m_ItemListOrder;
		for (auto& itemList : m_ItemLists) {
			std::vector<POperationItem> clonedItems;
			for (auto& pItem : itemList.second)
				clonedItems.push_back(std::make_shared<COperationItem>(*pItem));
			pClone->m_ItemLists.insert(std::make_pair(itemList.first, clonedItems));
		}
		return pClone;
	}

	std::string COperation::getUUID() const
	{
		return m_sUUID;
	}

	std::string COperation::getTypeName() const
	{
		return m_sTypeName;
	}

	bool COperation::isEnabled() const
	{
		return m_bEnabled;
	}

	void COperation::setEnabled(bool bEnabled)
	{
		m_bEnabled = bEnabled;
	}

	std::string COperation::getComment() const
	{
		return m_sComment;
	}

	void COperation::setComment(const std::string& sComment)
	{
		m_sComment = sComment;
	}

	COperationValueList& COperation::values()
	{
		return m_Values;
	}

	const COperationValueList& COperation::values() const
	{
		return m_Values;
	}

	std::vector<std::string> COperation::getItemListNames() const
	{
		return m_ItemListOrder;
	}

	uint32_t COperation::getItemCount(const std::string& sListName) const
	{
		auto iIter = m_ItemLists.find(sListName);
		if (iIter == m_ItemLists.end())
			return 0;
		return (uint32_t)iIter->second.size();
	}

	POperationItem COperation::getItem(const std::string& sListName, uint32_t nIndex) const
	{
		auto iIter = m_ItemLists.find(sListName);
		if ((iIter == m_ItemLists.end()) || (nIndex >= iIter->second.size()))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONITEMINDEX, sListName + "[" + std::to_string(nIndex) + "]");

		return iIter->second.at(nIndex);
	}

	POperationItem COperation::addItem(const std::string& sListName)
	{
		auto iIter = m_ItemLists.find(sListName);
		if (iIter == m_ItemLists.end()) {
			m_ItemListOrder.push_back(sListName);
			iIter = m_ItemLists.insert(std::make_pair(sListName, std::vector<POperationItem>())).first;
		}

		auto pItem = std::make_shared<COperationItem>();
		iIter->second.push_back(pItem);
		return pItem;
	}

	void COperation::removeItem(const std::string& sListName, uint32_t nIndex)
	{
		auto iIter = m_ItemLists.find(sListName);
		if ((iIter == m_ItemLists.end()) || (nIndex >= iIter->second.size()))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONITEMINDEX, sListName + "[" + std::to_string(nIndex) + "]");

		iIter->second.erase(iIter->second.begin() + nIndex);
		if (iIter->second.empty()) {
			m_ItemLists.erase(iIter);
			m_ItemListOrder.erase(std::remove(m_ItemListOrder.begin(), m_ItemListOrder.end(), sListName), m_ItemListOrder.end());
		}
	}


	/*************************************************************************************************************************
	 COperationSequence
	**************************************************************************************************************************/

	class COperationSequenceXMLStringWriter : public pugi::xml_writer {
	public:
		std::string m_sResult;

		void write(const void* data, size_t size) override
		{
			m_sResult.append(static_cast<const char*>(data), size);
		}
	};

	static std::string getLocalName(const std::string& sQualifiedName)
	{
		auto nColon = sQualifiedName.find(':');
		if (nColon == std::string::npos)
			return sQualifiedName;
		return sQualifiedName.substr(nColon + 1);
	}

	COperationSequence::COperationSequence(const std::string& sName, eOperationSequenceOrigin origin)
		: m_sName(sName),
		m_bIsTemplate(false),
		m_bIsPublished(false),
		m_nRevision(0),
		m_Origin(origin)
	{
	}

	COperationSequence::~COperationSequence()
	{
	}

	bool COperationSequence::isSupportedNamespace(const std::string& sNamespace)
	{
		return (sNamespace == AMC_OPERATIONSEQUENCE_NAMESPACE) || (sNamespace == AMC_OPERATIONSEQUENCE_LEGACYNAMESPACE);
	}

	bool COperationSequence::isSupportedRootName(const std::string& sRootName)
	{
		std::string sLocalName = getLocalName(sRootName);
		return (sLocalName == AMC_OPERATIONSEQUENCE_ROOTNAME) || (sLocalName == AMC_OPERATIONSEQUENCE_LEGACYROOTNAME);
	}

	POperationSequence COperationSequence::parseFromXMLString(const std::string& sXMLString, eOperationSequenceOrigin origin)
	{
		pugi::xml_document document;
		pugi::xml_parse_result parseResult = document.load_string(sXMLString.c_str());
		if (!parseResult)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, parseResult.description());

		auto rootNode = document.document_element();
		if (rootNode.empty())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, "missing root element");

		if (!isSupportedRootName(rootNode.name()))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, std::string("unknown root element ") + rootNode.name());

		std::string sNamespaceAttribute = "xmlns";
		std::string sRootName = rootNode.name();
		auto nColon = sRootName.find(':');
		if (nColon != std::string::npos)
			sNamespaceAttribute = "xmlns:" + sRootName.substr(0, nColon);

		std::string sNamespace = rootNode.attribute(sNamespaceAttribute.c_str()).as_string();
		if (!isSupportedNamespace(sNamespace))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, "unknown namespace \"" + sNamespace + "\"");

		return parseFromXMLNode(rootNode, origin);
	}

	POperationSequence COperationSequence::parseFromXMLNode(const pugi::xml_node& rootNode, eOperationSequenceOrigin origin)
	{
		if (!isSupportedRootName(rootNode.name()))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, std::string("unknown root element ") + rootNode.name());

		auto pSequence = std::make_shared<COperationSequence>("", origin);
		pSequence->parseFromXMLNode(rootNode);
		return pSequence;
	}

	void COperationSequence::parseFromXMLNode(const pugi::xml_node& rootNode)
	{
		m_sName = rootNode.attribute("name").as_string();
		m_sDisplayName = rootNode.attribute("displayname").as_string();
		m_sDescription = rootNode.attribute("description").as_string();
		m_sContext = rootNode.attribute("context").as_string();
		m_sBindingGroup = rootNode.attribute("bindinggroup").as_string();

		std::string sTemplate = rootNode.attribute("template").as_string();
		m_bIsTemplate = (sTemplate == "1") || (sTemplate == "true");

		std::set<std::string> usedUUIDs;

		for (pugi::xml_node childNode : rootNode.children()) {
			if (childNode.type() != pugi::node_element)
				continue;

			std::string sChildName = getLocalName(childNode.name());
			if (sChildName != "operation")
				throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, "unexpected element " + sChildName);

			if (m_Operations.size() >= AMC_OPERATIONSEQUENCE_MAXOPERATIONCOUNT)
				throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, "too many operations");

			std::string sType = childNode.attribute("type").as_string();
			if (sType.empty())
				throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, "operation " + std::to_string(m_Operations.size() + 1) + " has no type");

			std::string sUUID = childNode.attribute("uuid").as_string();
			if (sUUID.empty()) {
				sUUID = AMCCommon::CUtils::createUUID();
			}
			else {
				if (!AMCCommon::CUtils::stringIsUUIDString(sUUID))
					throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, "invalid operation uuid \"" + sUUID + "\"");
				sUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);
			}

			if (!usedUUIDs.insert(sUUID).second)
				throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, "duplicate operation uuid " + sUUID);

			auto pOperation = std::make_shared<COperation>(sUUID, sType);

			for (pugi::xml_attribute attribute : childNode.attributes()) {
				std::string sAttributeName = attribute.name();
				if ((sAttributeName == "type") || (sAttributeName == "uuid"))
					continue;

				if (sAttributeName == "enabled") {
					std::string sEnabled = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(attribute.as_string()));
					if ((sEnabled == "0") || (sEnabled == "false"))
						pOperation->setEnabled(false);
					else if ((sEnabled == "1") || (sEnabled == "true") || sEnabled.empty())
						pOperation->setEnabled(true);
					else
						throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONSEQUENCEXML, "invalid enabled value \"" + sEnabled + "\"");
					continue;
				}

				if (sAttributeName == "comment") {
					pOperation->setComment(attribute.as_string());
					continue;
				}

				if ((sAttributeName == "xmlns") || (sAttributeName.find(':') != std::string::npos))
					continue;

				pOperation->values().setValue(sAttributeName, attribute.as_string());
			}

			for (pugi::xml_node itemNode : childNode.children()) {
				if (itemNode.type() != pugi::node_element)
					continue;

				auto pItem = pOperation->addItem(getLocalName(itemNode.name()));
				for (pugi::xml_attribute attribute : itemNode.attributes()) {
					std::string sAttributeName = attribute.name();
					if ((sAttributeName == "xmlns") || (sAttributeName.find(':') != std::string::npos))
						continue;
					pItem->values().setValue(sAttributeName, attribute.as_string());
				}
			}

			m_Operations.push_back(pOperation);
		}
	}

	void COperationSequence::serializeToXMLNode(pugi::xml_node& parentNode) const
	{
		auto rootNode = parentNode.append_child(AMC_OPERATIONSEQUENCE_ROOTNAME);
		rootNode.append_attribute("xmlns").set_value(AMC_OPERATIONSEQUENCE_NAMESPACE);
		rootNode.append_attribute("name").set_value(m_sName.c_str());
		if (!m_sDisplayName.empty())
			rootNode.append_attribute("displayname").set_value(m_sDisplayName.c_str());
		if (!m_sDescription.empty())
			rootNode.append_attribute("description").set_value(m_sDescription.c_str());
		if (!m_sContext.empty())
			rootNode.append_attribute("context").set_value(m_sContext.c_str());
		if (!m_sBindingGroup.empty())
			rootNode.append_attribute("bindinggroup").set_value(m_sBindingGroup.c_str());
		if (m_bIsTemplate)
			rootNode.append_attribute("template").set_value("1");

		for (auto& pOperation : m_Operations) {
			auto operationNode = rootNode.append_child("operation");
			operationNode.append_attribute("type").set_value(pOperation->getTypeName().c_str());
			operationNode.append_attribute("uuid").set_value(pOperation->getUUID().c_str());
			if (!pOperation->isEnabled())
				operationNode.append_attribute("enabled").set_value("0");

			for (auto& value : pOperation->values().getValues())
				operationNode.append_attribute(value.first.c_str()).set_value(value.second.c_str());

			if (!pOperation->getComment().empty())
				operationNode.append_attribute("comment").set_value(pOperation->getComment().c_str());

			for (auto& sListName : pOperation->getItemListNames()) {
				uint32_t nCount = pOperation->getItemCount(sListName);
				for (uint32_t nIndex = 0; nIndex < nCount; nIndex++) {
					auto pItem = pOperation->getItem(sListName, nIndex);
					auto itemNode = operationNode.append_child(sListName.c_str());
					for (auto& value : pItem->values().getValues())
						itemNode.append_attribute(value.first.c_str()).set_value(value.second.c_str());
				}
			}
		}
	}

	std::string COperationSequence::serializeToXMLString() const
	{
		pugi::xml_document document;
		auto declarationNode = document.append_child(pugi::node_declaration);
		declarationNode.append_attribute("version").set_value("1.0");
		declarationNode.append_attribute("encoding").set_value("UTF-8");

		serializeToXMLNode(document);

		COperationSequenceXMLStringWriter writer;
		document.save(writer, "\t", pugi::format_default, pugi::encoding_utf8);
		return writer.m_sResult;
	}

	POperationSequence COperationSequence::createWorkingCopy(eOperationSequenceOrigin origin) const
	{
		auto pCopy = std::make_shared<COperationSequence>(m_sName, origin);
		pCopy->m_sDisplayName = m_sDisplayName;
		pCopy->m_sDescription = m_sDescription;
		pCopy->m_sContext = m_sContext;
		pCopy->m_sBindingGroup = m_sBindingGroup;
		pCopy->m_bIsTemplate = m_bIsTemplate;
		pCopy->m_nRevision = m_nRevision;
		for (auto& pOperation : m_Operations)
			pCopy->m_Operations.push_back(pOperation->clone());

		return pCopy;
	}

	POperationSequence COperationSequence::createPublishedCopy(uint32_t nRevision, const std::string& sRevisionUUID, eOperationSequenceOrigin origin, const std::string& sTimestamp, const std::string& sUserUUID) const
	{
		auto pCopy = createWorkingCopy(origin);
		pCopy->m_bIsPublished = true;
		pCopy->m_nRevision = nRevision;
		pCopy->m_sRevisionUUID = sRevisionUUID;
		pCopy->m_sTimestamp = sTimestamp;
		pCopy->m_sUserUUID = sUserUUID;
		return pCopy;
	}

	void COperationSequence::checkMutable() const
	{
		if (m_bIsPublished)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONSEQUENCEISPUBLISHED, m_sName + " revision " + std::to_string(m_nRevision));
	}

	std::string COperationSequence::getName() const
	{
		return m_sName;
	}

	void COperationSequence::setName(const std::string& sName)
	{
		checkMutable();
		m_sName = sName;
	}

	std::string COperationSequence::getDisplayName() const
	{
		return m_sDisplayName;
	}

	void COperationSequence::setDisplayName(const std::string& sDisplayName)
	{
		checkMutable();
		m_sDisplayName = sDisplayName;
	}

	std::string COperationSequence::getDescription() const
	{
		return m_sDescription;
	}

	void COperationSequence::setDescription(const std::string& sDescription)
	{
		checkMutable();
		m_sDescription = sDescription;
	}

	std::string COperationSequence::getContext() const
	{
		return m_sContext;
	}

	void COperationSequence::setContext(const std::string& sContext)
	{
		checkMutable();
		m_sContext = sContext;
	}

	std::string COperationSequence::getBindingGroup() const
	{
		return m_sBindingGroup;
	}

	void COperationSequence::setBindingGroup(const std::string& sBindingGroup)
	{
		checkMutable();
		m_sBindingGroup = sBindingGroup;
	}

	bool COperationSequence::isTemplate() const
	{
		return m_bIsTemplate;
	}

	void COperationSequence::setTemplate(bool bIsTemplate)
	{
		checkMutable();
		m_bIsTemplate = bIsTemplate;
	}

	bool COperationSequence::isPublished() const
	{
		return m_bIsPublished;
	}

	uint32_t COperationSequence::getRevision() const
	{
		return m_nRevision;
	}

	void COperationSequence::setBaseRevision(uint32_t nRevision)
	{
		checkMutable();
		m_nRevision = nRevision;
	}

	std::string COperationSequence::getRevisionUUID() const
	{
		return m_sRevisionUUID;
	}

	eOperationSequenceOrigin COperationSequence::getOrigin() const
	{
		return m_Origin;
	}

	std::string COperationSequence::getTimestamp() const
	{
		return m_sTimestamp;
	}

	std::string COperationSequence::getUserUUID() const
	{
		return m_sUserUUID;
	}

	uint32_t COperationSequence::getOperationCount() const
	{
		return (uint32_t)m_Operations.size();
	}

	POperation COperationSequence::getOperation(uint32_t nIndex) const
	{
		if (nIndex >= m_Operations.size())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONINDEX, m_sName + ": " + std::to_string(nIndex));

		return m_Operations.at(nIndex);
	}

	POperation COperationSequence::findOperation(const std::string& sUUID) const
	{
		int64_t nIndex = findOperationIndex(sUUID);
		if (nIndex < 0)
			return nullptr;

		return m_Operations.at((size_t)nIndex);
	}

	int64_t COperationSequence::findOperationIndex(const std::string& sUUID) const
	{
		if (!AMCCommon::CUtils::stringIsUUIDString(sUUID))
			return -1;

		std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);
		for (size_t nIndex = 0; nIndex < m_Operations.size(); nIndex++)
			if (m_Operations.at(nIndex)->getUUID() == sNormalizedUUID)
				return (int64_t)nIndex;

		return -1;
	}

	std::vector<POperation> COperationSequence::getOperations() const
	{
		return m_Operations;
	}

	POperation COperationSequence::insertOperation(uint32_t nIndex, const std::string& sTypeName, const std::string& sUUID)
	{
		checkMutable();

		if (nIndex > m_Operations.size())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONINDEX, m_sName + ": " + std::to_string(nIndex));
		if (m_Operations.size() >= AMC_OPERATIONSEQUENCE_MAXOPERATIONCOUNT)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONINDEX, m_sName + ": too many operations");

		std::string sNewUUID = sUUID.empty() ? AMCCommon::CUtils::createUUID() : AMCCommon::CUtils::normalizeUUIDString(sUUID);
		if (findOperationIndex(sNewUUID) >= 0)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDPARAM, "duplicate operation uuid " + sNewUUID);

		auto pOperation = std::make_shared<COperation>(sNewUUID, sTypeName);
		m_Operations.insert(m_Operations.begin() + nIndex, pOperation);
		return pOperation;
	}

	void COperationSequence::removeOperation(const std::string& sUUID)
	{
		checkMutable();

		int64_t nIndex = findOperationIndex(sUUID);
		if (nIndex < 0)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONNOTFOUND, sUUID);

		m_Operations.erase(m_Operations.begin() + (size_t)nIndex);
	}

	void COperationSequence::moveOperation(const std::string& sUUID, uint32_t nNewIndex)
	{
		checkMutable();

		int64_t nIndex = findOperationIndex(sUUID);
		if (nIndex < 0)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONNOTFOUND, sUUID);
		if (nNewIndex >= m_Operations.size())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONINDEX, m_sName + ": " + std::to_string(nNewIndex));

		auto pOperation = m_Operations.at((size_t)nIndex);
		m_Operations.erase(m_Operations.begin() + (size_t)nIndex);
		m_Operations.insert(m_Operations.begin() + nNewIndex, pOperation);
	}

	std::string COperationSequence::originToString(eOperationSequenceOrigin origin)
	{
		switch (origin) {
		case eOperationSequenceOrigin::Resource: return "resource";
		case eOperationSequenceOrigin::String: return "string";
		case eOperationSequenceOrigin::Code: return "code";
		case eOperationSequenceOrigin::Editor: return "editor";
		case eOperationSequenceOrigin::Document: return "document";
		default: return "unknown";
		}
	}

	eOperationSequenceOrigin COperationSequence::originFromInteger(uint32_t nOrigin)
	{
		switch (nOrigin) {
		case (uint32_t)eOperationSequenceOrigin::Resource: return eOperationSequenceOrigin::Resource;
		case (uint32_t)eOperationSequenceOrigin::String: return eOperationSequenceOrigin::String;
		case (uint32_t)eOperationSequenceOrigin::Code: return eOperationSequenceOrigin::Code;
		case (uint32_t)eOperationSequenceOrigin::Editor: return eOperationSequenceOrigin::Editor;
		case (uint32_t)eOperationSequenceOrigin::Document: return eOperationSequenceOrigin::Document;
		default: return eOperationSequenceOrigin::Unknown;
		}
	}

}
