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


Abstract: This is a stub class definition of CNLightDriverBoard

*/

#include "libmcdriver_raylase_nlightdriverboard.hpp"
#include "libmcdriver_raylase_interfaceexception.hpp"

#include "libmcdriver_raylase_apifield.hpp"




using namespace LibMCDriver_Raylase::Impl;


CNLightDriverBoard::CNLightDriverBoard(PRaylaseCardImpl pCardImpl)
    : m_pCardImpl (pCardImpl)
{
    if (pCardImpl.get() == nullptr)
        throw ELibMCDriver_RaylaseInterfaceException(LIBMCDRIVER_RAYLASE_ERROR_INVALIDPARAM);

}

CNLightDriverBoard::~CNLightDriverBoard()
{

}


void CNLightDriverBoard::InitializeLaser(const bool bEnableAutomaticLaserModeSwitching)
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    pNLightDriverImpl->initializeNLightLaser(m_pCardImpl->getHandle ());
    pNLightDriverImpl->setAutomaticLaserModesEnable(bEnableAutomaticLaserModeSwitching);
    
}

void CNLightDriverBoard::DisableLaser()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    pNLightDriverImpl->disableNLightLaser(m_pCardImpl->getHandle());
}

bool CNLightDriverBoard::AutomaticLaserModeSwitchingIsEnabled()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    return pNLightDriverImpl->automaticLaserModesAreEnabled();
}

void CNLightDriverBoard::EnableAutomaticLaserModeSwitching()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    pNLightDriverImpl->setAutomaticLaserModesEnable(true);
}

void CNLightDriverBoard::DisableAutomaticLaserModeSwitching()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    pNLightDriverImpl->setAutomaticLaserModesEnable(false);

}

void CNLightDriverBoard::SetLaserModeMaxPowerOverride(const LibMCDriver_Raylase_uint32 nLaserMode, const LibMCDriver_Raylase_double dMaxPowerInWatts)
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    pNLightDriverImpl->setLaserModeMaxPowerOverride(nLaserMode, dMaxPowerInWatts);

}

LibMCDriver_Raylase_double CNLightDriverBoard::GetLaserModeMaxPowerOverride(const LibMCDriver_Raylase_uint32 nLaserMode)
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    if (pNLightDriverImpl->hasLaserModeMaxPowerOverride (nLaserMode))
        return pNLightDriverImpl->getLaserModeMaxPowerOverride(nLaserMode);

    return m_pCardImpl->getMaxLaserPowerInWatts();
}

void CNLightDriverBoard::ClearLaserModeMaxPowerOverride(const LibMCDriver_Raylase_uint32 nLaserMode)
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    pNLightDriverImpl->clearLaserModeMaxPowerOverride(nLaserMode);
}

void CNLightDriverBoard::ClearAllLaserModeMaxPowerOverrides()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    pNLightDriverImpl->clearAllLaserModeMaxPowerOverrides();
}


void CNLightDriverBoard::SetLaserMode(const LibMCDriver_Raylase_uint32 nLaserMode)
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    pNLightDriverImpl->setNLightLaserMode (m_pCardImpl->getHandle(), nLaserMode);
}


void CNLightDriverBoard::ClearError()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    pNLightDriverImpl->clearNLightError(m_pCardImpl->getHandle());
}

LibMCDriver_Raylase_uint32 CNLightDriverBoard::GetRawDeviceState()
{
    return 0;
}

bool CNLightDriverBoard::HasError()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    return pNLightDriverImpl->laserHasError(m_pCardImpl->getHandle());
}

bool CNLightDriverBoard::IsReady()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    return pNLightDriverImpl->laserIsReady(m_pCardImpl->getHandle());
}

bool CNLightDriverBoard::ExternalControlIsReady()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    return pNLightDriverImpl->laserExternalControlIsReady(m_pCardImpl->getHandle());
}

bool CNLightDriverBoard::IsEmission()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    return pNLightDriverImpl->laserIsEmission(m_pCardImpl->getHandle());
}

bool CNLightDriverBoard::IsFirmwareReady()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    return pNLightDriverImpl->laserIsFirmwareReady(m_pCardImpl->getHandle());
}

bool CNLightDriverBoard::IsWaterFlow()
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    return pNLightDriverImpl->laserIsWaterFlow(m_pCardImpl->getHandle());
}

void CNLightDriverBoard::SetModeChangeDelays(const LibMCDriver_Raylase_uint32 nModeChangeSignalDelayInMicroseconds, const LibMCDriver_Raylase_uint32 nModeChangeApplyDelayInMicroseconds)
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    pNLightDriverImpl->setModeChangeDelays(nModeChangeSignalDelayInMicroseconds, nModeChangeApplyDelayInMicroseconds);
}

void CNLightDriverBoard::GetModeChangeDelays(LibMCDriver_Raylase_uint32& nModeChangeSignalDelayInMicroseconds, LibMCDriver_Raylase_uint32& nModeChangeApplyDelayInMicroseconds)
{
    auto pNLightDriverImpl = m_pCardImpl->getNlightImplementation();
    nModeChangeSignalDelayInMicroseconds = pNLightDriverImpl->getModeChangeSignalDelay();
    nModeChangeApplyDelayInMicroseconds = pNLightDriverImpl->getModeChangeApplyDelay();

}


