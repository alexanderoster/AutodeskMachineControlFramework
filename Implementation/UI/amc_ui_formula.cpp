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

#include "amc_ui_formula.hpp"
#include "libmc_exceptiontypes.hpp"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace AMC {

	EUIFormulaRuntimeError::EUIFormulaRuntimeError(const std::string& sMessage)
		: std::runtime_error(sMessage)
	{
	}

	static bool isFormulaWhitespace(char cChar)
	{
		return (cChar == ' ') || (cChar == '\t') || (cChar == '\r') || (cChar == '\n');
	}

	static bool isFormulaDigit(char cChar)
	{
		return (cChar >= '0') && (cChar <= '9');
	}

	static bool isFormulaWordStart(char cChar)
	{
		return ((cChar >= 'a') && (cChar <= 'z')) || ((cChar >= 'A') && (cChar <= 'Z')) || (cChar == '_') || (cChar == '$');
	}

	static bool isFormulaWordCharacter(char cChar)
	{
		return isFormulaWordStart(cChar) || isFormulaDigit(cChar) || (cChar == '.');
	}

	// Accepts an optional sign, digits with an optional fraction and an optional exponent.
	static bool parseNumberString(const std::string& sString, bool& bIsInteger, int64_t& nValue, double& dValue)
	{
		size_t nStart = 0;
		size_t nEnd = sString.length();
		while ((nStart < nEnd) && isFormulaWhitespace(sString[nStart]))
			nStart++;
		while ((nEnd > nStart) && isFormulaWhitespace(sString[nEnd - 1]))
			nEnd--;

		std::string sTrimmed = sString.substr(nStart, nEnd - nStart);
		if (sTrimmed.empty())
			return false;

		size_t nIndex = 0;
		if ((sTrimmed[nIndex] == '+') || (sTrimmed[nIndex] == '-'))
			nIndex++;

		size_t nDigitCount = 0;
		while ((nIndex < sTrimmed.length()) && isFormulaDigit(sTrimmed[nIndex])) {
			nIndex++;
			nDigitCount++;
		}

		bool bHasFraction = false;
		if ((nIndex < sTrimmed.length()) && (sTrimmed[nIndex] == '.')) {
			bHasFraction = true;
			nIndex++;
			while ((nIndex < sTrimmed.length()) && isFormulaDigit(sTrimmed[nIndex])) {
				nIndex++;
				nDigitCount++;
			}
		}

		if (nDigitCount == 0)
			return false;

		bool bHasExponent = false;
		if ((nIndex < sTrimmed.length()) && ((sTrimmed[nIndex] == 'e') || (sTrimmed[nIndex] == 'E'))) {
			bHasExponent = true;
			nIndex++;
			if ((nIndex < sTrimmed.length()) && ((sTrimmed[nIndex] == '+') || (sTrimmed[nIndex] == '-')))
				nIndex++;

			size_t nExponentDigits = 0;
			while ((nIndex < sTrimmed.length()) && isFormulaDigit(sTrimmed[nIndex])) {
				nIndex++;
				nExponentDigits++;
			}
			if (nExponentDigits == 0)
				return false;
		}

		if (nIndex != sTrimmed.length())
			return false;

		if (!bHasFraction && !bHasExponent) {
			std::istringstream integerStream(sTrimmed);
			integerStream.imbue(std::locale::classic());
			int64_t nParsedValue = 0;
			integerStream >> nParsedValue;
			if (!integerStream.fail()) {
				bIsInteger = true;
				nValue = nParsedValue;
				dValue = (double)nParsedValue;
				return true;
			}
		}

		std::istringstream doubleStream(sTrimmed);
		doubleStream.imbue(std::locale::classic());
		double dParsedValue = 0.0;
		doubleStream >> dParsedValue;
		if (doubleStream.fail() || !std::isfinite(dParsedValue))
			return false;

		bIsInteger = false;
		nValue = 0;
		dValue = dParsedValue;
		return true;
	}

	static int64_t roundToInteger(double dValue)
	{
		if (!((dValue > -9.2e18) && (dValue < 9.2e18)))
			throw EUIFormulaRuntimeError("number is out of the integer range");

		return (int64_t)std::llround(dValue);
	}

	static std::string formatDouble(double dValue)
	{
		if (dValue == 0.0)
			return "0";

		std::ostringstream sStream;
		sStream.imbue(std::locale::classic());
		sStream << std::setprecision(15) << dValue;
		return sStream.str();
	}

	static std::string formatFixed(double dValue, int64_t nDecimals)
	{
		double dHalfUnit = 0.5 * std::pow(10.0, -(double)nDecimals);
		if (std::fabs(dValue) < dHalfUnit)
			dValue = 0.0;

		std::ostringstream sStream;
		sStream.imbue(std::locale::classic());
		sStream << std::fixed << std::setprecision((int)nDecimals) << dValue;
		return sStream.str();
	}


	CUIFormulaValue::CUIFormulaValue()
		: m_Type(eUIFormulaValueType::String), m_bValue(false), m_nValue(0), m_dValue(0.0)
	{
	}

	CUIFormulaValue CUIFormulaValue::fromBool(bool bValue)
	{
		CUIFormulaValue value;
		value.m_Type = eUIFormulaValueType::Bool;
		value.m_bValue = bValue;
		return value;
	}

	CUIFormulaValue CUIFormulaValue::fromInteger(int64_t nValue)
	{
		CUIFormulaValue value;
		value.m_Type = eUIFormulaValueType::Integer;
		value.m_nValue = nValue;
		return value;
	}

	CUIFormulaValue CUIFormulaValue::fromDouble(double dValue)
	{
		if (!std::isfinite(dValue))
			throw EUIFormulaRuntimeError("result is not a finite number");

		CUIFormulaValue value;
		value.m_Type = eUIFormulaValueType::Double;
		value.m_dValue = dValue;
		return value;
	}

	CUIFormulaValue CUIFormulaValue::fromString(const std::string& sValue)
	{
		CUIFormulaValue value;
		value.m_Type = eUIFormulaValueType::String;
		value.m_sValue = sValue;
		return value;
	}

	eUIFormulaValueType CUIFormulaValue::getType() const
	{
		return m_Type;
	}

	bool CUIFormulaValue::isNumeric() const
	{
		return (m_Type != eUIFormulaValueType::String);
	}

	bool CUIFormulaValue::toBool() const
	{
		switch (m_Type) {
		case eUIFormulaValueType::Bool:
			return m_bValue;
		case eUIFormulaValueType::Integer:
			return (m_nValue != 0);
		case eUIFormulaValueType::Double:
			return (m_dValue != 0.0);
		default: {
			if (m_sValue.empty() || (m_sValue == "0"))
				return false;

			std::string sLower = m_sValue;
			for (auto& cChar : sLower)
				cChar = (char)std::tolower((unsigned char)cChar);
			return (sLower != "false");
		}
		}
	}

	int64_t CUIFormulaValue::toInteger() const
	{
		switch (m_Type) {
		case eUIFormulaValueType::Bool:
			return m_bValue ? 1 : 0;
		case eUIFormulaValueType::Integer:
			return m_nValue;
		case eUIFormulaValueType::Double:
			return roundToInteger(m_dValue);
		default: {
			if (m_sValue.empty())
				return 0;

			bool bIsInteger = false;
			int64_t nValue = 0;
			double dValue = 0.0;
			if (!parseNumberString(m_sValue, bIsInteger, nValue, dValue))
				throw EUIFormulaRuntimeError("'" + m_sValue + "' is not a number");

			return bIsInteger ? nValue : roundToInteger(dValue);
		}
		}
	}

	double CUIFormulaValue::toDouble() const
	{
		switch (m_Type) {
		case eUIFormulaValueType::Bool:
			return m_bValue ? 1.0 : 0.0;
		case eUIFormulaValueType::Integer:
			return (double)m_nValue;
		case eUIFormulaValueType::Double:
			return m_dValue;
		default: {
			if (m_sValue.empty())
				return 0.0;

			bool bIsInteger = false;
			int64_t nValue = 0;
			double dValue = 0.0;
			if (!parseNumberString(m_sValue, bIsInteger, nValue, dValue))
				throw EUIFormulaRuntimeError("'" + m_sValue + "' is not a number");

			return dValue;
		}
		}
	}

	std::string CUIFormulaValue::toString() const
	{
		switch (m_Type) {
		case eUIFormulaValueType::Bool:
			return m_bValue ? "1" : "0";
		case eUIFormulaValueType::Integer:
			return std::to_string(m_nValue);
		case eUIFormulaValueType::Double:
			return formatDouble(m_dValue);
		default:
			return m_sValue;
		}
	}

	bool CUIFormulaValue::tryGetNumber(bool& bIsInteger, int64_t& nValue, double& dValue) const
	{
		switch (m_Type) {
		case eUIFormulaValueType::Bool:
			bIsInteger = true;
			nValue = m_bValue ? 1 : 0;
			dValue = (double)nValue;
			return true;
		case eUIFormulaValueType::Integer:
			bIsInteger = true;
			nValue = m_nValue;
			dValue = (double)m_nValue;
			return true;
		case eUIFormulaValueType::Double:
			bIsInteger = false;
			nValue = 0;
			dValue = m_dValue;
			return true;
		default:
			return parseNumberString(m_sValue, bIsInteger, nValue, dValue);
		}
	}


	enum class eUIFormulaTokenType : uint32_t {
		End = 0,
		Literal = 1,
		Word = 2,
		LeftParenthesis = 3,
		RightParenthesis = 4,
		Comma = 5,
		Plus = 6,
		Minus = 7,
		Star = 8,
		Slash = 9,
		Percent = 10,
		Equal = 11,
		NotEqual = 12,
		Less = 13,
		LessEqual = 14,
		Greater = 15,
		GreaterEqual = 16,
		And = 17,
		Or = 18,
		Not = 19
	};

	struct sUIFormulaToken {
		eUIFormulaTokenType m_Type;
		std::string m_sText;
		CUIFormulaValue m_Value;
		size_t m_nPosition;
	};

	struct sUIFormulaFunctionDefinition {
		const char* m_pszName;
		eUIFormulaFunction m_Function;
		size_t m_nMinArguments;
		size_t m_nMaxArguments;
	};

	static const sUIFormulaFunctionDefinition s_FormulaFunctions[] = {
		{ "if", eUIFormulaFunction::If, 3, 3 },
		{ "between", eUIFormulaFunction::Between, 3, 3 },
		{ "coalesce", eUIFormulaFunction::Coalesce, 1, AMC_UI_FORMULA_MAXARGUMENTS },
		{ "min", eUIFormulaFunction::Min, 1, AMC_UI_FORMULA_MAXARGUMENTS },
		{ "max", eUIFormulaFunction::Max, 1, AMC_UI_FORMULA_MAXARGUMENTS },
		{ "clamp", eUIFormulaFunction::Clamp, 3, 3 },
		{ "abs", eUIFormulaFunction::Abs, 1, 1 },
		{ "round", eUIFormulaFunction::Round, 1, 2 },
		{ "floor", eUIFormulaFunction::Floor, 1, 1 },
		{ "ceil", eUIFormulaFunction::Ceil, 1, 1 },
		{ "concat", eUIFormulaFunction::Concat, 1, AMC_UI_FORMULA_MAXARGUMENTS },
		{ "fixed", eUIFormulaFunction::Fixed, 2, 2 },
		{ "upper", eUIFormulaFunction::Upper, 1, 1 },
		{ "lower", eUIFormulaFunction::Lower, 1, 1 },
		{ "len", eUIFormulaFunction::Len, 1, 1 },
		{ "contains", eUIFormulaFunction::Contains, 2, 2 },
		{ "startswith", eUIFormulaFunction::StartsWith, 2, 2 }
	};

	class CUIFormulaParser {
	private:
		CUIFormula* m_pFormula;
		std::vector<sUIFormulaToken> m_Tokens;
		size_t m_nTokenIndex;
		uint32_t m_nDepth;

		class CDepthGuard {
		private:
			CUIFormulaParser* m_pParser;
		public:
			CDepthGuard(CUIFormulaParser* pParser, size_t nPosition)
				: m_pParser(pParser)
			{
				m_pParser->m_nDepth++;
				if (m_pParser->m_nDepth > AMC_UI_FORMULA_MAXDEPTH)
					m_pParser->throwSyntaxError("formula is nested too deeply", nPosition);
			}

			~CDepthGuard()
			{
				m_pParser->m_nDepth--;
			}
		};

		[[noreturn]] void throwSyntaxError(const std::string& sMessage, size_t nPosition)
		{
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDEXPRESSIONVALUE, sMessage + " at position " + std::to_string(nPosition + 1) + " in formula \"" + m_pFormula->m_sFormula + "\"");
		}

		void addToken(eUIFormulaTokenType tokenType, const std::string& sText, size_t nPosition)
		{
			sUIFormulaToken token;
			token.m_Type = tokenType;
			token.m_sText = sText;
			token.m_nPosition = nPosition;
			m_Tokens.push_back(token);
		}

		void addLiteralToken(const CUIFormulaValue& value, const std::string& sText, size_t nPosition)
		{
			sUIFormulaToken token;
			token.m_Type = eUIFormulaTokenType::Literal;
			token.m_sText = sText;
			token.m_Value = value;
			token.m_nPosition = nPosition;
			m_Tokens.push_back(token);
		}

		void tokenize()
		{
			const std::string& sFormula = m_pFormula->m_sFormula;
			size_t nLength = sFormula.length();
			size_t nIndex = 0;

			while (nIndex < nLength) {
				char cChar = sFormula[nIndex];
				char cNext = (nIndex + 1 < nLength) ? sFormula[nIndex + 1] : 0;
				size_t nStart = nIndex;

				if (isFormulaWhitespace(cChar)) {
					nIndex++;
				}
				else if (isFormulaDigit(cChar) || ((cChar == '.') && isFormulaDigit(cNext))) {
					while ((nIndex < nLength) && (isFormulaDigit(sFormula[nIndex]) || (sFormula[nIndex] == '.')))
						nIndex++;
					if ((nIndex < nLength) && ((sFormula[nIndex] == 'e') || (sFormula[nIndex] == 'E'))) {
						nIndex++;
						if ((nIndex < nLength) && ((sFormula[nIndex] == '+') || (sFormula[nIndex] == '-')))
							nIndex++;
						while ((nIndex < nLength) && isFormulaDigit(sFormula[nIndex]))
							nIndex++;
					}

					std::string sNumber = sFormula.substr(nStart, nIndex - nStart);
					if ((nIndex < nLength) && isFormulaWordCharacter(sFormula[nIndex]))
						throwSyntaxError("invalid number '" + sNumber + sFormula[nIndex] + "'", nStart);

					bool bIsInteger = false;
					int64_t nValue = 0;
					double dValue = 0.0;
					if (!parseNumberString(sNumber, bIsInteger, nValue, dValue))
						throwSyntaxError("invalid number '" + sNumber + "'", nStart);

					addLiteralToken(bIsInteger ? CUIFormulaValue::fromInteger(nValue) : CUIFormulaValue::fromDouble(dValue), sNumber, nStart);
				}
				else if ((cChar == '\'') || (cChar == '"')) {
					char cQuote = cChar;
					std::string sValue;
					nIndex++;
					bool bTerminated = false;
					while (nIndex < nLength) {
						if (sFormula[nIndex] == cQuote) {
							if ((nIndex + 1 < nLength) && (sFormula[nIndex + 1] == cQuote)) {
								sValue += cQuote;
								nIndex += 2;
							}
							else {
								nIndex++;
								bTerminated = true;
								break;
							}
						}
						else {
							sValue += sFormula[nIndex];
							nIndex++;
						}
					}

					if (!bTerminated)
						throwSyntaxError("unterminated string", nStart);

					addLiteralToken(CUIFormulaValue::fromString(sValue), sFormula.substr(nStart, nIndex - nStart), nStart);
				}
				else if (isFormulaWordStart(cChar)) {
					while ((nIndex < nLength) && isFormulaWordCharacter(sFormula[nIndex]))
						nIndex++;

					std::string sWord = sFormula.substr(nStart, nIndex - nStart);
					if ((sWord.back() == '.') || (sWord.find("..") != std::string::npos))
						throwSyntaxError("invalid reference '" + sWord + "'", nStart);

					if (sWord == "and")
						addToken(eUIFormulaTokenType::And, sWord, nStart);
					else if (sWord == "or")
						addToken(eUIFormulaTokenType::Or, sWord, nStart);
					else if (sWord == "not")
						addToken(eUIFormulaTokenType::Not, sWord, nStart);
					else if (sWord == "true")
						addLiteralToken(CUIFormulaValue::fromBool(true), sWord, nStart);
					else if (sWord == "false")
						addLiteralToken(CUIFormulaValue::fromBool(false), sWord, nStart);
					else
						addToken(eUIFormulaTokenType::Word, sWord, nStart);
				}
				else {
					nIndex++;
					switch (cChar) {
					case '(': addToken(eUIFormulaTokenType::LeftParenthesis, "(", nStart); break;
					case ')': addToken(eUIFormulaTokenType::RightParenthesis, ")", nStart); break;
					case ',': addToken(eUIFormulaTokenType::Comma, ",", nStart); break;
					case '+': addToken(eUIFormulaTokenType::Plus, "+", nStart); break;
					case '-': addToken(eUIFormulaTokenType::Minus, "-", nStart); break;
					case '*': addToken(eUIFormulaTokenType::Star, "*", nStart); break;
					case '/': addToken(eUIFormulaTokenType::Slash, "/", nStart); break;
					case '%': addToken(eUIFormulaTokenType::Percent, "%", nStart); break;
					case '=':
						if (cNext != '=')
							throwSyntaxError("use '==' to compare values", nStart);
						nIndex++;
						addToken(eUIFormulaTokenType::Equal, "==", nStart);
						break;
					case '!':
						if (cNext == '=') {
							nIndex++;
							addToken(eUIFormulaTokenType::NotEqual, "!=", nStart);
						}
						else {
							addToken(eUIFormulaTokenType::Not, "!", nStart);
						}
						break;
					case '<':
						if (cNext == '=') {
							nIndex++;
							addToken(eUIFormulaTokenType::LessEqual, "<=", nStart);
						}
						else {
							addToken(eUIFormulaTokenType::Less, "<", nStart);
						}
						break;
					case '>':
						if (cNext == '=') {
							nIndex++;
							addToken(eUIFormulaTokenType::GreaterEqual, ">=", nStart);
						}
						else {
							addToken(eUIFormulaTokenType::Greater, ">", nStart);
						}
						break;
					case '&':
						if (cNext != '&')
							throwSyntaxError("use 'and' or '&&'", nStart);
						nIndex++;
						addToken(eUIFormulaTokenType::And, "&&", nStart);
						break;
					case '|':
						if (cNext != '|')
							throwSyntaxError("use 'or' or '||'", nStart);
						nIndex++;
						addToken(eUIFormulaTokenType::Or, "||", nStart);
						break;
					default:
						throwSyntaxError(std::string("unexpected character '") + cChar + "'", nStart);
					}
				}
			}

			addToken(eUIFormulaTokenType::End, "", nLength);
		}

		const sUIFormulaToken& currentToken()
		{
			return m_Tokens.at(m_nTokenIndex);
		}

		bool acceptToken(eUIFormulaTokenType tokenType)
		{
			if (currentToken().m_Type != tokenType)
				return false;
			m_nTokenIndex++;
			return true;
		}

		[[noreturn]] void throwUnexpectedToken(const std::string& sExpected)
		{
			auto& token = currentToken();
			if (token.m_Type == eUIFormulaTokenType::End)
				throwSyntaxError("unexpected end of formula, expected " + sExpected, token.m_nPosition);
			throwSyntaxError("unexpected '" + token.m_sText + "', expected " + sExpected, token.m_nPosition);
		}

		size_t addNode(eUIFormulaNodeType nodeType, eUIFormulaOperator nodeOperator, const std::vector<size_t>& children)
		{
			sUIFormulaNode node;
			node.m_Type = nodeType;
			node.m_Operator = nodeOperator;
			node.m_Function = eUIFormulaFunction::None;
			node.m_nReferenceIndex = 0;
			node.m_Children = children;
			m_pFormula->m_Nodes.push_back(node);
			return m_pFormula->m_Nodes.size() - 1;
		}

		size_t addReference(const std::string& sPath, size_t nPosition)
		{
			auto& references = m_pFormula->m_References;
			for (size_t nIndex = 0; nIndex < references.size(); nIndex++) {
				if (references[nIndex].m_sPath == sPath)
					return nIndex;
			}

			sUIFormulaReference reference;
			reference.m_sPath = sPath;

			if ((sPath.rfind("session.", 0) == 0) || (sPath.rfind("$session.", 0) == 0) || (sPath.rfind("$user.", 0) == 0)) {
				reference.m_Type = eUIFormulaReferenceType::Session;
			}
			else {
				std::vector<std::string> segments;
				size_t nSegmentStart = 0;
				while (true) {
					size_t nDot = sPath.find('.', nSegmentStart);
					segments.push_back(sPath.substr(nSegmentStart, (nDot == std::string::npos) ? std::string::npos : nDot - nSegmentStart));
					if (nDot == std::string::npos)
						break;
					nSegmentStart = nDot + 1;
				}

				bool bInstanceIsValid = (segments.at(0).find('$') == std::string::npos);

				if ((segments.size() == 2) && (segments.at(1) == "$state") && bInstanceIsValid) {
					reference.m_Type = eUIFormulaReferenceType::StateName;
					reference.m_sInstanceName = segments.at(0);
				}
				else if ((segments.size() == 3) && bInstanceIsValid && (segments.at(1).find('$') == std::string::npos) && (segments.at(2).find('$') == std::string::npos)) {
					reference.m_Type = eUIFormulaReferenceType::Parameter;
					reference.m_sInstanceName = segments.at(0);
					reference.m_sGroupName = segments.at(1);
					reference.m_sParameterName = segments.at(2);
				}
				else {
					throwSyntaxError("invalid reference '" + sPath + "', expected instance.group.parameter, instance.$state or a session reference", nPosition);
				}
			}

			references.push_back(reference);
			return references.size() - 1;
		}

		size_t parseOr()
		{
			CDepthGuard depthGuard(this, currentToken().m_nPosition);

			size_t nLeft = parseAnd();
			while (acceptToken(eUIFormulaTokenType::Or)) {
				size_t nRight = parseAnd();
				nLeft = addNode(eUIFormulaNodeType::Or, eUIFormulaOperator::None, { nLeft, nRight });
			}
			return nLeft;
		}

		size_t parseAnd()
		{
			size_t nLeft = parseNot();
			while (acceptToken(eUIFormulaTokenType::And)) {
				size_t nRight = parseNot();
				nLeft = addNode(eUIFormulaNodeType::And, eUIFormulaOperator::None, { nLeft, nRight });
			}
			return nLeft;
		}

		size_t parseNot()
		{
			size_t nPosition = currentToken().m_nPosition;
			if (acceptToken(eUIFormulaTokenType::Not)) {
				CDepthGuard depthGuard(this, nPosition);
				size_t nOperand = parseNot();
				return addNode(eUIFormulaNodeType::Not, eUIFormulaOperator::None, { nOperand });
			}

			return parseComparison();
		}

		bool getComparisonOperator(eUIFormulaOperator& comparisonOperator)
		{
			switch (currentToken().m_Type) {
			case eUIFormulaTokenType::Equal: comparisonOperator = eUIFormulaOperator::Equal; return true;
			case eUIFormulaTokenType::NotEqual: comparisonOperator = eUIFormulaOperator::NotEqual; return true;
			case eUIFormulaTokenType::Less: comparisonOperator = eUIFormulaOperator::Less; return true;
			case eUIFormulaTokenType::LessEqual: comparisonOperator = eUIFormulaOperator::LessEqual; return true;
			case eUIFormulaTokenType::Greater: comparisonOperator = eUIFormulaOperator::Greater; return true;
			case eUIFormulaTokenType::GreaterEqual: comparisonOperator = eUIFormulaOperator::GreaterEqual; return true;
			default: return false;
			}
		}

		size_t parseComparison()
		{
			size_t nLeft = parseAdditive();

			eUIFormulaOperator comparisonOperator = eUIFormulaOperator::None;
			if (getComparisonOperator(comparisonOperator)) {
				m_nTokenIndex++;
				size_t nRight = parseAdditive();
				nLeft = addNode(eUIFormulaNodeType::Binary, comparisonOperator, { nLeft, nRight });

				if (getComparisonOperator(comparisonOperator))
					throwSyntaxError("comparisons cannot be chained, combine them with 'and'", currentToken().m_nPosition);
			}

			return nLeft;
		}

		size_t parseAdditive()
		{
			size_t nLeft = parseTerm();
			while (true) {
				eUIFormulaOperator additiveOperator;
				if (acceptToken(eUIFormulaTokenType::Plus))
					additiveOperator = eUIFormulaOperator::Add;
				else if (acceptToken(eUIFormulaTokenType::Minus))
					additiveOperator = eUIFormulaOperator::Subtract;
				else
					break;

				size_t nRight = parseTerm();
				nLeft = addNode(eUIFormulaNodeType::Binary, additiveOperator, { nLeft, nRight });
			}
			return nLeft;
		}

		size_t parseTerm()
		{
			size_t nLeft = parseUnary();
			while (true) {
				eUIFormulaOperator termOperator;
				if (acceptToken(eUIFormulaTokenType::Star))
					termOperator = eUIFormulaOperator::Multiply;
				else if (acceptToken(eUIFormulaTokenType::Slash))
					termOperator = eUIFormulaOperator::Divide;
				else if (acceptToken(eUIFormulaTokenType::Percent))
					termOperator = eUIFormulaOperator::Modulo;
				else
					break;

				size_t nRight = parseUnary();
				nLeft = addNode(eUIFormulaNodeType::Binary, termOperator, { nLeft, nRight });
			}
			return nLeft;
		}

		size_t parseUnary()
		{
			size_t nPosition = currentToken().m_nPosition;
			if (acceptToken(eUIFormulaTokenType::Minus)) {
				CDepthGuard depthGuard(this, nPosition);
				size_t nOperand = parseUnary();
				return addNode(eUIFormulaNodeType::Negate, eUIFormulaOperator::None, { nOperand });
			}

			return parsePrimary();
		}

		size_t parseCall(const sUIFormulaToken& nameToken)
		{
			const sUIFormulaFunctionDefinition* pDefinition = nullptr;
			for (auto& definition : s_FormulaFunctions) {
				if (nameToken.m_sText == definition.m_pszName) {
					pDefinition = &definition;
					break;
				}
			}

			if (pDefinition == nullptr)
				throwSyntaxError("unknown function '" + nameToken.m_sText + "'", nameToken.m_nPosition);

			std::vector<size_t> arguments;
			if (!acceptToken(eUIFormulaTokenType::RightParenthesis)) {
				while (true) {
					if (arguments.size() >= AMC_UI_FORMULA_MAXARGUMENTS)
						throwSyntaxError("too many arguments for function '" + nameToken.m_sText + "'", currentToken().m_nPosition);

					arguments.push_back(parseOr());

					if (acceptToken(eUIFormulaTokenType::RightParenthesis))
						break;
					if (!acceptToken(eUIFormulaTokenType::Comma))
						throwUnexpectedToken("',' or ')'");
				}
			}

			if ((arguments.size() < pDefinition->m_nMinArguments) || (arguments.size() > pDefinition->m_nMaxArguments)) {
				std::string sExpected = (pDefinition->m_nMinArguments == pDefinition->m_nMaxArguments) ? std::to_string(pDefinition->m_nMinArguments) :
					(pDefinition->m_nMaxArguments == AMC_UI_FORMULA_MAXARGUMENTS) ? "at least " + std::to_string(pDefinition->m_nMinArguments) :
					std::to_string(pDefinition->m_nMinArguments) + " to " + std::to_string(pDefinition->m_nMaxArguments);
				throwSyntaxError("function '" + nameToken.m_sText + "' expects " + sExpected + " argument(s)", nameToken.m_nPosition);
			}

			size_t nNode = addNode(eUIFormulaNodeType::Call, eUIFormulaOperator::None, arguments);
			m_pFormula->m_Nodes.at(nNode).m_Function = pDefinition->m_Function;
			return nNode;
		}

		size_t parsePrimary()
		{
			sUIFormulaToken token = currentToken();

			switch (token.m_Type) {
			case eUIFormulaTokenType::Literal: {
				m_nTokenIndex++;
				size_t nNode = addNode(eUIFormulaNodeType::Literal, eUIFormulaOperator::None, {});
				m_pFormula->m_Nodes.at(nNode).m_Literal = token.m_Value;
				return nNode;
			}

			case eUIFormulaTokenType::LeftParenthesis: {
				m_nTokenIndex++;
				size_t nNode = parseOr();
				if (!acceptToken(eUIFormulaTokenType::RightParenthesis))
					throwUnexpectedToken("')'");
				return nNode;
			}

			case eUIFormulaTokenType::Word: {
				m_nTokenIndex++;

				if (token.m_sText.find('.') != std::string::npos) {
					size_t nReferenceIndex = addReference(token.m_sText, token.m_nPosition);
					size_t nNode = addNode(eUIFormulaNodeType::Reference, eUIFormulaOperator::None, {});
					m_pFormula->m_Nodes.at(nNode).m_nReferenceIndex = nReferenceIndex;
					return nNode;
				}

				if (!acceptToken(eUIFormulaTokenType::LeftParenthesis))
					throwSyntaxError("unknown name '" + token.m_sText + "', expected a function call or a reference like instance.group.parameter", token.m_nPosition);

				return parseCall(token);
			}

			default:
				throwUnexpectedToken("a value, reference or function");
			}
		}

	public:

		CUIFormulaParser(CUIFormula* pFormula)
			: m_pFormula(pFormula), m_nTokenIndex(0), m_nDepth(0)
		{
		}

		void parse()
		{
			const std::string& sFormula = m_pFormula->m_sFormula;
			if (sFormula.length() > AMC_UI_FORMULA_MAXLENGTH)
				throwSyntaxError("formula is longer than " + std::to_string(AMC_UI_FORMULA_MAXLENGTH) + " characters", 0);

			tokenize();

			if (currentToken().m_Type == eUIFormulaTokenType::End)
				throwSyntaxError("empty formula", 0);

			m_pFormula->m_nRootNode = parseOr();

			if (currentToken().m_Type != eUIFormulaTokenType::End)
				throwUnexpectedToken("an operator or the end of the formula");
		}
	};


	static void getNumericOperand(const CUIFormulaValue& value, bool& bIsInteger, int64_t& nValue, double& dValue)
	{
		if ((value.getType() == eUIFormulaValueType::String) && value.toString().empty()) {
			bIsInteger = true;
			nValue = 0;
			dValue = 0.0;
			return;
		}

		if (!value.tryGetNumber(bIsInteger, nValue, dValue))
			throw EUIFormulaRuntimeError("'" + value.toString() + "' is not a number");
	}

	static CUIFormulaValue makeNumber(bool bIsInteger, int64_t nValue, double dValue)
	{
		return bIsInteger ? CUIFormulaValue::fromInteger(nValue) : CUIFormulaValue::fromDouble(dValue);
	}

	// Strings compare as strings; otherwise both sides must be numbers. Returns false if the values are not comparable.
	static bool compareValues(const CUIFormulaValue& left, const CUIFormulaValue& right, int& nResult)
	{
		if ((left.getType() == eUIFormulaValueType::String) && (right.getType() == eUIFormulaValueType::String)) {
			int nCompare = left.toString().compare(right.toString());
			nResult = (nCompare < 0) ? -1 : ((nCompare > 0) ? 1 : 0);
			return true;
		}

		bool bLeftInteger = false, bRightInteger = false;
		int64_t nLeft = 0, nRight = 0;
		double dLeft = 0.0, dRight = 0.0;
		if (!left.tryGetNumber(bLeftInteger, nLeft, dLeft) || !right.tryGetNumber(bRightInteger, nRight, dRight))
			return false;

		if (bLeftInteger && bRightInteger)
			nResult = (nLeft < nRight) ? -1 : ((nLeft > nRight) ? 1 : 0);
		else
			nResult = (dLeft < dRight) ? -1 : ((dLeft > dRight) ? 1 : 0);

		return true;
	}

	static int compareOrdered(const CUIFormulaValue& left, const CUIFormulaValue& right)
	{
		int nResult = 0;
		if (!compareValues(left, right, nResult))
			throw EUIFormulaRuntimeError("cannot compare '" + left.toString() + "' with '" + right.toString() + "'");
		return nResult;
	}

	static CUIFormulaValue evaluateBinary(eUIFormulaOperator binaryOperator, const CUIFormulaValue& left, const CUIFormulaValue& right)
	{
		switch (binaryOperator) {
		case eUIFormulaOperator::Equal:
		case eUIFormulaOperator::NotEqual: {
			int nResult = 0;
			bool bEqual = compareValues(left, right, nResult) && (nResult == 0);
			return CUIFormulaValue::fromBool((binaryOperator == eUIFormulaOperator::Equal) ? bEqual : !bEqual);
		}
		case eUIFormulaOperator::Less:
			return CUIFormulaValue::fromBool(compareOrdered(left, right) < 0);
		case eUIFormulaOperator::LessEqual:
			return CUIFormulaValue::fromBool(compareOrdered(left, right) <= 0);
		case eUIFormulaOperator::Greater:
			return CUIFormulaValue::fromBool(compareOrdered(left, right) > 0);
		case eUIFormulaOperator::GreaterEqual:
			return CUIFormulaValue::fromBool(compareOrdered(left, right) >= 0);
		default:
			break;
		}

		bool bLeftInteger = false, bRightInteger = false;
		int64_t nLeft = 0, nRight = 0;
		double dLeft = 0.0, dRight = 0.0;
		getNumericOperand(left, bLeftInteger, nLeft, dLeft);
		getNumericOperand(right, bRightInteger, nRight, dRight);
		bool bIntegers = bLeftInteger && bRightInteger;

		const int64_t nMaxInteger = (std::numeric_limits<int64_t>::max)();
		const int64_t nMinInteger = (std::numeric_limits<int64_t>::min)();

		switch (binaryOperator) {
		case eUIFormulaOperator::Add:
			if (bIntegers && !(((nRight > 0) && (nLeft > nMaxInteger - nRight)) || ((nRight < 0) && (nLeft < nMinInteger - nRight))))
				return CUIFormulaValue::fromInteger(nLeft + nRight);
			return CUIFormulaValue::fromDouble(dLeft + dRight);

		case eUIFormulaOperator::Subtract:
			if (bIntegers && !(((nRight < 0) && (nLeft > nMaxInteger + nRight)) || ((nRight > 0) && (nLeft < nMinInteger + nRight))))
				return CUIFormulaValue::fromInteger(nLeft - nRight);
			return CUIFormulaValue::fromDouble(dLeft - dRight);

		case eUIFormulaOperator::Multiply:
			if (bIntegers && (std::fabs(dLeft * dRight) < 9.0e18))
				return CUIFormulaValue::fromInteger(nLeft * nRight);
			return CUIFormulaValue::fromDouble(dLeft * dRight);

		case eUIFormulaOperator::Divide:
			if (dRight == 0.0)
				throw EUIFormulaRuntimeError("division by zero");
			return CUIFormulaValue::fromDouble(dLeft / dRight);

		case eUIFormulaOperator::Modulo:
			if (dRight == 0.0)
				throw EUIFormulaRuntimeError("division by zero");
			if (bIntegers) {
				if (nRight == -1)
					return CUIFormulaValue::fromInteger(0);
				return CUIFormulaValue::fromInteger(nLeft % nRight);
			}
			return CUIFormulaValue::fromDouble(std::fmod(dLeft, dRight));

		default:
			throw EUIFormulaRuntimeError("invalid operator");
		}
	}

	static int64_t getDecimalsArgument(const CUIFormulaValue& value)
	{
		int64_t nDecimals = value.toInteger();
		if ((nDecimals < 0) || (nDecimals > AMC_UI_FORMULA_MAXDECIMALS))
			throw EUIFormulaRuntimeError("number of decimals must be between 0 and " + std::to_string(AMC_UI_FORMULA_MAXDECIMALS));
		return nDecimals;
	}

	static CUIFormulaValue roundedToIntegerIfPossible(double dValue)
	{
		if ((dValue > -9.0e18) && (dValue < 9.0e18))
			return CUIFormulaValue::fromInteger((int64_t)dValue);
		return CUIFormulaValue::fromDouble(dValue);
	}


	CUIFormula::CUIFormula(const std::string& sFormula)
		: m_sFormula(sFormula), m_nRootNode(0), m_bRuntimeErrorReported(false)
	{
		CUIFormulaParser parser(this);
		parser.parse();
	}

	const std::string& CUIFormula::getFormula() const
	{
		return m_sFormula;
	}

	const std::vector<sUIFormulaReference>& CUIFormula::getReferences() const
	{
		return m_References;
	}

	bool CUIFormula::isSingleReference(bool& bIsNegated) const
	{
		auto& rootNode = m_Nodes.at(m_nRootNode);
		if (rootNode.m_Type == eUIFormulaNodeType::Reference) {
			bIsNegated = false;
			return true;
		}

		if ((rootNode.m_Type == eUIFormulaNodeType::Not) && (m_Nodes.at(rootNode.m_Children.at(0)).m_Type == eUIFormulaNodeType::Reference)) {
			bIsNegated = true;
			return true;
		}

		return false;
	}

	CUIFormulaValue CUIFormula::evaluate(CUIFormulaResolver* pResolver) const
	{
		return evaluateNode(m_nRootNode, pResolver);
	}

	bool CUIFormula::markRuntimeErrorReported()
	{
		return !m_bRuntimeErrorReported.exchange(true);
	}

	CUIFormulaValue CUIFormula::evaluateNode(size_t nNodeIndex, CUIFormulaResolver* pResolver) const
	{
		auto& node = m_Nodes.at(nNodeIndex);

		switch (node.m_Type) {
		case eUIFormulaNodeType::Literal:
			return node.m_Literal;

		case eUIFormulaNodeType::Reference:
			if (pResolver == nullptr)
				throw EUIFormulaRuntimeError("references cannot be resolved without a resolver");
			return pResolver->resolveReference(m_References.at(node.m_nReferenceIndex));

		case eUIFormulaNodeType::Negate: {
			auto operand = evaluateNode(node.m_Children.at(0), pResolver);
			bool bIsInteger = false;
			int64_t nValue = 0;
			double dValue = 0.0;
			getNumericOperand(operand, bIsInteger, nValue, dValue);
			if (bIsInteger && (nValue != (std::numeric_limits<int64_t>::min)()))
				return CUIFormulaValue::fromInteger(-nValue);
			return CUIFormulaValue::fromDouble(-dValue);
		}

		case eUIFormulaNodeType::Not:
			return CUIFormulaValue::fromBool(!evaluateNode(node.m_Children.at(0), pResolver).toBool());

		case eUIFormulaNodeType::And:
			if (!evaluateNode(node.m_Children.at(0), pResolver).toBool())
				return CUIFormulaValue::fromBool(false);
			return CUIFormulaValue::fromBool(evaluateNode(node.m_Children.at(1), pResolver).toBool());

		case eUIFormulaNodeType::Or:
			if (evaluateNode(node.m_Children.at(0), pResolver).toBool())
				return CUIFormulaValue::fromBool(true);
			return CUIFormulaValue::fromBool(evaluateNode(node.m_Children.at(1), pResolver).toBool());

		case eUIFormulaNodeType::Binary:
			return evaluateBinary(node.m_Operator, evaluateNode(node.m_Children.at(0), pResolver), evaluateNode(node.m_Children.at(1), pResolver));

		case eUIFormulaNodeType::Call:
			return evaluateCall(node, pResolver);

		default:
			throw EUIFormulaRuntimeError("invalid formula node");
		}
	}

	CUIFormulaValue CUIFormula::evaluateCall(const sUIFormulaNode& node, CUIFormulaResolver* pResolver) const
	{
		auto& children = node.m_Children;

		switch (node.m_Function) {
		case eUIFormulaFunction::If:
			if (evaluateNode(children.at(0), pResolver).toBool())
				return evaluateNode(children.at(1), pResolver);
			return evaluateNode(children.at(2), pResolver);

		case eUIFormulaFunction::Between: {
			auto value = evaluateNode(children.at(0), pResolver);
			auto lowerBound = evaluateNode(children.at(1), pResolver);
			auto upperBound = evaluateNode(children.at(2), pResolver);
			return CUIFormulaValue::fromBool((compareOrdered(value, lowerBound) >= 0) && (compareOrdered(value, upperBound) <= 0));
		}

		case eUIFormulaFunction::Coalesce: {
			CUIFormulaValue value;
			for (auto nChild : children) {
				value = evaluateNode(nChild, pResolver);
				if (!value.toString().empty())
					return value;
			}
			return value;
		}

		case eUIFormulaFunction::Min:
		case eUIFormulaFunction::Max: {
			bool bAllIntegers = true;
			int64_t nResult = 0;
			double dResult = 0.0;
			for (size_t nIndex = 0; nIndex < children.size(); nIndex++) {
				bool bIsInteger = false;
				int64_t nValue = 0;
				double dValue = 0.0;
				getNumericOperand(evaluateNode(children.at(nIndex), pResolver), bIsInteger, nValue, dValue);

				bool bTake = (nIndex == 0);
				if (!bTake) {
					if (bAllIntegers && bIsInteger)
						bTake = (node.m_Function == eUIFormulaFunction::Min) ? (nValue < nResult) : (nValue > nResult);
					else
						bTake = (node.m_Function == eUIFormulaFunction::Min) ? (dValue < dResult) : (dValue > dResult);
				}

				bAllIntegers = bAllIntegers && bIsInteger;
				if (bTake) {
					nResult = nValue;
					dResult = dValue;
				}
			}
			if (bAllIntegers)
				return CUIFormulaValue::fromInteger(nResult);
			return CUIFormulaValue::fromDouble(dResult);
		}

		case eUIFormulaFunction::Clamp: {
			bool bValueInteger = false, bLowerInteger = false, bUpperInteger = false;
			int64_t nValue = 0, nLower = 0, nUpper = 0;
			double dValue = 0.0, dLower = 0.0, dUpper = 0.0;
			getNumericOperand(evaluateNode(children.at(0), pResolver), bValueInteger, nValue, dValue);
			getNumericOperand(evaluateNode(children.at(1), pResolver), bLowerInteger, nLower, dLower);
			getNumericOperand(evaluateNode(children.at(2), pResolver), bUpperInteger, nUpper, dUpper);

			if (bValueInteger && bLowerInteger && bUpperInteger) {
				if (nLower > nUpper)
					throw EUIFormulaRuntimeError("clamp: lower bound is greater than upper bound");
				return CUIFormulaValue::fromInteger((nValue < nLower) ? nLower : ((nValue > nUpper) ? nUpper : nValue));
			}

			if (dLower > dUpper)
				throw EUIFormulaRuntimeError("clamp: lower bound is greater than upper bound");
			return CUIFormulaValue::fromDouble((dValue < dLower) ? dLower : ((dValue > dUpper) ? dUpper : dValue));
		}

		case eUIFormulaFunction::Abs: {
			bool bIsInteger = false;
			int64_t nValue = 0;
			double dValue = 0.0;
			getNumericOperand(evaluateNode(children.at(0), pResolver), bIsInteger, nValue, dValue);
			if (bIsInteger && (nValue != (std::numeric_limits<int64_t>::min)()))
				return CUIFormulaValue::fromInteger((nValue < 0) ? -nValue : nValue);
			return CUIFormulaValue::fromDouble(std::fabs(dValue));
		}

		case eUIFormulaFunction::Round: {
			bool bIsInteger = false;
			int64_t nValue = 0;
			double dValue = 0.0;
			getNumericOperand(evaluateNode(children.at(0), pResolver), bIsInteger, nValue, dValue);

			int64_t nDecimals = 0;
			if (children.size() > 1)
				nDecimals = getDecimalsArgument(evaluateNode(children.at(1), pResolver));

			if (bIsInteger)
				return CUIFormulaValue::fromInteger(nValue);
			if (nDecimals == 0)
				return roundedToIntegerIfPossible(std::round(dValue));

			double dFactor = std::pow(10.0, (double)nDecimals);
			return CUIFormulaValue::fromDouble(std::round(dValue * dFactor) / dFactor);
		}

		case eUIFormulaFunction::Floor:
		case eUIFormulaFunction::Ceil: {
			bool bIsInteger = false;
			int64_t nValue = 0;
			double dValue = 0.0;
			getNumericOperand(evaluateNode(children.at(0), pResolver), bIsInteger, nValue, dValue);
			if (bIsInteger)
				return CUIFormulaValue::fromInteger(nValue);
			return roundedToIntegerIfPossible((node.m_Function == eUIFormulaFunction::Floor) ? std::floor(dValue) : std::ceil(dValue));
		}

		case eUIFormulaFunction::Concat: {
			std::string sResult;
			for (auto nChild : children)
				sResult += evaluateNode(nChild, pResolver).toString();
			return CUIFormulaValue::fromString(sResult);
		}

		case eUIFormulaFunction::Fixed: {
			bool bIsInteger = false;
			int64_t nValue = 0;
			double dValue = 0.0;
			getNumericOperand(evaluateNode(children.at(0), pResolver), bIsInteger, nValue, dValue);
			int64_t nDecimals = getDecimalsArgument(evaluateNode(children.at(1), pResolver));
			return CUIFormulaValue::fromString(formatFixed(dValue, nDecimals));
		}

		case eUIFormulaFunction::Upper:
		case eUIFormulaFunction::Lower: {
			std::string sValue = evaluateNode(children.at(0), pResolver).toString();
			for (auto& cChar : sValue) {
				if (node.m_Function == eUIFormulaFunction::Upper)
					cChar = (char)std::toupper((unsigned char)cChar);
				else
					cChar = (char)std::tolower((unsigned char)cChar);
			}
			return CUIFormulaValue::fromString(sValue);
		}

		case eUIFormulaFunction::Len: {
			std::string sValue = evaluateNode(children.at(0), pResolver).toString();
			int64_t nCodePoints = 0;
			for (auto cChar : sValue) {
				if ((((unsigned char)cChar) & 0xC0) != 0x80)
					nCodePoints++;
			}
			return CUIFormulaValue::fromInteger(nCodePoints);
		}

		case eUIFormulaFunction::Contains: {
			std::string sValue = evaluateNode(children.at(0), pResolver).toString();
			std::string sPart = evaluateNode(children.at(1), pResolver).toString();
			return CUIFormulaValue::fromBool(sValue.find(sPart) != std::string::npos);
		}

		case eUIFormulaFunction::StartsWith: {
			std::string sValue = evaluateNode(children.at(0), pResolver).toString();
			std::string sPrefix = evaluateNode(children.at(1), pResolver).toString();
			return CUIFormulaValue::fromBool(sValue.rfind(sPrefix, 0) == 0);
		}

		default:
			throw EUIFormulaRuntimeError("invalid function");
		}
	}

}
