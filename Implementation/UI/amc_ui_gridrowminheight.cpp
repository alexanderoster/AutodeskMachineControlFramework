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

#define __AMCIMPL_API_CONSTANTS

#include "amc_ui_gridrowminheight.hpp"

#include "amc_api_constants.hpp"

#include "common_utils.hpp"

using namespace AMC;

sGridRowMinHeight::sGridRowMinHeight()
	: m_fHeight (0.0f), m_bIsSet (false), m_sUnit (""), m_bIgnoredOnUnit (false)
{

}

bool CUIGridRowMinHeight::appliesToUnit(const std::string& sUnit)
{
	std::string sLowerUnit = normalizeUnit(sUnit);
	return sLowerUnit == "free" || sLowerUnit == "auto";
}

std::string CUIGridRowMinHeight::normalizeUnit(const std::string& sUnit)
{
	return AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(sUnit));
}

sGridRowMinHeight CUIGridRowMinHeight::readFromXML(pugi::xml_node& xmlNode, const std::string& sRowUnit)
{
	sGridRowMinHeight minHeight;

	auto minHeightAttrib = xmlNode.attribute(AMC_API_KEY_UI_ROWMINHEIGHT);
	if (minHeightAttrib.empty())
		return minHeight;

	minHeight.m_fHeight = minHeightAttrib.as_float(0.0f);
	if (minHeight.m_fHeight <= 0.0f)
		return minHeight;

	if (!appliesToUnit(sRowUnit)) {
		minHeight.m_bIgnoredOnUnit = true;
		return minHeight;
	}

	minHeight.m_bIsSet = true;
	minHeight.m_sUnit = normalizeUnit(xmlNode.attribute(AMC_API_KEY_UI_ROWMINHEIGHTUNIT).as_string());

	return minHeight;
}
