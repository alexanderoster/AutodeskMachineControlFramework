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

#include "amc_ui_expression.hpp"
#include "amc_statemachinedata.hpp"
#include "common_utils.hpp"
#include "libmc_exceptiontypes.hpp"
#include <sstream>
#include <iomanip>

using namespace AMC;

namespace AMC {

	CUIExpressionFormulaResolver::CUIExpressionFormulaResolver(CStateMachineData* pStateMachineData, CUIExpressionSessionContext* pSessionContext)
		: m_pStateMachineData(pStateMachineData), m_pSessionContext(pSessionContext)
	{
	}

	CUIFormulaValue CUIExpressionFormulaResolver::resolveReference(const sUIFormulaReference& reference)
	{
		switch (reference.m_Type) {
		case eUIFormulaReferenceType::Session: {
			if (m_pSessionContext == nullptr)
				return CUIFormulaValue::fromString("");

			switch (m_pSessionContext->getSessionReferenceDataType(reference.m_sPath)) {
			case eParameterDataType::Bool:
				return CUIFormulaValue::fromBool(m_pSessionContext->evaluateSessionReferenceAsBool(reference.m_sPath));
			case eParameterDataType::Integer:
				return CUIFormulaValue::fromInteger(m_pSessionContext->evaluateSessionReferenceAsInteger(reference.m_sPath));
			case eParameterDataType::Double:
				return CUIFormulaValue::fromDouble(m_pSessionContext->evaluateSessionReferenceAsNumber(reference.m_sPath));
			default:
				return CUIFormulaValue::fromString(m_pSessionContext->evaluateSessionReference(reference.m_sPath));
			}
		}

		case eUIFormulaReferenceType::StateName:
			LibMCAssertNotNull(m_pStateMachineData);
			return CUIFormulaValue::fromString(m_pStateMachineData->getInstanceStateName(reference.m_sInstanceName));

		case eUIFormulaReferenceType::Parameter: {
			LibMCAssertNotNull(m_pStateMachineData);
			auto pParameterGroup = m_pStateMachineData->getParameterHandler(reference.m_sInstanceName)->findGroup(reference.m_sGroupName, true);

			switch (pParameterGroup->getParameterDataTypeByName(reference.m_sParameterName)) {
			case eParameterDataType::Bool:
				return CUIFormulaValue::fromBool(pParameterGroup->getBoolParameterValueByName(reference.m_sParameterName));
			case eParameterDataType::Integer:
				return CUIFormulaValue::fromInteger(pParameterGroup->getIntParameterValueByName(reference.m_sParameterName));
			case eParameterDataType::Double:
				return CUIFormulaValue::fromDouble(pParameterGroup->getDoubleParameterValueByName(reference.m_sParameterName));
			default:
				return CUIFormulaValue::fromString(pParameterGroup->getParameterValueByName(reference.m_sParameterName));
			}
		}

		default:
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDEXPRESSIONVALUE, reference.m_sPath);
		}
	}

}

CUIExpression::CUIExpression()
	: m_bHasSessionReferences(false)
{

}

CUIExpression::CUIExpression(const pugi::xml_node& xmlNode, const std::string& attributeName, bool bValueMustExist)
	: m_bHasSessionReferences(false)
{
	readFromXML(xmlNode, attributeName, "", bValueMustExist);
}

CUIExpression::CUIExpression(const pugi::xml_node& xmlNode, const std::string& attributeName)
	: m_bHasSessionReferences(false)
{
	readFromXML(xmlNode, attributeName, "", false);
}

CUIExpression::CUIExpression(const pugi::xml_node& xmlNode, const std::string& attributeName, const std::string& defaultValue)
	: m_bHasSessionReferences(false)
{
	readFromXML(xmlNode, attributeName, defaultValue, false);
}

CUIExpression::CUIExpression(const pugi::xml_node& xmlNode, const std::string& attributeName, const char* defaultValue)
	: m_bHasSessionReferences(false)
{
	readFromXML(xmlNode, attributeName, (defaultValue != nullptr) ? defaultValue : "", false);
}

void CUIExpression::setFixedValue(const std::string& sValue)
{
	m_sFixedValue = sValue;
	m_sExpressionValue = "";
	compileSyncValue();
}

void CUIExpression::setSyncValue(const std::string& sValue)
{
	m_sExpressionValue = sValue;
	m_sFixedValue = "";
	compileSyncValue();
}

void CUIExpression::compileSyncValue()
{
	m_pFormula = nullptr;
	m_bHasSessionReferences = false;

	if (AMCCommon::CUtils::trimString(m_sExpressionValue).empty())
		return;

	auto pFormula = std::make_shared<CUIFormula>(m_sExpressionValue);

	bool bIsNegated = false;
	if (pFormula->isSingleReference(bIsNegated)) {
		auto& reference = pFormula->getReferences().at(0);
		m_sExpressionValue = (bIsNegated ? "!" : "") + reference.m_sPath;
		m_bHasSessionReferences = (reference.m_Type == eUIFormulaReferenceType::Session);
	}
	else {
		m_pFormula = pFormula;
		for (auto& reference : pFormula->getReferences()) {
			if (reference.m_Type == eUIFormulaReferenceType::Session)
				m_bHasSessionReferences = true;
		}
	}
}

CUIFormulaValue CUIExpression::evaluateFormula(CStateMachineData* pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	LibMCAssertNotNull(m_pFormula.get());

	CUIExpressionFormulaResolver resolver(pStateMachineData, pSessionContext);
	return m_pFormula->evaluate(&resolver);
}

void CUIExpression::reportFormulaError(CStateMachineData* pStateMachineData, const std::exception& error)
{
	if ((pStateMachineData != nullptr) && (m_pFormula.get() != nullptr) && m_pFormula->markRuntimeErrorReported())
		pStateMachineData->reportWarning("UI formula \"" + m_pFormula->getFormula() + "\" could not be evaluated: " + error.what() + ". Further errors of this formula are not logged.");
}

void CUIExpression::readFromXML(const pugi::xml_node& xmlNode, const std::string& attributeName, const std::string& defaultValue, bool bValueMustExist)
{
	auto valueAttrib = xmlNode.attribute(attributeName.c_str());
	auto syncvalueAttrib = xmlNode.attribute(("sync:" + attributeName).c_str());
	auto formatvalueAttrib = xmlNode.attribute(("format:" + attributeName).c_str());

	std::string sValue = valueAttrib.as_string();
	std::string sSyncValue = syncvalueAttrib.as_string();
	m_sFormatString = formatvalueAttrib.as_string();

	if ((!sValue.empty()) && (!sSyncValue.empty()))
		throw ELibMCCustomException(LIBMC_ERROR_EXPRESSIONVALUEGIVENTWICE, attributeName);

	if (!sSyncValue.empty()) {
		m_sExpressionValue = sSyncValue;
		compileSyncValue();
	}
	else {
		if (!valueAttrib.empty()) {
			m_sFixedValue = sValue;
		}
		else {

			if (bValueMustExist) 
				throw ELibMCCustomException(LIBMC_ERROR_EXPRESSIONVALUEMISSING, attributeName);

			m_sFixedValue = defaultValue;
		}
	}
}

bool CUIExpression::isSessionReference(const std::string& sReference)
{
	return (sReference.rfind("session.", 0) == 0) || (sReference.rfind("$session.", 0) == 0) || (sReference.rfind("$user.", 0) == 0);
}

std::string CUIExpression::getSessionReference()
{
	if (m_pFormula.get() != nullptr) {
		for (auto& reference : m_pFormula->getReferences()) {
			if (reference.m_Type == eUIFormulaReferenceType::Session)
				return reference.m_sPath;
		}
		return "";
	}

	std::string sReference = AMCCommon::CUtils::trimString(m_sExpressionValue);
	if ((!sReference.empty()) && (sReference.at(0) == '!'))
		sReference = AMCCommon::CUtils::trimString(sReference.substr(1));

	if (isSessionReference(sReference))
		return sReference;

	return "";
}

void CUIExpression::getSessionReferences(std::vector<std::string>& references)
{
	if (m_pFormula.get() != nullptr) {
		for (auto& reference : m_pFormula->getReferences()) {
			if (reference.m_Type == eUIFormulaReferenceType::Session)
				references.push_back(reference.m_sPath);
		}
		return;
	}

	std::string sReference = getSessionReference();
	if (!sReference.empty())
		references.push_back(sReference);
}

bool CUIExpression::hasSessionReferences()
{
	return m_bHasSessionReferences;
}

bool CUIExpression::isFormula()
{
	return (m_pFormula.get() != nullptr);
}

std::string CUIExpression::evaluateValueEx(CStateMachineData* pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	if (m_pFormula.get() != nullptr) {
		try {
			return evaluateFormula(pStateMachineData, pSessionContext).toString();
		}
		catch (std::exception& error) {
			reportFormulaError(pStateMachineData, error);
			return "";
		}
	}

	if (!m_sExpressionValue.empty()) {
		std::string sSessionReference = AMCCommon::CUtils::trimString(m_sExpressionValue);
		if (isSessionReference(sSessionReference)) {
			if (pSessionContext == nullptr)
				return "";
			return pSessionContext->evaluateSessionReference(sSessionReference);
		}

		LibMCAssertNotNull(pStateMachineData);

		std::string sParameterInstanceName, sParameterGroupName, sParameterName;
		CStateMachineData::extractParameterDetailsFromDotString(m_sExpressionValue, sParameterInstanceName, sParameterGroupName, sParameterName, true, true);

		if (!sParameterName.empty()) {

			auto pParameterHandler = pStateMachineData->getParameterHandler(sParameterInstanceName);

			auto pParameterGroup = pParameterHandler->findGroup(sParameterGroupName, true);
			return pParameterGroup->getParameterValueByName(sParameterName);
		}
		else {

			if (sParameterGroupName == "$state") {
				return pStateMachineData->getInstanceStateName(sParameterInstanceName);
			}
			else {
				throw ELibMCCustomException(LIBMC_ERROR_INVALIDEXPRESSIONVALUE, m_sExpressionValue);
			}
		}
	}
	else {
		return m_sFixedValue;
	}
}

const char* parseDotString(const char* pChar, uint32_t& nPrecision)
{
	LibMCAssertNotNull(pChar);

	nPrecision = 0;

	while (true) {
		char ch = *pChar;
		if ((ch >= '0') && (ch <= '9')) {
			nPrecision = nPrecision * 10 + (uint32_t) (ch - '0');
		}
		else {
			if (ch == 'f') {
				return pChar;
			}
			else {
				throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDUIFORMAT);
			}
		} 


		pChar++;
	}

	throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDUIFORMAT);

}

std::string CUIExpression::evaluateStringValue(CStateMachineData* pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	if (m_sFormatString.empty ())
		return evaluateValueEx(pStateMachineData, pSessionContext);

	std::stringstream sStream;
	const char* pChar = m_sFormatString.c_str();

	uint32_t nPrecision = 0;

	while (*pChar) {
		char ch = *pChar;
		pChar++;

		if (ch != '%') {
			sStream << ch;
		}
		else {
			switch (*pChar) {
			case '%':
				sStream << '%';
				break;

			case 'd':
				sStream << evaluateIntegerValue (pStateMachineData, pSessionContext);
				break;

			case 'f':
				sStream << evaluateNumberValue(pStateMachineData, pSessionContext);
				break;

			case 's':
				sStream << evaluateValueEx(pStateMachineData, pSessionContext);
				break;

			case '.': 
				pChar = parseDotString(pChar + 1, nPrecision);

				sStream << std::fixed << std::setprecision(nPrecision) << evaluateNumberValue(pStateMachineData, pSessionContext);
				break;

			default:
				throw ELibMCCustomException(LIBMC_ERROR_INVALIDUIFORMAT, m_sFormatString);
			}

			pChar++;
		}
	}

	return sStream.str();
	
	
}

std::string CUIExpression::evaluateStringValue(PStateMachineData pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	return evaluateStringValue(pStateMachineData.get(), pSessionContext);
}

double CUIExpression::evaluateNumberValue(CStateMachineData* pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	if (m_pFormula.get() != nullptr) {
		try {
			return evaluateFormula(pStateMachineData, pSessionContext).toDouble();
		}
		catch (std::exception& error) {
			reportFormulaError(pStateMachineData, error);
			return 0.0;
		}
	}

	if (!m_sExpressionValue.empty()) {
		std::string sSessionReference = AMCCommon::CUtils::trimString(m_sExpressionValue);
		if (isSessionReference(sSessionReference)) {
			if (pSessionContext == nullptr)
				return 0.0;
			return pSessionContext->evaluateSessionReferenceAsNumber(sSessionReference);
		}

		LibMCAssertNotNull(pStateMachineData);

		std::string sParameterInstanceName, sParameterGroupName, sParameterName;
		CStateMachineData::extractParameterDetailsFromDotString(m_sExpressionValue, sParameterInstanceName, sParameterGroupName, sParameterName, false, false);

		auto pParameterHandler = pStateMachineData->getParameterHandler(sParameterInstanceName);
		auto pParameterGroup = pParameterHandler->findGroup(sParameterGroupName, true);
		return pParameterGroup->getDoubleParameterValueByName(sParameterName);

	}
	else {
		std::string sExpression = AMCCommon::CUtils::trimString(m_sFixedValue);
		if (sExpression.empty())
			return 0;

		std::string::const_iterator it = sExpression.begin();
		while (it != sExpression.end() && (std::isdigit(*it) || (*it == '+') || (*it == '-') || (*it == '.'))) ++it;
		bool bIsNumber = !sExpression.empty() && it == sExpression.end();

		if (!bIsNumber)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDNUMBEREXPRESSION, sExpression);

		return AMCCommon::CUtils::stringToDouble(sExpression);
	}

}

double CUIExpression::evaluateNumberValue(PStateMachineData pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	return evaluateNumberValue(pStateMachineData.get(), pSessionContext);
}

int64_t CUIExpression::evaluateIntegerValue(CStateMachineData* pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	if (m_pFormula.get() != nullptr) {
		try {
			return evaluateFormula(pStateMachineData, pSessionContext).toInteger();
		}
		catch (std::exception& error) {
			reportFormulaError(pStateMachineData, error);
			return 0;
		}
	}

	if (!m_sExpressionValue.empty()) {
		std::string sSessionReference = AMCCommon::CUtils::trimString(m_sExpressionValue);
		if (isSessionReference(sSessionReference)) {
			if (pSessionContext == nullptr)
				return 0;
			return pSessionContext->evaluateSessionReferenceAsInteger(sSessionReference);
		}

		LibMCAssertNotNull(pStateMachineData);

		std::string sParameterInstanceName, sParameterGroupName, sParameterName;
		CStateMachineData::extractParameterDetailsFromDotString(m_sExpressionValue, sParameterInstanceName, sParameterGroupName, sParameterName, false, false);

		auto pParameterHandler = pStateMachineData->getParameterHandler(sParameterInstanceName);
		auto pParameterGroup = pParameterHandler->findGroup(sParameterGroupName, true);
		return pParameterGroup->getIntParameterValueByName(sParameterName);

	}
	else {
		std::string sExpression = AMCCommon::CUtils::trimString(m_sFixedValue);
		if (sExpression.empty())
			return 0;

		std::string::const_iterator it = sExpression.begin();
		while (it != sExpression.end() && (std::isdigit(*it) || (*it == '+') || (*it == '-'))) ++it;
		bool bIsNumber = !sExpression.empty() && it == sExpression.end();

		if (!bIsNumber)
			throw ELibMCCustomException (LIBMC_ERROR_INVALIDINTEGEREXPRESSION, sExpression);

		return AMCCommon::CUtils::stringToIntegerWithAccuracy(sExpression, PARAMETER_INTEGERACCURACY);
	}

}

int64_t CUIExpression::evaluateIntegerValue(PStateMachineData pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	return evaluateIntegerValue(pStateMachineData.get(), pSessionContext);
}

bool CUIExpression::evaluateBoolValue(CStateMachineData* pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	if (m_pFormula.get() != nullptr) {
		try {
			return evaluateFormula(pStateMachineData, pSessionContext).toBool();
		}
		catch (std::exception& error) {
			reportFormulaError(pStateMachineData, error);
			return false;
		}
	}

	if (!m_sExpressionValue.empty()) {
		std::string sTrimmedExpression = AMCCommon::CUtils::trimString(m_sExpressionValue);

		if (sTrimmedExpression.empty())
			return false;

		std::string sExpression;
		bool bInvert;
		if (sTrimmedExpression.at(0) == '!') {
			sExpression = AMCCommon::CUtils::trimString(sTrimmedExpression.substr(1));
			bInvert = true;
		}
		else {
			sExpression = sTrimmedExpression;
			bInvert = false;
		}

		if (isSessionReference(sExpression)) {
			bool bSessionValue = false;
			if (pSessionContext != nullptr)
				bSessionValue = pSessionContext->evaluateSessionReferenceAsBool(sExpression);
			return bInvert ? !bSessionValue : bSessionValue;
		}

		LibMCAssertNotNull(pStateMachineData);

		std::string sParameterInstanceName, sParameterGroupName, sParameterName;
		CStateMachineData::extractParameterDetailsFromDotString(sExpression, sParameterInstanceName, sParameterGroupName, sParameterName, false, false);

		auto pParameterHandler = pStateMachineData->getParameterHandler(sParameterInstanceName);
		auto pParameterGroup = pParameterHandler->findGroup(sParameterGroupName, true);
		
		bool bValue = (pParameterGroup->getBoolParameterValueByName(sParameterName));

		if (bInvert)
			return !bValue;

		return bValue;

	}
	else {
		std::string sTrimmedExpression = AMCCommon::CUtils::trimString(m_sFixedValue);
		if (sTrimmedExpression.empty())
			return false;

		std::string sExpression;
		bool bInvert;
		if (sTrimmedExpression.at(0) == '!') {
			sExpression = sTrimmedExpression.substr(1);
			bInvert = true;
		}
		else {
			sExpression = sTrimmedExpression;
			bInvert = false;
		}

		// Handle "true" / "false" string literals (e.g. from XML boolean attributes)
		if (sExpression == "true")
			return !bInvert;
		if (sExpression == "false")
			return bInvert;

		std::string::const_iterator it = sExpression.begin();
		while (it != sExpression.end() && (std::isdigit(*it) || (*it == '+') || (*it == '-'))) ++it;
		bool bIsNumber = !sExpression.empty() && it == sExpression.end();

		if (!bIsNumber)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDBOOLEANEXPRESSION, sExpression);

		int64_t nValue = AMCCommon::CUtils::stringToIntegerWithAccuracy(sExpression, PARAMETER_INTEGERACCURACY);
		if (bInvert)
			return (nValue == 0);

		return (nValue != 0);
	}




}

bool CUIExpression::evaluateBoolValue(PStateMachineData pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	return evaluateBoolValue(pStateMachineData.get (), pSessionContext);
}

std::string CUIExpression::evaluateUUIDValue(CStateMachineData* pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	std::string sValue = evaluateStringValue(pStateMachineData, pSessionContext);

	if (AMCCommon::CUtils::stringIsUUIDString(sValue))
		return AMCCommon::CUtils::normalizeUUIDString(sValue);

	return AMCCommon::CUtils::createEmptyUUID();
	
}

std::string CUIExpression::evaluateUUIDValue(PStateMachineData pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	return evaluateUUIDValue(pStateMachineData.get(), pSessionContext);
}


void CUIExpression::checkExpressionSyntax(CStateMachineData* pStateMachineData)
{
	if (m_pFormula.get() != nullptr) {
		LibMCAssertNotNull(pStateMachineData);

		CUIExpressionFormulaResolver resolver(pStateMachineData, nullptr);
		for (auto& reference : m_pFormula->getReferences()) {
			if (reference.m_Type == eUIFormulaReferenceType::StateName)
				pStateMachineData->getParameterHandler(reference.m_sInstanceName);
			else if (reference.m_Type == eUIFormulaReferenceType::Parameter)
				resolver.resolveReference(reference);
		}
		return;
	}

	// Session references are validated against the declared session variables after the UI is loaded.
	if (!getSessionReference().empty())
		return;

	evaluateValueEx(pStateMachineData, nullptr);
}

void CUIExpression::checkExpressionSyntax(PStateMachineData pStateMachineData)
{
	checkExpressionSyntax(pStateMachineData.get());
}

bool CUIExpression::needsSync()
{
	return !m_sExpressionValue.empty();
}

bool CUIExpression::isEmpty(CStateMachineData* pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	std::string sValue = evaluateValueEx(pStateMachineData, pSessionContext);
	return sValue.empty();
}

bool CUIExpression::isEmpty(PStateMachineData pStateMachineData, CUIExpressionSessionContext* pSessionContext)
{
	return isEmpty(pStateMachineData.get (), pSessionContext);
}
