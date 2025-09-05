/*++

Copyright (C) 2025 Autodesk Inc.

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


Abstract: This is the class declaration of COpenFOAMCaseInstance

*/


#ifndef __LIBMCDRIVER_OPENFOAM_DICTBUILDER
#define __LIBMCDRIVER_OPENFOAM_DICTBUILDER

#include "libmcdriver_openfoam_interfaces.hpp"
#include <sstream>


namespace LibMCDriver_OpenFOAM {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of COpenFOAMDictBuilder 
**************************************************************************************************************************/

enum class eOpenFOAMVersion : uint32_t
{
	ofvUndefined = 0,
	ofv2406 = 1
};

enum class eOpenFOAMDictType : uint32_t
{
	ofdInclude = 0,
	ofdDictionary = 1,
	ofdVolScalarField = 2,
	ofdVolVectorField = 3
};

class COpenFOAMDictBuilder {
private:

	eOpenFOAMVersion m_Version;
	std::stringstream m_Stream;

	std::string m_sFooterString;
	std::string m_sIdentString;

	uint32_t m_nCurrentBlockDepth;
	uint32_t m_nKeyCharLength;

	void writeDictHeader (const std::string& sObjectType, eOpenFOAMDictType dictType);

	std::string getVersionTagString();

	void writeLineIndent ();

	std::string extendKey (const std::string & sKey);

public:

	COpenFOAMDictBuilder(const std::string & sObjectType, eOpenFOAMVersion openFOAMVersion, uint32_t nKeyCharLength, eOpenFOAMDictType dictType);

	virtual ~COpenFOAMDictBuilder();

	std::string getAsString ();

	void writeDouble (const std::string & sKey, double dValue);
	void writeInteger (const std::string& sKey, int64_t nValue);
	void writeString (const std::string& sKey, const std::string & sValue);
	void writeBool(const std::string& sKey, const bool bValue);
	void writeVec2d(const std::string& sKey, double dV1, double dV2);
	void writeVec4d(const std::string& sKey, double dV1, double dV2, double dV3, double dV4);
	void writeVec2i(const std::string& sKey, int64_t nV1, int64_t nV2);
	void writeVec3i(const std::string& sKey, int64_t nV1, int64_t nV2, int64_t nV3);
	void writeVec4i(const std::string& sKey, int64_t nV1, int64_t nV2, int64_t nV3, int64_t nV4);
	void writeVec3dEnum(const std::string& sKey, double dV1, double dV2, double dV3);
	void writeVec3dValue(const std::string& sKey, double dV1, double dV2, double dV3);


	void writeIncludeEtc(const std::string & sIncludePath);
	void writeInclude(const std::string& sIncludePath);

	void writeHexBlock (int64_t nIdx0, int64_t nIdx1, int64_t nIdx2, int64_t nIdx3, int64_t nIdx4, int64_t nIdx5, int64_t nIdx6, int64_t nIdx7, uint32_t nCountX, uint32_t nCountY, uint32_t nCountZ);

	void beginBlock (const std::string& sKey);
	void endBlock ();

	void beginEnumBlock(const std::string& sKey);
	void endEnumBlock();

	// A Enum string is without ; at the en
	void writeEnumString(const std::string& sValue);
};

typedef std::shared_ptr<COpenFOAMDictBuilder> POpenFOAMDictBuilder;

} // namespace Impl
} // namespace LibMCDriver_OpenFOAM

#endif // __LIBMCDRIVER_OPENFOAM_DICTBUILDER
