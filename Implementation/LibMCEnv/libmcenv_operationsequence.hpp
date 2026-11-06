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


Abstract: This is the class declaration of the operation sequence model classes

*/


#ifndef __LIBMCENV_OPERATIONSEQUENCE
#define __LIBMCENV_OPERATIONSEQUENCE

#include "libmcenv_interfaces.hpp"

// Parent classes
#include "libmcenv_base.hpp"
#include "libmcenv_iterator.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "amc_operationsequence.hpp"
#include "amc_operationsequencehandler.hpp"


namespace LibMCEnv {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of COperationItem
**************************************************************************************************************************/

class COperationItem : public virtual IOperationItem, public virtual CBase {
private:
	AMC::POperationItem m_pItem;
	AMC::POperationSequence m_pSequence;
	uint32_t m_nIndex;

public:

	COperationItem(AMC::POperationItem pItem, AMC::POperationSequence pSequence, uint32_t nIndex);

	virtual ~COperationItem();

	LibMCEnv_uint32 GetIndex() override;

	bool HasValue(const std::string& sName) override;

	std::string GetRawValue(const std::string& sName) override;

	void SetRawValue(const std::string& sName, const std::string& sValue) override;

	void ClearValue(const std::string& sName) override;

};


/*************************************************************************************************************************
 Class declaration of COperation
**************************************************************************************************************************/

class COperation : public virtual IOperation, public virtual CBase {
private:
	AMC::POperation m_pOperation;
	AMC::POperationSequence m_pSequence;
	AMC::POperationSequenceRegistry m_pRegistry;

	AMC::POperationParameterDefinition findDefinition(const std::string& sName);

public:

	COperation(AMC::POperation pOperation, AMC::POperationSequence pSequence, AMC::POperationSequenceRegistry pRegistry);

	virtual ~COperation();

	AMC::POperation getOperation();

	std::string GetUUID() override;

	LibMCEnv_uint32 GetIndex() override;

	std::string GetTypeName() override;

	bool IsEnabled() override;

	void SetEnabled(const bool bIsEnabled) override;

	std::string GetComment() override;

	void SetComment(const std::string& sComment) override;

	bool HasValue(const std::string& sName) override;

	std::string GetRawValue(const std::string& sName) override;

	LibMCEnv::eOperationValueKind GetValueKind(const std::string& sName) override;

	void SetRawValue(const std::string& sName, const std::string& sValue) override;

	void ClearValue(const std::string& sName) override;

	LibMCEnv_uint32 GetItemCount(const std::string& sListName) override;

	IOperationItem* GetItem(const std::string& sListName, const LibMCEnv_uint32 nIndex) override;

	IOperationItem* AddItem(const std::string& sListName) override;

	void RemoveItem(const std::string& sListName, const LibMCEnv_uint32 nIndex) override;

};


/*************************************************************************************************************************
 Class declaration of COperationIterator
**************************************************************************************************************************/

class COperationIterator : public virtual IOperationIterator, public virtual CIterator {
private:
	AMC::POperationSequence m_pSequence;
	AMC::POperationSequenceRegistry m_pRegistry;

public:

	COperationIterator(AMC::POperationSequence pSequence, AMC::POperationSequenceRegistry pRegistry);

	virtual ~COperationIterator();

	IOperation* GetCurrent() override;

	IIterator* Clone() override;

};


/*************************************************************************************************************************
 Class declaration of COperationSequenceValidationResult
**************************************************************************************************************************/

class COperationSequenceValidationResult : public virtual IOperationSequenceValidationResult, public virtual CBase {
private:
	AMC::POperationSequenceValidationResult m_pResult;

public:

	COperationSequenceValidationResult(AMC::POperationSequenceValidationResult pResult);

	virtual ~COperationSequenceValidationResult();

	bool IsValid() override;

	LibMCEnv_uint32 GetIssueCount() override;

	void GetIssue(const LibMCEnv_uint32 nIndex, LibMCEnv::eOperationSequenceIssueSeverity& eSeverity, std::string& sOperationUUID, std::string& sParameterName, std::string& sMessage) override;

};


/*************************************************************************************************************************
 Class declaration of COperationSequence
**************************************************************************************************************************/

class COperationSequence : public virtual IOperationSequence, public virtual CBase {
private:
	AMC::POperationSequence m_pSequence;
	AMC::POperationSequenceRegistry m_pRegistry;
	AMC::eOperationSequenceOrigin m_WorkingCopyOrigin;

public:

	COperationSequence(AMC::POperationSequence pSequence, AMC::POperationSequenceRegistry pRegistry, AMC::eOperationSequenceOrigin workingCopyOrigin);

	virtual ~COperationSequence();

	AMC::POperationSequence getSequence();

	AMC::POperationSequenceRegistry getRegistry();

	AMC::eOperationSequenceOrigin getWorkingCopyOrigin();

	std::string GetName() override;

	std::string GetDisplayName() override;

	void SetDisplayName(const std::string& sDisplayName) override;

	std::string GetDescription() override;

	void SetDescription(const std::string& sDescription) override;

	std::string GetContext() override;

	void SetContext(const std::string& sContext) override;

	std::string GetBindingGroup() override;

	void SetBindingGroup(const std::string& sGroupName) override;

	bool IsPublished() override;

	LibMCEnv_uint32 GetRevision() override;

	std::string GetRevisionUUID() override;

	bool IsValid() override;

	IOperationSequenceValidationResult* GetValidationResult() override;

	LibMCEnv::eOperationSequenceOrigin GetOrigin() override;

	std::string GetTimestamp() override;

	std::string GetUserUUID() override;

	LibMCEnv_uint32 GetOperationCount() override;

	IOperation* GetOperation(const LibMCEnv_uint32 nIndex) override;

	IOperation* FindOperation(const std::string& sUUID, const bool bFailIfNotExisting) override;

	IOperationIterator* ListOperations() override;

	IOperation* AddOperation(const std::string& sTypeName) override;

	IOperation* InsertOperation(const LibMCEnv_uint32 nIndex, const std::string& sTypeName) override;

	void RemoveOperation(const std::string& sUUID) override;

	void MoveOperation(const std::string& sUUID, const LibMCEnv_uint32 nNewIndex) override;

	IOperationSequence* CreateWorkingCopy() override;

	std::string SerializeToXMLString() override;

	IXMLDocument* SerializeToXMLDocument() override;

};


/*************************************************************************************************************************
 Class declaration of COperationSequenceIterator
**************************************************************************************************************************/

class COperationSequenceIterator : public virtual IOperationSequenceIterator, public virtual CIterator {
public:

	COperationSequenceIterator();

	virtual ~COperationSequenceIterator();

	IOperationSequence* GetCurrent() override;

	IIterator* Clone() override;

	void addSequence(std::shared_ptr<COperationSequence> pSequence);

};

} // namespace Impl
} // namespace LibMCEnv

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCENV_OPERATIONSEQUENCE
