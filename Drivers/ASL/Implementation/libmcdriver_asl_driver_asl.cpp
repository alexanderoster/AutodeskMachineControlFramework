/*++

Copyright (C) 2024 ASL Inc.

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


Abstract: This is a stub class definition of CDriver_ASL

*/

#include "libmcdriver_asl_driver_asl.hpp"
#include "libmcdriver_asl_interfaceexception.hpp"
#include "libmcdriver_asl_drivercontext.hpp"

// Include custom headers here.
using namespace LibMCDriver_ASL::Impl;

/*************************************************************************************************************************
 Class definition of CDriver_ASL 
**************************************************************************************************************************/

CDriver_ASL::CDriver_ASL(const std::string& sName, LibMCEnv::PDriverEnvironment pDriverEnvironment)
	: m_sName(sName), m_pDriverEnvironment(pDriverEnvironment), m_bSimulationMode(false)
{
	if (pDriverEnvironment.get() == nullptr)
		throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_INVALIDPARAM);

}

CDriver_ASL::~CDriver_ASL()
{

}

void CDriver_ASL::SetToSimulationMode()
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}

bool CDriver_ASL::IsSimulationMode()
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}

IDriverContext * CDriver_ASL::Connect(const std::string & sIdentifier, const std::string & sCOMPort)
{
	std::unique_ptr<serial::Serial> pConnection;

	pConnection.reset(new serial::Serial(sCOMPort, 1000000));

	if (!pConnection->isOpen()) {
		pConnection.reset();
		throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_BUFFERTOOSMALL);
	}

	return new CDriverContext(sIdentifier, sCOMPort, pConnection.release(), m_pDriverEnvironment);
}

bool CDriver_ASL::ContextExists(const std::string & sIdentifier)
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}

IDriverContext * CDriver_ASL::FindContext(const std::string & sIdentifier)
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}


void CDriver_ASL::Configure(const std::string& sConfigurationString)
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}

std::string CDriver_ASL::GetName()
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}

std::string CDriver_ASL::GetType()
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}

void CDriver_ASL::GetVersion(LibMCDriver_ASL_uint32& nMajor, LibMCDriver_ASL_uint32& nMinor, LibMCDriver_ASL_uint32& nMicro, std::string& sBuild)
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}

void CDriver_ASL::QueryParameters()
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}

void CDriver_ASL::QueryParametersEx(LibMCEnv::PDriverStatusUpdateSession pDriverUpdateInstance)
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}
