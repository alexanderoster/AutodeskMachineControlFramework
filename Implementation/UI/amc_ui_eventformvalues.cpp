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


#include "amc_ui_eventformvalues.hpp"
#include "amc_parametergroup.hpp"
#include "amc_parameterhandler.hpp"

#include "libmc_exceptiontypes.hpp"

namespace AMC {

	CUIEventFormValues::CUIEventFormValues(AMCCommon::PChrono pGlobalChrono)
		: m_pGlobalChrono(pGlobalChrono)
	{
		LibMCAssertNotNull(m_pGlobalChrono.get());
	}

	CUIEventFormValues::~CUIEventFormValues()
	{
	}

	PParameterGroup CUIEventFormValues::findPostedGroup(const std::string& sElementPath, const std::string& sPropertyName)
	{
		if (sPropertyName != AMC_UI_EVENTFORMVALUE_PROPERTY)
			return nullptr;

		auto iIter = m_PostedGroups.find(sElementPath);
		if (iIter == m_PostedGroups.end())
			return nullptr;

		return iIter->second;
	}

	void CUIEventFormValues::capturePostedValue(const std::string& sElementPath, PParameterGroup pClientVariableGroup, const std::string& sPostedValue)
	{
		LibMCAssertNotNull(pClientVariableGroup.get());

		// The copy takes the parameter types from the client variables; the value is set from the
		// payload, not read back, so a poll that already overwrote the client variable is harmless.
		auto pPostedGroup = std::make_shared<CParameterGroup>(sElementPath, "posted form values", m_pGlobalChrono);
		pPostedGroup->addDuplicatesFromGroup(pClientVariableGroup.get());
		pPostedGroup->setParameterValueByName(AMC_UI_EVENTFORMVALUE_PROPERTY, sPostedValue);

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_PostedGroups[sElementPath] = pPostedGroup;
	}

	PParameterGroup CUIEventFormValues::groupForRead(CParameterHandler* pClientVariableHandler, const std::string& sElementPath, const std::string& sPropertyName)
	{
		LibMCAssertNotNull(pClientVariableHandler);

		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);
			auto pPostedGroup = findPostedGroup(sElementPath, sPropertyName);
			if (pPostedGroup.get() != nullptr)
				return pPostedGroup;
		}

		return pClientVariableHandler->findGroup(sElementPath, true);
	}

	PParameterGroup CUIEventFormValues::groupForWrite(CParameterHandler* pClientVariableHandler, const std::string& sElementPath, const std::string& sPropertyName)
	{
		LibMCAssertNotNull(pClientVariableHandler);

		{
			std::lock_guard<std::mutex> lockGuard(m_Mutex);
			if (findPostedGroup(sElementPath, sPropertyName).get() != nullptr)
				m_PostedGroups.erase(sElementPath);
		}

		return pClientVariableHandler->findGroup(sElementPath, true);
	}

}
