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


Abstract: This is the class definition of the operation type definition classes

*/

#include "libmcenv_operationtypedefinition.hpp"
#include "libmcenv_operationsequenceerrors.hpp"
#include "libmcenv_interfaceexception.hpp"

using namespace LibMCEnv::Impl;


/*************************************************************************************************************************
 Class definition of COperationParameterDefinition
**************************************************************************************************************************/

COperationParameterDefinition::COperationParameterDefinition(AMC::POperationParameterDefinition pDefinition, bool bIsMutable)
	: m_pDefinition(pDefinition), m_bIsMutable(bIsMutable)
{
	if (pDefinition.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

COperationParameterDefinition::~COperationParameterDefinition()
{
}

void COperationParameterDefinition::checkMutable()
{
	if (!m_bIsMutable)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_OPERATIONTYPEREGISTRATIONNOTALLOWED, "parameter " + m_pDefinition->getName() + " is read-only");
}

AMC::POperationParameterDefinition COperationParameterDefinition::getDefinition()
{
	return m_pDefinition;
}

bool COperationParameterDefinition::isMutable()
{
	return m_bIsMutable;
}

std::string COperationParameterDefinition::GetName()
{
	return m_pDefinition->getName();
}

std::string COperationParameterDefinition::GetDisplayName()
{
	return m_pDefinition->getDisplayName();
}

void COperationParameterDefinition::SetDisplayName(const std::string& sDisplayName)
{
	checkMutable();
	m_pDefinition->setDisplayName(sDisplayName);
}

std::string COperationParameterDefinition::GetDescription()
{
	return m_pDefinition->getDescription();
}

void COperationParameterDefinition::SetDescription(const std::string& sDescription)
{
	checkMutable();
	m_pDefinition->setDescription(sDescription);
}

LibMCEnv::eOperationParameterType COperationParameterDefinition::GetParameterType()
{
	return (LibMCEnv::eOperationParameterType)m_pDefinition->getParameterType();
}

std::string COperationParameterDefinition::GetDefaultValue()
{
	return m_pDefinition->getDefaultValue();
}

std::string COperationParameterDefinition::GetUnit()
{
	return m_pDefinition->getUnit();
}

void COperationParameterDefinition::SetUnit(const std::string& sUnit)
{
	checkMutable();
	m_pDefinition->setUnit(sUnit);
}

bool COperationParameterDefinition::HasRange()
{
	return m_pDefinition->hasRange();
}

void COperationParameterDefinition::GetRange(LibMCEnv_double& dMinimum, LibMCEnv_double& dMaximum)
{
	callOperationSequenceCore([&]() { m_pDefinition->getRange(dMinimum, dMaximum); });
}

LibMCEnv::eOperationExpressionMode COperationParameterDefinition::GetExpressionMode()
{
	return (LibMCEnv::eOperationExpressionMode)m_pDefinition->getExpressionMode();
}

void COperationParameterDefinition::SetExpressionMode(const LibMCEnv::eOperationExpressionMode eExpressionMode)
{
	checkMutable();
	callOperationSequenceCore([&]() { m_pDefinition->setExpressionMode((AMC::eOperationExpressionMode)eExpressionMode); });
}

bool COperationParameterDefinition::IsRequired()
{
	return m_pDefinition->isRequired();
}

void COperationParameterDefinition::SetRequired(const bool bIsRequired)
{
	checkMutable();
	m_pDefinition->setRequired(bIsRequired);
}

void COperationParameterDefinition::AddEnumOption(const std::string& sValue, const std::string& sDisplayName)
{
	checkMutable();
	callOperationSequenceCore([&]() { m_pDefinition->addEnumOption(sValue, sDisplayName); });
}

LibMCEnv_uint32 COperationParameterDefinition::GetEnumOptionCount()
{
	return m_pDefinition->getEnumOptionCount();
}

void COperationParameterDefinition::GetEnumOption(const LibMCEnv_uint32 nIndex, std::string& sValue, std::string& sDisplayName)
{
	callOperationSequenceCore([&]() { m_pDefinition->getEnumOption(nIndex, sValue, sDisplayName); });
}


/*************************************************************************************************************************
 Class definition of COperationParameterDefinitionIterator
**************************************************************************************************************************/

COperationParameterDefinitionIterator::COperationParameterDefinitionIterator()
{
}

COperationParameterDefinitionIterator::~COperationParameterDefinitionIterator()
{
}

IOperationParameterDefinition* COperationParameterDefinitionIterator::GetCurrent()
{
	if ((m_nCurrentIndex < 0) || (m_nCurrentIndex >= (int32_t)m_List.size()))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDITERATOR);

	auto pDefinition = std::dynamic_pointer_cast<COperationParameterDefinition> (m_List[m_nCurrentIndex]);
	if (pDefinition.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCAST);

	return new COperationParameterDefinition(pDefinition->getDefinition(), pDefinition->isMutable());
}

IIterator* COperationParameterDefinitionIterator::Clone()
{
	std::unique_ptr<COperationParameterDefinitionIterator> pNewIterator(new COperationParameterDefinitionIterator());
	for (auto pBase : m_List)
		pNewIterator->m_List.push_back(pBase);
	return pNewIterator.release();
}

void COperationParameterDefinitionIterator::addDefinition(std::shared_ptr<COperationParameterDefinition> pDefinition)
{
	if (pDefinition.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
	m_List.push_back(pDefinition);
}


/*************************************************************************************************************************
 Class definition of COperationParameterContainer
**************************************************************************************************************************/

COperationParameterContainer::COperationParameterContainer(bool bIsMutable)
	: m_bIsMutable(bIsMutable)
{
}

COperationParameterContainer::~COperationParameterContainer()
{
}

bool COperationParameterContainer::isMutable()
{
	return m_bIsMutable;
}

void COperationParameterContainer::checkMutable()
{
	if (!m_bIsMutable)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_OPERATIONTYPEREGISTRATIONNOTALLOWED);
}

IOperationParameterDefinition* COperationParameterContainer::AddStringParameter(const std::string& sName, const std::string& sDisplayName, const std::string& sDefaultValue)
{
	checkMutable();
	auto pDefinition = callOperationSequenceCore([&]() { return getContainer()->addStringParameter(sName, sDisplayName, sDefaultValue); });
	return new COperationParameterDefinition(pDefinition, m_bIsMutable);
}

IOperationParameterDefinition* COperationParameterContainer::AddIntegerParameter(const std::string& sName, const std::string& sDisplayName, const LibMCEnv_int64 nDefaultValue, const LibMCEnv_int64 nMinimum, const LibMCEnv_int64 nMaximum)
{
	checkMutable();
	auto pDefinition = callOperationSequenceCore([&]() { return getContainer()->addIntegerParameter(sName, sDisplayName, nDefaultValue, nMinimum, nMaximum); });
	return new COperationParameterDefinition(pDefinition, m_bIsMutable);
}

IOperationParameterDefinition* COperationParameterContainer::AddDoubleParameter(const std::string& sName, const std::string& sDisplayName, const LibMCEnv_double dDefaultValue, const LibMCEnv_double dMinimum, const LibMCEnv_double dMaximum)
{
	checkMutable();
	auto pDefinition = callOperationSequenceCore([&]() { return getContainer()->addDoubleParameter(sName, sDisplayName, dDefaultValue, dMinimum, dMaximum); });
	return new COperationParameterDefinition(pDefinition, m_bIsMutable);
}

IOperationParameterDefinition* COperationParameterContainer::AddBoolParameter(const std::string& sName, const std::string& sDisplayName, const bool bDefaultValue)
{
	checkMutable();
	auto pDefinition = callOperationSequenceCore([&]() { return getContainer()->addBoolParameter(sName, sDisplayName, bDefaultValue); });
	return new COperationParameterDefinition(pDefinition, m_bIsMutable);
}

IOperationParameterDefinition* COperationParameterContainer::AddEnumParameter(const std::string& sName, const std::string& sDisplayName, const std::string& sDefaultValue)
{
	checkMutable();
	auto pDefinition = callOperationSequenceCore([&]() { return getContainer()->addEnumParameter(sName, sDisplayName, sDefaultValue); });
	return new COperationParameterDefinition(pDefinition, m_bIsMutable);
}

bool COperationParameterContainer::HasParameter(const std::string& sName)
{
	return getContainer()->hasParameter(sName);
}

IOperationParameterDefinition* COperationParameterContainer::FindParameter(const std::string& sName, const bool bFailIfNotExisting)
{
	auto pDefinition = callOperationSequenceCore([&]() { return getContainer()->findParameter(sName, bFailIfNotExisting); });
	if (pDefinition.get() == nullptr)
		return nullptr;
	return new COperationParameterDefinition(pDefinition, m_bIsMutable);
}

IOperationParameterDefinitionIterator* COperationParameterContainer::ListParameters()
{
	std::unique_ptr<COperationParameterDefinitionIterator> pIterator(new COperationParameterDefinitionIterator());
	for (auto& pDefinition : getContainer()->getParameters())
		pIterator->addDefinition(std::make_shared<COperationParameterDefinition>(pDefinition, m_bIsMutable));
	return pIterator.release();
}


/*************************************************************************************************************************
 Class definition of COperationItemListDefinition
**************************************************************************************************************************/

COperationItemListDefinition::COperationItemListDefinition(AMC::POperationItemListDefinition pItemList, bool bIsMutable)
	: COperationParameterContainer(bIsMutable), m_pItemList(pItemList)
{
	if (pItemList.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

COperationItemListDefinition::~COperationItemListDefinition()
{
}

AMC::COperationParameterContainer* COperationItemListDefinition::getContainer()
{
	return m_pItemList.get();
}

AMC::POperationItemListDefinition COperationItemListDefinition::getItemList()
{
	return m_pItemList;
}

std::string COperationItemListDefinition::GetName()
{
	return m_pItemList->getName();
}

std::string COperationItemListDefinition::GetDisplayName()
{
	return m_pItemList->getDisplayName();
}

LibMCEnv_uint32 COperationItemListDefinition::GetMaxItemCount()
{
	return m_pItemList->getMaxItemCount();
}


/*************************************************************************************************************************
 Class definition of COperationItemListDefinitionIterator
**************************************************************************************************************************/

COperationItemListDefinitionIterator::COperationItemListDefinitionIterator()
{
}

COperationItemListDefinitionIterator::~COperationItemListDefinitionIterator()
{
}

IOperationItemListDefinition* COperationItemListDefinitionIterator::GetCurrent()
{
	if ((m_nCurrentIndex < 0) || (m_nCurrentIndex >= (int32_t)m_List.size()))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDITERATOR);

	auto pItemList = std::dynamic_pointer_cast<COperationItemListDefinition> (m_List[m_nCurrentIndex]);
	if (pItemList.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCAST);

	return new COperationItemListDefinition(pItemList->getItemList(), pItemList->isMutable());
}

IIterator* COperationItemListDefinitionIterator::Clone()
{
	std::unique_ptr<COperationItemListDefinitionIterator> pNewIterator(new COperationItemListDefinitionIterator());
	for (auto pBase : m_List)
		pNewIterator->m_List.push_back(pBase);
	return pNewIterator.release();
}

void COperationItemListDefinitionIterator::addItemList(std::shared_ptr<COperationItemListDefinition> pItemList)
{
	if (pItemList.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
	m_List.push_back(pItemList);
}


/*************************************************************************************************************************
 Class definition of COperationTypeDefinition
**************************************************************************************************************************/

COperationTypeDefinition::COperationTypeDefinition(AMC::POperationTypeDefinition pType, bool bIsMutable, AMC::PResourcePackage pResourcePackage)
	: COperationParameterContainer(bIsMutable), m_pType(pType), m_pResourcePackage(pResourcePackage)
{
	if (pType.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

COperationTypeDefinition::~COperationTypeDefinition()
{
}

AMC::COperationParameterContainer* COperationTypeDefinition::getContainer()
{
	return m_pType.get();
}

AMC::POperationTypeDefinition COperationTypeDefinition::getType()
{
	return m_pType;
}

AMC::PResourcePackage COperationTypeDefinition::getResourcePackage()
{
	return m_pResourcePackage;
}

std::string COperationTypeDefinition::GetName()
{
	return m_pType->getName();
}

std::string COperationTypeDefinition::GetTargetState()
{
	return m_pType->getTargetState();
}

std::string COperationTypeDefinition::GetDisplayName()
{
	return m_pType->getDisplayName();
}

void COperationTypeDefinition::SetDisplayName(const std::string& sDisplayName)
{
	checkMutable();
	m_pType->setDisplayName(sDisplayName);
}

std::string COperationTypeDefinition::GetDescription()
{
	return m_pType->getDescription();
}

void COperationTypeDefinition::SetDescription(const std::string& sDescription)
{
	checkMutable();
	m_pType->setDescription(sDescription);
}

std::string COperationTypeDefinition::GetCategory()
{
	return m_pType->getCategory();
}

void COperationTypeDefinition::SetCategory(const std::string& sCategory)
{
	checkMutable();
	m_pType->setCategory(sCategory);
}

std::string COperationTypeDefinition::GetIconResource()
{
	return m_pType->getIconResource();
}

void COperationTypeDefinition::SetIconResource(const std::string& sResourceName)
{
	checkMutable();

	if (!sResourceName.empty() && (m_pResourcePackage.get() != nullptr)) {
		if (m_pResourcePackage->findEntryByName(sResourceName, false).get() == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_OPERATIONICONRESOURCENOTFOUND, sResourceName);
	}

	m_pType->setIconResource(sResourceName);
}

std::string COperationTypeDefinition::GetAllowedContexts()
{
	return m_pType->getAllowedContexts();
}

void COperationTypeDefinition::SetAllowedContexts(const std::string& sContexts)
{
	checkMutable();
	callOperationSequenceCore([&]() { m_pType->setAllowedContexts(sContexts); });
}

IOperationItemListDefinition* COperationTypeDefinition::AddItemList(const std::string& sName, const std::string& sDisplayName, const LibMCEnv_uint32 nMaxItemCount)
{
	checkMutable();
	auto pItemList = callOperationSequenceCore([&]() { return m_pType->addItemList(sName, sDisplayName, nMaxItemCount); });
	return new COperationItemListDefinition(pItemList, m_bIsMutable);
}

IOperationItemListDefinition* COperationTypeDefinition::FindItemList(const std::string& sName, const bool bFailIfNotExisting)
{
	auto pItemList = callOperationSequenceCore([&]() { return m_pType->findItemList(sName, bFailIfNotExisting); });
	if (pItemList.get() == nullptr)
		return nullptr;
	return new COperationItemListDefinition(pItemList, m_bIsMutable);
}

IOperationItemListDefinitionIterator* COperationTypeDefinition::ListItemLists()
{
	std::unique_ptr<COperationItemListDefinitionIterator> pIterator(new COperationItemListDefinitionIterator());
	for (auto& pItemList : m_pType->getItemLists())
		pIterator->addItemList(std::make_shared<COperationItemListDefinition>(pItemList, m_bIsMutable));
	return pIterator.release();
}


/*************************************************************************************************************************
 Class definition of COperationTypeDefinitionIterator
**************************************************************************************************************************/

COperationTypeDefinitionIterator::COperationTypeDefinitionIterator()
{
}

COperationTypeDefinitionIterator::~COperationTypeDefinitionIterator()
{
}

IOperationTypeDefinition* COperationTypeDefinitionIterator::GetCurrent()
{
	if ((m_nCurrentIndex < 0) || (m_nCurrentIndex >= (int32_t)m_List.size()))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDITERATOR);

	auto pType = std::dynamic_pointer_cast<COperationTypeDefinition> (m_List[m_nCurrentIndex]);
	if (pType.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCAST);

	return new COperationTypeDefinition(pType->getType(), pType->isMutable(), pType->getResourcePackage());
}

IIterator* COperationTypeDefinitionIterator::Clone()
{
	std::unique_ptr<COperationTypeDefinitionIterator> pNewIterator(new COperationTypeDefinitionIterator());
	for (auto pBase : m_List)
		pNewIterator->m_List.push_back(pBase);
	return pNewIterator.release();
}

void COperationTypeDefinitionIterator::addType(std::shared_ptr<COperationTypeDefinition> pType)
{
	if (pType.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
	m_List.push_back(pType);
}
