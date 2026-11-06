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

#include "amc_operationvalues.hpp"
#include "common_utils.hpp"

#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace AMC {

	static bool isFormulaWordCharacter(char cChar)
	{
		return ((cChar >= 'a') && (cChar <= 'z')) || ((cChar >= 'A') && (cChar <= 'Z')) || ((cChar >= '0') && (cChar <= '9'))
			|| (cChar == '_') || (cChar == '.') || (cChar == '$');
	}

	std::string COperationValues::formatDouble(double dValue)
	{
		if (std::isfinite(dValue) && (dValue == std::floor(dValue)) && (std::fabs(dValue) < 1.0e15))
			return std::to_string((int64_t)dValue);

		char buffer[64];
		snprintf(buffer, sizeof(buffer), "%.15g", dValue);
		return std::string(buffer);
	}

	bool COperationValues::tryParseNumber(const std::string& sValue, double& dValue)
	{
		std::string sTrimmed = AMCCommon::CUtils::trimString(sValue);
		if (sTrimmed.empty())
			return false;

		const char* pStart = sTrimmed.c_str();
		char* pEnd = nullptr;
		double dParsed = strtod(pStart, &pEnd);
		if ((pEnd == pStart) || (*pEnd != 0))
			return false;
		if (!std::isfinite(dParsed))
			return false;

		dValue = dParsed;
		return true;
	}

	bool COperationValues::isBlank(const std::string& sValue)
	{
		return (sValue.find_first_not_of(" \t\r\n") == std::string::npos);
	}

	bool COperationValues::isSingleReference(const std::string& sValue, std::string& sParameterName)
	{
		std::string sTrimmed = AMCCommon::CUtils::trimString(sValue);
		if ((sTrimmed.length() < 2) || (sTrimmed[0] != '$'))
			return false;

		std::string sName = sTrimmed.substr(1);
		if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString(sName))
			return false;

		sParameterName = sName;
		return true;
	}

	bool COperationValues::valueCountsAsExplicit(eOperationParameterType parameterType, const std::string& sRawValue)
	{
		if ((parameterType == eOperationParameterType::String) || (parameterType == eOperationParameterType::Enum))
			return true;

		return !isBlank(sRawValue);
	}

	eOperationValueKind COperationValues::classifyValue(eOperationParameterType parameterType, eOperationExpressionMode expressionMode, const std::string& sRawValue)
	{
		if (!valueCountsAsExplicit(parameterType, sRawValue))
			return eOperationValueKind::Default;

		if (expressionMode == eOperationExpressionMode::LiteralOnly)
			return eOperationValueKind::Literal;

		if ((parameterType == eOperationParameterType::String) || (parameterType == eOperationParameterType::Enum)) {
			if ((!sRawValue.empty()) && (sRawValue[0] == '$'))
				return eOperationValueKind::Reference;
			return eOperationValueKind::Literal;
		}

		std::string sParameterName;
		if (isSingleReference(sRawValue, sParameterName))
			return eOperationValueKind::Reference;

		double dValue = 0.0;
		if (tryParseNumber(sRawValue, dValue))
			return eOperationValueKind::Literal;

		if (parameterType == eOperationParameterType::Bool) {
			std::string sLower = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(sRawValue));
			if ((sLower == "true") || (sLower == "false"))
				return eOperationValueKind::Literal;
		}

		if (expressionMode == eOperationExpressionMode::LiteralOrReference)
			return eOperationValueKind::Literal;

		return eOperationValueKind::Formula;
	}

	std::string COperationValues::expandFormula(const std::string& sFormula, const std::string& sInstanceName, const std::string& sGroupName, std::vector<std::string>& referencedNames)
	{
		std::string sResult;
		size_t nLength = sFormula.length();
		size_t nIndex = 0;

		while (nIndex < nLength) {
			char cChar = sFormula[nIndex];

			if ((cChar == '\'') || (cChar == '"')) {
				size_t nEnd = sFormula.find(cChar, nIndex + 1);
				if (nEnd == std::string::npos) {
					sResult += sFormula.substr(nIndex);
					break;
				}
				sResult += sFormula.substr(nIndex, nEnd - nIndex + 1);
				nIndex = nEnd + 1;
			}
			else if (isFormulaWordCharacter(cChar)) {
				size_t nEnd = nIndex;
				while ((nEnd < nLength) && isFormulaWordCharacter(sFormula[nEnd]))
					nEnd++;

				std::string sWord = sFormula.substr(nIndex, nEnd - nIndex);
				if ((sWord.length() > 1) && (sWord[0] == '$') && (sWord.find_first_of(".$", 1) == std::string::npos)) {
					std::string sParameterName = sWord.substr(1);
					referencedNames.push_back(sParameterName);
					sResult += sInstanceName + "." + sGroupName + "." + sParameterName;
				}
				else {
					sResult += sWord;
				}
				nIndex = nEnd;
			}
			else {
				sResult += cChar;
				nIndex++;
			}
		}

		return sResult;
	}

}
