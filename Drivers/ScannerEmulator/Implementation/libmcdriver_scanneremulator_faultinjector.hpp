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
This file does not depend on LibMCEnv, so that the engine can be reused by other drivers.

*/


#ifndef __LIBMCDRIVER_SCANNEREMULATOR_FAULTINJECTOR
#define __LIBMCDRIVER_SCANNEREMULATOR_FAULTINJECTOR

#include "libmcdriver_scanneremulator_timeline.hpp"

#include <vector>
#include <random>
#include <string>
#include <cstdint>

namespace LibMCDriver_ScannerEmulator {
namespace Impl {

	enum class eFaultTriggerType : uint8_t {
		Immediate = 0,
		AtCommand = 1,
		AtTime = 2,
		AtExecution = 3
	};

	struct sFaultTrigger {
		eFaultType m_FaultType;
		eFaultPersistence m_Persistence;
		eFaultTriggerType m_TriggerType;
		uint32_t m_nCommandIndex;
		double m_dTimeInMicroseconds;
		uint32_t m_nExecutionCountdown;
	};

	struct sRandomFault {
		eFaultType m_FaultType;
		eFaultPersistence m_Persistence;
		double m_dProbability;
	};

	struct sExecutionFaultPlan {
		bool m_bHasStartFault;
		eFaultType m_StartFault;
		eFaultPersistence m_StartPersistence;

		bool m_bHasRunFault;
		eFaultType m_RunFault;
		eFaultPersistence m_RunPersistence;
		double m_dRunFaultTime;

		bool m_bSlowdown;
		eFaultPersistence m_SlowdownPersistence;
	};

	class CEmulatorFaultInjector {
	private:
		std::mt19937_64 m_RandomGenerator;
		std::vector<sFaultTrigger> m_Triggers;
		std::vector<sRandomFault> m_RandomFaults;

		double drawUniform();

	public:
		CEmulatorFaultInjector();

		static void validateFaultType(eFaultType faultType);
		static void validatePersistence(eFaultPersistence persistence);
		static bool isStartFault(eFaultType faultType);
		static bool isRunFault(eFaultType faultType);
		static std::string faultTypeToString(eFaultType faultType);
		static eFaultType stringToFaultType(const std::string& sFaultType);
		static void throwFaultError(eFaultType faultType, const std::string& sScannerName);

		void setSeed(uint64_t nSeed);

		void addTrigger(const sFaultTrigger& trigger);
		void setRandomFault(eFaultType faultType, eFaultPersistence persistence, double dProbability);
		void clear();

		// Consumes the triggers that apply to the next execution and draws the random faults.
		sExecutionFaultPlan planExecution(const CEmulatorTimeline& timeline, bool bListHasMarks);
	};

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator

#endif // __LIBMCDRIVER_SCANNEREMULATOR_FAULTINJECTOR
