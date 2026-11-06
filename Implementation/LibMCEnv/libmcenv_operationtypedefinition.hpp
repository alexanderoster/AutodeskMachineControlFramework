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


Abstract: This is the class declaration of the operation type definition classes

*/


#ifndef __LIBMCENV_OPERATIONTYPEDEFINITION
#define __LIBMCENV_OPERATIONTYPEDEFINITION

#include "libmcenv_interfaces.hpp"

// Parent classes
#include "libmcenv_base.hpp"
#include "libmcenv_iterator.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "amc_operationtypes.hpp"
#include "amc_resourcepackage.hpp"


namespace LibMCEnv {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of COperationParameterDefinition
**************************************************************************************************************************/

class COperationParameterDefinition : public virtual IOperationParameterDefinition, public virtual CBase {
private:
	AMC::POperationParameterDefinition m_pDefinition;
	bool m_bIsMutable;

	void checkMutable();

public:

	COperationParameterDefinition(AMC::POperationParameterDefinition pDefinition, bool bIsMutable);

	virtual ~COperationParameterDefinition();

	AMC::POperationParameterDefinition getDefinition();

	bool isMutable();

	std::string GetName() override;

	std::string GetDisplayName() override;

	void SetDisplayName(const std::string& sDisplayName) override;

	std::string GetDescription() override;

	void SetDescription(const std::string& sDescription) override;

	LibMCEnv::eOperationParameterType GetParameterType() override;

	std::string GetDefaultValue() override;

	std::string GetUnit() override;

	void SetUnit(const std::string& sUnit) override;

	bool HasRange() override;

	void GetRange(LibMCEnv_double& dMinimum, LibMCEnv_double& dMaximum) override;

	LibMCEnv::eOperationExpressionMode GetExpressionMode() override;

	void SetExpressionMode(const LibMCEnv::eOperationExpressionMode eExpressionMode) override;

	bool IsRequired() override;

	void SetRequired(const bool bIsRequired) override;

	void AddEnumOption(const std::string& sValue, const std::string& sDisplayName) override;

	LibMCEnv_uint32 GetEnumOptionCount() override;

	void GetEnumOption(const LibMCEnv_uint32 nIndex, std::string& sValue, std::string& sDisplayName) override;

};


/*************************************************************************************************************************
 Class declaration of COperationParameterDefinitionIterator
**************************************************************************************************************************/

class COperationParameterDefinitionIterator : public virtual IOperationParameterDefinitionIterator, public virtual CIterator {
public:

	COperationParameterDefinitionIterator();

	virtual ~COperationParameterDefinitionIterator();

	IOperationParameterDefinition* GetCurrent() override;

	IIterator* Clone() override;

	void addDefinition(std::shared_ptr<COperationParameterDefinition> pDefinition);

};


/*************************************************************************************************************************
 Class declaration of COperationParameterContainer
**************************************************************************************************************************/

class COperationParameterContainer : public virtual IOperationParameterContainer, public virtual CBase {
protected:
	bool m_bIsMutable;

	virtual AMC::COperationParameterContainer* getContainer() = 0;

	void checkMutable();

public:

	COperationParameterContainer(bool bIsMutable);

	virtual ~COperationParameterContainer();

	bool isMutable();

	IOperationParameterDefinition* AddStringParameter(const std::string& sName, const std::string& sDisplayName, const std::string& sDefaultValue) override;

	IOperationParameterDefinition* AddIntegerParameter(const std::string& sName, const std::string& sDisplayName, const LibMCEnv_int64 nDefaultValue, const LibMCEnv_int64 nMinimum, const LibMCEnv_int64 nMaximum) override;

	IOperationParameterDefinition* AddDoubleParameter(const std::string& sName, const std::string& sDisplayName, const LibMCEnv_double dDefaultValue, const LibMCEnv_double dMinimum, const LibMCEnv_double dMaximum) override;

	IOperationParameterDefinition* AddBoolParameter(const std::string& sName, const std::string& sDisplayName, const bool bDefaultValue) override;

	IOperationParameterDefinition* AddEnumParameter(const std::string& sName, const std::string& sDisplayName, const std::string& sDefaultValue) override;

	bool HasParameter(const std::string& sName) override;

	IOperationParameterDefinition* FindParameter(const std::string& sName, const bool bFailIfNotExisting) override;

	IOperationParameterDefinitionIterator* ListParameters() override;

};


/*************************************************************************************************************************
 Class declaration of COperationItemListDefinition
**************************************************************************************************************************/

class COperationItemListDefinition : public virtual IOperationItemListDefinition, public virtual COperationParameterContainer {
private:
	AMC::POperationItemListDefinition m_pItemList;

protected:
	AMC::COperationParameterContainer* getContainer() override;

public:

	COperationItemListDefinition(AMC::POperationItemListDefinition pItemList, bool bIsMutable);

	virtual ~COperationItemListDefinition();

	AMC::POperationItemListDefinition getItemList();

	std::string GetName() override;

	std::string GetDisplayName() override;

	LibMCEnv_uint32 GetMaxItemCount() override;

};


/*************************************************************************************************************************
 Class declaration of COperationItemListDefinitionIterator
**************************************************************************************************************************/

class COperationItemListDefinitionIterator : public virtual IOperationItemListDefinitionIterator, public virtual CIterator {
public:

	COperationItemListDefinitionIterator();

	virtual ~COperationItemListDefinitionIterator();

	IOperationItemListDefinition* GetCurrent() override;

	IIterator* Clone() override;

	void addItemList(std::shared_ptr<COperationItemListDefinition> pItemList);

};


/*************************************************************************************************************************
 Class declaration of COperationTypeDefinition
**************************************************************************************************************************/

class COperationTypeDefinition : public virtual IOperationTypeDefinition, public virtual COperationParameterContainer {
private:
	AMC::POperationTypeDefinition m_pType;
	AMC::PResourcePackage m_pResourcePackage;

protected:
	AMC::COperationParameterContainer* getContainer() override;

public:

	COperationTypeDefinition(AMC::POperationTypeDefinition pType, bool bIsMutable, AMC::PResourcePackage pResourcePackage);

	virtual ~COperationTypeDefinition();

	AMC::POperationTypeDefinition getType();

	AMC::PResourcePackage getResourcePackage();

	std::string GetName() override;

	std::string GetTargetState() override;

	std::string GetDisplayName() override;

	void SetDisplayName(const std::string& sDisplayName) override;

	std::string GetDescription() override;

	void SetDescription(const std::string& sDescription) override;

	std::string GetCategory() override;

	void SetCategory(const std::string& sCategory) override;

	std::string GetIconResource() override;

	void SetIconResource(const std::string& sResourceName) override;

	std::string GetAllowedContexts() override;

	void SetAllowedContexts(const std::string& sContexts) override;

	IOperationItemListDefinition* AddItemList(const std::string& sName, const std::string& sDisplayName, const LibMCEnv_uint32 nMaxItemCount) override;

	IOperationItemListDefinition* FindItemList(const std::string& sName, const bool bFailIfNotExisting) override;

	IOperationItemListDefinitionIterator* ListItemLists() override;

};


/*************************************************************************************************************************
 Class declaration of COperationTypeDefinitionIterator
**************************************************************************************************************************/

class COperationTypeDefinitionIterator : public virtual IOperationTypeDefinitionIterator, public virtual CIterator {
public:

	COperationTypeDefinitionIterator();

	virtual ~COperationTypeDefinitionIterator();

	IOperationTypeDefinition* GetCurrent() override;

	IIterator* Clone() override;

	void addType(std::shared_ptr<COperationTypeDefinition> pType);

};

} // namespace Impl
} // namespace LibMCEnv

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCENV_OPERATIONTYPEDEFINITION
