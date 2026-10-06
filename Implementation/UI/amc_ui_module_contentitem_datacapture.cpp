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

*/

#define __AMCIMPL_UI_MODULE
#define __AMCIMPL_API_CONSTANTS

#include "amc_ui_module_contentitem_datacapture.hpp"
#include "libmc_interfaceexception.hpp"

#include "amc_api_constants.hpp"
#include "Common/common_utils.hpp"
#include "amc_parameterhandler.hpp"

using namespace AMC;


PUIModule_ContentDataCapture CUIModule_ContentDataCapture::makeFromXML(const pugi::xml_node& xmlNode, const std::string& sItemName, const std::string& sModulePath)
{
	auto titleAttrib = xmlNode.attribute("title");
	CUIExpression titleExpression(xmlNode, "title", std::string("Data Capture"));

	// The capture uuid usually comes from a state machine parameter that the plugin sets when it creates the capture.
	auto captureUUIDAttrib = xmlNode.attribute("captureuuid");
	CUIExpression captureUUIDExpression(xmlNode, "captureuuid", std::string(""));

	auto defaultColumnsAttrib = xmlNode.attribute("defaultcolumns");
	CUIExpression defaultColumnsExpression(xmlNode, "defaultcolumns", std::string(""));

	return std::make_shared<CUIModule_ContentDataCapture>(
		titleAttrib.empty() ? "Data Capture" : titleAttrib.as_string(),
		titleExpression,
		captureUUIDAttrib.empty() ? "" : captureUUIDAttrib.as_string(),
		captureUUIDExpression,
		defaultColumnsAttrib.empty() ? "" : defaultColumnsAttrib.as_string(),
		defaultColumnsExpression,
		sItemName,
		sModulePath
	);
}

CUIModule_ContentDataCapture::CUIModule_ContentDataCapture(const std::string& sTitle, const CUIExpression& titleExpression, const std::string& sCaptureUUID, const CUIExpression& captureUUIDExpression, const std::string& sDefaultColumns, const CUIExpression& defaultColumnsExpression, const std::string& sItemName, const std::string& sModulePath)
	: CUIModule_ContentItem(AMCCommon::CUtils::createUUID(), sItemName, sModulePath),
	  m_sTitle(sTitle),
	  m_sCaptureUUID(sCaptureUUID),
	  m_sDefaultColumns(sDefaultColumns),
	  m_TitleExpression(titleExpression),
	  m_CaptureUUIDExpression(captureUUIDExpression),
	  m_DefaultColumnsExpression(defaultColumnsExpression)
{
}

CUIModule_ContentDataCapture::~CUIModule_ContentDataCapture()
{
}

std::string CUIModule_ContentDataCapture::getItemType()
{
	return "datacapture";
}

void CUIModule_ContentDataCapture::registerFrontendAttributes()
{
	registerItemStringAttribute("title", m_TitleExpression);
	registerItemStringAttribute("captureuuid", m_CaptureUUIDExpression);
	registerItemStringAttribute("defaultcolumns", m_DefaultColumnsExpression);
}
