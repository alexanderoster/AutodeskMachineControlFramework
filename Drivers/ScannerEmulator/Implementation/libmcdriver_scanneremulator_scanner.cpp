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


Abstract: This is a stub class definition of CScanner

*/

#include "libmcdriver_scanneremulator_scanner.hpp"
#include "libmcdriver_scanneremulator_executionstatistics.hpp"
#include "libmcdriver_scanneremulator_interfaceexception.hpp"

#include <cmath>
#include <chrono>
#include <algorithm>

#define SCANNEREMULATOR_SKYWRITINGNAMESPACE "http://schemas.scanlab.com/skywriting/2023/01"
#define SCANNEREMULATOR_MAXWAITSLICE 50
#define SCANNEREMULATOR_INFINITETIMEOUT 0xFFFFFFFF
#define SCANNEREMULATOR_PI 3.14159265358979323846

using namespace LibMCDriver_ScannerEmulator;
using namespace LibMCDriver_ScannerEmulator::Impl;

/*************************************************************************************************************************
 Class definition of CScanner 
**************************************************************************************************************************/

CScanner::CScanner(PScannerInstance pInstance, LibMCEnv::PDriverEnvironment pDriverEnvironment)
	: m_pInstance(pInstance), m_pDriverEnvironment(pDriverEnvironment)
{
	if (pInstance.get() == nullptr)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);
	if (pDriverEnvironment.get() == nullptr)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);
}

CScanner::~CScanner()
{
}

PScannerInstance CScanner::getInstance()
{
	return m_pInstance;
}

static double readSkywritingValue(LibMCEnv::CToolpathLayer* pLayer, uint32_t nSegmentIndex, const std::string& sName, double dFactorToMicroseconds, double dDefaultValue)
{
	std::string sNameInMicroseconds = sName + "_us";
	bool bHasValue = pLayer->SegmentProfileHasValue(nSegmentIndex, SCANNEREMULATOR_SKYWRITINGNAMESPACE, sName);
	bool bHasValueInMicroseconds = pLayer->SegmentProfileHasValue(nSegmentIndex, SCANNEREMULATOR_SKYWRITINGNAMESPACE, sNameInMicroseconds);

	if (bHasValue && bHasValueInMicroseconds)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSKYWRITINGPARAMETER, "skywriting value provided twice in segment " + std::to_string(nSegmentIndex) + ": " + sName);

	if (bHasValue)
		return pLayer->GetSegmentProfileDoubleValue(nSegmentIndex, SCANNEREMULATOR_SKYWRITINGNAMESPACE, sName) * dFactorToMicroseconds;
	if (bHasValueInMicroseconds)
		return pLayer->GetSegmentProfileDoubleValue(nSegmentIndex, SCANNEREMULATOR_SKYWRITINGNAMESPACE, sNameInMicroseconds);

	return dDefaultValue;
}

static double readSkywritingLimit(LibMCEnv::CToolpathLayer* pLayer, uint32_t nSegmentIndex, double dDefaultValue)
{
	bool bHasCosine = pLayer->SegmentProfileHasValue(nSegmentIndex, SCANNEREMULATOR_SKYWRITINGNAMESPACE, "limit");
	bool bHasDegree = pLayer->SegmentProfileHasValue(nSegmentIndex, SCANNEREMULATOR_SKYWRITINGNAMESPACE, "limit_degree");

	if (bHasCosine && bHasDegree)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSKYWRITINGPARAMETER, "skywriting limit provided twice in segment " + std::to_string(nSegmentIndex));

	if (bHasCosine)
		return pLayer->GetSegmentProfileDoubleValue(nSegmentIndex, SCANNEREMULATOR_SKYWRITINGNAMESPACE, "limit");
	if (bHasDegree)
		return cos(pLayer->GetSegmentProfileDoubleValue(nSegmentIndex, SCANNEREMULATOR_SKYWRITINGNAMESPACE, "limit_degree") / 180.0 * SCANNEREMULATOR_PI);

	return dDefaultValue;
}

static bool delaysAreEqual(const sEmulatorDelays& delays1, const sEmulatorDelays& delays2)
{
	return (delays1.m_dLaserOnDelay == delays2.m_dLaserOnDelay) && (delays1.m_dLaserOffDelay == delays2.m_dLaserOffDelay)
		&& (delays1.m_dJumpDelay == delays2.m_dJumpDelay) && (delays1.m_dMarkDelay == delays2.m_dMarkDelay) && (delays1.m_dPolygonDelay == delays2.m_dPolygonDelay);
}

static bool skywritingsAreEqual(const sEmulatorSkywriting& skywriting1, const sEmulatorSkywriting& skywriting2)
{
	if (skywriting1.m_Mode != skywriting2.m_Mode)
		return false;
	if (skywriting1.m_Mode == eSkywritingMode::Off)
		return true;

	return (skywriting1.m_dTimelag == skywriting2.m_dTimelag) && (skywriting1.m_dLaserOnShift == skywriting2.m_dLaserOnShift)
		&& (skywriting1.m_dNPrev == skywriting2.m_dNPrev) && (skywriting1.m_dNPost == skywriting2.m_dNPost) && (skywriting1.m_dLimit == skywriting2.m_dLimit);
}

void CScanner::buildLayerCommandList(CScannerInstance* pInstance, LibMCEnv::CToolpathLayer* pLayer, bool bFailIfNonAssignedDataExists, CEmulatorCommandList& commandList)
{
	if ((pInstance == nullptr) || (pLayer == nullptr))
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);

	uint32_t nScannerLaserIndex = pInstance->getLaserIndex();
	double dUnitsInMM = pInstance->getUnitsInMM();
	double dMaxPowerInWatts = pInstance->getMaxPowerInWatts();
	sEmulatorSettings defaults = pInstance->getDefaultSettings();

	bool bHasCurrentSettings = false;
	sEmulatorSettings currentSettings = defaults;

	std::vector<LibMCEnv::sFloatHatch2D> hatches;
	std::vector<LibMCEnv::sFloatPosition2D> points;

	uint32_t nSegmentCount = pLayer->GetSegmentCount();
	for (uint32_t nSegmentIndex = 0; nSegmentIndex < nSegmentCount; nSegmentIndex++) {

		LibMCEnv::eToolpathSegmentType segmentType;
		uint32_t nPointCount = 0;
		pLayer->GetSegmentInfo(nSegmentIndex, segmentType, nPointCount);

		if ((segmentType != LibMCEnv::eToolpathSegmentType::Hatch) && (segmentType != LibMCEnv::eToolpathSegmentType::Polyline))
			continue;
		if (nPointCount == 0)
			continue;

		double dLaserIndex = pLayer->GetSegmentProfileDoubleValueDef(nSegmentIndex, "", "laserindex", 0.0);
		int64_t nLaserIndex = (int64_t)std::round(dLaserIndex);
		if ((std::abs(dLaserIndex - (double)nLaserIndex) > 0.001) || (nLaserIndex < 0) || (nLaserIndex > 0xFFFFFFFFLL))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDLASERINDEX, "segment " + std::to_string(nSegmentIndex) + " has an invalid laser index: " + std::to_string(dLaserIndex));

		if ((nLaserIndex == 0) && bFailIfNonAssignedDataExists)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_LASERINDEXHASNOASSIGNEDSCANNER, "segment " + std::to_string(nSegmentIndex) + " has no laser index");

		if ((nScannerLaserIndex != 0) && ((uint32_t)nLaserIndex != nScannerLaserIndex))
			continue;

		sEmulatorSettings segmentSettings;
		segmentSettings.m_dJumpSpeed = pLayer->GetSegmentProfileTypedValueDef(nSegmentIndex, LibMCEnv::eToolpathProfileValueType::JumpSpeed, defaults.m_dJumpSpeed);
		segmentSettings.m_dMarkSpeed = pLayer->GetSegmentProfileTypedValueDef(nSegmentIndex, LibMCEnv::eToolpathProfileValueType::Speed, defaults.m_dMarkSpeed);

		double dPowerInWatts = pLayer->GetSegmentProfileTypedValueDef(nSegmentIndex, LibMCEnv::eToolpathProfileValueType::LaserPower, defaults.m_dPower * dMaxPowerInWatts / 100.0);
		segmentSettings.m_dPower = dPowerInWatts / dMaxPowerInWatts * 100.0;
		if (!std::isfinite(segmentSettings.m_dPower) || (segmentSettings.m_dPower < 0.0) || (segmentSettings.m_dPower > 100.0 + 1.0E-9))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPOWER, "segment " + std::to_string(nSegmentIndex) + " has an invalid laser power: " + std::to_string(dPowerInWatts) + "W");
		if (segmentSettings.m_dPower > 100.0)
			segmentSettings.m_dPower = 100.0;

		segmentSettings.m_Delays.m_dLaserOnDelay = pLayer->GetSegmentProfileTypedValueDef(nSegmentIndex, LibMCEnv::eToolpathProfileValueType::LaserOnDelay, defaults.m_Delays.m_dLaserOnDelay);
		segmentSettings.m_Delays.m_dLaserOffDelay = pLayer->GetSegmentProfileTypedValueDef(nSegmentIndex, LibMCEnv::eToolpathProfileValueType::LaserOffDelay, defaults.m_Delays.m_dLaserOffDelay);
		segmentSettings.m_Delays.m_dJumpDelay = pLayer->GetSegmentProfileTypedValueDef(nSegmentIndex, LibMCEnv::eToolpathProfileValueType::JumpDelay, defaults.m_Delays.m_dJumpDelay);
		segmentSettings.m_Delays.m_dMarkDelay = pLayer->GetSegmentProfileTypedValueDef(nSegmentIndex, LibMCEnv::eToolpathProfileValueType::MarkDelay, defaults.m_Delays.m_dMarkDelay);
		segmentSettings.m_Delays.m_dPolygonDelay = pLayer->GetSegmentProfileTypedValueDef(nSegmentIndex, LibMCEnv::eToolpathProfileValueType::PolyDelay, defaults.m_Delays.m_dPolygonDelay);

		int64_t nSkywritingMode = pLayer->GetSegmentProfileIntegerValueDef(nSegmentIndex, SCANNEREMULATOR_SKYWRITINGNAMESPACE, "mode", (int64_t)defaults.m_Skywriting.m_Mode);
		if ((nSkywritingMode < 0) || (nSkywritingMode > 4))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSKYWRITINGPARAMETER, "segment " + std::to_string(nSegmentIndex) + " has an invalid skywriting mode: " + std::to_string(nSkywritingMode));

		segmentSettings.m_Skywriting = defaults.m_Skywriting;
		segmentSettings.m_Skywriting.m_Mode = (eSkywritingMode)nSkywritingMode;
		if (segmentSettings.m_Skywriting.m_Mode != eSkywritingMode::Off) {
			segmentSettings.m_Skywriting.m_dTimelag = readSkywritingValue(pLayer, nSegmentIndex, "timelag", 1.0, defaults.m_Skywriting.m_dTimelag);
			segmentSettings.m_Skywriting.m_dLaserOnShift = readSkywritingValue(pLayer, nSegmentIndex, "laseronshift", 1.0 / 64.0, defaults.m_Skywriting.m_dLaserOnShift);
			segmentSettings.m_Skywriting.m_dNPrev = readSkywritingValue(pLayer, nSegmentIndex, "nprev", 10.0, defaults.m_Skywriting.m_dNPrev);
			segmentSettings.m_Skywriting.m_dNPost = readSkywritingValue(pLayer, nSegmentIndex, "npost", 10.0, defaults.m_Skywriting.m_dNPost);
			segmentSettings.m_Skywriting.m_dLimit = readSkywritingLimit(pLayer, nSegmentIndex, defaults.m_Skywriting.m_dLimit);
		}

		if (!bHasCurrentSettings || (segmentSettings.m_dJumpSpeed != currentSettings.m_dJumpSpeed))
			commandList.addSetJumpSpeed(segmentSettings.m_dJumpSpeed);
		if (!bHasCurrentSettings || (segmentSettings.m_dMarkSpeed != currentSettings.m_dMarkSpeed))
			commandList.addSetMarkSpeed(segmentSettings.m_dMarkSpeed);
		if (!bHasCurrentSettings || (segmentSettings.m_dPower != currentSettings.m_dPower))
			commandList.addSetPower(segmentSettings.m_dPower);
		if (!bHasCurrentSettings || !delaysAreEqual(segmentSettings.m_Delays, currentSettings.m_Delays))
			commandList.addSetDelays(segmentSettings.m_Delays);
		if (!bHasCurrentSettings || !skywritingsAreEqual(segmentSettings.m_Skywriting, currentSettings.m_Skywriting)) {
			if (segmentSettings.m_Skywriting.m_Mode == eSkywritingMode::Off)
				commandList.addDisableSkywriting();
			else
				commandList.addEnableSkywriting(segmentSettings.m_Skywriting);
		}

		currentSettings = segmentSettings;
		bHasCurrentSettings = true;

		double dPreSegmentDelay = pLayer->GetSegmentProfileTypedValueDef(nSegmentIndex, LibMCEnv::eToolpathProfileValueType::PreSegmentDelay, 0.0);
		double dPostSegmentDelay = pLayer->GetSegmentProfileTypedValueDef(nSegmentIndex, LibMCEnv::eToolpathProfileValueType::PostSegmentDelay, 0.0);

		if (dPreSegmentDelay > 0.0)
			commandList.addCustomDelay(dPreSegmentDelay);

		if (segmentType == LibMCEnv::eToolpathSegmentType::Hatch) {
			pLayer->GetSegmentHatchDataInMM(nSegmentIndex, hatches);
			for (auto& hatch : hatches) {
				commandList.addJump(hatch.m_X1 / dUnitsInMM, hatch.m_Y1 / dUnitsInMM);
				commandList.addMark(hatch.m_X2 / dUnitsInMM, hatch.m_Y2 / dUnitsInMM);
			}
		}
		else {
			pLayer->GetSegmentPolylineDataInMM(nSegmentIndex, points);
			for (size_t nPointIndex = 0; nPointIndex < points.size(); nPointIndex++) {
				double dX = points[nPointIndex].m_Coordinates[0] / dUnitsInMM;
				double dY = points[nPointIndex].m_Coordinates[1] / dUnitsInMM;
				if (nPointIndex == 0)
					commandList.addJump(dX, dY);
				else
					commandList.addMark(dX, dY);
			}
		}

		if (dPostSegmentDelay > 0.0)
			commandList.addCustomDelay(dPostSegmentDelay);
	}
}

bool CScanner::waitForInstance(CScannerInstance* pInstance, LibMCEnv::CDriverEnvironment* pDriverEnvironment, uint32_t nTimeOutInMilliseconds)
{
	if ((pInstance == nullptr) || (pDriverEnvironment == nullptr))
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);

	auto startTime = std::chrono::steady_clock::now();

	while (true) {
		double dRemainingRealTimeInMilliseconds = 0.0;
		if (pInstance->checkFinished(dRemainingRealTimeInMilliseconds))
			return true;

		uint64_t nElapsedMilliseconds = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
		if ((nTimeOutInMilliseconds != SCANNEREMULATOR_INFINITETIMEOUT) && (nElapsedMilliseconds >= nTimeOutInMilliseconds))
			return false;

		double dSleepTime = std::ceil(dRemainingRealTimeInMilliseconds);
		if (dSleepTime < 1.0)
			dSleepTime = 1.0;
		if (dSleepTime > (double)SCANNEREMULATOR_MAXWAITSLICE)
			dSleepTime = (double)SCANNEREMULATOR_MAXWAITSLICE;
		uint32_t nSleepTime = (uint32_t)dSleepTime;

		if (nTimeOutInMilliseconds != SCANNEREMULATOR_INFINITETIMEOUT) {
			uint32_t nRemainingTimeOut = (uint32_t)(nTimeOutInMilliseconds - nElapsedMilliseconds);
			if (nSleepTime > nRemainingTimeOut)
				nSleepTime = nRemainingTimeOut;
			if (nSleepTime < 1)
				nSleepTime = 1;
		}

		pDriverEnvironment->Sleep(nSleepTime);
	}
}

std::string CScanner::GetName()
{
	return m_pInstance->getName();
}

void CScanner::GetViewport(LibMCDriver_ScannerEmulator_double & dX1, LibMCDriver_ScannerEmulator_double & dY1, LibMCDriver_ScannerEmulator_double & dX2, LibMCDriver_ScannerEmulator_double & dY2)
{
	auto viewport = m_pInstance->getViewport();
	dX1 = viewport.m_dX1;
	dY1 = viewport.m_dY1;
	dX2 = viewport.m_dX2;
	dY2 = viewport.m_dY2;
}

LibMCDriver_ScannerEmulator_double CScanner::GetUnitsInMM()
{
	return m_pInstance->getUnitsInMM();
}

void CScanner::SetLaserIndex(const LibMCDriver_ScannerEmulator_uint32 nLaserIndex)
{
	m_pInstance->setLaserIndex(nLaserIndex);
}

LibMCDriver_ScannerEmulator_uint32 CScanner::GetLaserIndex()
{
	return m_pInstance->getLaserIndex();
}

void CScanner::SetMaxLaserPower(const LibMCDriver_ScannerEmulator_double dMaxPowerInWatts)
{
	m_pInstance->setMaxPowerInWatts(dMaxPowerInWatts);
}

LibMCDriver_ScannerEmulator_double CScanner::GetMaxLaserPower()
{
	return m_pInstance->getMaxPowerInWatts();
}

void CScanner::SetFastMode(const bool bFastMode)
{
	m_pInstance->setFastMode(bFastMode);
}

bool CScanner::IsFastMode()
{
	return m_pInstance->isFastMode();
}

void CScanner::SetTimeScale(const LibMCDriver_ScannerEmulator_double dTimeScale)
{
	m_pInstance->setTimeScale(dTimeScale);
}

LibMCDriver_ScannerEmulator_double CScanner::GetTimeScale()
{
	return m_pInstance->getTimeScale();
}

void CScanner::SetOutOfViewportPolicy(const LibMCDriver_ScannerEmulator::eOutOfViewportPolicy ePolicy)
{
	m_pInstance->setOutOfViewportPolicy(ePolicy);
}

LibMCDriver_ScannerEmulator::eOutOfViewportPolicy CScanner::GetOutOfViewportPolicy()
{
	return m_pInstance->getOutOfViewportPolicy();
}

void CScanner::SetTransformationMatrix(const LibMCDriver_ScannerEmulator_double dM11, const LibMCDriver_ScannerEmulator_double dM12, const LibMCDriver_ScannerEmulator_double dM21, const LibMCDriver_ScannerEmulator_double dM22)
{
	m_pInstance->setTransformationMatrix(dM11, dM12, dM21, dM22);
}

void CScanner::SetTransformationOffset(const LibMCDriver_ScannerEmulator_double dOffsetX, const LibMCDriver_ScannerEmulator_double dOffsetY)
{
	m_pInstance->setTransformationOffset(dOffsetX, dOffsetY);
}

void CScanner::GetTransformation(LibMCDriver_ScannerEmulator_double & dM11, LibMCDriver_ScannerEmulator_double & dM12, LibMCDriver_ScannerEmulator_double & dM21, LibMCDriver_ScannerEmulator_double & dM22, LibMCDriver_ScannerEmulator_double & dOffsetX, LibMCDriver_ScannerEmulator_double & dOffsetY)
{
	auto transformation = m_pInstance->getTransformation();
	dM11 = transformation.m_dM11;
	dM12 = transformation.m_dM12;
	dM21 = transformation.m_dM21;
	dM22 = transformation.m_dM22;
	dOffsetX = transformation.m_dOffsetX;
	dOffsetY = transformation.m_dOffsetY;
}

void CScanner::ResetTransformation()
{
	m_pInstance->resetTransformation();
}

void CScanner::SetJumpSpeed(const LibMCDriver_ScannerEmulator_double dJumpSpeed)
{
	m_pInstance->setDefaultJumpSpeed(dJumpSpeed);
}

LibMCDriver_ScannerEmulator_double CScanner::GetJumpSpeed()
{
	return m_pInstance->getDefaultSettings().m_dJumpSpeed;
}

void CScanner::SetMarkSpeed(const LibMCDriver_ScannerEmulator_double dMarkSpeed)
{
	m_pInstance->setDefaultMarkSpeed(dMarkSpeed);
}

LibMCDriver_ScannerEmulator_double CScanner::GetMarkSpeed()
{
	return m_pInstance->getDefaultSettings().m_dMarkSpeed;
}

void CScanner::SetPower(const LibMCDriver_ScannerEmulator_double dPowerInPercent)
{
	m_pInstance->setDefaultPower(dPowerInPercent);
}

LibMCDriver_ScannerEmulator_double CScanner::GetPower()
{
	return m_pInstance->getDefaultSettings().m_dPower;
}

void CScanner::SetDelays(const LibMCDriver_ScannerEmulator_double dLaserOnDelay, const LibMCDriver_ScannerEmulator_double dLaserOffDelay, const LibMCDriver_ScannerEmulator_double dJumpDelay, const LibMCDriver_ScannerEmulator_double dMarkDelay, const LibMCDriver_ScannerEmulator_double dPolygonDelay)
{
	m_pInstance->setDefaultDelays({ dLaserOnDelay, dLaserOffDelay, dJumpDelay, dMarkDelay, dPolygonDelay });
}

void CScanner::GetDelays(LibMCDriver_ScannerEmulator_double & dLaserOnDelay, LibMCDriver_ScannerEmulator_double & dLaserOffDelay, LibMCDriver_ScannerEmulator_double & dJumpDelay, LibMCDriver_ScannerEmulator_double & dMarkDelay, LibMCDriver_ScannerEmulator_double & dPolygonDelay)
{
	auto delays = m_pInstance->getDefaultSettings().m_Delays;
	dLaserOnDelay = delays.m_dLaserOnDelay;
	dLaserOffDelay = delays.m_dLaserOffDelay;
	dJumpDelay = delays.m_dJumpDelay;
	dMarkDelay = delays.m_dMarkDelay;
	dPolygonDelay = delays.m_dPolygonDelay;
}

void CScanner::SetSkywriting(const LibMCDriver_ScannerEmulator::eSkywritingMode eMode, const LibMCDriver_ScannerEmulator_double dTimelag, const LibMCDriver_ScannerEmulator_double dLaserOnShift, const LibMCDriver_ScannerEmulator_double dNPrev, const LibMCDriver_ScannerEmulator_double dNPost, const LibMCDriver_ScannerEmulator_double dLimit)
{
	m_pInstance->setDefaultSkywriting({ eMode, dTimelag, dLaserOnShift, dNPrev, dNPost, dLimit });
}

void CScanner::GetSkywriting(LibMCDriver_ScannerEmulator::eSkywritingMode & eMode, LibMCDriver_ScannerEmulator_double & dTimelag, LibMCDriver_ScannerEmulator_double & dLaserOnShift, LibMCDriver_ScannerEmulator_double & dNPrev, LibMCDriver_ScannerEmulator_double & dNPost, LibMCDriver_ScannerEmulator_double & dLimit)
{
	auto skywriting = m_pInstance->getDefaultSettings().m_Skywriting;
	eMode = skywriting.m_Mode;
	dTimelag = skywriting.m_dTimelag;
	dLaserOnShift = skywriting.m_dLaserOnShift;
	dNPrev = skywriting.m_dNPrev;
	dNPost = skywriting.m_dNPost;
	dLimit = skywriting.m_dLimit;
}

void CScanner::ClearList()
{
	m_pInstance->clearList();
}

LibMCDriver_ScannerEmulator_uint32 CScanner::GetListCommandCount()
{
	return m_pInstance->getListCommandCount();
}

void CScanner::AddJumpMovement(const LibMCDriver_ScannerEmulator_double dTargetX, const LibMCDriver_ScannerEmulator_double dTargetY)
{
	CEmulatorCommandList commandList;
	commandList.addJump(dTargetX, dTargetY);
	m_pInstance->appendToList(commandList);
}

void CScanner::AddMarkMovement(const LibMCDriver_ScannerEmulator_double dTargetX, const LibMCDriver_ScannerEmulator_double dTargetY)
{
	CEmulatorCommandList commandList;
	commandList.addMark(dTargetX, dTargetY);
	m_pInstance->appendToList(commandList);
}

void CScanner::AddSetJumpSpeed(const LibMCDriver_ScannerEmulator_double dJumpSpeed)
{
	CEmulatorCommandList commandList;
	commandList.addSetJumpSpeed(dJumpSpeed);
	m_pInstance->appendToList(commandList);
}

void CScanner::AddSetMarkSpeed(const LibMCDriver_ScannerEmulator_double dMarkSpeed)
{
	CEmulatorCommandList commandList;
	commandList.addSetMarkSpeed(dMarkSpeed);
	m_pInstance->appendToList(commandList);
}

void CScanner::AddSetPower(const LibMCDriver_ScannerEmulator_double dPowerInPercent)
{
	CEmulatorCommandList commandList;
	commandList.addSetPower(dPowerInPercent);
	m_pInstance->appendToList(commandList);
}

void CScanner::AddSetDelays(const LibMCDriver_ScannerEmulator_double dLaserOnDelay, const LibMCDriver_ScannerEmulator_double dLaserOffDelay, const LibMCDriver_ScannerEmulator_double dJumpDelay, const LibMCDriver_ScannerEmulator_double dMarkDelay, const LibMCDriver_ScannerEmulator_double dPolygonDelay)
{
	CEmulatorCommandList commandList;
	commandList.addSetDelays({ dLaserOnDelay, dLaserOffDelay, dJumpDelay, dMarkDelay, dPolygonDelay });
	m_pInstance->appendToList(commandList);
}

void CScanner::AddCustomDelay(const LibMCDriver_ScannerEmulator_double dDelay)
{
	CEmulatorCommandList commandList;
	commandList.addCustomDelay(dDelay);
	m_pInstance->appendToList(commandList);
}

void CScanner::AddEnableSkywriting(const LibMCDriver_ScannerEmulator::eSkywritingMode eMode, const LibMCDriver_ScannerEmulator_double dTimelag, const LibMCDriver_ScannerEmulator_double dLaserOnShift, const LibMCDriver_ScannerEmulator_double dNPrev, const LibMCDriver_ScannerEmulator_double dNPost, const LibMCDriver_ScannerEmulator_double dLimit)
{
	CEmulatorCommandList commandList;
	commandList.addEnableSkywriting({ eMode, dTimelag, dLaserOnShift, dNPrev, dNPost, dLimit });
	m_pInstance->appendToList(commandList);
}

void CScanner::AddDisableSkywriting()
{
	CEmulatorCommandList commandList;
	commandList.addDisableSkywriting();
	m_pInstance->appendToList(commandList);
}

void CScanner::AddLayerToList(LibMCEnv::PToolpathLayer pLayer, const bool bFailIfNonAssignedDataExists)
{
	if (pLayer.get() == nullptr)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);

	CEmulatorCommandList commandList;
	buildLayerCommandList(m_pInstance.get(), pLayer.get(), bFailIfNonAssignedDataExists, commandList);
	m_pInstance->appendToList(commandList);
}

void CScanner::ExecuteList()
{
	m_pInstance->executeList();
}

bool CScanner::WaitForList(const LibMCDriver_ScannerEmulator_uint32 nTimeOutInMilliseconds)
{
	return waitForInstance(m_pInstance.get(), m_pDriverEnvironment.get(), nTimeOutInMilliseconds);
}

void CScanner::StopExecution()
{
	m_pInstance->stopExecution();
}

bool CScanner::IsBusy()
{
	return m_pInstance->isBusy();
}

void CScanner::DrawLayer(const std::string & sStreamUUID, const LibMCDriver_ScannerEmulator_uint32 nLayerIndex)
{
	auto pToolpathAccessor = m_pDriverEnvironment->CreateToolpathAccessor(sStreamUUID);
	auto pLayer = pToolpathAccessor->LoadLayer(nLayerIndex);

	CEmulatorCommandList commandList;
	buildLayerCommandList(m_pInstance.get(), pLayer.get(), false, commandList);

	m_pInstance->clearList();
	if (commandList.getCommandCount() == 0)
		return;

	m_pInstance->appendToList(commandList);
	m_pInstance->executeList();
	waitForInstance(m_pInstance.get(), m_pDriverEnvironment.get(), SCANNEREMULATOR_INFINITETIMEOUT);
}

void CScanner::GetStatus(LibMCDriver_ScannerEmulator::eScannerState & eState, LibMCDriver_ScannerEmulator_uint32 & nCommandIndex, LibMCDriver_ScannerEmulator_double & dProgress)
{
	auto status = m_pInstance->getStatus();
	eState = status.m_State;
	nCommandIndex = status.m_nCommandIndex;
	dProgress = status.m_dProgress;
}

void CScanner::GetPosition(LibMCDriver_ScannerEmulator_double & dX, LibMCDriver_ScannerEmulator_double & dY)
{
	auto status = m_pInstance->getStatus();
	dX = status.m_dPositionX;
	dY = status.m_dPositionY;
}

bool CScanner::IsLaserOn()
{
	return m_pInstance->getStatus().m_bLaserOn;
}

void CScanner::GetHeadStatus(bool & bPositionOK, bool & bTemperatureOK, bool & bPowerOK)
{
	auto status = m_pInstance->getStatus();
	bPositionOK = status.m_bPositionOK;
	bTemperatureOK = status.m_bTemperatureOK;
	bPowerOK = status.m_bPowerOK;
}

IExecutionStatistics * CScanner::GetLastExecutionStatistics()
{
	return new CExecutionStatistics(m_pInstance->getLastExecutionStatistics());
}

void CScanner::EnableRecording(const LibMCDriver_ScannerEmulator_double dSampleIntervalInMicroseconds)
{
	m_pInstance->enableRecording(dSampleIntervalInMicroseconds);
}

void CScanner::DisableRecording()
{
	m_pInstance->disableRecording();
}

bool CScanner::HasRecording()
{
	return m_pInstance->hasRecording();
}

void CScanner::WriteRecordingToDataTable(LibMCEnv::PDataTable pDataTable)
{
	if (pDataTable.get() == nullptr)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);

	sRecordingData recordingData;
	m_pInstance->getRecording(recordingData);

	auto replaceColumn = [&pDataTable](const std::string& sIdentifier, const std::string& sDescription, LibMCEnv::eDataTableColumnType columnType) {
		if (pDataTable->HasColumn(sIdentifier))
			pDataTable->RemoveColumn(sIdentifier);
		pDataTable->AddColumn(sIdentifier, sDescription, columnType);
	};

	replaceColumn("time_us", "Emulated time in microseconds", LibMCEnv::eDataTableColumnType::DoubleColumn);
	replaceColumn("x", "Position X in scanner units", LibMCEnv::eDataTableColumnType::DoubleColumn);
	replaceColumn("y", "Position Y in scanner units", LibMCEnv::eDataTableColumnType::DoubleColumn);
	replaceColumn("laser_on", "Laser signal", LibMCEnv::eDataTableColumnType::Uint32Column);
	replaceColumn("power", "Laser power in percent", LibMCEnv::eDataTableColumnType::DoubleColumn);
	replaceColumn("command_index", "Command index", LibMCEnv::eDataTableColumnType::Uint32Column);

	pDataTable->SetDoubleColumnValues("time_us", recordingData.m_TimesInMicroseconds);
	pDataTable->SetDoubleColumnValues("x", recordingData.m_PositionsX);
	pDataTable->SetDoubleColumnValues("y", recordingData.m_PositionsY);
	pDataTable->SetUint32ColumnValues("laser_on", recordingData.m_LaserOn);
	pDataTable->SetDoubleColumnValues("power", recordingData.m_Powers);
	pDataTable->SetUint32ColumnValues("command_index", recordingData.m_CommandIndices);
}

void CScanner::SetFaultSeed(const LibMCDriver_ScannerEmulator_uint64 nSeed)
{
	m_pInstance->setFaultSeed(nSeed);
}

void CScanner::InjectFault(const LibMCDriver_ScannerEmulator::eFaultType eFaultType, const LibMCDriver_ScannerEmulator::eFaultPersistence ePersistence)
{
	m_pInstance->injectFault(eFaultType, ePersistence);
}

void CScanner::ScheduleFaultAtCommand(const LibMCDriver_ScannerEmulator::eFaultType eFaultType, const LibMCDriver_ScannerEmulator::eFaultPersistence ePersistence, const LibMCDriver_ScannerEmulator_uint32 nCommandIndex)
{
	sFaultTrigger trigger = {};
	trigger.m_FaultType = eFaultType;
	trigger.m_Persistence = ePersistence;
	trigger.m_TriggerType = eFaultTriggerType::AtCommand;
	trigger.m_nCommandIndex = nCommandIndex;
	m_pInstance->addFaultTrigger(trigger);
}

void CScanner::ScheduleFaultAtTime(const LibMCDriver_ScannerEmulator::eFaultType eFaultType, const LibMCDriver_ScannerEmulator::eFaultPersistence ePersistence, const LibMCDriver_ScannerEmulator_double dTimeInMicroseconds)
{
	sFaultTrigger trigger = {};
	trigger.m_FaultType = eFaultType;
	trigger.m_Persistence = ePersistence;
	trigger.m_TriggerType = eFaultTriggerType::AtTime;
	trigger.m_dTimeInMicroseconds = dTimeInMicroseconds;
	m_pInstance->addFaultTrigger(trigger);
}

void CScanner::ScheduleFaultAtExecution(const LibMCDriver_ScannerEmulator::eFaultType eFaultType, const LibMCDriver_ScannerEmulator::eFaultPersistence ePersistence, const LibMCDriver_ScannerEmulator_uint32 nExecutionCount)
{
	sFaultTrigger trigger = {};
	trigger.m_FaultType = eFaultType;
	trigger.m_Persistence = ePersistence;
	trigger.m_TriggerType = eFaultTriggerType::AtExecution;
	trigger.m_nExecutionCountdown = nExecutionCount;
	m_pInstance->addFaultTrigger(trigger);
}

void CScanner::SetRandomFault(const LibMCDriver_ScannerEmulator::eFaultType eFaultType, const LibMCDriver_ScannerEmulator::eFaultPersistence ePersistence, const LibMCDriver_ScannerEmulator_double dProbability)
{
	m_pInstance->setRandomFault(eFaultType, ePersistence, dProbability);
}

void CScanner::SetSlowdownFactor(const LibMCDriver_ScannerEmulator_double dFactor)
{
	m_pInstance->setSlowdownFactor(dFactor);
}

LibMCDriver_ScannerEmulator_double CScanner::GetSlowdownFactor()
{
	return m_pInstance->getSlowdownFactor();
}

void CScanner::ClearFaultInjection()
{
	m_pInstance->clearFaultInjection();
}

LibMCDriver_ScannerEmulator::eFaultType CScanner::GetActiveFault()
{
	return m_pInstance->getActiveFault();
}

void CScanner::ResetFault()
{
	m_pInstance->resetFault();
}

LibMCDriver_ScannerEmulator_uint32 CScanner::GetFaultCount()
{
	return m_pInstance->getFaultCount();
}

