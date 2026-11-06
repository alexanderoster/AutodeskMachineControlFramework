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

#include "amc_operationtypes.hpp"
#include "amc_operationvalues.hpp"

#include "libmc_exceptiontypes.hpp"
#include "common_utils.hpp"

namespace AMC {

	/*************************************************************************************************************************
	 COperationParameterDefinition
	**************************************************************************************************************************/

	COperationParameterDefinition::COperationParameterDefinition(const std::string& sName, const std::string& sDisplayName, eOperationParameterType parameterType, const std::string& sDefaultValue, bool bHasRange, double dMinimum, double dMaximum)
		: m_sName(sName),
		m_sDisplayName(sDisplayName),
		m_ParameterType(parameterType),
		m_sDefaultValue(sDefaultValue),
		m_bHasRange(bHasRange),
		m_dMinimum(bHasRange ? dMinimum : 0.0),
		m_dMaximum(bHasRange ? dMaximum : 0.0),
		m_ExpressionMode(eOperationExpressionMode::LiteralOrReference),
		m_bRequired(false)
	{
		if (isNumeric() || (parameterType == eOperationParameterType::Bool))
			m_ExpressionMode = eOperationExpressionMode::Formula;
	}

	COperationParameterDefinition::~COperationParameterDefinition()
	{
	}

	std::string COperationParameterDefinition::getName() const
	{
		return m_sName;
	}

	std::string COperationParameterDefinition::getDisplayName() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_sDisplayName;
	}

	void COperationParameterDefinition::setDisplayName(const std::string& sDisplayName)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_sDisplayName = sDisplayName;
	}

	std::string COperationParameterDefinition::getDescription() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_sDescription;
	}

	void COperationParameterDefinition::setDescription(const std::string& sDescription)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_sDescription = sDescription;
	}

	eOperationParameterType COperationParameterDefinition::getParameterType() const
	{
		return m_ParameterType;
	}

	bool COperationParameterDefinition::isNumeric() const
	{
		return (m_ParameterType == eOperationParameterType::Integer) || (m_ParameterType == eOperationParameterType::Double);
	}

	bool COperationParameterDefinition::isStringLike() const
	{
		return (m_ParameterType == eOperationParameterType::String) || (m_ParameterType == eOperationParameterType::Enum);
	}

	std::string COperationParameterDefinition::getDefaultValue() const
	{
		return m_sDefaultValue;
	}

	std::string COperationParameterDefinition::getUnit() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_sUnit;
	}

	void COperationParameterDefinition::setUnit(const std::string& sUnit)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_sUnit = sUnit;
	}

	bool COperationParameterDefinition::hasRange() const
	{
		return m_bHasRange;
	}

	void COperationParameterDefinition::getRange(double& dMinimum, double& dMaximum) const
	{
		if (!m_bHasRange)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONPARAMETERHASNORANGE, m_sName);

		dMinimum = m_dMinimum;
		dMaximum = m_dMaximum;
	}

	double COperationParameterDefinition::clampValue(double dValue) const
	{
		if (!m_bHasRange)
			return dValue;
		if (dValue < m_dMinimum)
			return m_dMinimum;
		if (dValue > m_dMaximum)
			return m_dMaximum;
		return dValue;
	}

	eOperationExpressionMode COperationParameterDefinition::getExpressionMode() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_ExpressionMode;
	}

	void COperationParameterDefinition::setExpressionMode(eOperationExpressionMode expressionMode)
	{
		switch (expressionMode) {
		case eOperationExpressionMode::LiteralOnly:
		case eOperationExpressionMode::LiteralOrReference:
			break;
		case eOperationExpressionMode::Formula:
			if (isStringLike())
				throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONEXPRESSIONMODE, m_sName + ": formulas are only allowed for numeric and bool parameters");
			break;
		default:
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONEXPRESSIONMODE, m_sName);
		}

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_ExpressionMode = expressionMode;
	}

	bool COperationParameterDefinition::isRequired() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_bRequired;
	}

	void COperationParameterDefinition::setRequired(bool bRequired)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_bRequired = bRequired;
	}

	void COperationParameterDefinition::addEnumOption(const std::string& sValue, const std::string& sDisplayName)
	{
		if (m_ParameterType != eOperationParameterType::Enum)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONTYPEDEFINITIONMISMATCH, m_sName + " is not an enum parameter");
		if (sValue.empty())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDPARAM, m_sName + ": empty enum option");

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		for (auto& option : m_EnumOptions) {
			if (option.first == sValue) {
				if (option.second != sDisplayName)
					throw ELibMCCustomException(LIBMC_ERROR_OPERATIONTYPEDEFINITIONMISMATCH, m_sName + ": enum option " + sValue + " already exists with a different display name");
				return;
			}
		}

		m_EnumOptions.push_back(std::make_pair(sValue, sDisplayName));
	}

	uint32_t COperationParameterDefinition::getEnumOptionCount() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return (uint32_t)m_EnumOptions.size();
	}

	void COperationParameterDefinition::getEnumOption(uint32_t nIndex, std::string& sValue, std::string& sDisplayName) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		if (nIndex >= m_EnumOptions.size())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONENUMOPTIONINDEX, m_sName + ": " + std::to_string(nIndex));

		sValue = m_EnumOptions.at(nIndex).first;
		sDisplayName = m_EnumOptions.at(nIndex).second;
	}

	bool COperationParameterDefinition::hasEnumOption(const std::string& sValue) const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		for (auto& option : m_EnumOptions)
			if (option.first == sValue)
				return true;
		return false;
	}

	std::vector<std::pair<std::string, std::string>> COperationParameterDefinition::getEnumOptions() const
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_EnumOptions;
	}

	bool COperationParameterDefinition::hasSameDefinition(const std::string& sDisplayName, eOperationParameterType parameterType, const std::string& sDefaultValue, bool bHasRange, double dMinimum, double dMaximum) const
	{
		if (parameterType != m_ParameterType)
			return false;
		if (sDefaultValue != m_sDefaultValue)
			return false;
		if (bHasRange != m_bHasRange)
			return false;
		if (bHasRange && ((dMinimum != m_dMinimum) || (dMaximum != m_dMaximum)))
			return false;

		return (sDisplayName == getDisplayName());
	}

	std::string COperationParameterDefinition::parameterTypeToString(eOperationParameterType parameterType)
	{
		switch (parameterType) {
		case eOperationParameterType::String: return "string";
		case eOperationParameterType::Integer: return "integer";
		case eOperationParameterType::Double: return "double";
		case eOperationParameterType::Bool: return "bool";
		case eOperationParameterType::Enum: return "enum";
		default: return "unknown";
		}
	}

	std::string COperationParameterDefinition::expressionModeToString(eOperationExpressionMode expressionMode)
	{
		switch (expressionMode) {
		case eOperationExpressionMode::LiteralOnly: return "literal";
		case eOperationExpressionMode::LiteralOrReference: return "reference";
		case eOperationExpressionMode::Formula: return "formula";
		default: return "unknown";
		}
	}


	/*************************************************************************************************************************
	 COperationParameterContainer
	**************************************************************************************************************************/

	COperationParameterContainer::COperationParameterContainer()
	{
	}

	COperationParameterContainer::~COperationParameterContainer()
	{
	}

	bool COperationParameterContainer::nameIsReserved(const std::string& sName)
	{
		return isReservedAttributeName(sName);
	}

	bool COperationParameterContainer::isReservedAttributeName(const std::string& sName)
	{
		return (sName == "type") || (sName == "uuid") || (sName == "enabled") || (sName == "comment");
	}

	bool COperationParameterContainer::isValidIdentifier(const std::string& sName)
	{
		return AMCCommon::CUtils::stringIsValidAlphanumericNameString(sName);
	}

	POperationParameterDefinition COperationParameterContainer::addParameter(const std::string& sName, const std::string& sDisplayName, eOperationParameterType parameterType, const std::string& sDefaultValue, bool bHasRange, double dMinimum, double dMaximum)
	{
		if (!isValidIdentifier(sName))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONPARAMETERNAME, sName);
		if (nameIsReserved(sName))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONPARAMETERNAME, sName + " is a reserved name");

		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);

		auto iIter = m_ParameterMap.find(sName);
		if (iIter != m_ParameterMap.end()) {
			if (!iIter->second->hasSameDefinition(sDisplayName, parameterType, sDefaultValue, bHasRange, dMinimum, dMaximum))
				throw ELibMCCustomException(LIBMC_ERROR_OPERATIONTYPEDEFINITIONMISMATCH, "parameter " + sName);
			return iIter->second;
		}

		auto pDefinition = std::make_shared<COperationParameterDefinition>(sName, sDisplayName, parameterType, sDefaultValue, bHasRange, dMinimum, dMaximum);
		m_Parameters.push_back(pDefinition);
		m_ParameterMap.insert(std::make_pair(sName, pDefinition));
		return pDefinition;
	}

	POperationParameterDefinition COperationParameterContainer::addStringParameter(const std::string& sName, const std::string& sDisplayName, const std::string& sDefaultValue)
	{
		return addParameter(sName, sDisplayName, eOperationParameterType::String, sDefaultValue, false, 0.0, 0.0);
	}

	POperationParameterDefinition COperationParameterContainer::addIntegerParameter(const std::string& sName, const std::string& sDisplayName, int64_t nDefaultValue, int64_t nMinimum, int64_t nMaximum)
	{
		bool bHasRange = (nMaximum >= nMinimum);
		if (bHasRange && ((nDefaultValue < nMinimum) || (nDefaultValue > nMaximum)))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONPARAMETERDEFAULT, sName + ": " + std::to_string(nDefaultValue));

		return addParameter(sName, sDisplayName, eOperationParameterType::Integer, std::to_string(nDefaultValue), bHasRange, (double)nMinimum, (double)nMaximum);
	}

	POperationParameterDefinition COperationParameterContainer::addDoubleParameter(const std::string& sName, const std::string& sDisplayName, double dDefaultValue, double dMinimum, double dMaximum)
	{
		bool bHasRange = (dMaximum >= dMinimum);
		if (bHasRange && ((dDefaultValue < dMinimum) || (dDefaultValue > dMaximum)))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONPARAMETERDEFAULT, sName + ": " + COperationValues::formatDouble(dDefaultValue));

		return addParameter(sName, sDisplayName, eOperationParameterType::Double, COperationValues::formatDouble(dDefaultValue), bHasRange, dMinimum, dMaximum);
	}

	POperationParameterDefinition COperationParameterContainer::addBoolParameter(const std::string& sName, const std::string& sDisplayName, bool bDefaultValue)
	{
		return addParameter(sName, sDisplayName, eOperationParameterType::Bool, bDefaultValue ? "1" : "0", false, 0.0, 0.0);
	}

	POperationParameterDefinition COperationParameterContainer::addEnumParameter(const std::string& sName, const std::string& sDisplayName, const std::string& sDefaultValue)
	{
		if (sDefaultValue.empty())
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONPARAMETERDEFAULT, sName + ": enum default must not be empty");

		return addParameter(sName, sDisplayName, eOperationParameterType::Enum, sDefaultValue, false, 0.0, 0.0);
	}

	bool COperationParameterContainer::hasParameter(const std::string& sName) const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		return (m_ParameterMap.find(sName) != m_ParameterMap.end());
	}

	POperationParameterDefinition COperationParameterContainer::findParameter(const std::string& sName, bool bFailIfNotExisting) const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		auto iIter = m_ParameterMap.find(sName);
		if (iIter != m_ParameterMap.end())
			return iIter->second;

		if (bFailIfNotExisting)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONPARAMETERNOTFOUND, sName);

		return nullptr;
	}

	std::vector<POperationParameterDefinition> COperationParameterContainer::getParameters() const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		return m_Parameters;
	}


	/*************************************************************************************************************************
	 COperationItemListDefinition
	**************************************************************************************************************************/

	COperationItemListDefinition::COperationItemListDefinition(const std::string& sName, const std::string& sDisplayName, uint32_t nMaxItemCount)
		: m_sName(sName), m_sDisplayName(sDisplayName), m_nMaxItemCount(nMaxItemCount)
	{
	}

	COperationItemListDefinition::~COperationItemListDefinition()
	{
	}

	std::string COperationItemListDefinition::getName() const
	{
		return m_sName;
	}

	std::string COperationItemListDefinition::getDisplayName() const
	{
		return m_sDisplayName;
	}

	uint32_t COperationItemListDefinition::getMaxItemCount() const
	{
		return m_nMaxItemCount;
	}


	/*************************************************************************************************************************
	 COperationTypeDefinition
	**************************************************************************************************************************/

	COperationTypeDefinition::COperationTypeDefinition(const std::string& sName, const std::string& sTargetState)
		: m_sName(sName), m_sTargetState(sTargetState), m_sDisplayName(sName)
	{
	}

	COperationTypeDefinition::~COperationTypeDefinition()
	{
	}

	bool COperationTypeDefinition::nameIsReserved(const std::string& sName)
	{
		if (COperationParameterContainer::nameIsReserved(sName))
			return true;

		return (m_ItemListMap.find(sName) != m_ItemListMap.end());
	}

	std::string COperationTypeDefinition::getName() const
	{
		return m_sName;
	}

	std::string COperationTypeDefinition::getTargetState() const
	{
		return m_sTargetState;
	}

	std::string COperationTypeDefinition::getDisplayName() const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		return m_sDisplayName;
	}

	void COperationTypeDefinition::setDisplayName(const std::string& sDisplayName)
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		m_sDisplayName = sDisplayName;
	}

	std::string COperationTypeDefinition::getDescription() const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		return m_sDescription;
	}

	void COperationTypeDefinition::setDescription(const std::string& sDescription)
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		m_sDescription = sDescription;
	}

	std::string COperationTypeDefinition::getCategory() const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		return m_sCategory;
	}

	void COperationTypeDefinition::setCategory(const std::string& sCategory)
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		m_sCategory = sCategory;
	}

	std::string COperationTypeDefinition::getIconResource() const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		return m_sIconResource;
	}

	void COperationTypeDefinition::setIconResource(const std::string& sIconResource)
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		m_sIconResource = sIconResource;
	}

	std::string COperationTypeDefinition::getAllowedContexts() const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		return m_sAllowedContexts;
	}

	std::set<std::string> COperationTypeDefinition::getAllowedContextSet() const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		return m_AllowedContextSet;
	}

	void COperationTypeDefinition::setAllowedContexts(const std::string& sContexts)
	{
		std::vector<std::string> contexts;
		AMCCommon::CUtils::splitString(sContexts, ",", contexts);

		std::set<std::string> contextSet;
		std::string sNormalized;
		for (auto& sContext : contexts) {
			std::string sTrimmed = AMCCommon::CUtils::trimString(sContext);
			if (sTrimmed.empty())
				continue;
			if (contextSet.insert(sTrimmed).second) {
				if (!sNormalized.empty())
					sNormalized += ",";
				sNormalized += sTrimmed;
			}
		}

		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		m_AllowedContextSet = contextSet;
		m_sAllowedContexts = sNormalized;
	}

	bool COperationTypeDefinition::isAllowedInContext(const std::string& sContext) const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		if (m_AllowedContextSet.empty())
			return true;

		return (m_AllowedContextSet.find(sContext) != m_AllowedContextSet.end());
	}

	POperationItemListDefinition COperationTypeDefinition::addItemList(const std::string& sName, const std::string& sDisplayName, uint32_t nMaxItemCount)
	{
		if (!isValidIdentifier(sName) || isReservedAttributeName(sName))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONPARAMETERNAME, "item list " + sName);
		if (nMaxItemCount == 0)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDPARAM, "item list " + sName + " needs a maximum item count larger than 0");
		if (hasParameter(sName))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDOPERATIONPARAMETERNAME, "item list " + sName + " collides with a parameter");

		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		auto iIter = m_ItemListMap.find(sName);
		if (iIter != m_ItemListMap.end()) {
			if ((iIter->second->getDisplayName() != sDisplayName) || (iIter->second->getMaxItemCount() != nMaxItemCount))
				throw ELibMCCustomException(LIBMC_ERROR_OPERATIONTYPEDEFINITIONMISMATCH, "item list " + sName);
			return iIter->second;
		}

		auto pItemList = std::make_shared<COperationItemListDefinition>(sName, sDisplayName, nMaxItemCount);
		m_ItemLists.push_back(pItemList);
		m_ItemListMap.insert(std::make_pair(sName, pItemList));
		return pItemList;
	}

	POperationItemListDefinition COperationTypeDefinition::findItemList(const std::string& sName, bool bFailIfNotExisting) const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		auto iIter = m_ItemListMap.find(sName);
		if (iIter != m_ItemListMap.end())
			return iIter->second;

		if (bFailIfNotExisting)
			throw ELibMCCustomException(LIBMC_ERROR_OPERATIONITEMLISTNOTFOUND, m_sName + "." + sName);

		return nullptr;
	}

	std::vector<POperationItemListDefinition> COperationTypeDefinition::getItemLists() const
	{
		std::lock_guard<std::mutex> lockGuard(m_ContainerMutex);
		return m_ItemLists;
	}

}
