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


Abstract: This is a stub class definition of COpenFOAMDictBuilder

*/

#include "libmcdriver_openfoam_dictbuilder.hpp"
#include "libmcdriver_openfoam_interfaceexception.hpp"


using namespace LibMCDriver_OpenFOAM::Impl;

#define OPENFOAM_MAXKEYCHARLENGTH 256



COpenFOAMDictBuilder::COpenFOAMDictBuilder(const std::string& sObjectType, eOpenFOAMVersion openFOAMVersion, uint32_t nKeyCharLength, eOpenFOAMDictType dictType)
    : m_Version (openFOAMVersion), m_nCurrentBlockDepth (0), m_nKeyCharLength (nKeyCharLength)
{

    m_sFooterString = "\n// ************************************************************************* //\n\n";
    m_sIdentString = "    ";

    writeDictHeader(sObjectType, dictType);

    if (nKeyCharLength > OPENFOAM_MAXKEYCHARLENGTH)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDKEYCHARLENGTH);


}

COpenFOAMDictBuilder::~COpenFOAMDictBuilder()
{

}

std::string COpenFOAMDictBuilder::getAsString()
{
    return m_Stream.str() + m_sFooterString;
}

void COpenFOAMDictBuilder::writeDictHeader(const std::string& sObjectType, eOpenFOAMDictType dictType)
{
    m_Stream << "/*--------------------------------*- C++ -*----------------------------------*\\" << std::endl;
    m_Stream << "| =========                 |                                                 |" << std::endl;
    m_Stream << "| \\\\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox           |" << std::endl;
    m_Stream << "|  \\\\    /   O peration     | Version:  " << getVersionTagString () << "                                 |" << std::endl;
    m_Stream << "|   \\\\  /    A nd           | Website:  www.openfoam.com                      |" << std::endl;
    m_Stream << "|    \\\\/     M anipulation  |                                                 |" << std::endl;
    m_Stream << "\\*---------------------------------------------------------------------------*/" << std::endl;
    
    bool bWriteFoamFileBlock = (dictType != eOpenFOAMDictType::ofdInclude);


    if (bWriteFoamFileBlock) {
        beginBlock("FoamFile");
        writeString("version", "2.0");
        writeString("format", "ascii");
        switch (dictType) {
            case eOpenFOAMDictType::ofdDictionary:
                writeString("class", "dictionary");
                break;

            case eOpenFOAMDictType::ofdVolScalarField:
                writeString("class", "volScalarField");
                break;

            case eOpenFOAMDictType::ofdVolVectorField:
                writeString("class", "volVectorField");
                break;

        }
        writeString("object", sObjectType);
        endBlock();
    }


    m_Stream << "// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //" << std::endl;
    m_Stream << std::endl;
}


std::string COpenFOAMDictBuilder::getVersionTagString()
{
    switch (m_Version) {
        case eOpenFOAMVersion::ofv2406: return "v2406";
        default:
            throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDOPENFOAMVERSION);
    }
}

void COpenFOAMDictBuilder::writeDouble(const std::string& sKey, double dValue)
{
    writeLineIndent();

    if (!sKey.empty())
        m_Stream << extendKey(sKey);
    m_Stream << dValue << ";" << std::endl;
}

void COpenFOAMDictBuilder::writeInteger(const std::string& sKey, int64_t nValue)
{
    writeLineIndent();

    if (!sKey.empty())
        m_Stream << extendKey(sKey);
    m_Stream << nValue << ";" << std::endl;

}

void COpenFOAMDictBuilder::writeVec2d(const std::string& sKey, double dV1, double dV2)
{

    writeLineIndent();

    if (!sKey.empty())
        m_Stream << extendKey(sKey);
    m_Stream << "(" << dV1 << " " << dV2 << ")" << std::endl;

}

void COpenFOAMDictBuilder::writeVec3dEnum(const std::string& sKey, double dV1, double dV2, double dV3)
{
    writeLineIndent();

    if (!sKey.empty())
        m_Stream << extendKey(sKey);
    m_Stream << "(" << dV1 << " " << dV2 << " " << dV3 << ")" << std::endl;

}

void COpenFOAMDictBuilder::writeVec3dValue(const std::string& sKey, double dV1, double dV2, double dV3)
{
    writeLineIndent();

    if (!sKey.empty())
        m_Stream << extendKey(sKey);
    m_Stream << "(" << dV1 << " " << dV2 << " " << dV3 << ");" << std::endl;

}

void COpenFOAMDictBuilder::writeVec4d(const std::string& sKey, double dV1, double dV2, double dV3, double dV4)
{
    writeLineIndent();

    if (!sKey.empty())
        m_Stream << extendKey(sKey);
    m_Stream << "(" << dV1 << " " << dV2 << " " << dV3 << " " << dV4 << ")" << std::endl;

}

void COpenFOAMDictBuilder::writeVec2i(const std::string& sKey, int64_t nV1, int64_t nV2)
{
    writeLineIndent();

    if (!sKey.empty())
        m_Stream << extendKey(sKey);
    m_Stream << "(" << nV1 << " " << nV2 << ")" << std::endl;

}

void COpenFOAMDictBuilder::writeVec3i(const std::string& sKey, int64_t nV1, int64_t nV2, int64_t nV3)
{
    writeLineIndent();

    if (!sKey.empty())
        m_Stream << extendKey(sKey);
    m_Stream << "(" << nV1 << " " << nV2 << " " << nV3 << ")" << std::endl;

}

void COpenFOAMDictBuilder::writeVec4i(const std::string& sKey, int64_t nV1, int64_t nV2, int64_t nV3, int64_t nV4)
{
    writeLineIndent();

    if (!sKey.empty())
        m_Stream << extendKey(sKey);
    m_Stream << "(" << nV1 << " " << nV2 << " " << nV3 << " " << nV4 << ")" << std::endl;

}

void COpenFOAMDictBuilder::writeIncludeEtc(const std::string& sIncludePath)
{
    writeLineIndent();
    m_Stream << "#includeEtc \"" << sIncludePath << "\"" << std::endl;

}

void COpenFOAMDictBuilder::writeInclude(const std::string& sIncludePath)
{
    writeLineIndent();
    m_Stream << "#include \"" << sIncludePath << "\"" << std::endl;

}


void COpenFOAMDictBuilder::writeHexBlock(int64_t nIdx0, int64_t nIdx1, int64_t nIdx2, int64_t nIdx3, int64_t nIdx4, int64_t nIdx5, int64_t nIdx6, int64_t nIdx7, uint32_t nCountX, uint32_t nCountY, uint32_t nCountZ)
{
    writeLineIndent();
	m_Stream << "hex (" << nIdx0 << " " << nIdx1 << " " << nIdx2 << " " << nIdx3 << " "
		<< nIdx4 << " " << nIdx5 << " " << nIdx6 << " " << nIdx7 << ") "
		<< "(" << nCountX << " " << nCountY << " " << nCountZ << ") simpleGrading (1 1 1)" << std::endl;

}


void COpenFOAMDictBuilder::writeString(const std::string& sKey, const std::string& sValue)
{
    writeLineIndent();

    m_Stream << extendKey (sKey) << sValue << ";" << std::endl;
    
}

void COpenFOAMDictBuilder::writeBool(const std::string& sKey, const bool bValue)
{
    if (bValue) {
        writeString(sKey, "true");
    } 
    else {
        writeString(sKey, "false");
    }
    
}


void COpenFOAMDictBuilder::beginBlock(const std::string& sKey)
{
    writeLineIndent();
    m_Stream << sKey << std::endl;

    writeLineIndent();
    m_Stream << "{" << std::endl;

    m_nCurrentBlockDepth++;
}

void COpenFOAMDictBuilder::endBlock()
{
    if (m_nCurrentBlockDepth > 0) {
        m_nCurrentBlockDepth--;
            writeLineIndent();

        m_Stream << "}" << std::endl;
        m_Stream << std::endl;
    }
}

void COpenFOAMDictBuilder::beginEnumBlock(const std::string& sKey)
{
    writeLineIndent();
    m_Stream << sKey << std::endl;

    writeLineIndent();
    m_Stream << "(" << std::endl;

    m_nCurrentBlockDepth++;
}

void COpenFOAMDictBuilder::endEnumBlock()
{
    if (m_nCurrentBlockDepth > 0) {
        m_nCurrentBlockDepth--;
        writeLineIndent();

        m_Stream << ");" << std::endl;
        m_Stream << std::endl;
    }
}

void COpenFOAMDictBuilder::writeEnumString(const std::string& sValue)
{
    writeLineIndent();

    m_Stream << sValue << " " << std::endl;
}

void COpenFOAMDictBuilder::writeLineIndent()
{
    for (uint32_t nIndentIndex = 0; nIndentIndex < m_nCurrentBlockDepth; nIndentIndex++) 
	{
		m_Stream << m_sIdentString;
	}

}

std::string COpenFOAMDictBuilder::extendKey(const std::string & sKey)
{
    
    if (sKey.length() < m_nKeyCharLength) {
        std::string sReturnKey = sKey;
        sReturnKey.append(m_nKeyCharLength - sKey.length(), ' ');
        return sReturnKey;
    } 

    return sKey;
}


