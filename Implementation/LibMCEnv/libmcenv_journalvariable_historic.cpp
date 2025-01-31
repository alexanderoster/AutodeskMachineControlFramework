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


Abstract: This is a stub class definition of CJournalVariable

*/

#include "libmcenv_journalvariable_historic.hpp"
#include "libmcenv_interfaceexception.hpp"


// Include custom headers here.
#include <cmath>

using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CJournalVariable 
**************************************************************************************************************************/
CJournalVariable_Historic::CJournalVariable_Historic(AMC::PStateJournalReader pJournalReader, const std::string& sVariableName)
    : m_pJournalReader (pJournalReader), m_sVariableName (sVariableName)
{
    if (pJournalReader.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

    if (sVariableName.empty())
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_EMPTYJOURNALVARIABLENAME);

}

CJournalVariable_Historic::~CJournalVariable_Historic()
{

}

std::string CJournalVariable_Historic::GetVariableName()
{
    return m_sVariableName;
}

LibMCEnv_double CJournalVariable_Historic::ComputeDoubleSample(const LibMCEnv_uint64 nTimeInMicroSeconds)
{
    return m_pJournalReader->computeDoubleSample(m_sVariableName, nTimeInMicroSeconds);
}

LibMCEnv_int64 CJournalVariable_Historic::ComputeIntegerSample(const LibMCEnv_uint64 nTimeInMicroSeconds)
{
    return m_pJournalReader->computeIntegerSample(m_sVariableName, nTimeInMicroSeconds);
}



