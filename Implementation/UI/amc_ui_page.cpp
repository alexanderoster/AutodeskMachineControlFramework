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

#define __AMCIMPL_UI_PAGE
#define __AMCIMPL_UI_MODULE

#include "amc_ui_page.hpp"
#include "amc_ui_module.hpp"
#include "amc_parameterhandler.hpp"
#include "libmc_exceptiontypes.hpp"
#include "common_utils.hpp"

#define __AMCIMPL_API_CONSTANTS
#include "amc_api_constants.hpp"

#include "amc_ui_module_contentitem_form.hpp"

using namespace AMC;


CUIPage::CUIPage(const std::string& sName, CUIModule_UIEventHandler* pUIEventHandler)
	: m_sName(sName), m_pUIEventHandler(pUIEventHandler), m_nGridColumns(1), m_nGridRows(1)
{
	if (sName.empty())
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	LibMCAssertNotNull(pUIEventHandler);

	m_sUUID = AMCCommon::CUtils::createUUID();


}

CUIPage::~CUIPage()
{

}

std::string CUIPage::getName()
{
	return m_sName;
}

void CUIPage::addModule(PUIModule pModule)
{
	if (pModule.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	auto sName = pModule->getName();
	if (sName.empty ())
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDMODULENAME);

	m_Modules.push_back(pModule);

	pModule->populateItemMap(m_ItemMapOfPage);
	
}

void CUIPage::configurePostLoading()
{
	for (auto pModule : m_Modules)
		pModule->configurePostLoading();
}


uint32_t CUIPage::getModuleCount()
{
	return (uint32_t)m_Modules.size();
}

PUIModule CUIPage::getModule(const uint32_t nIndex)
{
	if (nIndex >= m_Modules.size())
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDINDEX);

	return m_Modules.at (nIndex);
}

void CUIPage::ensureUIEventExists(const std::string& sEventName)
{
	m_pUIEventHandler->ensureUIEventExists(sEventName);
}


/////////////////////////////////////////////////////////////////////////////////////
// Legacy UI System
/////////////////////////////////////////////////////////////////////////////////////

void CUIPage::writeLegacyModulesToJSON(CJSONWriter& writer, CJSONWriterArray& moduleArray, CParameterHandler* pLegacyClientVariableHandler)
{
	for (auto module : m_Modules) {
		CJSONWriterObject moduleObject(writer);		
		module->writeLegacyDefinitionToJSON(writer, moduleObject, pLegacyClientVariableHandler);

		moduleArray.addObject(moduleObject);
	}
}


PUIModuleItem CUIPage::findModuleItemByUUID(const std::string& sUUID)
{
	auto iIter = m_ItemMapOfPage.find (sUUID);
	if (iIter != m_ItemMapOfPage.end())
		return iIter->second;

	return nullptr;

}

void CUIPage::registerFormName(const std::string& sFormUUID, const std::string& sFormName)
{
	auto sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sFormUUID);

	auto iIter = m_FormNameMap.find(sFormName);
	if (iIter != m_FormNameMap.end())
		throw ELibMCCustomException(LIBMC_ERROR_DUPLICATEFORMNAME, sFormName);

	m_FormNameMap.insert(std::make_pair(sFormName, sNormalizedUUID));

}

std::string CUIPage::findFormUUIDByName(const std::string& sFormName)
{
	auto iIter = m_FormNameMap.find(sFormName);
	if (iIter != m_FormNameMap.end()) {
		return iIter->second;
	}
	return "";
}

void CUIPage::populateClientVariables(CParameterHandler* pParameterHandler)
{
	LibMCAssertNotNull(pParameterHandler);
	auto pGroup = pParameterHandler->addGroup(m_sName, "page");
	pGroup->addNewStringParameter ("custom", "custom page parameter", "");

	for (auto pModule : m_Modules) {
		pModule->populateClientVariables(pParameterHandler);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// New UI Frontend System
/////////////////////////////////////////////////////////////////////////////////////

void CUIPage::frontendWritePageStatusToJSON(CJSONWriter& writer, CJSONWriterObject& pageObject, CUIFrontendState* pFrontendState)
{
	pageObject.addString("name", m_sName);
	pageObject.addString("uuid", AMCCommon::CUtils::normalizeUUIDString(m_sUUID));

	if (!m_sCaption.empty())
		pageObject.addString("caption", m_sCaption);
	if (!m_sDescription.empty())
		pageObject.addString("description", m_sDescription);
	if (!m_sIcon.empty())
		pageObject.addString("icon", m_sIcon);

	if (m_nGridColumns > 1)
		pageObject.addInteger("gridcolumns", m_nGridColumns);
	if (m_nGridRows > 1)
		pageObject.addInteger("gridrows", m_nGridRows);

	CJSONWriterArray moduleArray(writer);

	for (auto pModule : m_Modules) {
		CJSONWriterObject moduleObject(writer);

		pModule->frontendWriteModuleStatusToJSON(writer, moduleObject, pFrontendState);

		moduleArray.addObject(moduleObject);
	}


	pageObject.addArray("modules", moduleArray);

}

