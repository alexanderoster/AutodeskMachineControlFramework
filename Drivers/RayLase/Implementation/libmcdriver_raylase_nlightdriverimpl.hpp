/*++

Copyright (C) 2020 Autodesk Inc.

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


Abstract: This is the class declaration of CNLightDriverBoard

*/


#ifndef __LIBMCDRIVER_RAYLASE_NLIGHTDRIVERIMPL
#define __LIBMCDRIVER_RAYLASE_NLIGHTDRIVERIMPL

#include "libmcdriver_raylase_interfaces.hpp"
#include "libmcdriver_raylase_sdk.hpp"

#include <map>

#define RAYLASE_NLIGHT_MAXLASERMODE 15
#define RAYLASE_NLIGHT_MAXCHANGEMODEDELAY 1000000
#define RAYLASE_NLIGHT_SPISTATUSREQUESTCOMMAND 0x03000000UL

#define RAYLASE_NLIGHT_BPPREADYBIT 0
#define RAYLASE_NLIGHT_EXTCONTROLREADYBIT 1
#define RAYLASE_NLIGHT_EXTWATERFLOWBIT 2
#define RAYLASE_NLIGHT_READYBIT 3
#define RAYLASE_NLIGHT_ERRORBIT 4
#define RAYLASE_NLIGHT_EMISSIONBIT 5
#define RAYLASE_NLIGHT_FIRMWAREREADYBIT 6

#define RAYLASE_MINLASERPOWER 0.1
#define RAYLASE_MAXLASERPOWER 100000.0


namespace LibMCDriver_Raylase {
namespace Impl {

	enum class eNlightDriverBoardIOPins : uint32_t {
		PRO_START = 1 << 7,
		PRO_B1 = 1 << 8,
		PRO_B2 = 1 << 9,
		PRO_B3 = 1 << 10,
		PRO_B4 = 1 << 11,
		PRO_B5 = 1 << 12,
		PRO_B6 = 1 << 13,
		PRO_B7 = 1 << 14,
		SYSTEM_ON = 1 << 15,
		ENABLE_PROFILE = 1 << 16,
		ARM_LASER = 1 << 18,
		CLEAR_ERROR = 1 << 19,
		ENABLE_AIMING_LASER = 1 << 20,
		ENABLE_EXTERNAL_CONTROL = 1 << 21,
		ENABLE_24V = 1 << 22,
		GATE_IN = 1 << 23
	};

/*************************************************************************************************************************
 Class declaration of CNLightDriverBoard 
**************************************************************************************************************************/

class CNLightDriverImpl {
private:
	PRaylaseSDK m_pSDK;
	LibMCEnv::PDriverEnvironment m_pDriverEnvironment;

	bool m_AutomaticLaserModesEnabled;

	uint32_t m_nModeChangeSignalDelayInMicroseconds;
	uint32_t m_nModeChangeApplyDelayInMicroseconds;

	uint32_t m_nLaserReactionDelayInMilliseconds;
	uint32_t m_nLaserReactionRetries;

	uint32_t m_nSPITimeoutInMilliseconds;
	uint32_t m_nSPIModuleIndex;
	uint32_t m_nSPIRetryCount;

	// Keeps a list of max power values for specific laser nodes (in Watts)
	std::map<uint32_t, double> m_LaserModeMaxPowerOverrides;

public:

    CNLightDriverImpl (PRaylaseSDK pSDK, LibMCEnv::PDriverEnvironment pDriverEnvironment);

    virtual ~CNLightDriverImpl();
	
	void initializeNLightLaser(rlHandle cardHandle);

	void disableNLightLaser(rlHandle cardHandle);

	void clearNLightError(rlHandle cardHandle);

	uint32_t retrieveSPIStatus(rlHandle cardHandle);

	void setNLightLaserMode(rlHandle cardHandle, uint32_t nLaserMode);

	void addNLightLaserModeToList(rlListHandle listHandle, uint32_t nLaserMode);

	bool automaticLaserModesAreEnabled();

	void setAutomaticLaserModesEnable(bool bValue);

	void setModeChangeDelays(uint32_t nModeChangeSignalDelayInMicroseconds, uint32_t nModeChangeApplyDelayInMicroseconds);

	uint32_t getModeChangeSignalDelay();

	uint32_t getModeChangeApplyDelay();

	uint32_t getMaxAFXMode();

	bool laserHasError(rlHandle cardHandle);

	bool laserIsReady(rlHandle cardHandle);

	bool laserExternalControlIsReady(rlHandle cardHandle);

	bool laserIsEmission(rlHandle cardHandle);

	bool laserIsFirmwareReady(rlHandle cardHandle);

	bool laserIsWaterFlow(rlHandle cardHandle);

	bool laserBPPIsReady(rlHandle cardHandle);

	void setLaserModeMaxPowerOverride(const uint32_t nLaserMode, const double dMaxPowerInWatts);

	bool hasLaserModeMaxPowerOverride(const uint32_t nLaserMode);

	double getLaserModeMaxPowerOverride(const uint32_t nLaserMode);

	void clearLaserModeMaxPowerOverride(const uint32_t nLaserMode);

	void clearAllLaserModeMaxPowerOverrides();

};

typedef std::shared_ptr<CNLightDriverImpl> PNLightDriverImpl;

} // namespace Impl
} // namespace LibMCDriver_Raylase

#endif // __LIBMCDRIVER_RAYLASE_NLIGHTDRIVERIMPL
