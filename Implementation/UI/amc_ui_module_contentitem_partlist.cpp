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

#define __AMCIMPL_UI_MODULE
#define __AMCIMPL_API_CONSTANTS

#include "amc_ui_module_contentitem_partlist.hpp"
#include "libmc_interfaceexception.hpp"

#include "amc_api_constants.hpp"
#include "Common/common_utils.hpp"
#include "amc_parameterhandler.hpp"

using namespace AMC;


PUIModule_ContentPartList CUIModule_ContentPartList::makeFromXML(const pugi::xml_node& xmlNode, const std::string& sItemName, const std::string& sModulePath)
{
	auto buildUUIDAttrib = xmlNode.attribute("builduuid");
	CUIExpression buildUUIDExpression(xmlNode, "builduuid", std::string("00000000-0000-0000-0000-000000000000"));

	CUIExpression loadingTextExpression(xmlNode, "loadingtext", std::string("Loading build details..."));

	return std::make_shared<CUIModule_ContentPartList>(
		buildUUIDAttrib.as_string(),
		buildUUIDExpression,
		loadingTextExpression,
		sItemName,
		sModulePath
	);
}

CUIModule_ContentPartList::CUIModule_ContentPartList(
	const std::string& sBuildUUID, const CUIExpression& buildUUIDExpression,
	const CUIExpression& loadingText,
	const std::string& sItemName, const std::string& sModulePath)
	: CUIModule_ContentItem(AMCCommon::CUtils::createUUID(), sItemName, sModulePath),
	  m_sBuildUUID(sBuildUUID),
	  m_BuildUUIDExpression(buildUUIDExpression),
	  m_LoadingText(loadingText)
{
}

CUIModule_ContentPartList::~CUIModule_ContentPartList()
{
}

void CUIModule_ContentPartList::addLegacyContentToJSON(CJSONWriter& writer, CJSONWriterObject& object, CParameterHandler* pClientVariableHandler, uint32_t nStateID)
{
	object.addString(AMC_API_KEY_UI_ITEMTYPE, "partlist");
	object.addString(AMC_API_KEY_UI_ITEMUUID, m_sUUID);
	object.addString("builduuid", m_sBuildUUID);
}

std::string CUIModule_ContentPartList::getItemType()
{
	return "partlist";
}

void CUIModule_ContentPartList::registerFrontendAttributes()
{
	registerItemUUIDAttribute("builduuid", m_BuildUUIDExpression);
	registerItemStringAttribute("loadingtext", m_LoadingText);
}
