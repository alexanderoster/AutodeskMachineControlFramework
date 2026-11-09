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


Abstract: The state of an emulated scanner. All methods are thread safe.
This file does not depend on LibMCEnv, so that the engine can be reused by other drivers.

*/


#ifndef __LIBMCDRIVER_SCANNEREMULATOR_SCANNERINSTANCE
#define __LIBMCDRIVER_SCANNEREMULATOR_SCANNERINSTANCE

#include "libmcdriver_scanneremulator_timeline.hpp"
#include "libmcdriver_scanneremulator_faultinjector.hpp"
#include "libmcdriver_scanneremulator_imageinstance.hpp"

#include <mutex>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace LibMCDriver_ScannerEmulator {
namespace Impl {

	struct sExecutionStatisticsData {
		eExecutionResult m_Result;
		eFaultType m_Fault;
		uint32_t m_nCommandIndex;
		double m_dEndTimeInMicroseconds;
		double m_dEmulatedDurationInMicroseconds;
		double m_dRealtimeDurationInMicroseconds;
		sTimelineStatistics m_Statistics;
	};

	struct sScannerStatusData {
		eScannerState m_State;
		bool m_bBusy;
		uint32_t m_nCommandIndex;
		double m_dProgress;
		double m_dPositionX;
		double m_dPositionY;
		bool m_bLaserOn;
		bool m_bPositionOK;
		bool m_bTemperatureOK;
		bool m_bPowerOK;
		eFaultType m_ActiveFault;
		uint32_t m_nFaultCount;
		uint32_t m_nExecutionCount;
		double m_dLastDurationInMilliseconds;
	};

	struct sRecordingData {
		std::vector<double> m_TimesInMicroseconds;
		std::vector<double> m_PositionsX;
		std::vector<double> m_PositionsY;
		std::vector<uint32_t> m_LaserOn;
		std::vector<double> m_Powers;
		std::vector<uint32_t> m_CommandIndices;
	};

	struct sEmulatorExecution {
		PEmulatorTimeline m_pTimeline;
		double m_dRealStartTime;
		double m_dRate;
		bool m_bFastMode;
		double m_dEndTime;
		bool m_bHasRunFault;
		eFaultType m_RunFault;
		eFaultPersistence m_RunPersistence;
		bool m_bStopped;
		bool m_bFinalized;
		eExecutionResult m_Result;
		bool m_bRecorded;
		double m_dRecordingInterval;
	};

	class CScannerInstance {
	private:
		std::mutex m_Mutex;
		std::chrono::steady_clock::time_point m_TimeOrigin;

		std::string m_sName;
		sEmulatorViewport m_Viewport;
		double m_dUnitsInMM;
		bool m_bFromConfiguration;

		uint32_t m_nLaserIndex;
		double m_dMaxPowerInWatts;
		bool m_bFastMode;
		double m_dTimeScale;
		eOutOfViewportPolicy m_OutOfViewportPolicy;
		sEmulatorTransformation m_Transformation;
		sEmulatorSettings m_DefaultSettings;

		CEmulatorCommandList m_OpenList;

		double m_dPositionX;
		double m_dPositionY;

		std::unique_ptr<sEmulatorExecution> m_pExecution;
		uint32_t m_nExecutionCount;
		double m_dLastDurationInMilliseconds;

		CEmulatorFaultInjector m_FaultInjector;
		double m_dSlowdownFactor;
		eFaultType m_ActiveFault;
		eFaultType m_UnreportedFault;
		uint32_t m_nFaultCount;

		bool m_bRecordingEnabled;
		double m_dRecordingInterval;

		// Lock order: scanner mutex before image mutex.
		std::vector<std::weak_ptr<CScannerImageInstance>> m_Images;

		double getRealTimeInMicroseconds();
		double getEmulatedTime(const sEmulatorExecution& execution, double dRealTime);
		bool isBusyLocked();
		void updateExecutionLocked(double dRealTime);
		void finalizeExecutionLocked();
		void ensureIdleLocked();
		void latchFaultLocked(eFaultType faultType);
		void reportUnreportedFaultLocked();
		void checkActiveFaultForExecutionLocked(bool bListHasMarks);

	public:

		CScannerInstance(const std::string& sName, const sEmulatorViewport& viewport, double dUnitsInMM, bool bFromConfiguration);

		static void validateName(const std::string& sName);

		std::string getName();
		sEmulatorViewport getViewport();
		double getUnitsInMM();
		bool isFromConfiguration();

		void setLaserIndex(uint32_t nLaserIndex);
		uint32_t getLaserIndex();

		void setMaxPowerInWatts(double dMaxPowerInWatts);
		double getMaxPowerInWatts();

		void setFastMode(bool bFastMode);
		bool isFastMode();

		void setTimeScale(double dTimeScale);
		double getTimeScale();

		void setOutOfViewportPolicy(eOutOfViewportPolicy policy);
		eOutOfViewportPolicy getOutOfViewportPolicy();

		void setTransformationMatrix(double dM11, double dM12, double dM21, double dM22);
		void setTransformationOffset(double dOffsetX, double dOffsetY);
		sEmulatorTransformation getTransformation();
		void resetTransformation();

		void setDefaultJumpSpeed(double dJumpSpeed);
		void setDefaultMarkSpeed(double dMarkSpeed);
		void setDefaultPower(double dPowerInPercent);
		void setDefaultDelays(const sEmulatorDelays& delays);
		void setDefaultSkywriting(const sEmulatorSkywriting& skywriting);
		sEmulatorSettings getDefaultSettings();

		void clearList();
		uint32_t getListCommandCount();
		void appendToList(const CEmulatorCommandList& commandList);

		void executeList();
		bool checkFinished(double& dRemainingRealTimeInMilliseconds);
		void stopExecution();
		bool isBusy();

		sScannerStatusData getStatus();
		sExecutionStatisticsData getLastExecutionStatistics();

		void enableRecording(double dSampleIntervalInMicroseconds);
		void disableRecording();
		bool hasRecording();
		void getRecording(sRecordingData& recordingData);

		void attachImage(PScannerImageInstance pImage);

		void setFaultSeed(uint64_t nSeed);
		void injectFault(eFaultType faultType, eFaultPersistence persistence);
		void addFaultTrigger(const sFaultTrigger& trigger);
		void setRandomFault(eFaultType faultType, eFaultPersistence persistence, double dProbability);
		void setSlowdownFactor(double dFactor);
		double getSlowdownFactor();
		void clearFaultInjection();
		eFaultType getActiveFault();
		void resetFault();
		uint32_t getFaultCount();
	};

	typedef std::shared_ptr<CScannerInstance> PScannerInstance;

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator

#endif // __LIBMCDRIVER_SCANNEREMULATOR_SCANNERINSTANCE
