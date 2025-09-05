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


Abstract: This is a stub class definition of CStreamConnection

*/

#include "libmc_streamconnection.hpp"
#include "libmc_interfaceexception.hpp"

// Include custom headers here.
#include "common_utils.hpp"
#include "libmc_streamdata.hpp"

using namespace LibMC::Impl;

#include <iostream>

/*************************************************************************************************************************
 Class definition of CStreamConnection 
**************************************************************************************************************************/

CStreamConnection::CStreamConnection(const std::string& sStreamUUID)
    : m_sStreamUUID (AMCCommon::CUtils::normalizeUUIDString (sStreamUUID)),
    m_nDummy (0)
{

}


CStreamConnection::~CStreamConnection()
{

}


IStreamData * CStreamConnection::GetNewContent()
{
	std::unique_ptr<CStreamData> pStreamData(new CStreamData("image/jpeg"));

    auto & buffer = pStreamData->getBuffer();

    std::string sString = "{ \"test\": \"abc" + std::to_string (m_nDummy)  + "\" }";
    for (auto ch : sString) {
        buffer.push_back(ch);
    }

    m_nDummy++;

    return pStreamData.release();


}

uint32_t CStreamConnection::GetIdleDelay()
{
    return 50;
}

LibMC::eStreamConnectionType CStreamConnection::GetStreamType()
{
    return LibMC::eStreamConnectionType::JSONEventStream;
}
