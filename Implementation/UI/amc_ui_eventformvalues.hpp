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


#ifndef __AMC_UI_EVENTFORMVALUES
#define __AMC_UI_EVENTFORMVALUES

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "common_chrono.hpp"

#define AMC_UI_EVENTFORMVALUE_PROPERTY "value"

namespace AMC {

	class CParameterGroup;
	typedef std::shared_ptr<CParameterGroup> PParameterGroup;

	class CParameterHandler;

	class CUIEventFormValues;
	typedef std::shared_ptr<CUIEventFormValues> PUIEventFormValues;

	// The form values posted with one UI event, kept apart from the session's client variables.
	//
	// A posted value is written to the client variable "<entity path>.value", which every content
	// poll of a sync:value edit overwrites with the bound parameter. A poll between the payload
	// write and the handler's GetUIProperty would hand the handler the parameter instead of what
	// the user posted. Reads of "value" for a posted entity are therefore served from
	// a copy taken when the payload is written; everything else reads the client variables.
	class CUIEventFormValues {
	private:
		AMCCommon::PChrono m_pGlobalChrono;
		std::mutex m_Mutex;
		std::map<std::string, PParameterGroup> m_PostedGroups;

		PParameterGroup findPostedGroup(const std::string& sElementPath, const std::string& sPropertyName);

	public:
		CUIEventFormValues(AMCCommon::PChrono pGlobalChrono);
		virtual ~CUIEventFormValues();

		// Records sPostedValue as the event's value of sElementPath. pClientVariableGroup is that
		// entity's client variable group; it supplies the value's type, not its value.
		void capturePostedValue(const std::string& sElementPath, PParameterGroup pClientVariableGroup, const std::string& sPostedValue);

		// The group to read sPropertyName of sElementPath from during the event.
		PParameterGroup groupForRead(CParameterHandler* pClientVariableHandler, const std::string& sElementPath, const std::string& sPropertyName);

		// The group to write sPropertyName of sElementPath to during the event. Writing the value
		// of a posted entity replaces the posted value, so later reads see the written one.
		PParameterGroup groupForWrite(CParameterHandler* pClientVariableHandler, const std::string& sElementPath, const std::string& sPropertyName);
	};

}

#endif //__AMC_UI_EVENTFORMVALUES
