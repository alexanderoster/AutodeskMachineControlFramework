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


Abstract: This is a stub class definition of CDriver_OpenFOAM

*/

#include "libmcdriver_openfoam_driver_openfoam.hpp"
#include "libmcdriver_openfoam_interfaceexception.hpp"
#include "libmcdriver_openfoam_openfoamcase.hpp"

// Include custom headers here.
#define __STRINGIZE(x) #x
#define __STRINGIZE_VALUE_OF(x) __STRINGIZE(x)



using namespace LibMCDriver_OpenFOAM::Impl;

/*************************************************************************************************************************
 Class definition of CDriver_OpenFOAM 
**************************************************************************************************************************/


CDriver_OpenFOAM::CDriver_OpenFOAM(LibMCEnv::PDriverEnvironment pDriverEnvironment, const std::string& sName)
    : m_pDriverEnvironment (pDriverEnvironment), m_sName (sName)
{
    if (pDriverEnvironment.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);
}

CDriver_OpenFOAM::~CDriver_OpenFOAM()
{

}

void CDriver_OpenFOAM::Configure(const std::string& sConfigurationString)
{

}

std::string CDriver_OpenFOAM::GetName()
{
    return m_sName;
}

std::string CDriver_OpenFOAM::GetType()
{
    return getTypeString();
}

std::string CDriver_OpenFOAM::getTypeString ()
{
    return "openfoam-latest";
}

void CDriver_OpenFOAM::GetVersion(LibMCDriver_OpenFOAM_uint32& nMajor, LibMCDriver_OpenFOAM_uint32& nMinor, LibMCDriver_OpenFOAM_uint32& nMicro, std::string& sBuild)
{
    nMajor = LIBMCDRIVER_OPENFOAM_VERSION_MAJOR;
    nMinor = LIBMCDRIVER_OPENFOAM_VERSION_MINOR;
    nMicro = LIBMCDRIVER_OPENFOAM_VERSION_MICRO;
    sBuild = __STRINGIZE_VALUE_OF(__GITHASH);
}

void CDriver_OpenFOAM::QueryParameters()
{

}

void CDriver_OpenFOAM::QueryParametersEx(LibMCEnv::PDriverStatusUpdateSession pDriverUpdateInstance)
{

}


IOpenFOAMCase* CDriver_OpenFOAM::CreateCase(const std::string& sIdentifier)
{
    if (!COpenFOAMCaseInstance::checkIdentifier(sIdentifier))
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDCASEIDENTIFIER, sIdentifier);

    auto it = m_CaseInstanceMap.find(sIdentifier);
    if (it != m_CaseInstanceMap.end())
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEIDENTIFIERALREADYEXISTS, sIdentifier);

    auto pInstance = std::make_shared<COpenFOAMCaseInstance>(sIdentifier, m_pDriverEnvironment);
    m_CaseInstanceMap.insert(std::make_pair (pInstance->getIdentifier (), pInstance));
    m_CaseInstanceList.push_back(pInstance);

    return new COpenFOAMCase (pInstance);
}

bool CDriver_OpenFOAM::CaseExists(const std::string& sIdentifier)
{
    if (!COpenFOAMCaseInstance::checkIdentifier(sIdentifier))
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDCASEIDENTIFIER, sIdentifier);

    auto it = m_CaseInstanceMap.find(sIdentifier);
    if (it == m_CaseInstanceMap.end())
        return false;

    return true;
}

IOpenFOAMCase* CDriver_OpenFOAM::FindCase(const std::string& sIdentifier)
{
    if (!COpenFOAMCaseInstance::checkIdentifier (sIdentifier) )
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDCASEIDENTIFIER, sIdentifier);

	auto it = m_CaseInstanceMap.find(sIdentifier);
	if (it == m_CaseInstanceMap.end()) 
		throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEIDENTIFIERNOTFOUND, sIdentifier);

	return new COpenFOAMCase(it->second);
}

void CDriver_OpenFOAM::ReleaseCase(const std::string& sIdentifier)
{

    if (!COpenFOAMCaseInstance::checkIdentifier(sIdentifier))
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDCASEIDENTIFIER, sIdentifier);

    auto it = m_CaseInstanceMap.find(sIdentifier);
    if (it != m_CaseInstanceMap.end()) {
        auto pInstance = it->second;
		m_CaseInstanceList.erase(std::remove(m_CaseInstanceList.begin(), m_CaseInstanceList.end(), pInstance), m_CaseInstanceList.end());
		m_CaseInstanceMap.erase(it);

		// Release the instance
        pInstance->releaseCase();
		pInstance.reset();
    }
}


