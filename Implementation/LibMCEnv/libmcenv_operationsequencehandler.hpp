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


Abstract: This is the class declaration of COperationSequenceHandler, CDispatchedOperation and COperationSequenceExecution

*/


#ifndef __LIBMCENV_OPERATIONSEQUENCEHANDLER
#define __LIBMCENV_OPERATIONSEQUENCEHANDLER

#include "libmcenv_interfaces.hpp"

// Parent classes
#include "libmcenv_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "amc_operationsequencehandler.hpp"
#include "amc_resourcepackage.hpp"


namespace LibMCEnv {
namespace Impl {

class COperationSequence;


/*************************************************************************************************************************
 Class declaration of COperationSequenceHandler
**************************************************************************************************************************/

class COperationSequenceHandler : public virtual IOperationSequenceHandler, public virtual CBase {
private:
	AMC::POperationSequenceRegistry m_pRegistry;
	bool m_bAllowRegistration;
	AMC::PResourcePackage m_pResourcePackage;
	AMC::eOperationSequenceOrigin m_WorkingCopyOrigin;

	IOperationSequence* wrapSequence(AMC::POperationSequence pSequence);

	COperationSequence* castSequence(IOperationSequence* pSequence);

	std::string readResource(const std::string& sResourceName);

public:

	// bAllowRegistration is only true for the state environment of the owning state machine.
	COperationSequenceHandler(AMC::POperationSequenceRegistry pRegistry, bool bAllowRegistration, AMC::PResourcePackage pResourcePackage, AMC::eOperationSequenceOrigin workingCopyOrigin);

	virtual ~COperationSequenceHandler();

	std::string GetStateMachineInstance() override;

	IOperationTypeDefinition* RegisterOperationType(const std::string& sTypeName, const std::string& sTargetState) override;

	bool HasOperationType(const std::string& sTypeName) override;

	IOperationTypeDefinition* FindOperationType(const std::string& sTypeName, const bool bFailIfNotExisting) override;

	IOperationTypeDefinitionIterator* ListOperationTypes() override;

	IOperationSequence* CreateSequence(const std::string& sSequenceName) override;

	IOperationSequence* ParseSequenceFromString(const std::string& sXMLString) override;

	IOperationSequence* ParseSequenceFromResource(const std::string& sResourceName) override;

	IOperationSequence* ParseSequenceFromXMLDocument(IXMLDocument* pDocument, const std::string& sNodePath) override;

	IOperationSequenceValidationResult* ValidateSequence(IOperationSequence* pSequence) override;

	IOperationSequence* PublishSequence(IOperationSequence* pWorkingCopy, const std::string& sUserUUID, const bool bOverwriteNewerRevision) override;

	IOperationSequence* EnsureSequenceFromResource(const std::string& sSequenceName, const std::string& sResourceName) override;

	bool HasSequence(const std::string& sSequenceName) override;

	IOperationSequence* GetSequence(const std::string& sSequenceName) override;

	IOperationSequence* GetSequenceRevision(const std::string& sSequenceName, const LibMCEnv_uint32 nRevision) override;

	IOperationSequenceIterator* ListSequences() override;

	IOperationSequenceIterator* ListSequenceRevisions(const std::string& sSequenceName) override;

	IOperationSequence* FindSequenceRevisionByUUID(const std::string& sRevisionUUID, const bool bFailIfNotExisting) override;

	void ValidateDispatcherTransitions(const std::string& sDispatcherState) override;

	void SetStatusParameterGroup(const std::string& sGroupName, const std::string& sValuePrefix) override;

};


/*************************************************************************************************************************
 Class declaration of CDispatchedOperation
**************************************************************************************************************************/

class CDispatchedOperation : public virtual IDispatchedOperation, public virtual CBase {
private:
	AMC::PDispatchedOperation m_pOperation;

public:

	CDispatchedOperation(AMC::PDispatchedOperation pOperation);

	virtual ~CDispatchedOperation();

	std::string GetUUID() override;

	std::string GetTypeName() override;

	LibMCEnv_uint32 GetIndex() override;

	std::string GetExecutionName() override;

	std::string GetSequenceName() override;

	LibMCEnv_uint32 GetRevision() override;

	std::string GetComment() override;

	std::string GetTargetState() override;

	std::string GetReturnState() override;

	bool HasExplicitValue(const std::string& sName) override;

	std::string GetStringValue(const std::string& sName) override;

	LibMCEnv_int64 GetIntegerValue(const std::string& sName) override;

	LibMCEnv_double GetDoubleValue(const std::string& sName) override;

	bool GetBoolValue(const std::string& sName) override;

	LibMCEnv_double ReevaluateDoubleValue(const std::string& sName) override;

	LibMCEnv_int64 ReevaluateIntegerValue(const std::string& sName) override;

	LibMCEnv_uint32 GetItemCount(const std::string& sListName) override;

	std::string GetItemStringValue(const std::string& sListName, const LibMCEnv_uint32 nIndex, const std::string& sName) override;

	LibMCEnv_int64 GetItemIntegerValue(const std::string& sListName, const LibMCEnv_uint32 nIndex, const std::string& sName) override;

	LibMCEnv_double GetItemDoubleValue(const std::string& sListName, const LibMCEnv_uint32 nIndex, const std::string& sName) override;

	bool GetItemBoolValue(const std::string& sListName, const LibMCEnv_uint32 nIndex, const std::string& sName) override;

	void MarkFailed(const std::string& sMessage) override;

};


/*************************************************************************************************************************
 Class declaration of COperationSequenceExecution
**************************************************************************************************************************/

class COperationSequenceExecution : public virtual IOperationSequenceExecution, public virtual CBase {
private:
	AMC::POperationSequenceExecution m_pExecution;
	AMC::POperationSequenceRegistry m_pRegistry;

public:

	COperationSequenceExecution(AMC::POperationSequenceExecution pExecution, AMC::POperationSequenceRegistry pRegistry);

	virtual ~COperationSequenceExecution();

	std::string GetName() override;

	IOperationSequence* GetSequence() override;

	std::string GetBindingGroup() override;

	void SetBindingGroup(const std::string& sGroupName) override;

	std::string GetReturnState() override;

	LibMCEnv::eOperationSequenceExecutionStatus GetStatus() override;

	LibMCEnv_uint32 GetNextOperationIndex() override;

	LibMCEnv_uint32 GetIterationCount() override;

	IDispatchedOperation* DispatchNextOperation() override;

	void Restart() override;

	void Abort(const std::string& sReason) override;

};

} // namespace Impl
} // namespace LibMCEnv

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCENV_OPERATIONSEQUENCEHANDLER
