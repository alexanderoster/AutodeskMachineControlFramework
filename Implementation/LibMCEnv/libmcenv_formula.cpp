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


Abstract: This is a stub class definition of CFormula

*/

#include "libmcenv_formula.hpp"
#include "libmcenv_interfaceexception.hpp"

// Include custom headers here.
#include "amc_ui_expression.hpp"
#include "amc_ui_frontendstate.hpp"


using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CFormula 
**************************************************************************************************************************/

CFormula::CFormula(const std::string& sFormula, AMC::PStateMachineData pStateMachineData, AMC::PUIFrontendState pFrontendState)
	: m_pStateMachineData(pStateMachineData), m_pFrontendState(pFrontendState), m_bHasSessionReferences(false)
{
	if (pStateMachineData.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	try {
		m_pFormula = std::make_shared<AMC::CUIFormula>(sFormula);
	}
	catch (std::exception& E) {
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDFORMULA, "invalid formula \"" + sFormula + "\": " + E.what());
	}

	for (auto& reference : m_pFormula->getReferences()) {
		if (reference.m_Type == AMC::eUIFormulaReferenceType::Session) {
			if (pFrontendState.get() == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_FORMULASESSIONREFERENCESNOTAVAILABLE, "formula \"" + sFormula + "\" references " + reference.m_sPath + ", but session and user values are only available in UI events");
			m_bHasSessionReferences = true;
		}
	}
}

CFormula::~CFormula()
{

}

AMC::CUIFormulaValue CFormula::evaluateValue()
{
	AMC::CUIExpressionFormulaResolver resolver(m_pStateMachineData.get(), m_pFrontendState.get());

	try {
		return m_pFormula->evaluate(&resolver);
	}
	catch (std::exception& E) {
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_FORMULAEVALUATIONFAILED, "formula \"" + m_pFormula->getFormula() + "\" could not be evaluated: " + E.what());
	}
}

std::string CFormula::GetFormula()
{
	return m_pFormula->getFormula();
}

LibMCEnv_uint32 CFormula::GetReferenceCount()
{
	return (LibMCEnv_uint32)m_pFormula->getReferences().size();
}

std::string CFormula::GetReference(const LibMCEnv_uint32 nIndex)
{
	auto& references = m_pFormula->getReferences();
	if (nIndex >= references.size())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDFORMULAREFERENCEINDEX);

	return references.at(nIndex).m_sPath;
}

bool CFormula::HasSessionReferences()
{
	return m_bHasSessionReferences;
}

std::string CFormula::Evaluate()
{
	return evaluateValue().toString();
}

LibMCEnv_double CFormula::EvaluateAsDouble()
{
	auto value = evaluateValue();
	try {
		return value.toDouble();
	}
	catch (std::exception& E) {
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_FORMULAEVALUATIONFAILED, "formula \"" + m_pFormula->getFormula() + "\" is not a number: " + E.what());
	}
}

LibMCEnv_int64 CFormula::EvaluateAsInteger()
{
	auto value = evaluateValue();
	try {
		return value.toInteger();
	}
	catch (std::exception& E) {
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_FORMULAEVALUATIONFAILED, "formula \"" + m_pFormula->getFormula() + "\" is not a number: " + E.what());
	}
}

bool CFormula::EvaluateAsBool()
{
	return evaluateValue().toBool();
}

