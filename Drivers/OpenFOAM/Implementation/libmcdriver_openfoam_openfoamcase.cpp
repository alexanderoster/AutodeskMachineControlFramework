/*++

Copyright (C) 2025 Autodesk Inc.

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


Abstract: This is a stub class definition of COpenFOAMCase

*/

#include "libmcdriver_openfoam_openfoamcase.hpp"
#include "libmcdriver_openfoam_interfaceexception.hpp"

// Include custom headers here.


using namespace LibMCDriver_OpenFOAM::Impl;

/*************************************************************************************************************************
 Class definition of COpenFOAMCase 
**************************************************************************************************************************/

COpenFOAMCase::COpenFOAMCase(POpenFOAMCaseInstance pInstance)
	: m_pInstance (pInstance)
{
	if (pInstance.get () == nullptr)
		throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);

}

COpenFOAMCase::~COpenFOAMCase()
{

}


std::string COpenFOAMCase::GetIdentifier()
{
	return m_pInstance->getIdentifier();
}

LibMCDriver_OpenFOAM::eCaseStatus COpenFOAMCase::GetStatus()
{
	return m_pInstance->getStatus();
}


bool COpenFOAMCase::CanRun()
{
	return m_pInstance->canRun();
}

bool COpenFOAMCase::IsRunning()
{
	return (m_pInstance->getStatus() == eCaseStatus::Running);
}

bool COpenFOAMCase::IsSuccessful()
{
	return (m_pInstance->getStatus() == eCaseStatus::Success);
}

bool COpenFOAMCase::IsInError()
{
	return (m_pInstance->getStatus() == eCaseStatus::Failure);
}

void COpenFOAMCase::SetBuild(LibMCEnv::PBuild pBuild)
{
	m_pInstance->setBuild(pBuild);
}

bool COpenFOAMCase::HasBuild()
{
	return m_pInstance->hasBuild();
}

std::string COpenFOAMCase::GetBuildUUID()
{
	return m_pInstance->getBuildUUID();
}

void COpenFOAMCase::StartComputation()
{
	m_pInstance->startComputation();
}

void COpenFOAMCase::CreateOpenFOAMInputDeck(LibMCEnv::PZIPStreamWriter pZIPStream)
{
	m_pInstance->createOpenFOAMInputDeck(pZIPStream);

}

