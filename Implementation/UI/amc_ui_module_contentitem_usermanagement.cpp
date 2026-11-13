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

#include "amc_ui_module_contentitem_usermanagement.hpp"
#include "libmc_interfaceexception.hpp"

#include "amc_api_constants.hpp"
#include "Common/common_utils.hpp"

using namespace AMC;


PUIModule_ContentUserManagement CUIModule_ContentUserManagement::makeFromXML(const pugi::xml_node& xmlNode, const std::string& sItemName, const std::string& sModulePath)
{
	auto pItem = std::make_shared<CUIModule_ContentUserManagement>(sItemName, sModulePath);

	pItem->addStringAttribute("title", CUIExpression(xmlNode, "title", std::string("")));
	pItem->addStringAttribute("mode", CUIExpression(xmlNode, "mode", std::string("admin")));
	pItem->addStringAttribute("columns", CUIExpression(xmlNode, "columns", std::string("login,description,role,language,online")));
	pItem->addStringAttribute("roles", CUIExpression(xmlNode, "roles", std::string("")));

	pItem->addIntegerAttribute("entriesperpage", CUIExpression(xmlNode, "entriesperpage", std::string("25")));

	pItem->addBoolAttribute("allowcreate", CUIExpression(xmlNode, "allowcreate", std::string("true")));
	pItem->addBoolAttribute("allowedit", CUIExpression(xmlNode, "allowedit", std::string("true")));
	pItem->addBoolAttribute("allowroleedit", CUIExpression(xmlNode, "allowroleedit", std::string("true")));
	pItem->addBoolAttribute("allowpasswordreset", CUIExpression(xmlNode, "allowpasswordreset", std::string("true")));
	pItem->addBoolAttribute("allowdelete", CUIExpression(xmlNode, "allowdelete", std::string("true")));
	pItem->addBoolAttribute("allowsessions", CUIExpression(xmlNode, "allowsessions", std::string("true")));
	pItem->addBoolAttribute("allowownpassword", CUIExpression(xmlNode, "allowownpassword", std::string("true")));
	pItem->addBoolAttribute("showrolepermissions", CUIExpression(xmlNode, "showrolepermissions", std::string("true")));
	pItem->addBoolAttribute("confirmdelete", CUIExpression(xmlNode, "confirmdelete", std::string("true")));

	return pItem;
}

CUIModule_ContentUserManagement::CUIModule_ContentUserManagement(const std::string& sItemName, const std::string& sModulePath)
	: CUIModule_ContentItem(AMCCommon::CUtils::createUUID(), sItemName, sModulePath)
{
}

CUIModule_ContentUserManagement::~CUIModule_ContentUserManagement()
{
}

void CUIModule_ContentUserManagement::addStringAttribute(const std::string& sName, const CUIExpression& expression)
{
	m_StringAttributes.push_back(std::make_pair(sName, expression));
}

void CUIModule_ContentUserManagement::addBoolAttribute(const std::string& sName, const CUIExpression& expression)
{
	m_BoolAttributes.push_back(std::make_pair(sName, expression));
}

void CUIModule_ContentUserManagement::addIntegerAttribute(const std::string& sName, const CUIExpression& expression)
{
	m_IntegerAttributes.push_back(std::make_pair(sName, expression));
}

std::string CUIModule_ContentUserManagement::getItemType()
{
	return "usermanagement";
}

void CUIModule_ContentUserManagement::registerFrontendAttributes()
{
	for (auto& attribute : m_StringAttributes)
		registerItemStringAttribute(attribute.first, attribute.second);
	for (auto& attribute : m_BoolAttributes)
		registerItemBoolAttribute(attribute.first, attribute.second);
	for (auto& attribute : m_IntegerAttributes)
		registerItemIntegerAttribute(attribute.first, attribute.second);
}
