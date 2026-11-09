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


Abstract: This is the class declaration of CExecutionStatistics

*/


#ifndef __LIBMCDRIVER_SCANNEREMULATOR_EXECUTIONSTATISTICS
#define __LIBMCDRIVER_SCANNEREMULATOR_EXECUTIONSTATISTICS

#include "libmcdriver_scanneremulator_interfaces.hpp"
#include "libmcdriver_scanneremulator_base.hpp"
#include "libmcdriver_scanneremulator_scannerinstance.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif


namespace LibMCDriver_ScannerEmulator {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CExecutionStatistics 
**************************************************************************************************************************/

class CExecutionStatistics : public virtual IExecutionStatistics, public virtual CBase {
private:

	sExecutionStatisticsData m_Data;

public:

	CExecutionStatistics(const sExecutionStatisticsData& data);

	virtual ~CExecutionStatistics();

	void GetResult(LibMCDriver_ScannerEmulator::eExecutionResult & eResult, LibMCDriver_ScannerEmulator::eFaultType & eFault, LibMCDriver_ScannerEmulator_uint32 & nCommandIndex, LibMCDriver_ScannerEmulator_double & dTimeInMicroseconds) override;

	LibMCDriver_ScannerEmulator_double GetEmulatedDurationInMicroseconds() override;

	LibMCDriver_ScannerEmulator_double GetRealtimeDurationInMicroseconds() override;

	LibMCDriver_ScannerEmulator_double GetMarkLengthInMM() override;

	LibMCDriver_ScannerEmulator_double GetMarkTimeInMicroseconds() override;

	LibMCDriver_ScannerEmulator_double GetJumpLengthInMM() override;

	LibMCDriver_ScannerEmulator_double GetJumpTimeInMicroseconds() override;

	LibMCDriver_ScannerEmulator_double GetScannerDelayTimeInMicroseconds() override;

	LibMCDriver_ScannerEmulator_double GetCustomDelayTimeInMicroseconds() override;

	LibMCDriver_ScannerEmulator_double GetSkywritingTimeInMicroseconds() override;

	LibMCDriver_ScannerEmulator_double GetLaserOnTimeInMicroseconds() override;

	LibMCDriver_ScannerEmulator_uint32 GetCommandCount() override;

	LibMCDriver_ScannerEmulator_uint32 GetMarkSequenceCount() override;

	LibMCDriver_ScannerEmulator_uint32 GetOutOfViewportCount() override;

	LibMCDriver_ScannerEmulator_uint32 GetLaserOffDelayWarningCount() override;

};

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDRIVER_SCANNEREMULATOR_EXECUTIONSTATISTICS
