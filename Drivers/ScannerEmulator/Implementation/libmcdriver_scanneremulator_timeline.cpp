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

*/

#include "libmcdriver_scanneremulator_timeline.hpp"
#include "libmcdriver_scanneremulator_interfaceexception.hpp"

#include <cmath>
#include <algorithm>

#define SCANNEREMULATOR_MAXSPEED 1.0E7
#define SCANNEREMULATOR_MAXDELAY 1.0E7
#define SCANNEREMULATOR_MAXCUSTOMDELAY 1.0E9
#define SCANNEREMULATOR_MAXCOORDINATE 1.0E12
#define SCANNEREMULATOR_MINDETERMINANT 1.0E-12
#define SCANNEREMULATOR_DEFAULTJUMPSPEED 1000.0
#define SCANNEREMULATOR_DEFAULTMARKSPEED 500.0

using namespace LibMCDriver_ScannerEmulator::Impl;

namespace LibMCDriver_ScannerEmulator {
namespace Impl {

	sEmulatorSettings emulatorDefaultSettings()
	{
		sEmulatorSettings settings;
		settings.m_dJumpSpeed = SCANNEREMULATOR_DEFAULTJUMPSPEED;
		settings.m_dMarkSpeed = SCANNEREMULATOR_DEFAULTMARKSPEED;
		settings.m_dPower = 0.0;
		settings.m_Delays.m_dLaserOnDelay = 0.0;
		settings.m_Delays.m_dLaserOffDelay = 0.0;
		settings.m_Delays.m_dJumpDelay = 0.0;
		settings.m_Delays.m_dMarkDelay = 0.0;
		settings.m_Delays.m_dPolygonDelay = 0.0;
		settings.m_Skywriting.m_Mode = eSkywritingMode::Off;
		settings.m_Skywriting.m_dTimelag = 0.0;
		settings.m_Skywriting.m_dLaserOnShift = 0.0;
		settings.m_Skywriting.m_dNPrev = 0.0;
		settings.m_Skywriting.m_dNPost = 0.0;
		settings.m_Skywriting.m_dLimit = 0.0;
		return settings;
	}

	sEmulatorTransformation emulatorIdentityTransformation()
	{
		sEmulatorTransformation transformation;
		transformation.m_dM11 = 1.0;
		transformation.m_dM12 = 0.0;
		transformation.m_dM21 = 0.0;
		transformation.m_dM22 = 1.0;
		transformation.m_dOffsetX = 0.0;
		transformation.m_dOffsetY = 0.0;
		return transformation;
	}

	static bool isInRange(double dValue, double dMin, double dMax)
	{
		return std::isfinite(dValue) && (dValue >= dMin) && (dValue <= dMax);
	}

	void emulatorValidateCoordinate(double dX, double dY)
	{
		if (!isInRange(dX, -SCANNEREMULATOR_MAXCOORDINATE, SCANNEREMULATOR_MAXCOORDINATE) || !isInRange(dY, -SCANNEREMULATOR_MAXCOORDINATE, SCANNEREMULATOR_MAXCOORDINATE))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCOORDINATE, "invalid coordinate: " + std::to_string(dX) + "/" + std::to_string(dY));
	}

	void emulatorValidateSpeed(double dSpeed)
	{
		if (!isInRange(dSpeed, 0.0, SCANNEREMULATOR_MAXSPEED) || (dSpeed <= 0.0))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSPEED, "invalid speed: " + std::to_string(dSpeed));
	}

	void emulatorValidatePower(double dPowerInPercent)
	{
		if (!isInRange(dPowerInPercent, 0.0, 100.0))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPOWER, "invalid power: " + std::to_string(dPowerInPercent));
	}

	void emulatorValidateDelays(const sEmulatorDelays& delays)
	{
		if (!isInRange(delays.m_dLaserOnDelay, -SCANNEREMULATOR_MAXDELAY, SCANNEREMULATOR_MAXDELAY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDDELAY, "invalid laser on delay: " + std::to_string(delays.m_dLaserOnDelay));
		if (!isInRange(delays.m_dLaserOffDelay, -SCANNEREMULATOR_MAXDELAY, SCANNEREMULATOR_MAXDELAY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDDELAY, "invalid laser off delay: " + std::to_string(delays.m_dLaserOffDelay));
		if (!isInRange(delays.m_dJumpDelay, 0.0, SCANNEREMULATOR_MAXDELAY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDDELAY, "invalid jump delay: " + std::to_string(delays.m_dJumpDelay));
		if (!isInRange(delays.m_dMarkDelay, 0.0, SCANNEREMULATOR_MAXDELAY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDDELAY, "invalid mark delay: " + std::to_string(delays.m_dMarkDelay));
		if (!isInRange(delays.m_dPolygonDelay, 0.0, SCANNEREMULATOR_MAXDELAY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDDELAY, "invalid polygon delay: " + std::to_string(delays.m_dPolygonDelay));
	}

	void emulatorValidateSkywriting(const sEmulatorSkywriting& skywriting)
	{
		switch (skywriting.m_Mode) {
		case eSkywritingMode::Off:
		case eSkywritingMode::Mode1:
		case eSkywritingMode::Mode2:
		case eSkywritingMode::Mode3:
		case eSkywritingMode::Mode4:
			break;
		default:
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSKYWRITINGPARAMETER, "invalid skywriting mode: " + std::to_string((int)skywriting.m_Mode));
		}

		if (!isInRange(skywriting.m_dTimelag, 0.0, SCANNEREMULATOR_MAXDELAY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSKYWRITINGPARAMETER, "invalid skywriting timelag: " + std::to_string(skywriting.m_dTimelag));
		if (!isInRange(skywriting.m_dLaserOnShift, -SCANNEREMULATOR_MAXDELAY, SCANNEREMULATOR_MAXDELAY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSKYWRITINGPARAMETER, "invalid skywriting laser on shift: " + std::to_string(skywriting.m_dLaserOnShift));
		if (!isInRange(skywriting.m_dNPrev, 0.0, SCANNEREMULATOR_MAXDELAY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSKYWRITINGPARAMETER, "invalid skywriting nprev: " + std::to_string(skywriting.m_dNPrev));
		if (!isInRange(skywriting.m_dNPost, 0.0, SCANNEREMULATOR_MAXDELAY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSKYWRITINGPARAMETER, "invalid skywriting npost: " + std::to_string(skywriting.m_dNPost));
		if (!isInRange(skywriting.m_dLimit, -1.0, 1.0))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSKYWRITINGPARAMETER, "invalid skywriting limit: " + std::to_string(skywriting.m_dLimit));
	}

	void emulatorValidateViewport(const sEmulatorViewport& viewport)
	{
		bool bIsFinite = isInRange(viewport.m_dX1, -SCANNEREMULATOR_MAXCOORDINATE, SCANNEREMULATOR_MAXCOORDINATE) && isInRange(viewport.m_dY1, -SCANNEREMULATOR_MAXCOORDINATE, SCANNEREMULATOR_MAXCOORDINATE)
			&& isInRange(viewport.m_dX2, -SCANNEREMULATOR_MAXCOORDINATE, SCANNEREMULATOR_MAXCOORDINATE) && isInRange(viewport.m_dY2, -SCANNEREMULATOR_MAXCOORDINATE, SCANNEREMULATOR_MAXCOORDINATE);

		if ((!bIsFinite) || (viewport.m_dX1 >= viewport.m_dX2) || (viewport.m_dY1 >= viewport.m_dY2))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDVIEWPORT, "invalid viewport: (" + std::to_string(viewport.m_dX1) + ", " + std::to_string(viewport.m_dY1) + ") - (" + std::to_string(viewport.m_dX2) + ", " + std::to_string(viewport.m_dY2) + ")");
	}

	void emulatorValidateTransformation(const sEmulatorTransformation& transformation)
	{
		double values[6] = { transformation.m_dM11, transformation.m_dM12, transformation.m_dM21, transformation.m_dM22, transformation.m_dOffsetX, transformation.m_dOffsetY };
		for (double dValue : values) {
			if (!isInRange(dValue, -SCANNEREMULATOR_MAXCOORDINATE, SCANNEREMULATOR_MAXCOORDINATE))
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDTRANSFORMATION, "invalid transformation value: " + std::to_string(dValue));
		}

		double dDeterminant = transformation.m_dM11 * transformation.m_dM22 - transformation.m_dM12 * transformation.m_dM21;
		if (std::fabs(dDeterminant) < SCANNEREMULATOR_MINDETERMINANT)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDTRANSFORMATION, "transformation matrix is singular: determinant " + std::to_string(dDeterminant));
	}


	/*************************************************************************************************************************
	 CEmulatorCommandList
	**************************************************************************************************************************/

	CEmulatorCommandList::CEmulatorCommandList()
		: m_nMarkCount(0)
	{
	}

	void CEmulatorCommandList::clear()
	{
		m_Commands.clear();
		m_Delays.clear();
		m_Skywritings.clear();
		m_nMarkCount = 0;
	}

	void CEmulatorCommandList::append(const CEmulatorCommandList& otherList)
	{
		uint32_t nDelayOffset = (uint32_t)m_Delays.size();
		uint32_t nSkywritingOffset = (uint32_t)m_Skywritings.size();

		m_Delays.insert(m_Delays.end(), otherList.m_Delays.begin(), otherList.m_Delays.end());
		m_Skywritings.insert(m_Skywritings.end(), otherList.m_Skywritings.begin(), otherList.m_Skywritings.end());

		m_Commands.reserve(m_Commands.size() + otherList.m_Commands.size());
		for (auto command : otherList.m_Commands) {
			if (command.m_Type == eEmulatorCommandType::SetDelays)
				command.m_nParameterIndex += nDelayOffset;
			if (command.m_Type == eEmulatorCommandType::EnableSkywriting)
				command.m_nParameterIndex += nSkywritingOffset;
			m_Commands.push_back(command);
		}

		m_nMarkCount += otherList.m_nMarkCount;
	}

	uint32_t CEmulatorCommandList::getCommandCount() const
	{
		return (uint32_t)m_Commands.size();
	}

	bool CEmulatorCommandList::hasMarks() const
	{
		return m_nMarkCount > 0;
	}

	const sEmulatorCommand& CEmulatorCommandList::getCommand(uint32_t nIndex) const
	{
		if (nIndex >= m_Commands.size())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);
		return m_Commands[nIndex];
	}

	const sEmulatorDelays& CEmulatorCommandList::getDelays(uint32_t nParameterIndex) const
	{
		if (nParameterIndex >= m_Delays.size())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);
		return m_Delays[nParameterIndex];
	}

	const sEmulatorSkywriting& CEmulatorCommandList::getSkywriting(uint32_t nParameterIndex) const
	{
		if (nParameterIndex >= m_Skywritings.size())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);
		return m_Skywritings[nParameterIndex];
	}

	void CEmulatorCommandList::addCommand(eEmulatorCommandType commandType, uint32_t nParameterIndex, double dX, double dY)
	{
		sEmulatorCommand command;
		command.m_Type = commandType;
		command.m_nParameterIndex = nParameterIndex;
		command.m_dX = dX;
		command.m_dY = dY;
		m_Commands.push_back(command);
	}

	void CEmulatorCommandList::addJump(double dX, double dY)
	{
		emulatorValidateCoordinate(dX, dY);
		addCommand(eEmulatorCommandType::Jump, 0, dX, dY);
	}

	void CEmulatorCommandList::addMark(double dX, double dY)
	{
		emulatorValidateCoordinate(dX, dY);
		addCommand(eEmulatorCommandType::Mark, 0, dX, dY);
		m_nMarkCount++;
	}

	void CEmulatorCommandList::addSetJumpSpeed(double dJumpSpeed)
	{
		emulatorValidateSpeed(dJumpSpeed);
		addCommand(eEmulatorCommandType::SetJumpSpeed, 0, dJumpSpeed, 0.0);
	}

	void CEmulatorCommandList::addSetMarkSpeed(double dMarkSpeed)
	{
		emulatorValidateSpeed(dMarkSpeed);
		addCommand(eEmulatorCommandType::SetMarkSpeed, 0, dMarkSpeed, 0.0);
	}

	void CEmulatorCommandList::addSetPower(double dPowerInPercent)
	{
		emulatorValidatePower(dPowerInPercent);
		addCommand(eEmulatorCommandType::SetPower, 0, dPowerInPercent, 0.0);
	}

	void CEmulatorCommandList::addSetDelays(const sEmulatorDelays& delays)
	{
		emulatorValidateDelays(delays);
		m_Delays.push_back(delays);
		addCommand(eEmulatorCommandType::SetDelays, (uint32_t)(m_Delays.size() - 1), 0.0, 0.0);
	}

	void CEmulatorCommandList::addCustomDelay(double dDelay)
	{
		if (!isInRange(dDelay, 0.0, SCANNEREMULATOR_MAXCUSTOMDELAY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDDELAY, "invalid custom delay: " + std::to_string(dDelay));
		addCommand(eEmulatorCommandType::CustomDelay, 0, dDelay, 0.0);
	}

	void CEmulatorCommandList::addEnableSkywriting(const sEmulatorSkywriting& skywriting)
	{
		emulatorValidateSkywriting(skywriting);
		if (skywriting.m_Mode == eSkywritingMode::Off)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSKYWRITINGPARAMETER, "skywriting mode must not be off");
		m_Skywritings.push_back(skywriting);
		addCommand(eEmulatorCommandType::EnableSkywriting, (uint32_t)(m_Skywritings.size() - 1), 0.0, 0.0);
	}

	void CEmulatorCommandList::addDisableSkywriting()
	{
		addCommand(eEmulatorCommandType::DisableSkywriting, 0, 0.0, 0.0);
	}


	/*************************************************************************************************************************
	 CEmulatorTimeline
	**************************************************************************************************************************/

	CEmulatorTimeline::CEmulatorTimeline(uint32_t nCommandCount, double dStartX, double dStartY, double dStartPower)
		: m_dDuration(0.0), m_dStartX(dStartX), m_dStartY(dStartY), m_dStartPower(dStartPower)
	{
		m_CommandStartTimes.resize(nCommandCount, 0.0);
		m_Statistics = {};
		m_Statistics.m_nCommandCount = nCommandCount;
	}

	void CEmulatorTimeline::addSegment(const sTimelineSegment& segment)
	{
		m_Segments.push_back(segment);
	}

	void CEmulatorTimeline::addLaserInterval(double dOnTime, double dOffTime)
	{
		double dClampedOnTime = std::max(dOnTime, 0.0);
		if (dOffTime <= dClampedOnTime)
			return;

		if (!m_LaserIntervals.empty()) {
			auto& lastInterval = m_LaserIntervals.back();
			if (dClampedOnTime <= lastInterval.m_dOffTime) {
				lastInterval.m_dOnTime = std::min(lastInterval.m_dOnTime, dClampedOnTime);
				lastInterval.m_dOffTime = std::max(lastInterval.m_dOffTime, dOffTime);
				return;
			}
		}

		m_LaserIntervals.push_back({ dClampedOnTime, dOffTime });
	}

	void CEmulatorTimeline::setCommandStartTime(uint32_t nCommandIndex, double dTime)
	{
		if (nCommandIndex >= m_CommandStartTimes.size())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);
		m_CommandStartTimes[nCommandIndex] = dTime;
	}

	void CEmulatorTimeline::finish(double dDuration)
	{
		m_dDuration = dDuration;
		if (!m_LaserIntervals.empty())
			m_dDuration = std::max(m_dDuration, m_LaserIntervals.back().m_dOffTime);

		double dLaserOnTime = 0.0;
		for (auto& interval : m_LaserIntervals)
			dLaserOnTime += (interval.m_dOffTime - interval.m_dOnTime);
		m_Statistics.m_dLaserOnTimeInMicroseconds = dLaserOnTime;
	}

	sTimelineStatistics& CEmulatorTimeline::statistics()
	{
		return m_Statistics;
	}

	const sTimelineStatistics& CEmulatorTimeline::getStatistics() const
	{
		return m_Statistics;
	}

	double CEmulatorTimeline::getDuration() const
	{
		return m_dDuration;
	}

	uint32_t CEmulatorTimeline::getCommandCount() const
	{
		return (uint32_t)m_CommandStartTimes.size();
	}

	double CEmulatorTimeline::getCommandStartTime(uint32_t nCommandIndex) const
	{
		if (nCommandIndex >= m_CommandStartTimes.size())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);
		return m_CommandStartTimes[nCommandIndex];
	}

	const sTimelineSegment* CEmulatorTimeline::findSegment(double dTime) const
	{
		if (m_Segments.empty())
			return nullptr;

		auto iIter = std::upper_bound(m_Segments.begin(), m_Segments.end(), dTime,
			[](double dValue, const sTimelineSegment& segment) { return dValue < segment.m_dStartTime; });

		if (iIter == m_Segments.begin())
			return nullptr;

		return &(*(iIter - 1));
	}

	void CEmulatorTimeline::getPositionAt(double dTime, double& dX, double& dY) const
	{
		auto pSegment = findSegment(dTime);
		if (pSegment == nullptr) {
			dX = m_dStartX;
			dY = m_dStartY;
			return;
		}

		double dElapsed = dTime - pSegment->m_dStartTime;
		if ((pSegment->m_dDuration <= 0.0) || (dElapsed >= pSegment->m_dDuration)) {
			dX = pSegment->m_dEndX;
			dY = pSegment->m_dEndY;
			return;
		}

		double dFactor = dElapsed / pSegment->m_dDuration;
		dX = pSegment->m_dStartX + (pSegment->m_dEndX - pSegment->m_dStartX) * dFactor;
		dY = pSegment->m_dStartY + (pSegment->m_dEndY - pSegment->m_dStartY) * dFactor;
	}

	bool CEmulatorTimeline::isLaserOnAt(double dTime) const
	{
		if (m_LaserIntervals.empty())
			return false;

		auto iIter = std::upper_bound(m_LaserIntervals.begin(), m_LaserIntervals.end(), dTime,
			[](double dValue, const sLaserInterval& interval) { return dValue < interval.m_dOnTime; });

		if (iIter == m_LaserIntervals.begin())
			return false;

		return dTime < (iIter - 1)->m_dOffTime;
	}

	double CEmulatorTimeline::getPowerAt(double dTime) const
	{
		auto pSegment = findSegment(dTime);
		if (pSegment == nullptr)
			return m_dStartPower;

		return pSegment->m_dPower;
	}

	uint32_t CEmulatorTimeline::getCommandIndexAt(double dTime) const
	{
		if (m_CommandStartTimes.empty())
			return 0;

		auto iIter = std::upper_bound(m_CommandStartTimes.begin(), m_CommandStartTimes.end(), dTime);
		if (iIter == m_CommandStartTimes.begin())
			return 0;

		return (uint32_t)((iIter - 1) - m_CommandStartTimes.begin());
	}

	const std::vector<sTimelineSegment>& CEmulatorTimeline::getSegments() const
	{
		return m_Segments;
	}

	const std::vector<sLaserInterval>& CEmulatorTimeline::getLaserIntervals() const
	{
		return m_LaserIntervals;
	}


	/*************************************************************************************************************************
	 CEmulatorTimelineBuilder
	**************************************************************************************************************************/

	class CEmulatorTimelineBuilder {
	private:
		CEmulatorTimeline* m_pTimeline;
		sEmulatorSettings m_Settings;
		sEmulatorViewport m_Viewport;
		sEmulatorTransformation m_Transformation;
		eOutOfViewportPolicy m_OutOfViewportPolicy;
		double m_dUnitsInMM;

		double m_dTime;
		double m_dX;
		double m_dY;

		bool m_bInSequence;
		sEmulatorSkywriting m_SequenceSkywriting;
		uint32_t m_nLastMarkIndex;
		bool m_bHasLastDirection;
		double m_dLastDirectionX;
		double m_dLastDirectionY;
		double m_dLaserOnTime;

		bool m_bPendingJumpDelay;
		uint32_t m_nPendingJumpIndex;

		void addStationaryPhase(eTimelinePhase phase, double dDuration, uint32_t nCommandIndex)
		{
			if (dDuration <= 0.0)
				return;

			sTimelineSegment segment;
			segment.m_dStartTime = m_dTime;
			segment.m_dDuration = dDuration;
			segment.m_dStartX = m_dX;
			segment.m_dStartY = m_dY;
			segment.m_dEndX = m_dX;
			segment.m_dEndY = m_dY;
			segment.m_dPower = m_Settings.m_dPower;
			segment.m_nCommandIndex = nCommandIndex;
			segment.m_Phase = phase;
			m_pTimeline->addSegment(segment);

			auto& statistics = m_pTimeline->statistics();
			switch (phase) {
			case eTimelinePhase::JumpDelay:
			case eTimelinePhase::MarkDelay:
			case eTimelinePhase::PolygonDelay:
				statistics.m_dScannerDelayTimeInMicroseconds += dDuration;
				break;
			case eTimelinePhase::CustomDelay:
				statistics.m_dCustomDelayTimeInMicroseconds += dDuration;
				break;
			case eTimelinePhase::SkywritingRunUp:
			case eTimelinePhase::SkywritingRunOut:
				statistics.m_dSkywritingTimeInMicroseconds += dDuration;
				break;
			default:
				break;
			}

			m_dTime += dDuration;
		}

		void addMovement(eTimelinePhase phase, double dTargetX, double dTargetY, double dSpeedInMMPerSecond, uint32_t nCommandIndex)
		{
			double dLengthInMM = std::hypot(dTargetX - m_dX, dTargetY - m_dY) * m_dUnitsInMM;
			double dDuration = dLengthInMM / dSpeedInMMPerSecond * 1000000.0;

			auto& statistics = m_pTimeline->statistics();
			if (phase == eTimelinePhase::Mark) {
				statistics.m_dMarkLengthInMM += dLengthInMM;
				statistics.m_dMarkTimeInMicroseconds += dDuration;
			}
			else {
				statistics.m_dJumpLengthInMM += dLengthInMM;
				statistics.m_dJumpTimeInMicroseconds += dDuration;
			}

			if (dDuration > 0.0) {
				sTimelineSegment segment;
				segment.m_dStartTime = m_dTime;
				segment.m_dDuration = dDuration;
				segment.m_dStartX = m_dX;
				segment.m_dStartY = m_dY;
				segment.m_dEndX = dTargetX;
				segment.m_dEndY = dTargetY;
				segment.m_dPower = m_Settings.m_dPower;
				segment.m_nCommandIndex = nCommandIndex;
				segment.m_Phase = phase;
				m_pTimeline->addSegment(segment);
				m_dTime += dDuration;
			}

			m_dX = dTargetX;
			m_dY = dTargetY;
		}

		void transformTarget(uint32_t nCommandIndex, double dX, double dY, double& dTargetX, double& dTargetY)
		{
			dTargetX = m_Transformation.m_dM11 * dX + m_Transformation.m_dM12 * dY + m_Transformation.m_dOffsetX;
			dTargetY = m_Transformation.m_dM21 * dX + m_Transformation.m_dM22 * dY + m_Transformation.m_dOffsetY;

			bool bIsOutside = (dTargetX < m_Viewport.m_dX1) || (dTargetX > m_Viewport.m_dX2) || (dTargetY < m_Viewport.m_dY1) || (dTargetY > m_Viewport.m_dY2);
			if (!bIsOutside)
				return;

			m_pTimeline->statistics().m_nOutOfViewportCount++;

			switch (m_OutOfViewportPolicy) {
			case eOutOfViewportPolicy::Clamp:
				dTargetX = std::min(std::max(dTargetX, m_Viewport.m_dX1), m_Viewport.m_dX2);
				dTargetY = std::min(std::max(dTargetY, m_Viewport.m_dY1), m_Viewport.m_dY2);
				break;
			case eOutOfViewportPolicy::Warn:
				break;
			default:
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_OUTOFVIEWPORT, "command " + std::to_string(nCommandIndex) + ": target (" + std::to_string(dTargetX) + ", " + std::to_string(dTargetY)
					+ ") is outside of the viewport (" + std::to_string(m_Viewport.m_dX1) + ", " + std::to_string(m_Viewport.m_dY1) + ") - (" + std::to_string(m_Viewport.m_dX2) + ", " + std::to_string(m_Viewport.m_dY2) + ")");
			}
		}

		void flushJumpDelay(bool bApplyDelay)
		{
			if (!m_bPendingJumpDelay)
				return;

			m_bPendingJumpDelay = false;
			if (bApplyDelay)
				addStationaryPhase(eTimelinePhase::JumpDelay, m_Settings.m_Delays.m_dJumpDelay, m_nPendingJumpIndex);
		}

		bool sequenceHasSkywriting()
		{
			return m_SequenceSkywriting.m_Mode != eSkywritingMode::Off;
		}

		void startSequence(uint32_t nCommandIndex)
		{
			m_SequenceSkywriting = m_Settings.m_Skywriting;
			m_bInSequence = true;
			m_bHasLastDirection = false;
			m_pTimeline->statistics().m_nMarkSequenceCount++;

			if (sequenceHasSkywriting()) {
				flushJumpDelay(false);
				m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);
				addStationaryPhase(eTimelinePhase::SkywritingRunUp, m_SequenceSkywriting.m_dNPrev, nCommandIndex);
				m_dLaserOnTime = m_dTime + m_SequenceSkywriting.m_dTimelag + m_SequenceSkywriting.m_dLaserOnShift;
			}
			else {
				flushJumpDelay(true);
				m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);
				m_dLaserOnTime = m_dTime + m_Settings.m_Delays.m_dLaserOnDelay;
			}
		}

		void endSequence()
		{
			if (!m_bInSequence)
				return;

			m_bInSequence = false;

			if (sequenceHasSkywriting()) {
				m_pTimeline->addLaserInterval(m_dLaserOnTime, m_dTime + m_SequenceSkywriting.m_dTimelag);
				addStationaryPhase(eTimelinePhase::SkywritingRunOut, m_SequenceSkywriting.m_dNPost, m_nLastMarkIndex);
			}
			else {
				m_pTimeline->addLaserInterval(m_dLaserOnTime, m_dTime + m_Settings.m_Delays.m_dLaserOffDelay);
				if (m_Settings.m_Delays.m_dLaserOffDelay > m_Settings.m_Delays.m_dMarkDelay)
					m_pTimeline->statistics().m_nLaserOffDelayWarningCount++;
				addStationaryPhase(eTimelinePhase::MarkDelay, m_Settings.m_Delays.m_dMarkDelay, m_nLastMarkIndex);
			}
		}

		void handleCorner(uint32_t nCommandIndex, double dDirectionX, double dDirectionY)
		{
			if (!m_bHasLastDirection)
				return;

			double dCosine = m_dLastDirectionX * dDirectionX + m_dLastDirectionY * dDirectionY;

			bool bSkywritingCorner = false;
			switch (m_SequenceSkywriting.m_Mode) {
			case eSkywritingMode::Mode2:
				bSkywritingCorner = true;
				break;
			case eSkywritingMode::Mode3:
			case eSkywritingMode::Mode4:
				bSkywritingCorner = (dCosine < m_SequenceSkywriting.m_dLimit);
				break;
			default:
				break;
			}

			if (bSkywritingCorner) {
				m_pTimeline->addLaserInterval(m_dLaserOnTime, m_dTime + m_SequenceSkywriting.m_dTimelag);
				addStationaryPhase(eTimelinePhase::SkywritingRunOut, m_SequenceSkywriting.m_dNPost, nCommandIndex);
				addStationaryPhase(eTimelinePhase::SkywritingRunUp, m_SequenceSkywriting.m_dNPrev, nCommandIndex);
				m_dLaserOnTime = m_dTime + m_SequenceSkywriting.m_dTimelag + m_SequenceSkywriting.m_dLaserOnShift;
			}
			else {
				addStationaryPhase(eTimelinePhase::PolygonDelay, m_Settings.m_Delays.m_dPolygonDelay, nCommandIndex);
			}
		}

	public:

		CEmulatorTimelineBuilder(CEmulatorTimeline* pTimeline, const sEmulatorSettings& initialSettings, const sEmulatorViewport& viewport,
			const sEmulatorTransformation& transformation, eOutOfViewportPolicy outOfViewportPolicy, double dUnitsInMM, double dStartX, double dStartY)
			: m_pTimeline(pTimeline), m_Settings(initialSettings), m_Viewport(viewport), m_Transformation(transformation),
			m_OutOfViewportPolicy(outOfViewportPolicy), m_dUnitsInMM(dUnitsInMM),
			m_dTime(0.0), m_dX(dStartX), m_dY(dStartY),
			m_bInSequence(false), m_SequenceSkywriting(initialSettings.m_Skywriting), m_nLastMarkIndex(0),
			m_bHasLastDirection(false), m_dLastDirectionX(0.0), m_dLastDirectionY(0.0), m_dLaserOnTime(0.0),
			m_bPendingJumpDelay(false), m_nPendingJumpIndex(0)
		{
		}

		void processCommand(uint32_t nCommandIndex, const CEmulatorCommandList& commandList)
		{
			auto& command = commandList.getCommand(nCommandIndex);

			switch (command.m_Type) {
			case eEmulatorCommandType::Jump: {
				double dTargetX, dTargetY;
				transformTarget(nCommandIndex, command.m_dX, command.m_dY, dTargetX, dTargetY);

				endSequence();
				flushJumpDelay(true);
				m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);
				addMovement(eTimelinePhase::Jump, dTargetX, dTargetY, m_Settings.m_dJumpSpeed, nCommandIndex);
				m_bPendingJumpDelay = true;
				m_nPendingJumpIndex = nCommandIndex;
				break;
			}

			case eEmulatorCommandType::Mark: {
				double dTargetX, dTargetY;
				transformTarget(nCommandIndex, command.m_dX, command.m_dY, dTargetX, dTargetY);

				if (!m_bInSequence)
					startSequence(nCommandIndex);
				else
					m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);

				double dDeltaX = dTargetX - m_dX;
				double dDeltaY = dTargetY - m_dY;
				double dLength = std::hypot(dDeltaX, dDeltaY);
				if (dLength > 0.0) {
					double dDirectionX = dDeltaX / dLength;
					double dDirectionY = dDeltaY / dLength;
					handleCorner(nCommandIndex, dDirectionX, dDirectionY);
					m_bHasLastDirection = true;
					m_dLastDirectionX = dDirectionX;
					m_dLastDirectionY = dDirectionY;
				}

				addMovement(eTimelinePhase::Mark, dTargetX, dTargetY, m_Settings.m_dMarkSpeed, nCommandIndex);
				m_nLastMarkIndex = nCommandIndex;
				break;
			}

			case eEmulatorCommandType::CustomDelay:
				endSequence();
				flushJumpDelay(true);
				m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);
				addStationaryPhase(eTimelinePhase::CustomDelay, command.m_dX, nCommandIndex);
				break;

			case eEmulatorCommandType::SetJumpSpeed:
				m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);
				m_Settings.m_dJumpSpeed = command.m_dX;
				break;

			case eEmulatorCommandType::SetMarkSpeed:
				m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);
				m_Settings.m_dMarkSpeed = command.m_dX;
				break;

			case eEmulatorCommandType::SetPower:
				m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);
				m_Settings.m_dPower = command.m_dX;
				break;

			case eEmulatorCommandType::SetDelays:
				m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);
				m_Settings.m_Delays = commandList.getDelays(command.m_nParameterIndex);
				break;

			case eEmulatorCommandType::EnableSkywriting:
				m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);
				m_Settings.m_Skywriting = commandList.getSkywriting(command.m_nParameterIndex);
				break;

			case eEmulatorCommandType::DisableSkywriting:
				m_pTimeline->setCommandStartTime(nCommandIndex, m_dTime);
				m_Settings.m_Skywriting.m_Mode = eSkywritingMode::Off;
				break;

			default:
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM, "invalid command type at command " + std::to_string(nCommandIndex));
			}
		}

		void finish()
		{
			endSequence();
			flushJumpDelay(true);
			m_pTimeline->finish(m_dTime);
		}
	};


	/*************************************************************************************************************************
	 CEmulatorTimingModel
	**************************************************************************************************************************/

	PEmulatorTimeline CEmulatorTimingModel::computeTimeline(const CEmulatorCommandList& commandList, const sEmulatorSettings& initialSettings,
		const sEmulatorViewport& viewport, const sEmulatorTransformation& transformation, eOutOfViewportPolicy outOfViewportPolicy,
		double dUnitsInMM, double dStartX, double dStartY)
	{
		if (!(dUnitsInMM > 0.0) || !std::isfinite(dUnitsInMM))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDUNITS);

		uint32_t nCommandCount = commandList.getCommandCount();
		auto pTimeline = std::make_shared<CEmulatorTimeline>(nCommandCount, dStartX, dStartY, initialSettings.m_dPower);

		CEmulatorTimelineBuilder builder(pTimeline.get(), initialSettings, viewport, transformation, outOfViewportPolicy, dUnitsInMM, dStartX, dStartY);
		for (uint32_t nCommandIndex = 0; nCommandIndex < nCommandCount; nCommandIndex++)
			builder.processCommand(nCommandIndex, commandList);
		builder.finish();

		return pTimeline;
	}

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator
