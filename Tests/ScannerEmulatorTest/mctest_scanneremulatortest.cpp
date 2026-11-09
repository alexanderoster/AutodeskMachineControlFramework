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

#include "libmcplugin_impl.hpp"
#include "libmcdriver_scanneremulator_dynamic.hpp"

using namespace LibMCPlugin::Impl;

#include <cmath>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <fstream>


/*************************************************************************************************************************
 Import functionality for Driver into current plugin
**************************************************************************************************************************/
LIBMC_IMPORTDRIVERCLASSES(ScannerEmulator, ScannerEmulator)

namespace SE = LibMCDriver_ScannerEmulator;

#define SCANNEREMULATORTEST_TOLERANCE 1.0E-6


/*************************************************************************************************************************
 Check helpers
**************************************************************************************************************************/
static void checkTrue(bool bCondition, const std::string& sMessage)
{
	if (!bCondition)
		throw std::runtime_error("check failed: " + sMessage);
}

static void checkNear(double dActual, double dExpected, const std::string& sMessage, double dTolerance = SCANNEREMULATORTEST_TOLERANCE)
{
	if (!(std::fabs(dActual - dExpected) <= dTolerance))
		throw std::runtime_error("check failed: " + sMessage + " (expected " + std::to_string(dExpected) + ", got " + std::to_string(dActual) + ")");
}

static void checkEqual(uint32_t nActual, uint32_t nExpected, const std::string& sMessage)
{
	if (nActual != nExpected)
		throw std::runtime_error("check failed: " + sMessage + " (expected " + std::to_string(nExpected) + ", got " + std::to_string(nActual) + ")");
}

static void expectError(std::function<void()> callback, LibMCDriver_ScannerEmulatorResult nExpectedErrorCode, const std::string& sMessage)
{
	try {
		callback();
	}
	catch (SE::ELibMCDriver_ScannerEmulatorException& E) {
		if (E.getErrorCode() != nExpectedErrorCode)
			throw std::runtime_error("check failed: " + sMessage + " (expected error " + std::to_string(nExpectedErrorCode) + ", got " + std::to_string(E.getErrorCode()) + ": " + E.what() + ")");
		return;
	}

	throw std::runtime_error("check failed: " + sMessage + " (expected error " + std::to_string(nExpectedErrorCode) + ", but no error was thrown)");
}

static void checkPosition(SE::PScanner pScanner, double dExpectedX, double dExpectedY, const std::string& sMessage)
{
	double dX = 0.0;
	double dY = 0.0;
	pScanner->GetPosition(dX, dY);
	checkNear(dX, dExpectedX, sMessage + " x");
	checkNear(dY, dExpectedY, sMessage + " y");
}

static void checkResult(SE::PExecutionStatistics pStatistics, SE::eExecutionResult eExpectedResult, SE::eFaultType eExpectedFault, const std::string& sMessage)
{
	SE::eExecutionResult eResult = SE::eExecutionResult::None;
	SE::eFaultType eFault = SE::eFaultType::None;
	uint32_t nCommandIndex = 0;
	double dTime = 0.0;
	pStatistics->GetResult(eResult, eFault, nCommandIndex, dTime);
	checkTrue(eResult == eExpectedResult, sMessage + " result");
	checkTrue(eFault == eExpectedFault, sMessage + " fault");
}

static SE::PScanner createFastScanner(PDriver_ScannerEmulator pDriver, const std::string& sName)
{
	auto pScanner = pDriver->CreateScanner(sName, -100.0, -100.0, 100.0, 100.0, 1.0);
	pScanner->SetFastMode(true);
	pScanner->SetJumpSpeed(1000.0);
	pScanner->SetMarkSpeed(500.0);
	return pScanner;
}


/*************************************************************************************************************************
 Test scenarios
**************************************************************************************************************************/
static void testConfiguration(PDriver_ScannerEmulator pDriver)
{
	checkEqual(pDriver->GetScannerCount(), 2, "configured scanner count");
	checkTrue(pDriver->HasScanner("configured"), "has configured scanner");
	checkTrue(pDriver->GetScanner(0)->GetName() == "configured", "scanner ordering");
	checkTrue(pDriver->GetScanner(1)->GetName() == "second", "scanner ordering");

	auto pScanner = pDriver->FindScanner("configured");

	double dX1, dY1, dX2, dY2;
	pScanner->GetViewport(dX1, dY1, dX2, dY2);
	checkNear(dX1, -100.0, "viewport x1");
	checkNear(dY2, 100.0, "viewport y2");
	checkNear(pScanner->GetUnitsInMM(), 1.0, "units");
	checkEqual(pScanner->GetLaserIndex(), 1, "laser index");
	checkNear(pScanner->GetMaxLaserPower(), 400.0, "max power");
	checkTrue(pScanner->IsFastMode(), "fast mode");
	checkNear(pScanner->GetTimeScale(), 2.0, "time scale");
	checkTrue(pScanner->GetOutOfViewportPolicy() == SE::eOutOfViewportPolicy::Clamp, "out of viewport policy");
	checkNear(pScanner->GetJumpSpeed(), 2000.0, "jump speed");
	checkNear(pScanner->GetMarkSpeed(), 800.0, "mark speed");
	checkNear(pScanner->GetPower(), 50.0, "power");
	checkNear(pScanner->GetSlowdownFactor(), 3.0, "slowdown factor");

	double dLaserOnDelay, dLaserOffDelay, dJumpDelay, dMarkDelay, dPolygonDelay;
	pScanner->GetDelays(dLaserOnDelay, dLaserOffDelay, dJumpDelay, dMarkDelay, dPolygonDelay);
	checkNear(dLaserOnDelay, 10.0, "laser on delay");
	checkNear(dLaserOffDelay, 20.0, "laser off delay");
	checkNear(dJumpDelay, 30.0, "jump delay");
	checkNear(dMarkDelay, 40.0, "mark delay");
	checkNear(dPolygonDelay, 50.0, "polygon delay");

	SE::eSkywritingMode eMode;
	double dTimelag, dLaserOnShift, dNPrev, dNPost, dLimit;
	pScanner->GetSkywriting(eMode, dTimelag, dLaserOnShift, dNPrev, dNPost, dLimit);
	checkTrue(eMode == SE::eSkywritingMode::Mode1, "skywriting mode");
	checkNear(dTimelag, 5.0, "skywriting timelag");
	checkNear(dLaserOnShift, 2.0, "skywriting laser on shift");
	checkNear(dNPrev, 100.0, "skywriting nprev");
	checkNear(dNPost, 120.0, "skywriting npost");
	checkNear(dLimit, 0.9, "skywriting limit");

	double dM11, dM12, dM21, dM22, dOffsetX, dOffsetY;
	pScanner->GetTransformation(dM11, dM12, dM21, dM22, dOffsetX, dOffsetY);
	checkNear(dM11, 1.0, "m11");
	checkNear(dM12, 0.0, "m12");
	checkNear(dOffsetX, 1.0, "offset x");
	checkNear(dOffsetY, 2.0, "offset y");

	auto pSecond = pDriver->FindScanner("second");
	checkNear(pSecond->GetUnitsInMM(), 0.5, "second units");
	checkEqual(pSecond->GetLaserIndex(), 2, "second laser index");
	checkTrue(!pSecond->IsFastMode(), "second scanner is realtime");
	checkTrue(pSecond->GetOutOfViewportPolicy() == SE::eOutOfViewportPolicy::Fail, "default out of viewport policy");

	// The configured atexecution fault aborts the first list
	pScanner->AddJumpMovement(10.0, 0.0);
	pScanner->ExecuteList();
	expectError([&]() { pScanner->WaitForList(1000); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_LISTABORTED, "configured fault");
	checkResult(pScanner->GetLastExecutionStatistics(), SE::eExecutionResult::Faulted, SE::eFaultType::ListAborted, "configured fault");
	checkEqual(pScanner->GetFaultCount(), 1, "configured fault count");
	checkTrue(pScanner->GetActiveFault() == SE::eFaultType::None, "transient fault is not latched");

	pScanner->AddJumpMovement(10.0, 0.0);
	pScanner->ExecuteList();
	checkTrue(pScanner->WaitForList(1000), "second execution finishes");
	checkResult(pScanner->GetLastExecutionStatistics(), SE::eExecutionResult::Completed, SE::eFaultType::None, "second execution");
	checkPosition(pScanner, 11.0, 2.0, "configured offset");

	expectError([&]() { pDriver->RemoveScanner("configured"); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_CANNOTREMOVECONFIGUREDSCANNER, "remove configured scanner");
}

static void testScannerManagement(PDriver_ScannerEmulator pDriver)
{
	expectError([&]() { pDriver->CreateScanner("invalid", 10.0, 0.0, -10.0, 10.0, 1.0); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDVIEWPORT, "inverted viewport");
	expectError([&]() { pDriver->CreateScanner("invalid", -10.0, -10.0, 10.0, 10.0, 0.0); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDUNITS, "zero units");
	expectError([&]() { pDriver->CreateScanner("in valid", -10.0, -10.0, 10.0, 10.0, 1.0); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSCANNERNAME, "invalid name");
	expectError([&]() { pDriver->CreateScanner("second", -10.0, -10.0, 10.0, 10.0, 1.0); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERALREADYEXISTS, "duplicate name");
	expectError([&]() { pDriver->FindScanner("unknown"); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERNOTFOUND, "unknown scanner");

	auto pScanner = createFastScanner(pDriver, "management");
	checkEqual(pDriver->GetScannerCount(), 3, "scanner count after create");
	expectError([&]() { pScanner->ExecuteList(); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_EMPTYLIST, "empty list");
	expectError([&]() { pScanner->GetLastExecutionStatistics(); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_NOEXECUTIONAVAILABLE, "no execution yet");
	expectError([&]() { pScanner->SetJumpSpeed(0.0); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSPEED, "zero speed");
	expectError([&]() { pScanner->SetPower(101.0); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPOWER, "power above 100%");
	expectError([&]() { pScanner->SetTimeScale(0.0); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDTIMESCALE, "zero time scale");
	expectError([&]() { pScanner->SetTransformationMatrix(1.0, 2.0, 0.5, 1.0); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDTRANSFORMATION, "singular matrix");

	pDriver->RemoveScanner("management");
	checkTrue(!pDriver->HasScanner("management"), "scanner removed");
}

static void testTiming(PDriver_ScannerEmulator pDriver)
{
	auto pScanner = createFastScanner(pDriver, "timing");

	pScanner->AddJumpMovement(10.0, 0.0);
	pScanner->AddMarkMovement(10.0, 10.0);
	checkEqual(pScanner->GetListCommandCount(), 2, "list command count");
	pScanner->ExecuteList();
	checkTrue(!pScanner->IsBusy(), "fast mode finishes immediately");
	checkTrue(pScanner->WaitForList(1000), "wait for list");
	checkEqual(pScanner->GetListCommandCount(), 0, "list is cleared after execution");

	auto pStatistics = pScanner->GetLastExecutionStatistics();
	checkResult(pStatistics, SE::eExecutionResult::Completed, SE::eFaultType::None, "timing");
	checkNear(pStatistics->GetJumpLengthInMM(), 10.0, "jump length");
	checkNear(pStatistics->GetJumpTimeInMicroseconds(), 10000.0, "jump time");
	checkNear(pStatistics->GetMarkLengthInMM(), 10.0, "mark length");
	checkNear(pStatistics->GetMarkTimeInMicroseconds(), 20000.0, "mark time");
	checkNear(pStatistics->GetLaserOnTimeInMicroseconds(), 20000.0, "laser on time");
	checkNear(pStatistics->GetEmulatedDurationInMicroseconds(), 30000.0, "duration");
	checkNear(pStatistics->GetRealtimeDurationInMicroseconds(), 30000.0, "realtime duration");
	checkEqual(pStatistics->GetCommandCount(), 2, "command count");
	checkEqual(pStatistics->GetMarkSequenceCount(), 1, "mark sequence count");
	checkPosition(pScanner, 10.0, 10.0, "end position");

	// Units: 1 unit = 0.01 mm, so 1000 units are 10 mm
	auto pBitScanner = pDriver->CreateScanner("timingbits", -50000.0, -50000.0, 50000.0, 50000.0, 0.01);
	pBitScanner->SetFastMode(true);
	pBitScanner->SetJumpSpeed(1000.0);
	pBitScanner->AddJumpMovement(1000.0, 0.0);
	pBitScanner->ExecuteList();
	pBitScanner->WaitForList(1000);
	checkNear(pBitScanner->GetLastExecutionStatistics()->GetJumpLengthInMM(), 10.0, "jump length in bits");
	checkNear(pBitScanner->GetLastExecutionStatistics()->GetEmulatedDurationInMicroseconds(), 10000.0, "duration in bits");

	pDriver->RemoveScanner("timing");
	pDriver->RemoveScanner("timingbits");
}

static void testDelays(PDriver_ScannerEmulator pDriver)
{
	auto pScanner = createFastScanner(pDriver, "delays");
	pScanner->SetDelays(100.0, 50.0, 200.0, 300.0, 400.0);

	pScanner->AddJumpMovement(10.0, 0.0);
	pScanner->AddMarkMovement(20.0, 0.0);
	pScanner->AddMarkMovement(20.0, 10.0);
	pScanner->AddJumpMovement(0.0, 0.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);

	double dDiagonal = std::sqrt(500.0);

	auto pStatistics = pScanner->GetLastExecutionStatistics();
	checkNear(pStatistics->GetJumpLengthInMM(), 10.0 + dDiagonal, "jump length");
	checkNear(pStatistics->GetMarkTimeInMicroseconds(), 40000.0, "mark time");
	checkNear(pStatistics->GetScannerDelayTimeInMicroseconds(), 200.0 + 400.0 + 300.0 + 200.0, "scanner delays");
	checkNear(pStatistics->GetLaserOnTimeInMicroseconds(), 40000.0 + 400.0 - 100.0 + 50.0, "laser on time");
	checkNear(pStatistics->GetEmulatedDurationInMicroseconds(), 10000.0 + 200.0 + 20000.0 + 400.0 + 20000.0 + 300.0 + dDiagonal * 1000.0 + 200.0, "duration");
	checkEqual(pStatistics->GetLaserOffDelayWarningCount(), 0, "no laser off delay warning");

	// List commands override the defaults; laser off delay > mark delay is counted as warning
	pScanner->AddSetDelays(0.0, 50.0, 0.0, 0.0, 0.0);
	pScanner->AddSetMarkSpeed(1000.0);
	pScanner->AddMarkMovement(10.0, 0.0);
	pScanner->AddCustomDelay(1000.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);

	pStatistics = pScanner->GetLastExecutionStatistics();
	checkEqual(pStatistics->GetLaserOffDelayWarningCount(), 1, "laser off delay warning");
	checkNear(pStatistics->GetCustomDelayTimeInMicroseconds(), 1000.0, "custom delay");
	checkNear(pStatistics->GetMarkTimeInMicroseconds(), 10000.0, "mark time with list speed");
	checkNear(pStatistics->GetEmulatedDurationInMicroseconds(), 11000.0, "duration with custom delay");

	pDriver->RemoveScanner("delays");
}

static void testSkywriting(PDriver_ScannerEmulator pDriver)
{
	auto pScanner = createFastScanner(pDriver, "skywriting");
	pScanner->SetDelays(0.0, 0.0, 500.0, 700.0, 0.0);
	pScanner->SetSkywriting(SE::eSkywritingMode::Mode1, 20.0, 10.0, 100.0, 150.0, 0.0);

	pScanner->AddJumpMovement(10.0, 0.0);
	pScanner->AddMarkMovement(20.0, 0.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);

	auto pStatistics = pScanner->GetLastExecutionStatistics();
	checkNear(pStatistics->GetSkywritingTimeInMicroseconds(), 250.0, "skywriting time");
	checkNear(pStatistics->GetScannerDelayTimeInMicroseconds(), 0.0, "skywriting replaces jump and mark delay");
	checkNear(pStatistics->GetLaserOnTimeInMicroseconds(), 20000.0 - 10.0, "skywriting laser on time");
	checkNear(pStatistics->GetEmulatedDurationInMicroseconds(), 10000.0 + 100.0 + 20000.0 + 150.0, "skywriting duration");

	// Mode 3 adds run out and run up at corners sharper than the limit
	pScanner->ClearList();
	pScanner->SetDelays(0.0, 0.0, 0.0, 0.0, 400.0);
	pScanner->AddEnableSkywriting(SE::eSkywritingMode::Mode3, 0.0, 0.0, 100.0, 150.0, 0.5);
	pScanner->AddJumpMovement(0.0, 0.0);
	pScanner->AddMarkMovement(10.0, 0.0);
	pScanner->AddMarkMovement(10.0, 10.0);
	pScanner->AddMarkMovement(20.0, 10.0 + 10.0 * std::sqrt(3.0));
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);

	pStatistics = pScanner->GetLastExecutionStatistics();
	checkNear(pStatistics->GetSkywritingTimeInMicroseconds(), 100.0 + 250.0 + 150.0, "mode 3 skywriting time");
	checkNear(pStatistics->GetScannerDelayTimeInMicroseconds(), 400.0, "mode 3 polygon delay for flat corner");

	pDriver->RemoveScanner("skywriting");
}

static void testViewportAndTransformation(PDriver_ScannerEmulator pDriver)
{
	auto pScanner = createFastScanner(pDriver, "viewport");

	pScanner->AddJumpMovement(150.0, 0.0);
	expectError([&]() { pScanner->ExecuteList(); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_OUTOFVIEWPORT, "out of viewport");
	checkEqual(pScanner->GetListCommandCount(), 1, "list is kept after failed execution");

	pScanner->SetOutOfViewportPolicy(SE::eOutOfViewportPolicy::Clamp);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);
	checkEqual(pScanner->GetLastExecutionStatistics()->GetOutOfViewportCount(), 1, "out of viewport count");
	checkPosition(pScanner, 100.0, 0.0, "clamped position");

	pScanner->SetOutOfViewportPolicy(SE::eOutOfViewportPolicy::Fail);
	pScanner->AddJumpMovement(0.0, 0.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);

	pScanner->SetTransformationMatrix(0.0, -1.0, 1.0, 0.0);
	pScanner->SetTransformationOffset(5.0, 0.0);
	pScanner->AddJumpMovement(10.0, 0.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);
	checkPosition(pScanner, 5.0, 10.0, "transformed position");
	checkNear(pScanner->GetLastExecutionStatistics()->GetJumpLengthInMM(), std::sqrt(125.0), "transformed jump length");

	pScanner->ResetTransformation();
	pScanner->AddJumpMovement(1.0, 2.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);
	checkPosition(pScanner, 1.0, 2.0, "identity position");

	pDriver->RemoveScanner("viewport");
}

static void testRealtime(PDriver_ScannerEmulator pDriver)
{
	auto pScanner = createFastScanner(pDriver, "realtime");
	pScanner->SetFastMode(false);

	auto startTime = std::chrono::steady_clock::now();
	pScanner->AddJumpMovement(50.0, 0.0);
	pScanner->ExecuteList();
	checkTrue(pScanner->IsBusy(), "realtime execution is busy");

	SE::eScannerState eState;
	uint32_t nCommandIndex;
	double dProgress;
	pScanner->GetStatus(eState, nCommandIndex, dProgress);
	checkTrue(eState == SE::eScannerState::Busy, "busy state");
	expectError([&]() { pScanner->ExecuteList(); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERBUSY, "execute while busy");
	checkTrue(!pScanner->WaitForList(0), "wait with zero timeout");
	checkTrue(pScanner->WaitForList(5000), "wait for realtime list");

	double dElapsedMilliseconds = (double)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTime).count() / 1000.0;
	checkTrue(dElapsedMilliseconds >= 45.0, "realtime execution takes at least 45ms, took " + std::to_string(dElapsedMilliseconds) + "ms");
	checkPosition(pScanner, 50.0, 0.0, "realtime end position");

	pScanner->GetStatus(eState, nCommandIndex, dProgress);
	checkTrue(eState == SE::eScannerState::Idle, "idle state");
	checkNear(dProgress, 1.0, "progress");

	// Time scale 10 runs ten times faster than real time
	pScanner->SetTimeScale(10.0);
	pScanner->AddJumpMovement(-50.0, 0.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(5000);
	checkNear(pScanner->GetLastExecutionStatistics()->GetRealtimeDurationInMicroseconds(), 10000.0, "time scaled realtime duration");

	// Stopping interrupts the list in between
	pScanner->SetTimeScale(1.0);
	pScanner->AddJumpMovement(50.0, 0.0);
	pScanner->ExecuteList();
	pScanner->StopExecution();
	checkTrue(!pScanner->IsBusy(), "stopped scanner is idle");
	checkResult(pScanner->GetLastExecutionStatistics(), SE::eExecutionResult::Stopped, SE::eFaultType::None, "stopped execution");

	double dX, dY;
	pScanner->GetPosition(dX, dY);
	checkTrue((dX >= -50.0) && (dX < 50.0), "stopped position is in between");

	pDriver->RemoveScanner("realtime");
}

static void testFaults(PDriver_ScannerEmulator pDriver)
{
	auto pScanner = createFastScanner(pDriver, "faults");

	// Latched connection loss blocks the scanner until it is reset
	pScanner->InjectFault(SE::eFaultType::ConnectionLost, SE::eFaultPersistence::Latched);
	checkTrue(pScanner->GetActiveFault() == SE::eFaultType::ConnectionLost, "active connection lost");

	SE::eScannerState eState;
	uint32_t nCommandIndex;
	double dProgress;
	pScanner->GetStatus(eState, nCommandIndex, dProgress);
	checkTrue(eState == SE::eScannerState::Disconnected, "disconnected state");

	pScanner->AddJumpMovement(10.0, 0.0);
	expectError([&]() { pScanner->ExecuteList(); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERDISCONNECTED, "unreported connection lost");
	expectError([&]() { pScanner->ExecuteList(); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERDISCONNECTED, "latched connection lost");
	pScanner->ResetFault();
	checkTrue(pScanner->GetActiveFault() == SE::eFaultType::None, "fault reset");
	pScanner->ExecuteList();
	checkTrue(pScanner->WaitForList(1000), "execution after reset");

	// Fault at a command index
	pScanner->ScheduleFaultAtCommand(SE::eFaultType::PositionError, SE::eFaultPersistence::Transient, 1);
	pScanner->AddJumpMovement(20.0, 0.0);
	pScanner->AddJumpMovement(30.0, 0.0);
	pScanner->AddJumpMovement(40.0, 0.0);
	pScanner->ExecuteList();
	expectError([&]() { pScanner->WaitForList(1000); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERPOSITIONERROR, "position error at command");

	SE::eExecutionResult eResult;
	SE::eFaultType eFault;
	double dTime;
	pScanner->GetLastExecutionStatistics()->GetResult(eResult, eFault, nCommandIndex, dTime);
	checkTrue(eResult == SE::eExecutionResult::Faulted, "faulted result");
	checkTrue(eFault == SE::eFaultType::PositionError, "position error result");
	checkEqual(nCommandIndex, 1, "fault command index");
	checkNear(dTime, 10000.0, "fault time");
	checkPosition(pScanner, 20.0, 0.0, "position at fault");
	checkTrue(pScanner->GetActiveFault() == SE::eFaultType::None, "transient position error");

	// Latched fault at a time
	pScanner->ScheduleFaultAtTime(SE::eFaultType::TemperatureError, SE::eFaultPersistence::Latched, 5000.0);
	pScanner->AddJumpMovement(0.0, 0.0);
	pScanner->ExecuteList();
	expectError([&]() { pScanner->WaitForList(1000); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERTEMPERATUREERROR, "temperature error at time");

	bool bPositionOK, bTemperatureOK, bPowerOK;
	pScanner->GetHeadStatus(bPositionOK, bTemperatureOK, bPowerOK);
	checkTrue(bPositionOK && !bTemperatureOK && bPowerOK, "head status");
	pScanner->GetStatus(eState, nCommandIndex, dProgress);
	checkTrue(eState == SE::eScannerState::Error, "error state");
	checkPosition(pScanner, 15.0, 0.0, "position at temperature error");

	pScanner->AddJumpMovement(0.0, 0.0);
	expectError([&]() { pScanner->ExecuteList(); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERTEMPERATUREERROR, "latched temperature error");
	pScanner->ResetFault();

	// Transient start faults only hit the next execution
	pScanner->InjectFault(SE::eFaultType::Busy, SE::eFaultPersistence::Transient);
	expectError([&]() { pScanner->ExecuteList(); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERBUSY, "transient busy");
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);

	pScanner->InjectFault(SE::eFaultType::LaserNotReady, SE::eFaultPersistence::Latched);
	pScanner->AddJumpMovement(10.0, 0.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);
	pScanner->AddMarkMovement(0.0, 0.0);
	expectError([&]() { pScanner->ExecuteList(); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_LASERNOTREADY, "laser not ready with marks");
	pScanner->ResetFault();
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);

	// Fault at the second execution from now
	pScanner->ScheduleFaultAtExecution(SE::eFaultType::PowerError, SE::eFaultPersistence::Transient, 2);
	pScanner->AddJumpMovement(10.0, 0.0);
	pScanner->ExecuteList();
	checkTrue(pScanner->WaitForList(1000), "first execution before scheduled fault");
	pScanner->AddJumpMovement(0.0, 0.0);
	pScanner->ExecuteList();
	expectError([&]() { pScanner->WaitForList(1000); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERPOWERERROR, "power error at execution");

	// Slowdown halves the speed of the next execution
	pScanner->AddJumpMovement(0.0, 0.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);
	pScanner->SetFastMode(false);
	pScanner->SetSlowdownFactor(2.0);
	pScanner->InjectFault(SE::eFaultType::Slowdown, SE::eFaultPersistence::Transient);
	pScanner->AddJumpMovement(20.0, 0.0);
	pScanner->ExecuteList();
	checkTrue(pScanner->WaitForList(5000), "slowed down execution finishes");
	checkNear(pScanner->GetLastExecutionStatistics()->GetRealtimeDurationInMicroseconds(), 40000.0, "slowed down realtime duration");
	pScanner->SetFastMode(true);

	// Random faults with probability one always fire
	pScanner->SetFaultSeed(1234);
	pScanner->SetRandomFault(SE::eFaultType::ListAborted, SE::eFaultPersistence::Transient, 1.0);
	for (uint32_t nIndex = 0; nIndex < 3; nIndex++) {
		pScanner->AddJumpMovement(0.0, 0.0);
		pScanner->AddJumpMovement(10.0, 0.0);
		pScanner->ExecuteList();
		expectError([&]() { pScanner->WaitForList(1000); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_LISTABORTED, "random fault");
	}
	pScanner->SetRandomFault(SE::eFaultType::ListAborted, SE::eFaultPersistence::Transient, 0.0);
	pScanner->AddJumpMovement(0.0, 0.0);
	pScanner->ExecuteList();
	checkTrue(pScanner->WaitForList(1000), "random fault removed");

	// Scheduled faults are dropped by ClearFaultInjection
	pScanner->ScheduleFaultAtExecution(SE::eFaultType::PowerError, SE::eFaultPersistence::Transient, 1);
	pScanner->ClearFaultInjection();
	pScanner->AddJumpMovement(10.0, 0.0);
	pScanner->ExecuteList();
	checkTrue(pScanner->WaitForList(1000), "cleared fault injection");

	checkEqual(pScanner->GetFaultCount(), 10, "fault count");

	pDriver->RemoveScanner("faults");
}

static void testRecording(PDriver_ScannerEmulator pDriver, LibMCEnv::PStateEnvironment pStateEnvironment)
{
	auto pScanner = createFastScanner(pDriver, "recording");
	pScanner->SetPower(40.0);

	auto pDataTable = pStateEnvironment->CreateDataTable();
	expectError([&]() { pScanner->WriteRecordingToDataTable(pDataTable); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_NORECORDINGAVAILABLE, "no recording before execution");

	pScanner->EnableRecording(1000.0);
	pScanner->AddJumpMovement(10.0, 0.0);
	pScanner->AddMarkMovement(10.0, 10.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);
	checkTrue(pScanner->HasRecording(), "has recording");

	pScanner->WriteRecordingToDataTable(pDataTable);
	checkEqual(pDataTable->GetRowCount(), 31, "recording row count");
	checkEqual(pDataTable->GetColumnCount(), 6, "recording column count");

	std::vector<double> times, positionsX, positionsY, powers;
	std::vector<uint32_t> laserOn, commandIndices;
	pDataTable->GetDoubleColumnValues("time_us", times);
	pDataTable->GetDoubleColumnValues("x", positionsX);
	pDataTable->GetDoubleColumnValues("y", positionsY);
	pDataTable->GetDoubleColumnValues("power", powers);
	pDataTable->GetUint32ColumnValues("laser_on", laserOn);
	pDataTable->GetUint32ColumnValues("command_index", commandIndices);

	checkNear(times[0], 0.0, "first sample time");
	checkNear(times[30], 30000.0, "last sample time");
	checkNear(positionsX[5], 5.0, "jump sample x");
	checkEqual(laserOn[5], 0, "laser off during jump");
	checkEqual(commandIndices[5], 0, "jump command index");
	checkNear(positionsX[15], 10.0, "mark sample x");
	checkNear(positionsY[15], 2.5, "mark sample y");
	checkEqual(laserOn[15], 1, "laser on during mark");
	checkEqual(commandIndices[15], 1, "mark command index");
	checkNear(powers[15], 40.0, "power during mark");
	checkNear(positionsY[30], 10.0, "last sample y");
	checkEqual(laserOn[30], 0, "laser off at end");

	// Interval 0 records events only; rewriting replaces the columns
	pScanner->EnableRecording(0.0);
	pScanner->AddJumpMovement(0.0, 0.0);
	pScanner->AddMarkMovement(10.0, 0.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);
	pScanner->WriteRecordingToDataTable(pDataTable);
	checkEqual(pDataTable->GetRowCount(), 3, "event recording row count");
	checkEqual(pDataTable->GetColumnCount(), 6, "event recording column count");

	pScanner->DisableRecording();
	pScanner->AddJumpMovement(0.0, 0.0);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);
	checkTrue(!pScanner->HasRecording(), "recording disabled");

	pDriver->RemoveScanner("recording");
}

static void drawAndWait(SE::PScanner pScanner, double dJumpX, double dJumpY, double dMarkX, double dMarkY)
{
	pScanner->AddJumpMovement(dJumpX, dJumpY);
	pScanner->AddMarkMovement(dMarkX, dMarkY);
	pScanner->ExecuteList();
	pScanner->WaitForList(1000);
}

static void checkPixel(SE::PScannerImage pImage, uint32_t nPixelX, uint32_t nPixelY, uint32_t nExpectedColor, uint32_t nExpectedHitCount, const std::string& sMessage)
{
	checkEqual(pImage->GetPixelColor(nPixelX, nPixelY), nExpectedColor, sMessage + " color");
	checkEqual(pImage->GetPixelHitCount(nPixelX, nPixelY), nExpectedHitCount, sMessage + " hit count");
}

static void testScannerImage(PDriver_ScannerEmulator pDriver, LibMCEnv::PStateEnvironment pStateEnvironment)
{
	const uint32_t nRed = 0xFF0000;
	const uint32_t nGreen = 0x00FF00;
	const uint32_t nBlue = 0x0000FF;
	const uint32_t nYellow = 0xFFFF00;
	const uint32_t nBackground = 0x202020;

	expectError([&]() { pDriver->CreateScannerImage("bad name", 0.0, 0.0, 100.0, 100.0, 200, 200); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSCANNERIMAGENAME, "invalid image name");
	expectError([&]() { pDriver->CreateScannerImage("invalid", 100.0, 0.0, 100.0, 100.0, 200, 200); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDIMAGEWINDOW, "empty image window");
	expectError([&]() { pDriver->CreateScannerImage("invalid", 0.0, 0.0, 100.0, 100.0, 0, 200); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDIMAGESIZE, "zero image size");
	expectError([&]() { pDriver->CreateScannerImage("invalid", 0.0, 0.0, 100.0, 100.0, 16384, 16384); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDIMAGESIZE, "too many pixels");

	// 0.5 mm pixels. Pixel (0, 0) is the top left corner at (0 mm, 100 mm).
	auto pImage = pDriver->CreateScannerImage("debug", 0.0, 0.0, 100.0, 100.0, 200, 200);
	expectError([&]() { pDriver->CreateScannerImage("debug", 0.0, 0.0, 10.0, 10.0, 10, 10); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERIMAGEALREADYEXISTS, "duplicate image");
	checkTrue(pDriver->HasScannerImage("debug"), "has image");
	checkEqual(pDriver->GetScannerImageCount(), 1, "image count");
	checkTrue(pDriver->FindScannerImage("debug")->GetName() == "debug", "find image");

	uint32_t nPixelCountX = 0;
	uint32_t nPixelCountY = 0;
	pImage->GetPixelCount(nPixelCountX, nPixelCountY);
	checkEqual(nPixelCountX, 200, "pixel count x");
	checkEqual(nPixelCountY, 200, "pixel count y");
	double dX1, dY1, dX2, dY2;
	pImage->GetWindow(dX1, dY1, dX2, dY2);
	checkNear(dX2, 100.0, "window x2");
	checkEqual(pImage->GetBackgroundColor(), 0, "default background");
	checkNear(pImage->GetMicrostepInterval(), 10.0, "default microstep interval");
	checkTrue(pImage->IsRecording(), "recording after creation");

	expectError([&]() { pImage->SetMicrostepInterval(0.0); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDMICROSTEPINTERVAL, "invalid microstep interval");
	expectError([&]() { pImage->SetBackgroundColor(0x1000000); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCOLOR, "invalid background color");
	expectError([&]() { pImage->GetPixelColor(200, 0); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPIXELCOORDINATE, "invalid pixel coordinate");
	pImage->SetBackgroundColor(nBackground);

	auto pScannerA = createFastScanner(pDriver, "imagea");
	auto pScannerB = createFastScanner(pDriver, "imageb");

	expectError([&]() { pImage->AssignScanner(pScannerA.get(), -1.0, nRed, nGreen, false); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSPOTRADIUS, "negative spot radius");
	expectError([&]() { pImage->AssignScanner(pScannerA.get(), 1000.0, nRed, nGreen, false); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSPOTRADIUS, "huge spot radius");
	expectError([&]() { pImage->AssignScanner(pScannerA.get(), 0.0, 0x1000000, nGreen, false); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCOLOR, "invalid laser on color");

	pImage->AssignScanner(pScannerA.get(), 0.0, nRed, nGreen, false);
	pImage->AssignScanner(pScannerB.get(), 0.0, nBlue, nGreen, false);
	checkTrue(pImage->IsScannerAssigned("imagea"), "scanner a assigned");

	// Horizontal red line at y = 50 mm (row 100), vertical blue line at x = 50 mm (column 100)
	drawAndWait(pScannerA, 10.0, 50.0, 90.0, 50.0);
	checkPixel(pImage, 40, 100, nRed, 1, "red line");
	checkPixel(pImage, 100, 50, nBackground, 0, "untouched pixel");
	// The jump from (0, 0) to (10, 50) passes (5, 25)
	checkPixel(pImage, 10, 150, nBackground, 0, "laser off jump is not drawn");

	drawAndWait(pScannerB, 50.0, 10.0, 50.0, 90.0);
	checkPixel(pImage, 100, 40, nBlue, 1, "blue line");
	checkPixel(pImage, 100, 100, 0x800080, 2, "crossing blends");

	// Laser off microsteps: scanner a jumps from (90, 50) down to (90, 10) along column 180
	pImage->AssignScanner(pScannerA.get(), 0.0, nRed, nGreen, true);
	pScannerA->AddJumpMovement(90.0, 10.0);
	pScannerA->ExecuteList();
	pScannerA->WaitForList(1000);
	checkPixel(pImage, 180, 140, nGreen, 1, "laser off line");

	// Paused recording draws nothing
	pImage->SetRecording(false);
	checkTrue(!pImage->IsRecording(), "recording paused");
	drawAndWait(pScannerA, 90.0, 10.0, 10.0, 10.0);
	checkPixel(pImage, 100, 180, nBackground, 0, "paused recording");
	pImage->SetRecording(true);

	// Unassigned scanners draw nothing
	pImage->UnassignScanner("imageb");
	checkTrue(!pImage->IsScannerAssigned("imageb"), "scanner b unassigned");
	drawAndWait(pScannerB, 10.0, 90.0, 30.0, 90.0);
	checkPixel(pImage, 40, 20, nBackground, 0, "unassigned scanner");

	// A spot radius of 2 mm covers 4 pixels on each side of the path at row 20
	pImage->AssignScanner(pScannerB.get(), 2.0, nYellow, nGreen, false);
	drawAndWait(pScannerB, 10.0, 90.0, 30.0, 90.0);
	checkPixel(pImage, 40, 20, nYellow, 1, "spot center");
	checkPixel(pImage, 40, 24, nYellow, 1, "spot edge");
	checkPixel(pImage, 40, 25, nBackground, 0, "outside spot");

	// A small hatch, so that the written image shows a filled area
	pImage->AssignScanner(pScannerA.get(), 0.3, nRed, nGreen, false);
	for (uint32_t nLine = 0; nLine <= 20; nLine++) {
		double dY = 60.0 + nLine;
		pScannerA->AddJumpMovement(60.0, dY);
		pScannerA->AddMarkMovement(80.0, dY);
	}
	pScannerA->ExecuteList();
	pScannerA->WaitForList(1000);

	auto pWrongImageData = pStateEnvironment->CreateEmptyImage(100, 100, 50.8, 50.8, LibMCEnv::eImagePixelFormat::RGB24bit);
	expectError([&]() { pImage->WriteToImageData(pWrongImageData); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_IMAGESIZEMISMATCH, "image size mismatch");

	auto pImageData = pStateEnvironment->CreateEmptyImage(200, 200, 50.8, 50.8, LibMCEnv::eImagePixelFormat::RGB24bit);
	pImage->WriteToImageData(pImageData);

	std::vector<uint8_t> pixelBuffer;
	pImageData->GetPixels(100, 100, 1, 1, LibMCEnv::eImagePixelFormat::RGB24bit, pixelBuffer);
	checkEqual((uint32_t)pixelBuffer.size(), 3, "pixel buffer size");
	checkEqual(((uint32_t)pixelBuffer[0] << 16) | ((uint32_t)pixelBuffer[1] << 8) | (uint32_t)pixelBuffer[2], 0x800080, "written crossing pixel");
	pImageData->GetPixels(100, 50, 1, 1, LibMCEnv::eImagePixelFormat::RGB24bit, pixelBuffer);
	checkEqual(((uint32_t)pixelBuffer[0] << 16) | ((uint32_t)pixelBuffer[1] << 8) | (uint32_t)pixelBuffer[2], nBackground, "written background pixel");

	pImageData->EncodePNG();
	std::vector<uint8_t> pngData;
	pImageData->GetEncodedPNGData(pngData);
	checkTrue(pngData.size() > 0, "png data");

	std::ofstream pngStream("scanneremulator_image.png", std::ios::binary | std::ios::out);
	pngStream.write((const char*)pngData.data(), pngData.size());
	pngStream.close();
	pStateEnvironment->LogMessage("Wrote scanneremulator_image.png");

	pImage->Clear();
	checkPixel(pImage, 100, 100, nBackground, 0, "cleared image");

	// Removing a scanner unassigns it from all images
	pDriver->RemoveScanner("imagea");
	checkTrue(!pImage->IsScannerAssigned("imagea"), "removed scanner is unassigned");

	// A removed image stops drawing, even though the plugin still holds it
	pDriver->RemoveScannerImage("debug");
	checkTrue(!pDriver->HasScannerImage("debug"), "image removed");
	checkEqual(pDriver->GetScannerImageCount(), 0, "image count after removal");
	expectError([&]() { pDriver->FindScannerImage("debug"); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERIMAGENOTFOUND, "find removed image");
	expectError([&]() { pDriver->RemoveScannerImage("debug"); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERIMAGENOTFOUND, "remove removed image");
	expectError([&]() { pImage->AssignScanner(pScannerB.get(), 0.0, nRed, nGreen, false); }, LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERIMAGENOTFOUND, "assign to removed image");
	checkTrue(!pImage->IsScannerAssigned("imageb"), "removed image has no assignments");
	drawAndWait(pScannerB, 10.0, 90.0, 30.0, 90.0);
	checkPixel(pImage, 40, 20, nBackground, 0, "removed image does not draw");

	pDriver->RemoveScanner("imageb");
}


/*************************************************************************************************************************
 Class definition of CScannerEmulatorData
**************************************************************************************************************************/
class CScannerEmulatorData : public virtual CPluginData {
protected:
	// We need to globally store driver wrappers in the plugin
	PDriverCast_ScannerEmulator m_DriverCast_ScannerEmulator;

public:

	PDriver_ScannerEmulator acquireScannerEmulator(LibMCEnv::PStateEnvironment pStateEnvironment)
	{
		return m_DriverCast_ScannerEmulator.acquireDriver(pStateEnvironment, "scanneremulator");
	}

};

/*************************************************************************************************************************
 Class definition of CScannerEmulatorState
**************************************************************************************************************************/
typedef CState<CScannerEmulatorData> CScannerEmulatorState;


/*************************************************************************************************************************
 Class definition of CScannerEmulatorState_Init
**************************************************************************************************************************/
class CScannerEmulatorState_Init : public virtual CScannerEmulatorState {
public:

	CScannerEmulatorState_Init(const std::string& sStateName, PPluginData pPluginData)
		: CScannerEmulatorState(getStateName(), sStateName, pPluginData)
	{
	}

	static const std::string getStateName()
	{
		return "init";
	}


	void Execute(LibMCEnv::PStateEnvironment pStateEnvironment)
	{
		if (pStateEnvironment.get() == nullptr)
			throw ELibMCPluginInterfaceException(LIBMCPLUGIN_ERROR_INVALIDPARAM);

		auto pDriver = m_pPluginData->acquireScannerEmulator(pStateEnvironment);

		pStateEnvironment->LogMessage("Testing configuration...");
		testConfiguration(pDriver);
		pStateEnvironment->LogMessage("Testing scanner management...");
		testScannerManagement(pDriver);
		pStateEnvironment->LogMessage("Testing timing...");
		testTiming(pDriver);
		pStateEnvironment->LogMessage("Testing delays...");
		testDelays(pDriver);
		pStateEnvironment->LogMessage("Testing skywriting...");
		testSkywriting(pDriver);
		pStateEnvironment->LogMessage("Testing viewport and transformation...");
		testViewportAndTransformation(pDriver);
		pStateEnvironment->LogMessage("Testing realtime execution...");
		testRealtime(pDriver);
		pStateEnvironment->LogMessage("Testing faults...");
		testFaults(pDriver);
		pStateEnvironment->LogMessage("Testing recording...");
		testRecording(pDriver, pStateEnvironment);
		pStateEnvironment->LogMessage("Testing scanner images...");
		testScannerImage(pDriver, pStateEnvironment);

		pStateEnvironment->LogMessage("Scanner emulator test successful.");
		pStateEnvironment->SetNextState("success");
	}

};



/*************************************************************************************************************************
 Class definition of CScannerEmulatorState_Success
**************************************************************************************************************************/
class CScannerEmulatorState_Success : public virtual CScannerEmulatorState {
public:

	CScannerEmulatorState_Success(const std::string& sStateName, PPluginData pPluginData)
		: CScannerEmulatorState(getStateName(), sStateName, pPluginData)
	{
	}

	static const std::string getStateName()
	{
		return "success";
	}


	void Execute(LibMCEnv::PStateEnvironment pStateEnvironment)
	{
		pStateEnvironment->SetNextState("success");
	}

};


/*************************************************************************************************************************
 Class definition of CScannerEmulatorState_FatalError
**************************************************************************************************************************/
class CScannerEmulatorState_FatalError : public virtual CScannerEmulatorState {
public:

	CScannerEmulatorState_FatalError(const std::string& sStateName, PPluginData pPluginData)
		: CScannerEmulatorState(getStateName(), sStateName, pPluginData)
	{
	}

	static const std::string getStateName()
	{
		return "fatalerror";
	}


	void Execute(LibMCEnv::PStateEnvironment pStateEnvironment)
	{
		if (pStateEnvironment.get() == nullptr)
			throw ELibMCPluginInterfaceException(LIBMCPLUGIN_ERROR_INVALIDPARAM);

		pStateEnvironment->SetNextState("fatalerror");
	}

};



/*************************************************************************************************************************
 Class definition of CStateFactory
**************************************************************************************************************************/

CStateFactory::CStateFactory(const std::string& sInstanceName)
{
	m_pPluginData = std::make_shared<CScannerEmulatorData>();
}

IState* CStateFactory::CreateState(const std::string& sStateName)
{

	IState* pStateInstance = nullptr;

	if (createStateInstanceByName<CScannerEmulatorState_Init>(sStateName, pStateInstance, m_pPluginData))
		return pStateInstance;

	if (createStateInstanceByName<CScannerEmulatorState_Success>(sStateName, pStateInstance, m_pPluginData))
		return pStateInstance;

	if (createStateInstanceByName<CScannerEmulatorState_FatalError>(sStateName, pStateInstance, m_pPluginData))
		return pStateInstance;

	throw ELibMCPluginInterfaceException(LIBMCPLUGIN_ERROR_INVALIDSTATENAME);

}

