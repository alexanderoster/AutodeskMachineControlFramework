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


#ifndef __AMC_UI_FORMULA
#define __AMC_UI_FORMULA

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#define AMC_UI_FORMULA_MAXLENGTH 4096
#define AMC_UI_FORMULA_MAXDEPTH 64
#define AMC_UI_FORMULA_MAXARGUMENTS 32
#define AMC_UI_FORMULA_MAXDECIMALS 15

namespace AMC {

	enum class eUIFormulaValueType : uint32_t {
		Bool = 1,
		Integer = 2,
		Double = 3,
		String = 4
	};

	// Thrown while evaluating a formula, e.g. on division by zero or when a string is not a number.
	class EUIFormulaRuntimeError : public std::runtime_error {
	public:
		explicit EUIFormulaRuntimeError(const std::string& sMessage);
	};

	class CUIFormulaValue {
	private:
		eUIFormulaValueType m_Type;
		bool m_bValue;
		int64_t m_nValue;
		double m_dValue;
		std::string m_sValue;

	public:

		// Creates an empty string value.
		CUIFormulaValue();

		static CUIFormulaValue fromBool(bool bValue);
		static CUIFormulaValue fromInteger(int64_t nValue);
		// Throws EUIFormulaRuntimeError for NaN and infinity.
		static CUIFormulaValue fromDouble(double dValue);
		static CUIFormulaValue fromString(const std::string& sValue);

		eUIFormulaValueType getType() const;
		bool isNumeric() const;

		// Strings are true unless empty, "0" or "false".
		bool toBool() const;
		// Strings are parsed; an empty string is 0. Doubles are rounded to the nearest integer.
		int64_t toInteger() const;
		double toDouble() const;
		// Bools are written as "1" and "0", like boolean parameters.
		std::string toString() const;

		// Returns false if the value is a string that is not a number.
		bool tryGetNumber(bool& bIsInteger, int64_t& nValue, double& dValue) const;
	};

	enum class eUIFormulaReferenceType : uint32_t {
		Parameter = 1,
		StateName = 2,
		Session = 3
	};

	struct sUIFormulaReference {
		eUIFormulaReferenceType m_Type;
		// Path as written in the formula, e.g. "main.jobinfo.currentlayer" or "session.flag".
		std::string m_sPath;
		std::string m_sInstanceName;
		std::string m_sGroupName;
		std::string m_sParameterName;
	};

	class CUIFormulaResolver {
	public:
		virtual ~CUIFormulaResolver() {}

		virtual CUIFormulaValue resolveReference(const sUIFormulaReference& reference) = 0;
	};

	enum class eUIFormulaNodeType : uint32_t {
		Literal = 1,
		Reference = 2,
		Negate = 3,
		Not = 4,
		And = 5,
		Or = 6,
		Binary = 7,
		Call = 8
	};

	enum class eUIFormulaOperator : uint32_t {
		None = 0,
		Add = 1,
		Subtract = 2,
		Multiply = 3,
		Divide = 4,
		Modulo = 5,
		Equal = 6,
		NotEqual = 7,
		Less = 8,
		LessEqual = 9,
		Greater = 10,
		GreaterEqual = 11
	};

	enum class eUIFormulaFunction : uint32_t {
		None = 0,
		If = 1,
		Between = 2,
		Coalesce = 3,
		Min = 4,
		Max = 5,
		Clamp = 6,
		Abs = 7,
		Round = 8,
		Floor = 9,
		Ceil = 10,
		Concat = 11,
		Fixed = 12,
		Upper = 13,
		Lower = 14,
		Len = 15,
		Contains = 16,
		StartsWith = 17
	};

	struct sUIFormulaNode {
		eUIFormulaNodeType m_Type;
		eUIFormulaOperator m_Operator;
		eUIFormulaFunction m_Function;
		CUIFormulaValue m_Literal;
		size_t m_nReferenceIndex;
		std::vector<size_t> m_Children;
	};

	// An immutable, compiled UI formula. evaluate() may be called concurrently from several threads.
	class CUIFormula {
	private:
		std::string m_sFormula;
		std::vector<sUIFormulaReference> m_References;
		std::vector<sUIFormulaNode> m_Nodes;
		size_t m_nRootNode;

		std::atomic<bool> m_bRuntimeErrorReported;

		CUIFormulaValue evaluateNode(size_t nNodeIndex, CUIFormulaResolver* pResolver) const;
		CUIFormulaValue evaluateCall(const sUIFormulaNode& node, CUIFormulaResolver* pResolver) const;

		friend class CUIFormulaParser;

	public:

		// Throws ELibMCCustomException (LIBMC_ERROR_INVALIDEXPRESSIONVALUE) on syntax errors.
		CUIFormula(const std::string& sFormula);

		const std::string& getFormula() const;

		// Distinct references in order of first appearance.
		const std::vector<sUIFormulaReference>& getReferences() const;

		// True if the formula is a single reference, optionally negated with "!" or "not".
		bool isSingleReference(bool& bIsNegated) const;

		// Throws EUIFormulaRuntimeError on evaluation errors; resolver exceptions are passed through.
		CUIFormulaValue evaluate(CUIFormulaResolver* pResolver) const;

		// Returns true only for the first call, so that a failing formula is logged once.
		bool markRuntimeErrorReported();
	};

	typedef std::shared_ptr<CUIFormula> PUIFormula;

}


#endif //__AMC_UI_FORMULA
