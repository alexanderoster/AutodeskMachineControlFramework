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


Abstract: This is a stub class definition of CWorkingFileProcess

*/

#include "libmcenv_workingfileprocess.hpp"
#include "libmcenv_interfaceexception.hpp"
#include "libmcenv_datetimedifference.hpp"

// Include custom headers here.
#include "Common/common_utils.hpp"

#include <vector>
#include <algorithm>
#include <functional>
#include <thread>
#include <array>

using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CWorkingFileProcess 
**************************************************************************************************************************/



CWorkingFileProcess::CWorkingFileProcess(const std::string& sAbsoluteExecutableName, AMC::WProcessDirectory pExecutableDirectory)
	: 
	m_pExecutableDirectory(pExecutableDirectory),
	m_pWorkingDirectory(pExecutableDirectory)
{
	auto pProcessDirectoryInstance = pExecutableDirectory.lock();
	if (pProcessDirectoryInstance.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

	m_pProcessController = std::make_shared<AMC::CProcessController> (sAbsoluteExecutableName, pExecutableDirectory, pProcessDirectoryInstance->getGlobalChrono (), pProcessDirectoryInstance->getLogger ());
}

CWorkingFileProcess::~CWorkingFileProcess()
{
	m_pProcessController = nullptr;
}


LibMCEnv::eWorkingFileProcessStatus CWorkingFileProcess::GetStatus()
{

	auto processStatus = m_pProcessController->getStatus();
	switch (processStatus) {
		case AMC::eProcessControllerStatus::ProcessNotStarted:
			return LibMCEnv::eWorkingFileProcessStatus::ProcessNotStarted;
		case AMC::eProcessControllerStatus::ProcessStarted:
			return LibMCEnv::eWorkingFileProcessStatus::ProcessStarted;
		case AMC::eProcessControllerStatus::ProcessFinished:
			return LibMCEnv::eWorkingFileProcessStatus::ProcessFinished;
		case AMC::eProcessControllerStatus::ProcessTerminated:
			return LibMCEnv::eWorkingFileProcessStatus::ProcessTerminated;
		case AMC::eProcessControllerStatus::ProcessTimedOut:
			return LibMCEnv::eWorkingFileProcessStatus::ProcessTimedOut;			
		default:
			return LibMCEnv::eWorkingFileProcessStatus::Unknown;
	}
}

IDateTimeDifference * CWorkingFileProcess::GetRunTime()
{

	uint64_t nRunTimeInMicroseconds = m_pProcessController->getRunTimeInMicroseconds();
	return new CDateTimeDifference (nRunTimeInMicroseconds);
}

LibMCEnv_uint64 CWorkingFileProcess::GetRunTimeInMilliseconds()
{
	return m_pProcessController->getRunTimeInMilliseconds();
}

void CWorkingFileProcess::SetWorkingDirectory(IWorkingDirectory* pDirectory)
{
	if (pDirectory == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	auto pDirectoryInstance = dynamic_cast<CWorkingDirectory*> (pDirectory);
	if (pDirectoryInstance == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
	
	m_pWorkingDirectory = pDirectoryInstance->getProcessDirectory();
	m_pProcessController->setWorkingDirectory(m_pWorkingDirectory);

}

void CWorkingFileProcess::AddEnvironmentVariable(const std::string & sVariableName, const std::string & sValue)
{
	m_pProcessController->addEnvironmentVariable(sVariableName, sValue);
}

bool CWorkingFileProcess::EnvironmentVariableExists(const std::string & sVariableName)
{
	return m_pProcessController->environmentVariableExists(sVariableName);
}

void CWorkingFileProcess::RemoveEnvironmentVariable(const std::string & sVariableName)
{
	m_pProcessController->removeEnvironmentVariable(sVariableName);
	
}

uint32_t CWorkingFileProcess::GetEnvironmentVariableCount()
{
	return m_pProcessController->getEnvironmentVariableCount();
}

void CWorkingFileProcess::GetEnvironmentVariableByIndex(const LibMCEnv_uint32 nVariableIndex, std::string & sVariableName, std::string & sValue)
{
	m_pProcessController->getEnvironmentVariableByIndex(nVariableIndex, sVariableName, sValue);

}

void CWorkingFileProcess::ClearEnvironmentVariables()
{
	m_pProcessController->clearEnvironmentVariables();
}


void CWorkingFileProcess::StartProcess(const std::string & sArgumentString, LibMCEnv_uint32 nTimeOut)
{
	m_pProcessController->startProcess(sArgumentString, nTimeOut);
}

void CWorkingFileProcess::TerminateProcess()
{
	m_pProcessController->terminateProcess();
}

void CWorkingFileProcess::SetVerboseLogging(const bool bVerboseLogging)
{
	m_pProcessController->setVerboseLogging(bVerboseLogging);
}


