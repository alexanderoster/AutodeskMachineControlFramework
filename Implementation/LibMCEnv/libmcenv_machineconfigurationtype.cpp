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


Abstract: This is a stub class definition of CMachineConfigurationType

*/

#include "libmcenv_machineconfigurationtype.hpp"
#include "libmcenv_interfaceexception.hpp"

// Include custom headers here.


using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CMachineConfigurationType 
**************************************************************************************************************************/
CMachineConfigurationType::CMachineConfigurationType(LibMCData::PMachineConfigurationType pMachineConfigurationType)
	: m_pMachineConfigurationType (pMachineConfigurationType)
{
	if (pMachineConfigurationType.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);	
}

CMachineConfigurationType::~CMachineConfigurationType()
{

}

std::string CMachineConfigurationType::GetSchemaType()
{
	return m_pMachineConfigurationType->GetSchemaType();
}

std::string CMachineConfigurationType::GetTypeName()
{
	return m_pMachineConfigurationType->GetName();
}

std::string CMachineConfigurationType::GetTypeUUID()
{
	return m_pMachineConfigurationType->GetUUID();
}

LibMCEnv_uint32 CMachineConfigurationType::GetLatestXSDVersion()
{
	return m_pMachineConfigurationType->GetLatestXSDVersion();
}

void CMachineConfigurationType::RegisterConfigurationXSD(const std::string & sXSDString, const LibMCEnv_uint32 nXSDVersion, const std::string & sDefaultConfigurationXML)
{
	throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_NOTIMPLEMENTED);
}

void CMachineConfigurationType::RegisterConfigurationXSDFromResource(const std::string & sXSDResourceName, const LibMCEnv_uint32 nXSDVersion, const std::string & sDefaultConfigurationResourceName)
{
	throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_NOTIMPLEMENTED);
}

IMachineConfigurationVersion * CMachineConfigurationType::GetLatestConfiguration()
{
	throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_NOTIMPLEMENTED);
}

IMachineConfigurationVersion * CMachineConfigurationType::GetActiveConfiguration(const bool bFallBackToDefault)
{
	throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_NOTIMPLEMENTED);
}

