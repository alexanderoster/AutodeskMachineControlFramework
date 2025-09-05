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


Abstract: This is the class declaration of CWorkingFileProcess

*/


#ifndef __LIBMCENV_WORKINGFILEPROCESS
#define __LIBMCENV_WORKINGFILEPROCESS

#include "libmcenv_interfaces.hpp"
#include <map>

// Parent classes
#include "libmcenv_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "libmcenv_workingdirectory.hpp"
#include "Common/common_chrono.hpp"
#include "amc_logger.hpp"

#include "amc_processcontroller.hpp"
#include "amc_processdirectory.hpp"

namespace LibMCEnv {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CWorkingFileProcess 
**************************************************************************************************************************/

class CWorkingFileProcess : public virtual IWorkingFileProcess, public virtual CBase {
private:

	AMC::WProcessDirectory m_pExecutableDirectory;
	AMC::WProcessDirectory m_pWorkingDirectory;

	AMC::PProcessController m_pProcessController;

public:


	CWorkingFileProcess(const std::string & sAbsoluteExecutableName, AMC::WProcessDirectory pExecutableDirectory);

	virtual ~CWorkingFileProcess();

	LibMCEnv::eWorkingFileProcessStatus GetStatus() override;

	IDateTimeDifference * GetRunTime() override;

	LibMCEnv_uint64 GetRunTimeInMilliseconds() override;

	void SetWorkingDirectory(IWorkingDirectory* pDirectory) override;

	void AddEnvironmentVariable(const std::string & sVariableName, const std::string & sValue) override;

	bool EnvironmentVariableExists(const std::string & sVariableName) override;

	void RemoveEnvironmentVariable(const std::string & sVariableName) override;

	void ClearEnvironmentVariables() override;

	uint32_t GetEnvironmentVariableCount() override;

	void GetEnvironmentVariableByIndex(const LibMCEnv_uint32 nVariableIndex, std::string & sVariableName, std::string & sValue) override;

	void StartProcess(const std::string & sArgumentString, LibMCEnv_uint32 nTimeOut) override;

	void TerminateProcess() override;

	void SetVerboseLogging(const bool bVerboseLogging) override;



};

} // namespace Impl
} // namespace LibMCEnv

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCENV_WORKINGFILEPROCESS
