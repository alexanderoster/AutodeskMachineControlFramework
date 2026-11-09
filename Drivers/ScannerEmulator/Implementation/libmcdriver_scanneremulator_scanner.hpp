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


Abstract: This is the class declaration of CScanner

*/


#ifndef __LIBMCDRIVER_SCANNEREMULATOR_SCANNER
#define __LIBMCDRIVER_SCANNEREMULATOR_SCANNER

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
 Class declaration of CScanner 
**************************************************************************************************************************/

class CScanner : public virtual IScanner, public virtual CBase {
private:

	PScannerInstance m_pInstance;
	LibMCEnv::PDriverEnvironment m_pDriverEnvironment;

public:

	CScanner(PScannerInstance pInstance, LibMCEnv::PDriverEnvironment pDriverEnvironment);

	virtual ~CScanner();

	PScannerInstance getInstance();

	static void buildLayerCommandList(CScannerInstance* pInstance, LibMCEnv::CToolpathLayer* pLayer, bool bFailIfNonAssignedDataExists, CEmulatorCommandList& commandList);

	static bool waitForInstance(CScannerInstance* pInstance, LibMCEnv::CDriverEnvironment* pDriverEnvironment, uint32_t nTimeOutInMilliseconds);

	std::string GetName() override;

	void GetViewport(LibMCDriver_ScannerEmulator_double & dX1, LibMCDriver_ScannerEmulator_double & dY1, LibMCDriver_ScannerEmulator_double & dX2, LibMCDriver_ScannerEmulator_double & dY2) override;

	LibMCDriver_ScannerEmulator_double GetUnitsInMM() override;

	void SetLaserIndex(const LibMCDriver_ScannerEmulator_uint32 nLaserIndex) override;

	LibMCDriver_ScannerEmulator_uint32 GetLaserIndex() override;

	void SetMaxLaserPower(const LibMCDriver_ScannerEmulator_double dMaxPowerInWatts) override;

	LibMCDriver_ScannerEmulator_double GetMaxLaserPower() override;

	void SetFastMode(const bool bFastMode) override;

	bool IsFastMode() override;

	void SetTimeScale(const LibMCDriver_ScannerEmulator_double dTimeScale) override;

	LibMCDriver_ScannerEmulator_double GetTimeScale() override;

	void SetOutOfViewportPolicy(const LibMCDriver_ScannerEmulator::eOutOfViewportPolicy ePolicy) override;

	LibMCDriver_ScannerEmulator::eOutOfViewportPolicy GetOutOfViewportPolicy() override;

	void SetTransformationMatrix(const LibMCDriver_ScannerEmulator_double dM11, const LibMCDriver_ScannerEmulator_double dM12, const LibMCDriver_ScannerEmulator_double dM21, const LibMCDriver_ScannerEmulator_double dM22) override;

	void SetTransformationOffset(const LibMCDriver_ScannerEmulator_double dOffsetX, const LibMCDriver_ScannerEmulator_double dOffsetY) override;

	void GetTransformation(LibMCDriver_ScannerEmulator_double & dM11, LibMCDriver_ScannerEmulator_double & dM12, LibMCDriver_ScannerEmulator_double & dM21, LibMCDriver_ScannerEmulator_double & dM22, LibMCDriver_ScannerEmulator_double & dOffsetX, LibMCDriver_ScannerEmulator_double & dOffsetY) override;

	void ResetTransformation() override;

	void SetJumpSpeed(const LibMCDriver_ScannerEmulator_double dJumpSpeed) override;

	LibMCDriver_ScannerEmulator_double GetJumpSpeed() override;

	void SetMarkSpeed(const LibMCDriver_ScannerEmulator_double dMarkSpeed) override;

	LibMCDriver_ScannerEmulator_double GetMarkSpeed() override;

	void SetPower(const LibMCDriver_ScannerEmulator_double dPowerInPercent) override;

	LibMCDriver_ScannerEmulator_double GetPower() override;

	void SetDelays(const LibMCDriver_ScannerEmulator_double dLaserOnDelay, const LibMCDriver_ScannerEmulator_double dLaserOffDelay, const LibMCDriver_ScannerEmulator_double dJumpDelay, const LibMCDriver_ScannerEmulator_double dMarkDelay, const LibMCDriver_ScannerEmulator_double dPolygonDelay) override;

	void GetDelays(LibMCDriver_ScannerEmulator_double & dLaserOnDelay, LibMCDriver_ScannerEmulator_double & dLaserOffDelay, LibMCDriver_ScannerEmulator_double & dJumpDelay, LibMCDriver_ScannerEmulator_double & dMarkDelay, LibMCDriver_ScannerEmulator_double & dPolygonDelay) override;

	void SetSkywriting(const LibMCDriver_ScannerEmulator::eSkywritingMode eMode, const LibMCDriver_ScannerEmulator_double dTimelag, const LibMCDriver_ScannerEmulator_double dLaserOnShift, const LibMCDriver_ScannerEmulator_double dNPrev, const LibMCDriver_ScannerEmulator_double dNPost, const LibMCDriver_ScannerEmulator_double dLimit) override;

	void GetSkywriting(LibMCDriver_ScannerEmulator::eSkywritingMode & eMode, LibMCDriver_ScannerEmulator_double & dTimelag, LibMCDriver_ScannerEmulator_double & dLaserOnShift, LibMCDriver_ScannerEmulator_double & dNPrev, LibMCDriver_ScannerEmulator_double & dNPost, LibMCDriver_ScannerEmulator_double & dLimit) override;

	void ClearList() override;

	LibMCDriver_ScannerEmulator_uint32 GetListCommandCount() override;

	void AddJumpMovement(const LibMCDriver_ScannerEmulator_double dTargetX, const LibMCDriver_ScannerEmulator_double dTargetY) override;

	void AddMarkMovement(const LibMCDriver_ScannerEmulator_double dTargetX, const LibMCDriver_ScannerEmulator_double dTargetY) override;

	void AddSetJumpSpeed(const LibMCDriver_ScannerEmulator_double dJumpSpeed) override;

	void AddSetMarkSpeed(const LibMCDriver_ScannerEmulator_double dMarkSpeed) override;

	void AddSetPower(const LibMCDriver_ScannerEmulator_double dPowerInPercent) override;

	void AddSetDelays(const LibMCDriver_ScannerEmulator_double dLaserOnDelay, const LibMCDriver_ScannerEmulator_double dLaserOffDelay, const LibMCDriver_ScannerEmulator_double dJumpDelay, const LibMCDriver_ScannerEmulator_double dMarkDelay, const LibMCDriver_ScannerEmulator_double dPolygonDelay) override;

	void AddCustomDelay(const LibMCDriver_ScannerEmulator_double dDelay) override;

	void AddEnableSkywriting(const LibMCDriver_ScannerEmulator::eSkywritingMode eMode, const LibMCDriver_ScannerEmulator_double dTimelag, const LibMCDriver_ScannerEmulator_double dLaserOnShift, const LibMCDriver_ScannerEmulator_double dNPrev, const LibMCDriver_ScannerEmulator_double dNPost, const LibMCDriver_ScannerEmulator_double dLimit) override;

	void AddDisableSkywriting() override;

	void AddLayerToList(LibMCEnv::PToolpathLayer pLayer, const bool bFailIfNonAssignedDataExists) override;

	void ExecuteList() override;

	bool WaitForList(const LibMCDriver_ScannerEmulator_uint32 nTimeOutInMilliseconds) override;

	void StopExecution() override;

	bool IsBusy() override;

	void DrawLayer(const std::string & sStreamUUID, const LibMCDriver_ScannerEmulator_uint32 nLayerIndex) override;

	void GetStatus(LibMCDriver_ScannerEmulator::eScannerState & eState, LibMCDriver_ScannerEmulator_uint32 & nCommandIndex, LibMCDriver_ScannerEmulator_double & dProgress) override;

	void GetPosition(LibMCDriver_ScannerEmulator_double & dX, LibMCDriver_ScannerEmulator_double & dY) override;

	bool IsLaserOn() override;

	void GetHeadStatus(bool & bPositionOK, bool & bTemperatureOK, bool & bPowerOK) override;

	IExecutionStatistics * GetLastExecutionStatistics() override;

	void EnableRecording(const LibMCDriver_ScannerEmulator_double dSampleIntervalInMicroseconds) override;

	void DisableRecording() override;

	bool HasRecording() override;

	void WriteRecordingToDataTable(LibMCEnv::PDataTable pDataTable) override;

	void SetFaultSeed(const LibMCDriver_ScannerEmulator_uint64 nSeed) override;

	void InjectFault(const LibMCDriver_ScannerEmulator::eFaultType eFaultType, const LibMCDriver_ScannerEmulator::eFaultPersistence ePersistence) override;

	void ScheduleFaultAtCommand(const LibMCDriver_ScannerEmulator::eFaultType eFaultType, const LibMCDriver_ScannerEmulator::eFaultPersistence ePersistence, const LibMCDriver_ScannerEmulator_uint32 nCommandIndex) override;

	void ScheduleFaultAtTime(const LibMCDriver_ScannerEmulator::eFaultType eFaultType, const LibMCDriver_ScannerEmulator::eFaultPersistence ePersistence, const LibMCDriver_ScannerEmulator_double dTimeInMicroseconds) override;

	void ScheduleFaultAtExecution(const LibMCDriver_ScannerEmulator::eFaultType eFaultType, const LibMCDriver_ScannerEmulator::eFaultPersistence ePersistence, const LibMCDriver_ScannerEmulator_uint32 nExecutionCount) override;

	void SetRandomFault(const LibMCDriver_ScannerEmulator::eFaultType eFaultType, const LibMCDriver_ScannerEmulator::eFaultPersistence ePersistence, const LibMCDriver_ScannerEmulator_double dProbability) override;

	void SetSlowdownFactor(const LibMCDriver_ScannerEmulator_double dFactor) override;

	LibMCDriver_ScannerEmulator_double GetSlowdownFactor() override;

	void ClearFaultInjection() override;

	LibMCDriver_ScannerEmulator::eFaultType GetActiveFault() override;

	void ResetFault() override;

	LibMCDriver_ScannerEmulator_uint32 GetFaultCount() override;

};

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDRIVER_SCANNEREMULATOR_SCANNER
