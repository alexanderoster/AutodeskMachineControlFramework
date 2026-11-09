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


Abstract: The state of an emulated scanner.

*/

#include "libmcdriver_scanneremulator_scannerinstance.hpp"
#include "libmcdriver_scanneremulator_interfaceexception.hpp"

#include <cmath>
#include <algorithm>

#define SCANNEREMULATOR_MAXTIMESCALE 1.0E6
#define SCANNEREMULATOR_MAXSLOWDOWNFACTOR 1.0E6
#define SCANNEREMULATOR_DEFAULTSLOWDOWNFACTOR 2.0
#define SCANNEREMULATOR_DEFAULTMAXPOWER 100.0
#define SCANNEREMULATOR_MAXRECORDINGSAMPLES 10000000
#define SCANNEREMULATOR_MAXNAMELENGTH 64

namespace LibMCDriver_ScannerEmulator {
namespace Impl {

	CScannerInstance::CScannerInstance(const std::string& sName, const sEmulatorViewport& viewport, double dUnitsInMM, bool bFromConfiguration)
		: m_TimeOrigin(std::chrono::steady_clock::now()),
		m_sName(sName),
		m_Viewport(viewport),
		m_dUnitsInMM(dUnitsInMM),
		m_bFromConfiguration(bFromConfiguration),
		m_nLaserIndex(0),
		m_dMaxPowerInWatts(SCANNEREMULATOR_DEFAULTMAXPOWER),
		m_bFastMode(false),
		m_dTimeScale(1.0),
		m_OutOfViewportPolicy(eOutOfViewportPolicy::Fail),
		m_Transformation(emulatorIdentityTransformation()),
		m_DefaultSettings(emulatorDefaultSettings()),
		m_dPositionX(0.0),
		m_dPositionY(0.0),
		m_nExecutionCount(0),
		m_dLastDurationInMilliseconds(0.0),
		m_dSlowdownFactor(SCANNEREMULATOR_DEFAULTSLOWDOWNFACTOR),
		m_ActiveFault(eFaultType::None),
		m_UnreportedFault(eFaultType::None),
		m_nFaultCount(0),
		m_bRecordingEnabled(false),
		m_dRecordingInterval(0.0)
	{
		validateName(sName);
		emulatorValidateViewport(viewport);
		if (!std::isfinite(dUnitsInMM) || (dUnitsInMM <= 0.0))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDUNITS, "invalid units of scanner " + sName + ": " + std::to_string(dUnitsInMM));

		m_dPositionX = std::min(std::max(0.0, viewport.m_dX1), viewport.m_dX2);
		m_dPositionY = std::min(std::max(0.0, viewport.m_dY1), viewport.m_dY2);
	}

	void CScannerInstance::validateName(const std::string& sName)
	{
		if (sName.empty() || (sName.length() > SCANNEREMULATOR_MAXNAMELENGTH))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSCANNERNAME, "invalid scanner name: " + sName);

		for (char cChar : sName) {
			bool bIsValid = ((cChar >= 'a') && (cChar <= 'z')) || ((cChar >= 'A') && (cChar <= 'Z')) || ((cChar >= '0') && (cChar <= '9')) || (cChar == '_');
			if (!bIsValid)
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSCANNERNAME, "invalid scanner name: " + sName);
		}
	}

	double CScannerInstance::getRealTimeInMicroseconds()
	{
		auto duration = std::chrono::steady_clock::now() - m_TimeOrigin;
		return (double)std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
	}

	double CScannerInstance::getEmulatedTime(const sEmulatorExecution& execution, double dRealTime)
	{
		if (execution.m_bFinalized || execution.m_bFastMode)
			return execution.m_dEndTime;

		double dElapsed = std::max(0.0, dRealTime - execution.m_dRealStartTime) * execution.m_dRate;
		return std::min(dElapsed, execution.m_dEndTime);
	}

	bool CScannerInstance::isBusyLocked()
	{
		return (m_pExecution.get() != nullptr) && (!m_pExecution->m_bFinalized);
	}

	void CScannerInstance::updateExecutionLocked(double dRealTime)
	{
		if (!isBusyLocked())
			return;

		if (getEmulatedTime(*m_pExecution, dRealTime) >= m_pExecution->m_dEndTime)
			finalizeExecutionLocked();
	}

	void CScannerInstance::finalizeExecutionLocked()
	{
		auto& execution = *m_pExecution;
		execution.m_pTimeline->getPositionAt(execution.m_dEndTime, m_dPositionX, m_dPositionY);
		m_dLastDurationInMilliseconds = execution.m_dEndTime / 1000.0;

		if (execution.m_bHasRunFault) {
			execution.m_Result = eExecutionResult::Faulted;
			m_nFaultCount++;
			if (execution.m_RunPersistence == eFaultPersistence::Latched)
				latchFaultLocked(execution.m_RunFault);
			m_UnreportedFault = execution.m_RunFault;
		}
		else if (execution.m_bStopped) {
			execution.m_Result = eExecutionResult::Stopped;
		}
		else {
			execution.m_Result = eExecutionResult::Completed;
		}

		execution.m_bFinalized = true;

		for (auto& pWeakImage : m_Images) {
			auto pImage = pWeakImage.lock();
			if (pImage.get() == nullptr)
				continue;
			try {
				pImage->drawExecution(m_sName, *execution.m_pTimeline, execution.m_dEndTime, m_dUnitsInMM);
			}
			catch (...) {
				// Debug images must never change the outcome of an execution.
			}
		}
	}

	void CScannerInstance::ensureIdleLocked()
	{
		updateExecutionLocked(getRealTimeInMicroseconds());
		if (isBusyLocked())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERBUSY, "scanner is busy: " + m_sName);
	}

	void CScannerInstance::latchFaultLocked(eFaultType faultType)
	{
		if (faultType == eFaultType::Slowdown) {
			if (m_ActiveFault == eFaultType::None)
				m_ActiveFault = eFaultType::Slowdown;
		}
		else {
			m_ActiveFault = faultType;
		}
	}

	void CScannerInstance::reportUnreportedFaultLocked()
	{
		if (m_UnreportedFault == eFaultType::None)
			return;

		eFaultType faultType = m_UnreportedFault;
		m_UnreportedFault = eFaultType::None;
		CEmulatorFaultInjector::throwFaultError(faultType, m_sName);
	}

	void CScannerInstance::checkActiveFaultForExecutionLocked(bool bListHasMarks)
	{
		switch (m_ActiveFault) {
		case eFaultType::None:
		case eFaultType::Slowdown:
			return;
		case eFaultType::LaserNotReady:
			if (bListHasMarks)
				CEmulatorFaultInjector::throwFaultError(m_ActiveFault, m_sName);
			return;
		default:
			CEmulatorFaultInjector::throwFaultError(m_ActiveFault, m_sName);
		}
	}

	std::string CScannerInstance::getName()
	{
		return m_sName;
	}

	sEmulatorViewport CScannerInstance::getViewport()
	{
		return m_Viewport;
	}

	double CScannerInstance::getUnitsInMM()
	{
		return m_dUnitsInMM;
	}

	bool CScannerInstance::isFromConfiguration()
	{
		return m_bFromConfiguration;
	}

	void CScannerInstance::setLaserIndex(uint32_t nLaserIndex)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		ensureIdleLocked();
		m_nLaserIndex = nLaserIndex;
	}

	uint32_t CScannerInstance::getLaserIndex()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_nLaserIndex;
	}

	void CScannerInstance::setMaxPowerInWatts(double dMaxPowerInWatts)
	{
		if (!std::isfinite(dMaxPowerInWatts) || (dMaxPowerInWatts <= 0.0))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPOWER, "invalid maximum laser power: " + std::to_string(dMaxPowerInWatts));

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_dMaxPowerInWatts = dMaxPowerInWatts;
	}

	double CScannerInstance::getMaxPowerInWatts()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_dMaxPowerInWatts;
	}

	void CScannerInstance::setFastMode(bool bFastMode)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		ensureIdleLocked();
		m_bFastMode = bFastMode;
	}

	bool CScannerInstance::isFastMode()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_bFastMode;
	}

	void CScannerInstance::setTimeScale(double dTimeScale)
	{
		if (!std::isfinite(dTimeScale) || (dTimeScale <= 0.0) || (dTimeScale > SCANNEREMULATOR_MAXTIMESCALE))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDTIMESCALE, "invalid time scale: " + std::to_string(dTimeScale));

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		ensureIdleLocked();
		m_dTimeScale = dTimeScale;
	}

	double CScannerInstance::getTimeScale()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_dTimeScale;
	}

	void CScannerInstance::setOutOfViewportPolicy(eOutOfViewportPolicy policy)
	{
		switch (policy) {
		case eOutOfViewportPolicy::Fail:
		case eOutOfViewportPolicy::Clamp:
		case eOutOfViewportPolicy::Warn:
			break;
		default:
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM, "invalid out of viewport policy: " + std::to_string((int)policy));
		}

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_OutOfViewportPolicy = policy;
	}

	eOutOfViewportPolicy CScannerInstance::getOutOfViewportPolicy()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_OutOfViewportPolicy;
	}

	void CScannerInstance::setTransformationMatrix(double dM11, double dM12, double dM21, double dM22)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		sEmulatorTransformation transformation = m_Transformation;
		transformation.m_dM11 = dM11;
		transformation.m_dM12 = dM12;
		transformation.m_dM21 = dM21;
		transformation.m_dM22 = dM22;
		emulatorValidateTransformation(transformation);
		m_Transformation = transformation;
	}

	void CScannerInstance::setTransformationOffset(double dOffsetX, double dOffsetY)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		sEmulatorTransformation transformation = m_Transformation;
		transformation.m_dOffsetX = dOffsetX;
		transformation.m_dOffsetY = dOffsetY;
		emulatorValidateTransformation(transformation);
		m_Transformation = transformation;
	}

	sEmulatorTransformation CScannerInstance::getTransformation()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_Transformation;
	}

	void CScannerInstance::resetTransformation()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_Transformation = emulatorIdentityTransformation();
	}

	void CScannerInstance::setDefaultJumpSpeed(double dJumpSpeed)
	{
		emulatorValidateSpeed(dJumpSpeed);
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_DefaultSettings.m_dJumpSpeed = dJumpSpeed;
	}

	void CScannerInstance::setDefaultMarkSpeed(double dMarkSpeed)
	{
		emulatorValidateSpeed(dMarkSpeed);
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_DefaultSettings.m_dMarkSpeed = dMarkSpeed;
	}

	void CScannerInstance::setDefaultPower(double dPowerInPercent)
	{
		emulatorValidatePower(dPowerInPercent);
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_DefaultSettings.m_dPower = dPowerInPercent;
	}

	void CScannerInstance::setDefaultDelays(const sEmulatorDelays& delays)
	{
		emulatorValidateDelays(delays);
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_DefaultSettings.m_Delays = delays;
	}

	void CScannerInstance::setDefaultSkywriting(const sEmulatorSkywriting& skywriting)
	{
		emulatorValidateSkywriting(skywriting);
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_DefaultSettings.m_Skywriting = skywriting;
	}

	sEmulatorSettings CScannerInstance::getDefaultSettings()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_DefaultSettings;
	}

	void CScannerInstance::clearList()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		ensureIdleLocked();
		m_OpenList.clear();
	}

	uint32_t CScannerInstance::getListCommandCount()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_OpenList.getCommandCount();
	}

	void CScannerInstance::appendToList(const CEmulatorCommandList& commandList)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_OpenList.append(commandList);
	}

	void CScannerInstance::executeList()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		ensureIdleLocked();
		reportUnreportedFaultLocked();

		bool bListHasMarks = m_OpenList.hasMarks();
		checkActiveFaultForExecutionLocked(bListHasMarks);

		if (m_OpenList.getCommandCount() == 0)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_EMPTYLIST, "list of scanner " + m_sName + " is empty");

		auto pTimeline = CEmulatorTimingModel::computeTimeline(m_OpenList, m_DefaultSettings, m_Viewport, m_Transformation, m_OutOfViewportPolicy, m_dUnitsInMM, m_dPositionX, m_dPositionY);

		auto plan = m_FaultInjector.planExecution(*pTimeline, bListHasMarks);

		if (plan.m_bSlowdown) {
			m_nFaultCount++;
			if (plan.m_SlowdownPersistence == eFaultPersistence::Latched)
				latchFaultLocked(eFaultType::Slowdown);
		}

		if (plan.m_bHasStartFault) {
			m_nFaultCount++;
			if (plan.m_StartPersistence == eFaultPersistence::Latched)
				latchFaultLocked(plan.m_StartFault);
			CEmulatorFaultInjector::throwFaultError(plan.m_StartFault, m_sName);
		}

		bool bSlowdown = plan.m_bSlowdown || (m_ActiveFault == eFaultType::Slowdown);

		std::unique_ptr<sEmulatorExecution> pExecution(new sEmulatorExecution());
		pExecution->m_pTimeline = pTimeline;
		pExecution->m_dRealStartTime = getRealTimeInMicroseconds();
		pExecution->m_dRate = bSlowdown ? (m_dTimeScale / m_dSlowdownFactor) : m_dTimeScale;
		pExecution->m_bFastMode = m_bFastMode;
		pExecution->m_dEndTime = pTimeline->getDuration();
		pExecution->m_bHasRunFault = plan.m_bHasRunFault;
		pExecution->m_RunFault = plan.m_RunFault;
		pExecution->m_RunPersistence = plan.m_RunPersistence;
		if (plan.m_bHasRunFault)
			pExecution->m_dEndTime = std::min(plan.m_dRunFaultTime, pExecution->m_dEndTime);
		pExecution->m_bStopped = false;
		pExecution->m_bFinalized = false;
		pExecution->m_Result = eExecutionResult::Running;
		pExecution->m_bRecorded = m_bRecordingEnabled;
		pExecution->m_dRecordingInterval = m_dRecordingInterval;

		m_pExecution = std::move(pExecution);
		m_OpenList.clear();
		m_nExecutionCount++;

		updateExecutionLocked(m_pExecution->m_dRealStartTime);
	}

	bool CScannerInstance::checkFinished(double& dRemainingRealTimeInMilliseconds)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		double dRealTime = getRealTimeInMicroseconds();
		updateExecutionLocked(dRealTime);

		if (isBusyLocked()) {
			double dEmulatedTime = getEmulatedTime(*m_pExecution, dRealTime);
			dRemainingRealTimeInMilliseconds = (m_pExecution->m_dEndTime - dEmulatedTime) / m_pExecution->m_dRate / 1000.0;
			return false;
		}

		dRemainingRealTimeInMilliseconds = 0.0;
		reportUnreportedFaultLocked();
		return true;
	}

	void CScannerInstance::stopExecution()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		double dRealTime = getRealTimeInMicroseconds();
		updateExecutionLocked(dRealTime);

		if (!isBusyLocked())
			return;

		m_pExecution->m_dEndTime = getEmulatedTime(*m_pExecution, dRealTime);
		m_pExecution->m_bHasRunFault = false;
		m_pExecution->m_bStopped = true;
		finalizeExecutionLocked();
	}

	bool CScannerInstance::isBusy()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		updateExecutionLocked(getRealTimeInMicroseconds());
		return isBusyLocked();
	}

	sScannerStatusData CScannerInstance::getStatus()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		double dRealTime = getRealTimeInMicroseconds();
		updateExecutionLocked(dRealTime);

		sScannerStatusData status = {};
		status.m_bBusy = isBusyLocked();
		status.m_ActiveFault = m_ActiveFault;
		status.m_nFaultCount = m_nFaultCount;
		status.m_nExecutionCount = m_nExecutionCount;
		status.m_dLastDurationInMilliseconds = m_dLastDurationInMilliseconds;
		status.m_bPositionOK = (m_ActiveFault != eFaultType::PositionError);
		status.m_bTemperatureOK = (m_ActiveFault != eFaultType::TemperatureError);
		status.m_bPowerOK = (m_ActiveFault != eFaultType::PowerError);

		if (status.m_bBusy) {
			status.m_State = eScannerState::Busy;
		}
		else {
			switch (m_ActiveFault) {
			case eFaultType::None:
			case eFaultType::Slowdown:
				status.m_State = eScannerState::Idle;
				break;
			case eFaultType::ConnectionLost:
				status.m_State = eScannerState::Disconnected;
				break;
			default:
				status.m_State = eScannerState::Error;
				break;
			}
		}

		if (m_pExecution.get() != nullptr) {
			auto pTimeline = m_pExecution->m_pTimeline;
			double dEmulatedTime = getEmulatedTime(*m_pExecution, dRealTime);
			double dDuration = pTimeline->getDuration();

			status.m_nCommandIndex = pTimeline->getCommandIndexAt(dEmulatedTime);
			status.m_dProgress = (dDuration > 0.0) ? std::min(1.0, dEmulatedTime / dDuration) : 1.0;

			if (status.m_bBusy) {
				pTimeline->getPositionAt(dEmulatedTime, status.m_dPositionX, status.m_dPositionY);
				status.m_bLaserOn = pTimeline->isLaserOnAt(dEmulatedTime);
			}
			else {
				status.m_dPositionX = m_dPositionX;
				status.m_dPositionY = m_dPositionY;
				status.m_bLaserOn = false;
			}
		}
		else {
			status.m_nCommandIndex = 0;
			status.m_dProgress = 0.0;
			status.m_dPositionX = m_dPositionX;
			status.m_dPositionY = m_dPositionY;
			status.m_bLaserOn = false;
		}

		return status;
	}

	sExecutionStatisticsData CScannerInstance::getLastExecutionStatistics()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		double dRealTime = getRealTimeInMicroseconds();
		updateExecutionLocked(dRealTime);

		if (m_pExecution.get() == nullptr)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_NOEXECUTIONAVAILABLE, "scanner has not executed a list yet: " + m_sName);

		auto& execution = *m_pExecution;
		double dEmulatedTime = getEmulatedTime(execution, dRealTime);

		sExecutionStatisticsData data = {};
		data.m_Result = execution.m_bFinalized ? execution.m_Result : eExecutionResult::Running;
		data.m_Fault = (data.m_Result == eExecutionResult::Faulted) ? execution.m_RunFault : eFaultType::None;
		data.m_nCommandIndex = execution.m_pTimeline->getCommandIndexAt(dEmulatedTime);
		data.m_dEndTimeInMicroseconds = dEmulatedTime;
		data.m_dEmulatedDurationInMicroseconds = execution.m_pTimeline->getDuration();
		data.m_dRealtimeDurationInMicroseconds = data.m_dEmulatedDurationInMicroseconds / execution.m_dRate;
		data.m_Statistics = execution.m_pTimeline->getStatistics();

		return data;
	}

	void CScannerInstance::enableRecording(double dSampleIntervalInMicroseconds)
	{
		if (!std::isfinite(dSampleIntervalInMicroseconds) || (dSampleIntervalInMicroseconds < 0.0))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSAMPLEINTERVAL, "invalid sample interval: " + std::to_string(dSampleIntervalInMicroseconds));

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_bRecordingEnabled = true;
		m_dRecordingInterval = dSampleIntervalInMicroseconds;
	}

	void CScannerInstance::disableRecording()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_bRecordingEnabled = false;
	}

	bool CScannerInstance::hasRecording()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		updateExecutionLocked(getRealTimeInMicroseconds());
		return (m_pExecution.get() != nullptr) && m_pExecution->m_bFinalized && m_pExecution->m_bRecorded;
	}

	void CScannerInstance::getRecording(sRecordingData& recordingData)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		ensureIdleLocked();

		if ((m_pExecution.get() == nullptr) || !m_pExecution->m_bRecorded)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_NORECORDINGAVAILABLE, "no recording available for scanner " + m_sName);

		auto pTimeline = m_pExecution->m_pTimeline;
		double dEndTime = m_pExecution->m_dEndTime;
		double dInterval = m_pExecution->m_dRecordingInterval;

		std::vector<double> sampleTimes;
		if (dInterval > 0.0) {
			double dSampleCount = std::floor(dEndTime / dInterval) + 2.0;
			if (dSampleCount > (double)SCANNEREMULATOR_MAXRECORDINGSAMPLES)
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_RECORDINGTOOLARGE, "recording of scanner " + m_sName + " would have " + std::to_string((uint64_t)dSampleCount) + " samples");

			uint64_t nSampleCount = (uint64_t)std::floor(dEndTime / dInterval) + 1;
			sampleTimes.reserve(nSampleCount + 1);
			for (uint64_t nIndex = 0; nIndex < nSampleCount; nIndex++)
				sampleTimes.push_back((double)nIndex * dInterval);
			if (sampleTimes.back() < dEndTime)
				sampleTimes.push_back(dEndTime);
		}
		else {
			auto& segments = pTimeline->getSegments();
			auto& laserIntervals = pTimeline->getLaserIntervals();
			if ((segments.size() + 2 * laserIntervals.size() + 2) > SCANNEREMULATOR_MAXRECORDINGSAMPLES)
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_RECORDINGTOOLARGE, "recording of scanner " + m_sName + " has too many events");

			sampleTimes.reserve(segments.size() + 2 * laserIntervals.size() + 2);
			sampleTimes.push_back(0.0);
			for (auto& segment : segments) {
				if (segment.m_dStartTime < dEndTime)
					sampleTimes.push_back(segment.m_dStartTime);
			}
			for (auto& interval : laserIntervals) {
				if (interval.m_dOnTime < dEndTime)
					sampleTimes.push_back(interval.m_dOnTime);
				if (interval.m_dOffTime < dEndTime)
					sampleTimes.push_back(interval.m_dOffTime);
			}
			sampleTimes.push_back(dEndTime);

			std::sort(sampleTimes.begin(), sampleTimes.end());
			sampleTimes.erase(std::unique(sampleTimes.begin(), sampleTimes.end()), sampleTimes.end());
		}

		size_t nRowCount = sampleTimes.size();
		recordingData.m_TimesInMicroseconds = sampleTimes;
		recordingData.m_PositionsX.resize(nRowCount);
		recordingData.m_PositionsY.resize(nRowCount);
		recordingData.m_LaserOn.resize(nRowCount);
		recordingData.m_Powers.resize(nRowCount);
		recordingData.m_CommandIndices.resize(nRowCount);

		for (size_t nRow = 0; nRow < nRowCount; nRow++) {
			double dTime = sampleTimes[nRow];
			pTimeline->getPositionAt(dTime, recordingData.m_PositionsX[nRow], recordingData.m_PositionsY[nRow]);
			recordingData.m_LaserOn[nRow] = ((dTime < dEndTime) && pTimeline->isLaserOnAt(dTime)) ? 1 : 0;
			recordingData.m_Powers[nRow] = pTimeline->getPowerAt(dTime);
			recordingData.m_CommandIndices[nRow] = pTimeline->getCommandIndexAt(dTime);
		}
	}

	void CScannerInstance::attachImage(PScannerImageInstance pImage)
	{
		if (pImage.get() == nullptr)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_Images.erase(std::remove_if(m_Images.begin(), m_Images.end(),
			[](const std::weak_ptr<CScannerImageInstance>& pWeakImage) { return pWeakImage.expired(); }), m_Images.end());

		for (auto& pWeakImage : m_Images) {
			if (pWeakImage.lock() == pImage)
				return;
		}
		m_Images.push_back(pImage);
	}

	void CScannerInstance::setFaultSeed(uint64_t nSeed)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_FaultInjector.setSeed(nSeed);
	}

	void CScannerInstance::injectFault(eFaultType faultType, eFaultPersistence persistence)
	{
		CEmulatorFaultInjector::validateFaultType(faultType);
		CEmulatorFaultInjector::validatePersistence(persistence);

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		double dRealTime = getRealTimeInMicroseconds();
		updateExecutionLocked(dRealTime);

		if (!CEmulatorFaultInjector::isRunFault(faultType)) {
			if (persistence == eFaultPersistence::Latched) {
				m_nFaultCount++;
				latchFaultLocked(faultType);
			}
			else {
				sFaultTrigger trigger = {};
				trigger.m_FaultType = faultType;
				trigger.m_Persistence = persistence;
				trigger.m_TriggerType = eFaultTriggerType::Immediate;
				m_FaultInjector.addTrigger(trigger);
			}
			return;
		}

		if (isBusyLocked()) {
			m_pExecution->m_dEndTime = getEmulatedTime(*m_pExecution, dRealTime);
			m_pExecution->m_bHasRunFault = true;
			m_pExecution->m_RunFault = faultType;
			m_pExecution->m_RunPersistence = persistence;
			finalizeExecutionLocked();
		}
		else {
			m_nFaultCount++;
			if (persistence == eFaultPersistence::Latched)
				latchFaultLocked(faultType);
			m_UnreportedFault = faultType;
		}
	}

	void CScannerInstance::addFaultTrigger(const sFaultTrigger& trigger)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_FaultInjector.addTrigger(trigger);
	}

	void CScannerInstance::setRandomFault(eFaultType faultType, eFaultPersistence persistence, double dProbability)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_FaultInjector.setRandomFault(faultType, persistence, dProbability);
	}

	void CScannerInstance::setSlowdownFactor(double dFactor)
	{
		if (!std::isfinite(dFactor) || (dFactor < 1.0) || (dFactor > SCANNEREMULATOR_MAXSLOWDOWNFACTOR))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSLOWDOWNFACTOR, "invalid slowdown factor: " + std::to_string(dFactor));

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_dSlowdownFactor = dFactor;
	}

	double CScannerInstance::getSlowdownFactor()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_dSlowdownFactor;
	}

	void CScannerInstance::clearFaultInjection()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_FaultInjector.clear();
	}

	eFaultType CScannerInstance::getActiveFault()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		updateExecutionLocked(getRealTimeInMicroseconds());
		return m_ActiveFault;
	}

	void CScannerInstance::resetFault()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		ensureIdleLocked();
		m_ActiveFault = eFaultType::None;
		m_UnreportedFault = eFaultType::None;
	}

	uint32_t CScannerInstance::getFaultCount()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		updateExecutionLocked(getRealTimeInMicroseconds());
		return m_nFaultCount;
	}

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator
