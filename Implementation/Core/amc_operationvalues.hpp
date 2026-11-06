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


#ifndef __AMC_OPERATIONVALUES
#define __AMC_OPERATIONVALUES

#include "amc_operationtypes.hpp"

#include <string>
#include <vector>

namespace AMC {

	// Raw value semantics of operation parameters:
	//   literal      "5000", "inposition"
	//   reference    "$name", resolved against the binding group of the execution
	//   formula      "$name * 0.5", every $name word is expanded to <instance>.<group>.<name>
	class COperationValues {
	public:

		static std::string formatDouble(double dValue);

		// Strict parsing: the whole string (apart from surrounding whitespace) must be a number.
		static bool tryParseNumber(const std::string& sValue, double& dValue);

		static bool isBlank(const std::string& sValue);

		// Returns true if the value is a single "$name" reference with a valid identifier.
		static bool isSingleReference(const std::string& sValue, std::string& sParameterName);

		// Numeric and bool values that are blank are treated as not set, so that the default applies.
		static bool valueCountsAsExplicit(eOperationParameterType parameterType, const std::string& sRawValue);

		static eOperationValueKind classifyValue(eOperationParameterType parameterType, eOperationExpressionMode expressionMode, const std::string& sRawValue);

		// Expands every "$name" word to <instance>.<group>.<name>. Quoted strings and words that contain
		// a '.' or a later '$' (for example main.jobinfo.currentlayer or main.$state) are copied unchanged.
		static std::string expandFormula(const std::string& sFormula, const std::string& sInstanceName, const std::string& sGroupName, std::vector<std::string>& referencedNames);
	};

}

#endif //__AMC_OPERATIONVALUES
