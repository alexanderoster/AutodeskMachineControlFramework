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
	m_bSimulationMode = true;
}

bool CDriver_ASL::IsSimulationMode()
{
	return m_bSimulationMode;
}

IDriverContext * CDriver_ASL::Connect(const std::string & sIdentifier, const std::string & sCOMPort)
{
	if (sIdentifier.empty ())
		throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_EMPTYDRIVERCONTEXTIDENTIFIER);

	if (ContextExists (sIdentifier))
		throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_DUPLICATEDRIVERCONTEXTIDENTIFIER, "Duplicate driver context identifier: " + sIdentifier);

	auto pDriverContextInstance = std::make_shared<CDriverContextInstance> (sIdentifier, sCOMPort, m_pDriverEnvironment);

	m_InstanceMap.insert(std::make_pair (sIdentifier, pDriverContextInstance));

	return new CDriverContext(pDriverContextInstance);
}

bool CDriver_ASL::ContextExists(const std::string & sIdentifier)
{
	auto iIter = m_InstanceMap.find(sIdentifier);
	return (iIter != m_InstanceMap.end());
}

void CDriver_ASL::ClearContexts() 
{
	m_InstanceMap.clear();
}


IDriverContext * CDriver_ASL::FindContext(const std::string & sIdentifier)
{
	if (!CDriver_ASL::ContextExists(sIdentifier))
		throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_EMPTYDRIVERCONTEXTIDENTIFIER);
		
	return new CDriverContext(m_InstanceMap.find(sIdentifier)->second);
}


void CDriver_ASL::Configure(const std::string& sConfigurationString)
{
	m_pDriverEnvironment->RegisterDoubleParameter("0_settemp", "Set temperature of head one", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("1_settemp", "Set temperature of head two", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("2_settemp", "Set temperature of head three", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("3_settemp", "Set temperature of head four", 0.0);

	m_pDriverEnvironment->RegisterDoubleParameter("0_curtemp", "Actual temperature of head one", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("1_curtemp", "Actual temperature of head two", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("2_curtemp", "Actual temperature of head three", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("3_curtemp", "Actual temperature of head four", 0.0);

	m_pDriverEnvironment->RegisterDoubleParameter("0_printcnts", "Print counts of head one", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("1_printcnts", "Print counts of head two", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("2_printcnts", "Print counts of head three", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("3_printcnts", "Print counts of head four", 0.0);

	m_pDriverEnvironment->RegisterDoubleParameter("0_state", "State of head one", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("1_state", "State of head two", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("2_state", "State of head three", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("3_state", "State of head four", 0.0);

	m_pDriverEnvironment->RegisterDoubleParameter("0_progress", "Progress of image in head one", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("1_progress", "Progress of image in head two", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("2_progress", "Progress of image in head three", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("3_progress", "Progress of image in head four", 0.0);

	m_pDriverEnvironment->RegisterBoolParameter("0_hasdata", "Flag to reflect if the board has data in head one", false);
	m_pDriverEnvironment->RegisterBoolParameter("1_hasdata", "Flag to reflect if the board has data in head two", false);
	m_pDriverEnvironment->RegisterBoolParameter("2_hasdata", "Flag to reflect if the board has data in head three", false);
	m_pDriverEnvironment->RegisterBoolParameter("3_hasdata", "Flag to reflect if the board has data in head four", false);

	m_pDriverEnvironment->RegisterBoolParameter("0_verified", "Image verified for head one", false);
	m_pDriverEnvironment->RegisterBoolParameter("1_verified", "Image verified for head two", false);
	m_pDriverEnvironment->RegisterBoolParameter("2_verified", "Image verified for head three", false);
	m_pDriverEnvironment->RegisterBoolParameter("3_verified", "Image verified for head four", false);

	m_pDriverEnvironment->RegisterDoubleParameter("frequency", "The set frequency of the driver board", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("currentlocation", "The current location read by the driver board", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("startlocation", "The index for start of print", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter("timeon", "A counter on the board to show uptime", 0.0);


}

std::string CDriver_ASL::GetName()
{
	return m_sName;
}

std::string CDriver_ASL::getTypeStatic()
{
	return "x128-serial-1.0";
}

std::string CDriver_ASL::GetType()
{
	return getTypeStatic ();
}

void CDriver_ASL::GetVersion(LibMCDriver_ASL_uint32& nMajor, LibMCDriver_ASL_uint32& nMinor, LibMCDriver_ASL_uint32& nMicro, std::string& sBuild)
{
	throw ELibMCDriver_ASLInterfaceException(LIBMCDRIVER_ASL_ERROR_NOTIMPLEMENTED);
}

void CDriver_ASL::QueryParameters()
{
	QueryParametersEx(m_pDriverEnvironment->CreateStatusUpdateSession());
}

void CDriver_ASL::QueryParametersEx(LibMCEnv::PDriverStatusUpdateSession pDriverUpdateInstance)
{

}

