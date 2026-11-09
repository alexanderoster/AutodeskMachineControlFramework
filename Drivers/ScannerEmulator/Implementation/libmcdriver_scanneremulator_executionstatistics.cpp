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


Abstract: This is a stub class definition of CExecutionStatistics

*/

#include "libmcdriver_scanneremulator_executionstatistics.hpp"
#include "libmcdriver_scanneremulator_interfaceexception.hpp"

using namespace LibMCDriver_ScannerEmulator::Impl;

/*************************************************************************************************************************
 Class definition of CExecutionStatistics 
**************************************************************************************************************************/

CExecutionStatistics::CExecutionStatistics(const sExecutionStatisticsData& data)
	: m_Data(data)
{
}

CExecutionStatistics::~CExecutionStatistics()
{
}

void CExecutionStatistics::GetResult(LibMCDriver_ScannerEmulator::eExecutionResult & eResult, LibMCDriver_ScannerEmulator::eFaultType & eFault, LibMCDriver_ScannerEmulator_uint32 & nCommandIndex, LibMCDriver_ScannerEmulator_double & dTimeInMicroseconds)
{
	eResult = m_Data.m_Result;
	eFault = m_Data.m_Fault;
	nCommandIndex = m_Data.m_nCommandIndex;
	dTimeInMicroseconds = m_Data.m_dEndTimeInMicroseconds;
}

LibMCDriver_ScannerEmulator_double CExecutionStatistics::GetEmulatedDurationInMicroseconds()
{
	return m_Data.m_dEmulatedDurationInMicroseconds;
}

LibMCDriver_ScannerEmulator_double CExecutionStatistics::GetRealtimeDurationInMicroseconds()
{
	return m_Data.m_dRealtimeDurationInMicroseconds;
}

LibMCDriver_ScannerEmulator_double CExecutionStatistics::GetMarkLengthInMM()
{
	return m_Data.m_Statistics.m_dMarkLengthInMM;
}

LibMCDriver_ScannerEmulator_double CExecutionStatistics::GetMarkTimeInMicroseconds()
{
	return m_Data.m_Statistics.m_dMarkTimeInMicroseconds;
}

LibMCDriver_ScannerEmulator_double CExecutionStatistics::GetJumpLengthInMM()
{
	return m_Data.m_Statistics.m_dJumpLengthInMM;
}

LibMCDriver_ScannerEmulator_double CExecutionStatistics::GetJumpTimeInMicroseconds()
{
	return m_Data.m_Statistics.m_dJumpTimeInMicroseconds;
}

LibMCDriver_ScannerEmulator_double CExecutionStatistics::GetScannerDelayTimeInMicroseconds()
{
	return m_Data.m_Statistics.m_dScannerDelayTimeInMicroseconds;
}

LibMCDriver_ScannerEmulator_double CExecutionStatistics::GetCustomDelayTimeInMicroseconds()
{
	return m_Data.m_Statistics.m_dCustomDelayTimeInMicroseconds;
}

LibMCDriver_ScannerEmulator_double CExecutionStatistics::GetSkywritingTimeInMicroseconds()
{
	return m_Data.m_Statistics.m_dSkywritingTimeInMicroseconds;
}

LibMCDriver_ScannerEmulator_double CExecutionStatistics::GetLaserOnTimeInMicroseconds()
{
	return m_Data.m_Statistics.m_dLaserOnTimeInMicroseconds;
}

LibMCDriver_ScannerEmulator_uint32 CExecutionStatistics::GetCommandCount()
{
	return m_Data.m_Statistics.m_nCommandCount;
}

LibMCDriver_ScannerEmulator_uint32 CExecutionStatistics::GetMarkSequenceCount()
{
	return m_Data.m_Statistics.m_nMarkSequenceCount;
}

LibMCDriver_ScannerEmulator_uint32 CExecutionStatistics::GetOutOfViewportCount()
{
	return m_Data.m_Statistics.m_nOutOfViewportCount;
}

LibMCDriver_ScannerEmulator_uint32 CExecutionStatistics::GetLaserOffDelayWarningCount()
{
	return m_Data.m_Statistics.m_nLaserOffDelayWarningCount;
}

