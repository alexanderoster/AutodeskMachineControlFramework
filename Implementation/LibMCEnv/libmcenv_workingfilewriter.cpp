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


Abstract: This is a stub class definition of CWorkingFileWriter

*/

#include "libmcenv_workingfilewriter.hpp"
#include "libmcenv_interfaceexception.hpp"

// Include custom headers here.
#include <array>

using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CWorkingFileWriter 
**************************************************************************************************************************/
CWorkingFileWriter::CWorkingFileWriter(AMC::PProcessDirectoryWriter pWriterInstance, AMC::WProcessDirectory pProcessDirectory)
    : m_pWriterInstance (pWriterInstance), m_pProcessDirectory (pProcessDirectory)
{
    if (pWriterInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

}

CWorkingFileWriter::~CWorkingFileWriter()
{
    if (m_pWriterInstance.get() != nullptr) {
        m_pWriterInstance->finish();
        m_pWriterInstance = nullptr;
    }

}



LibMCEnv_uint64 CWorkingFileWriter::GetSize()
{
    return m_pWriterInstance->getWrittenBytes ();
}

std::string CWorkingFileWriter::GetFileName()
{
    return m_pWriterInstance->getLocalFileName();
}

void CWorkingFileWriter::FlushBuffer()
{
    return m_pWriterInstance->flushBuffer();
}


std::string CWorkingFileWriter::GetAbsoluteFileName()
{
    return m_pWriterInstance->getAbsoluteFileName();
}

IWorkingFile * CWorkingFileWriter::Finish()
{
    m_pWriterInstance->finish();

    std::string sLocalFileName = m_pWriterInstance->getLocalFileName();

    return new CWorkingFile(sLocalFileName, m_pProcessDirectory);
}

void CWorkingFileWriter::WriteData(const LibMCEnv_uint64 nBufferBufferSize, const LibMCEnv_uint8 * pBufferBuffer)
{
    m_pWriterInstance->writeData(pBufferBuffer, nBufferBufferSize);
}

void CWorkingFileWriter::WriteString(const std::string & sValue)
{
    uint64_t nLength = sValue.length();
    if (nLength > 0)
        m_pWriterInstance->writeData((const uint8_t*) sValue.c_str (), nLength);
}

void CWorkingFileWriter::WriteLine(const std::string & sValue, const bool bUnixLineEnding)
{
    uint64_t nLength = sValue.length();
    if (nLength > 0)
        m_pWriterInstance->writeData((const uint8_t*)sValue.c_str(), nLength);

    if (bUnixLineEnding) {
        std::array<uint8_t, 1> LF = { 10 };
        m_pWriterInstance->writeData(LF.data(), LF.size());
    }
    else {
        std::array<uint8_t, 2> CRLF = { 13, 10 };
        m_pWriterInstance->writeData(CRLF.data (), CRLF.size ());

    }
}


void CWorkingFileWriter::WriteFixedFloat(const LibMCEnv_double dValue, const LibMCEnv_uint32 nDigits)
{
    WriteString(std::to_string(dValue));
}

void CWorkingFileWriter::WriteFixedFloatLine(const LibMCEnv_double dValue, const LibMCEnv_uint32 nDigits, const bool bUnixLineEnding)
{
    WriteLine(std::to_string(dValue), bUnixLineEnding);
}

