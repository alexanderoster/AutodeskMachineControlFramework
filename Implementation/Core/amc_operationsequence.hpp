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


#ifndef __AMC_OPERATIONSEQUENCE
#define __AMC_OPERATIONSEQUENCE

#include "amc_operationtypes.hpp"

#include <pugixml.hpp>

#define AMC_OPERATIONSEQUENCE_NAMESPACE "http://schemas.autodesk.com/amc/operationsequence/2026/11"
#define AMC_OPERATIONSEQUENCE_LEGACYNAMESPACE "http://schemas.autodesk.com/amc/processsequence/2025/03"
#define AMC_OPERATIONSEQUENCE_ROOTNAME "operationsequence"
#define AMC_OPERATIONSEQUENCE_LEGACYROOTNAME "processsequence"
#define AMC_OPERATIONSEQUENCE_MAXOPERATIONCOUNT 65536

namespace AMC {

	class COperationValueList;
	class COperationItem;
	class COperation;
	class COperationSequence;
	class COperationSequenceValidationResult;

	typedef std::shared_ptr<COperationItem> POperationItem;
	typedef std::shared_ptr<COperation> POperation;
	typedef std::shared_ptr<COperationSequence> POperationSequence;
	typedef std::shared_ptr<COperationSequenceValidationResult> POperationSequenceValidationResult;

	typedef struct _sOperationSequenceIssue {
		eOperationSequenceIssueSeverity m_Severity;
		std::string m_sOperationUUID;
		std::string m_sParameterName;
		std::string m_sMessage;
	} sOperationSequenceIssue;

	class COperationSequenceValidationResult {
	private:
		std::vector<sOperationSequenceIssue> m_Issues;
		uint32_t m_nErrorCount;

	public:
		COperationSequenceValidationResult();

		void addIssue(eOperationSequenceIssueSeverity severity, const std::string& sOperationUUID, const std::string& sParameterName, const std::string& sMessage);
		void addError(const std::string& sOperationUUID, const std::string& sParameterName, const std::string& sMessage);
		void addWarning(const std::string& sOperationUUID, const std::string& sParameterName, const std::string& sMessage);

		bool isValid() const;
		uint32_t getIssueCount() const;
		uint32_t getErrorCount() const;
		const sOperationSequenceIssue& getIssue(uint32_t nIndex) const;
		const std::vector<sOperationSequenceIssue>& getIssues() const;

		// Joins all errors (and optionally warnings) into one line, e.g. for exception messages.
		std::string getSummary(bool bIncludeWarnings) const;
	};

	// Raw values in the order they were set, so that serialization keeps the order of the source.
	class COperationValueList {
	private:
		std::vector<std::pair<std::string, std::string>> m_Values;

	public:
		bool hasValue(const std::string& sName) const;
		bool getValue(const std::string& sName, std::string& sValue) const;
		void setValue(const std::string& sName, const std::string& sValue);
		bool clearValue(const std::string& sName);
		const std::vector<std::pair<std::string, std::string>>& getValues() const;
	};

	class COperationItem {
	private:
		COperationValueList m_Values;

	public:
		COperationValueList& values();
		const COperationValueList& values() const;
	};

	class COperation {
	private:
		std::string m_sUUID;
		std::string m_sTypeName;
		bool m_bEnabled;
		std::string m_sComment;
		COperationValueList m_Values;

		std::vector<std::string> m_ItemListOrder;
		std::map<std::string, std::vector<POperationItem>> m_ItemLists;

	public:
		COperation(const std::string& sUUID, const std::string& sTypeName);

		POperation clone() const;

		std::string getUUID() const;
		std::string getTypeName() const;

		bool isEnabled() const;
		void setEnabled(bool bEnabled);

		std::string getComment() const;
		void setComment(const std::string& sComment);

		COperationValueList& values();
		const COperationValueList& values() const;

		std::vector<std::string> getItemListNames() const;
		uint32_t getItemCount(const std::string& sListName) const;
		POperationItem getItem(const std::string& sListName, uint32_t nIndex) const;
		POperationItem addItem(const std::string& sListName);
		void removeItem(const std::string& sListName, uint32_t nIndex);
	};

	class COperationSequence {
	private:
		std::string m_sName;
		std::string m_sDisplayName;
		std::string m_sDescription;
		std::string m_sContext;
		std::string m_sBindingGroup;
		bool m_bIsTemplate;

		bool m_bIsPublished;
		uint32_t m_nRevision;
		std::string m_sRevisionUUID;
		eOperationSequenceOrigin m_Origin;
		std::string m_sTimestamp;
		std::string m_sUserUUID;

		std::vector<POperation> m_Operations;

		void parseFromXMLNode(const pugi::xml_node& rootNode);

	public:

		COperationSequence(const std::string& sName, eOperationSequenceOrigin origin);

		virtual ~COperationSequence();

		static POperationSequence parseFromXMLString(const std::string& sXMLString, eOperationSequenceOrigin origin);

		// The node must be an operationsequence or processsequence element. The namespace is checked by the caller.
		static POperationSequence parseFromXMLNode(const pugi::xml_node& rootNode, eOperationSequenceOrigin origin);

		static bool isSupportedNamespace(const std::string& sNamespace);

		static bool isSupportedRootName(const std::string& sRootName);

		std::string serializeToXMLString() const;

		void serializeToXMLNode(pugi::xml_node& parentNode) const;

		// Deep copy that is not published. The revision is kept as base revision.
		POperationSequence createWorkingCopy(eOperationSequenceOrigin origin) const;

		// Deep copy that is marked as published with the given metadata.
		POperationSequence createPublishedCopy(uint32_t nRevision, const std::string& sRevisionUUID, eOperationSequenceOrigin origin, const std::string& sTimestamp, const std::string& sUserUUID) const;

		void checkMutable() const;

		std::string getName() const;
		void setName(const std::string& sName);

		std::string getDisplayName() const;
		void setDisplayName(const std::string& sDisplayName);

		std::string getDescription() const;
		void setDescription(const std::string& sDescription);

		std::string getContext() const;
		void setContext(const std::string& sContext);

		std::string getBindingGroup() const;
		void setBindingGroup(const std::string& sBindingGroup);

		bool isTemplate() const;
		void setTemplate(bool bIsTemplate);

		bool isPublished() const;
		uint32_t getRevision() const;
		void setBaseRevision(uint32_t nRevision);
		std::string getRevisionUUID() const;
		eOperationSequenceOrigin getOrigin() const;
		std::string getTimestamp() const;
		std::string getUserUUID() const;

		uint32_t getOperationCount() const;
		POperation getOperation(uint32_t nIndex) const;
		POperation findOperation(const std::string& sUUID) const;
		int64_t findOperationIndex(const std::string& sUUID) const;
		std::vector<POperation> getOperations() const;

		POperation insertOperation(uint32_t nIndex, const std::string& sTypeName, const std::string& sUUID);
		void removeOperation(const std::string& sUUID);
		void moveOperation(const std::string& sUUID, uint32_t nNewIndex);

		static std::string originToString(eOperationSequenceOrigin origin);
		static eOperationSequenceOrigin originFromInteger(uint32_t nOrigin);
	};

}

#endif //__AMC_OPERATIONSEQUENCE
