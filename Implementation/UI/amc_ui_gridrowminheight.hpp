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

#ifndef __AMC_UI_GRIDROWMINHEIGHT
#define __AMC_UI_GRIDROWMINHEIGHT

#include "Libraries/PugiXML/pugixml.hpp"

#include <string>


namespace AMC {

	struct sGridRowMinHeight {
		float m_fHeight;
		bool m_bIsSet;
		std::string m_sUnit;
		bool m_bIgnoredOnUnit;

		sGridRowMinHeight();
	};

	class CUIGridRowMinHeight {
	public:

		// Reads the optional minheight and minheightunit attributes of a grid row element.
		// A minimum height only has an effect on rows that are not sized by their height,
		// that is free and auto rows. On any other unit the value is reported as ignored,
		// so that the caller can report the configuration issue to the user. A missing or
		// non-positive value is neither set nor ignored.
		static sGridRowMinHeight readFromXML(pugi::xml_node& xmlNode, const std::string& sRowUnit);

		static bool appliesToUnit(const std::string& sUnit);
		static std::string normalizeUnit(const std::string& sUnit);
	};

}

#endif //__AMC_UI_GRIDROWMINHEIGHT
