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


Abstract: This is a stub class definition of CScatterPlotDataChannel

*/

#include "libmcenv_scatterplotdatachannel.hpp"
#include "libmcenv_interfaceexception.hpp"

// Include custom headers here.
#include "libmcenv_scatterplotdatacolumniterator.hpp"


using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CScatterPlotDataChannel 
**************************************************************************************************************************/

CScatterPlotDataChannel::CScatterPlotDataChannel(const std::string& sChannelIdentifier)
    : m_sChannelIdentifier(sChannelIdentifier)
{
}

CScatterPlotDataChannel* CScatterPlotDataChannel::makeFrom(IScatterPlotDataChannel* pDataChannel)
{
    if (pDataChannel == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

    auto pReturnDataChannel = new CScatterPlotDataChannel(pDataChannel->GetChannelIdentifier());

    auto pDataColumns = pDataChannel->ListScatterPlotDataColumns();

    while (pDataColumns->MoveNext())
    {
        auto pDataColumn = dynamic_cast<IScatterPlotDataColumn*>(pDataColumns->GetCurrent());
        pReturnDataChannel->AddScatterPlotDataColumn(pDataColumn);
    }

    return pReturnDataChannel;
}

std::string CScatterPlotDataChannel::GetChannelIdentifier()
{
    return m_sChannelIdentifier;
}

void CScatterPlotDataChannel::AddScatterPlotDataColumn(IScatterPlotDataColumn* pColumnInstance)
{
    if (pColumnInstance == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

    auto sColumnIdentifier = pColumnInstance->GetColumnIdentifier();

    if (m_DataColumnsMap.find(sColumnIdentifier) == m_DataColumnsMap.end())
        m_DataColumnsMap[sColumnIdentifier] = std::unique_ptr<IScatterPlotDataColumn>(pColumnInstance);
    else
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM); // Todo : Exception
}

IScatterPlotDataColumnIterator * CScatterPlotDataChannel::ListScatterPlotDataColumns()
{
    std::unique_ptr<CScatterPlotDataColumnIterator> pResult(new CScatterPlotDataColumnIterator());

    for (const auto& it : m_DataColumnsMap) {
        auto dataColumn = std::shared_ptr<CScatterPlotDataColumn>(CScatterPlotDataColumn::makeFrom(it.second.get()));
        pResult->AddScatterPlotDataColumn(dataColumn);
    }

    return pResult.release();
}

