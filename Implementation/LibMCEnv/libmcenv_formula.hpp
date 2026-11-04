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


Abstract: This is the class declaration of CFormula

*/


#ifndef __LIBMCENV_FORMULA
#define __LIBMCENV_FORMULA

#include "libmcenv_interfaces.hpp"

// Parent classes
#include "libmcenv_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "header_protection.hpp"
#include "amc_ui_formula.hpp"
#include "amc_statemachinedata.hpp"

namespace AMC {
	amcDeclareDependingClass(CUIFrontendState, PUIFrontendState);
}

namespace LibMCEnv {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CFormula 
**************************************************************************************************************************/

class CFormula : public virtual IFormula, public virtual CBase {
private:

	AMC::PUIFormula m_pFormula;
	AMC::PStateMachineData m_pStateMachineData;

	// Client session of a UI event; null in all other environments.
	AMC::PUIFrontendState m_pFrontendState;

	bool m_bHasSessionReferences;

	AMC::CUIFormulaValue evaluateValue();

public:

	// Session and user references are rejected, unless a client session is given.
	CFormula(const std::string& sFormula, AMC::PStateMachineData pStateMachineData, AMC::PUIFrontendState pFrontendState);

	virtual ~CFormula();

	std::string GetFormula() override;

	LibMCEnv_uint32 GetReferenceCount() override;

	std::string GetReference(const LibMCEnv_uint32 nIndex) override;

	bool HasSessionReferences() override;

	std::string Evaluate() override;

	LibMCEnv_double EvaluateAsDouble() override;

	LibMCEnv_int64 EvaluateAsInteger() override;

	bool EvaluateAsBool() override;

};

} // namespace Impl
} // namespace LibMCEnv

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCENV_FORMULA
