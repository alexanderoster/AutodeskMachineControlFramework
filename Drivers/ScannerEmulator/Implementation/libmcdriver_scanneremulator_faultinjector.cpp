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


Abstract: Fault triggers of the scanner emulator.

*/

#include "libmcdriver_scanneremulator_faultinjector.hpp"
#include "libmcdriver_scanneremulator_interfaceexception.hpp"

#include <cmath>

namespace LibMCDriver_ScannerEmulator {
namespace Impl {

	CEmulatorFaultInjector::CEmulatorFaultInjector()
		: m_RandomGenerator(0)
	{
	}

	void CEmulatorFaultInjector::validateFaultType(eFaultType faultType)
	{
		switch (faultType) {
		case eFaultType::ConnectionLost:
		case eFaultType::PositionError:
		case eFaultType::TemperatureError:
		case eFaultType::PowerError:
		case eFaultType::ListAborted:
		case eFaultType::Busy:
		case eFaultType::Slowdown:
		case eFaultType::LaserNotReady:
			return;
		default:
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDFAULTTYPE, "invalid fault type: " + std::to_string((int)faultType));
		}
	}

	void CEmulatorFaultInjector::validatePersistence(eFaultPersistence persistence)
	{
		if ((persistence != eFaultPersistence::Latched) && (persistence != eFaultPersistence::Transient))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDFAULTTRIGGER, "invalid fault persistence: " + std::to_string((int)persistence));
	}

	bool CEmulatorFaultInjector::isStartFault(eFaultType faultType)
	{
		return (faultType == eFaultType::Busy) || (faultType == eFaultType::LaserNotReady);
	}

	bool CEmulatorFaultInjector::isRunFault(eFaultType faultType)
	{
		switch (faultType) {
		case eFaultType::ConnectionLost:
		case eFaultType::PositionError:
		case eFaultType::TemperatureError:
		case eFaultType::PowerError:
		case eFaultType::ListAborted:
			return true;
		default:
			return false;
		}
	}

	std::string CEmulatorFaultInjector::faultTypeToString(eFaultType faultType)
	{
		switch (faultType) {
		case eFaultType::ConnectionLost: return "connectionlost";
		case eFaultType::PositionError: return "positionerror";
		case eFaultType::TemperatureError: return "temperatureerror";
		case eFaultType::PowerError: return "powererror";
		case eFaultType::ListAborted: return "listaborted";
		case eFaultType::Busy: return "busy";
		case eFaultType::Slowdown: return "slowdown";
		case eFaultType::LaserNotReady: return "lasernotready";
		default: return "";
		}
	}

	eFaultType CEmulatorFaultInjector::stringToFaultType(const std::string& sFaultType)
	{
		if (sFaultType == "connectionlost") return eFaultType::ConnectionLost;
		if (sFaultType == "positionerror") return eFaultType::PositionError;
		if (sFaultType == "temperatureerror") return eFaultType::TemperatureError;
		if (sFaultType == "powererror") return eFaultType::PowerError;
		if (sFaultType == "listaborted") return eFaultType::ListAborted;
		if (sFaultType == "busy") return eFaultType::Busy;
		if (sFaultType == "slowdown") return eFaultType::Slowdown;
		if (sFaultType == "lasernotready") return eFaultType::LaserNotReady;

		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDFAULTTYPE, "invalid fault type: " + sFaultType);
	}

	void CEmulatorFaultInjector::throwFaultError(eFaultType faultType, const std::string& sScannerName)
	{
		std::string sMessage = "emulated fault on scanner " + sScannerName + ": " + faultTypeToString(faultType);

		switch (faultType) {
		case eFaultType::ConnectionLost:
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERDISCONNECTED, sMessage);
		case eFaultType::PositionError:
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERPOSITIONERROR, sMessage);
		case eFaultType::TemperatureError:
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERTEMPERATUREERROR, sMessage);
		case eFaultType::PowerError:
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERPOWERERROR, sMessage);
		case eFaultType::ListAborted:
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_LISTABORTED, sMessage);
		case eFaultType::Busy:
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERBUSY, sMessage);
		case eFaultType::LaserNotReady:
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_LASERNOTREADY, sMessage);
		default:
			break;
		}
	}

	double CEmulatorFaultInjector::drawUniform()
	{
		std::uniform_real_distribution<double> distribution(0.0, 1.0);
		return distribution(m_RandomGenerator);
	}

	void CEmulatorFaultInjector::setSeed(uint64_t nSeed)
	{
		m_RandomGenerator.seed(nSeed);
	}

	void CEmulatorFaultInjector::addTrigger(const sFaultTrigger& trigger)
	{
		validateFaultType(trigger.m_FaultType);
		validatePersistence(trigger.m_Persistence);

		if ((trigger.m_TriggerType == eFaultTriggerType::AtTime) && (!std::isfinite(trigger.m_dTimeInMicroseconds) || (trigger.m_dTimeInMicroseconds < 0.0)))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDFAULTTRIGGER, "invalid fault time: " + std::to_string(trigger.m_dTimeInMicroseconds));
		if ((trigger.m_TriggerType == eFaultTriggerType::AtExecution) && (trigger.m_nExecutionCountdown == 0))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDFAULTTRIGGER, "fault execution count must be positive");

		m_Triggers.push_back(trigger);
	}

	void CEmulatorFaultInjector::setRandomFault(eFaultType faultType, eFaultPersistence persistence, double dProbability)
	{
		validateFaultType(faultType);
		validatePersistence(persistence);
		if (!std::isfinite(dProbability) || (dProbability < 0.0) || (dProbability > 1.0))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPROBABILITY, "invalid fault probability: " + std::to_string(dProbability));

		for (auto iIter = m_RandomFaults.begin(); iIter != m_RandomFaults.end(); iIter++) {
			if (iIter->m_FaultType == faultType) {
				m_RandomFaults.erase(iIter);
				break;
			}
		}

		if (dProbability > 0.0)
			m_RandomFaults.push_back({ faultType, persistence, dProbability });
	}

	void CEmulatorFaultInjector::clear()
	{
		m_Triggers.clear();
		m_RandomFaults.clear();
	}

	sExecutionFaultPlan CEmulatorFaultInjector::planExecution(const CEmulatorTimeline& timeline, bool bListHasMarks)
	{
		sExecutionFaultPlan plan = {};
		plan.m_StartFault = eFaultType::None;
		plan.m_RunFault = eFaultType::None;

		double dDuration = timeline.getDuration();
		uint32_t nCommandCount = timeline.getCommandCount();

		auto considerFault = [&plan, bListHasMarks](eFaultType faultType, eFaultPersistence persistence, double dTime) {
			if (faultType == eFaultType::Slowdown) {
				if (!plan.m_bSlowdown) {
					plan.m_bSlowdown = true;
					plan.m_SlowdownPersistence = persistence;
				}
				return;
			}

			if (isStartFault(faultType)) {
				if ((faultType == eFaultType::LaserNotReady) && !bListHasMarks)
					return;

				if (!plan.m_bHasStartFault) {
					plan.m_bHasStartFault = true;
					plan.m_StartFault = faultType;
					plan.m_StartPersistence = persistence;
				}
				return;
			}

			if ((!plan.m_bHasRunFault) || (dTime < plan.m_dRunFaultTime)) {
				plan.m_bHasRunFault = true;
				plan.m_RunFault = faultType;
				plan.m_RunPersistence = persistence;
				plan.m_dRunFaultTime = dTime;
			}
		};

		std::vector<sFaultTrigger> remainingTriggers;
		for (auto& trigger : m_Triggers) {
			switch (trigger.m_TriggerType) {
			case eFaultTriggerType::Immediate:
				considerFault(trigger.m_FaultType, trigger.m_Persistence, 0.0);
				break;

			case eFaultTriggerType::AtCommand:
				if (trigger.m_nCommandIndex < nCommandCount)
					considerFault(trigger.m_FaultType, trigger.m_Persistence, timeline.getCommandStartTime(trigger.m_nCommandIndex));
				break;

			case eFaultTriggerType::AtTime:
				if (trigger.m_dTimeInMicroseconds <= dDuration)
					considerFault(trigger.m_FaultType, trigger.m_Persistence, trigger.m_dTimeInMicroseconds);
				break;

			case eFaultTriggerType::AtExecution: {
				double dFraction = drawUniform();
				sFaultTrigger remainingTrigger = trigger;
				remainingTrigger.m_nExecutionCountdown--;
				if (remainingTrigger.m_nExecutionCountdown == 0)
					considerFault(trigger.m_FaultType, trigger.m_Persistence, dFraction * dDuration);
				else
					remainingTriggers.push_back(remainingTrigger);
				break;
			}

			default:
				break;
			}
		}
		m_Triggers = remainingTriggers;

		for (auto& randomFault : m_RandomFaults) {
			double dValue = drawUniform();
			double dFraction = drawUniform();
			if (dValue < randomFault.m_dProbability)
				considerFault(randomFault.m_FaultType, randomFault.m_Persistence, dFraction * dDuration);
		}

		return plan;
	}

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator
