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


Abstract: Command lists, timelines and the timing model of the scanner emulator.
This file does not depend on LibMCEnv, so that the engine can be reused by other drivers.

*/


#ifndef __LIBMCDRIVER_SCANNEREMULATOR_TIMELINE
#define __LIBMCDRIVER_SCANNEREMULATOR_TIMELINE

#include "libmcdriver_scanneremulator_types.hpp"

#include <vector>
#include <memory>
#include <string>
#include <cstdint>

namespace LibMCDriver_ScannerEmulator {
namespace Impl {

	enum class eEmulatorCommandType : uint8_t {
		Jump = 0,
		Mark = 1,
		SetJumpSpeed = 2,
		SetMarkSpeed = 3,
		SetPower = 4,
		SetDelays = 5,
		CustomDelay = 6,
		EnableSkywriting = 7,
		DisableSkywriting = 8
	};

	enum class eTimelinePhase : uint8_t {
		Jump = 0,
		Mark = 1,
		JumpDelay = 2,
		MarkDelay = 3,
		PolygonDelay = 4,
		CustomDelay = 5,
		SkywritingRunUp = 6,
		SkywritingRunOut = 7
	};

	struct sEmulatorDelays {
		double m_dLaserOnDelay;
		double m_dLaserOffDelay;
		double m_dJumpDelay;
		double m_dMarkDelay;
		double m_dPolygonDelay;
	};

	struct sEmulatorSkywriting {
		eSkywritingMode m_Mode;
		double m_dTimelag;
		double m_dLaserOnShift;
		double m_dNPrev;
		double m_dNPost;
		double m_dLimit;
	};

	struct sEmulatorSettings {
		double m_dJumpSpeed;
		double m_dMarkSpeed;
		double m_dPower;
		sEmulatorDelays m_Delays;
		sEmulatorSkywriting m_Skywriting;
	};

	struct sEmulatorViewport {
		double m_dX1;
		double m_dY1;
		double m_dX2;
		double m_dY2;
	};

	struct sEmulatorTransformation {
		double m_dM11;
		double m_dM12;
		double m_dM21;
		double m_dM22;
		double m_dOffsetX;
		double m_dOffsetY;
	};

	struct sEmulatorCommand {
		eEmulatorCommandType m_Type;
		uint32_t m_nParameterIndex;
		double m_dX;
		double m_dY;
	};

	struct sTimelineSegment {
		double m_dStartTime;
		double m_dDuration;
		double m_dStartX;
		double m_dStartY;
		double m_dEndX;
		double m_dEndY;
		double m_dPower;
		uint32_t m_nCommandIndex;
		eTimelinePhase m_Phase;
	};

	struct sLaserInterval {
		double m_dOnTime;
		double m_dOffTime;
	};

	struct sTimelineStatistics {
		double m_dMarkLengthInMM;
		double m_dMarkTimeInMicroseconds;
		double m_dJumpLengthInMM;
		double m_dJumpTimeInMicroseconds;
		double m_dScannerDelayTimeInMicroseconds;
		double m_dCustomDelayTimeInMicroseconds;
		double m_dSkywritingTimeInMicroseconds;
		double m_dLaserOnTimeInMicroseconds;
		uint32_t m_nCommandCount;
		uint32_t m_nMarkSequenceCount;
		uint32_t m_nOutOfViewportCount;
		uint32_t m_nLaserOffDelayWarningCount;
	};

	sEmulatorSettings emulatorDefaultSettings();
	sEmulatorTransformation emulatorIdentityTransformation();

	void emulatorValidateCoordinate(double dX, double dY);
	void emulatorValidateSpeed(double dSpeed);
	void emulatorValidatePower(double dPowerInPercent);
	void emulatorValidateDelays(const sEmulatorDelays& delays);
	void emulatorValidateSkywriting(const sEmulatorSkywriting& skywriting);
	void emulatorValidateViewport(const sEmulatorViewport& viewport);
	void emulatorValidateTransformation(const sEmulatorTransformation& transformation);

	class CEmulatorCommandList {
	private:
		std::vector<sEmulatorCommand> m_Commands;
		std::vector<sEmulatorDelays> m_Delays;
		std::vector<sEmulatorSkywriting> m_Skywritings;
		uint32_t m_nMarkCount;

		void addCommand(eEmulatorCommandType commandType, uint32_t nParameterIndex, double dX, double dY);

	public:
		CEmulatorCommandList();

		void clear();
		void append(const CEmulatorCommandList& otherList);

		uint32_t getCommandCount() const;
		bool hasMarks() const;
		const sEmulatorCommand& getCommand(uint32_t nIndex) const;
		const sEmulatorDelays& getDelays(uint32_t nParameterIndex) const;
		const sEmulatorSkywriting& getSkywriting(uint32_t nParameterIndex) const;

		void addJump(double dX, double dY);
		void addMark(double dX, double dY);
		void addSetJumpSpeed(double dJumpSpeed);
		void addSetMarkSpeed(double dMarkSpeed);
		void addSetPower(double dPowerInPercent);
		void addSetDelays(const sEmulatorDelays& delays);
		void addCustomDelay(double dDelay);
		void addEnableSkywriting(const sEmulatorSkywriting& skywriting);
		void addDisableSkywriting();
	};

	class CEmulatorTimeline {
	private:
		std::vector<sTimelineSegment> m_Segments;
		std::vector<sLaserInterval> m_LaserIntervals;
		std::vector<double> m_CommandStartTimes;
		sTimelineStatistics m_Statistics;

		double m_dDuration;
		double m_dStartX;
		double m_dStartY;
		double m_dStartPower;

		const sTimelineSegment* findSegment(double dTime) const;

	public:
		CEmulatorTimeline(uint32_t nCommandCount, double dStartX, double dStartY, double dStartPower);

		void addSegment(const sTimelineSegment& segment);
		void addLaserInterval(double dOnTime, double dOffTime);
		void setCommandStartTime(uint32_t nCommandIndex, double dTime);
		void finish(double dDuration);

		sTimelineStatistics& statistics();
		const sTimelineStatistics& getStatistics() const;

		double getDuration() const;
		uint32_t getCommandCount() const;
		double getCommandStartTime(uint32_t nCommandIndex) const;

		void getPositionAt(double dTime, double& dX, double& dY) const;
		bool isLaserOnAt(double dTime) const;
		double getPowerAt(double dTime) const;
		uint32_t getCommandIndexAt(double dTime) const;

		const std::vector<sTimelineSegment>& getSegments() const;
		const std::vector<sLaserInterval>& getLaserIntervals() const;
	};

	typedef std::shared_ptr<CEmulatorTimeline> PEmulatorTimeline;

	class CEmulatorTimingModel {
	public:
		static PEmulatorTimeline computeTimeline(const CEmulatorCommandList& commandList, const sEmulatorSettings& initialSettings,
			const sEmulatorViewport& viewport, const sEmulatorTransformation& transformation, eOutOfViewportPolicy outOfViewportPolicy,
			double dUnitsInMM, double dStartX, double dStartY);
	};

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator

#endif // __LIBMCDRIVER_SCANNEREMULATOR_TIMELINE
