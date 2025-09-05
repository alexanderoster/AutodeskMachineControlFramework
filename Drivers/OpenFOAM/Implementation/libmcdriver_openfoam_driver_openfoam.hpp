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


Abstract: This is the class declaration of CDriver_OpenFOAM

*/


#ifndef __LIBMCDRIVER_OPENFOAM_DRIVER_OPENFOAM
#define __LIBMCDRIVER_OPENFOAM_DRIVER_OPENFOAM

#include "libmcdriver_openfoam_interfaces.hpp"

// Parent classes
#include "libmcdriver_openfoam_driver.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "libmcdriver_openfoam_caseinstance.hpp"
#include <map>
#include <vector>

namespace LibMCDriver_OpenFOAM {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CDriver_OpenFOAM 
**************************************************************************************************************************/

class CDriver_OpenFOAM : public virtual IDriver_OpenFOAM, public virtual CDriver {
private:

    LibMCEnv::PDriverEnvironment m_pDriverEnvironment;
    std::map<std::string, POpenFOAMCaseInstance> m_CaseInstanceMap;
    std::vector<POpenFOAMCaseInstance> m_CaseInstanceList;


    std::string m_sName;

public:

    CDriver_OpenFOAM (LibMCEnv::PDriverEnvironment pDriverEnvironment, const std::string & sName);

    virtual ~CDriver_OpenFOAM();   

    void Configure(const std::string& sConfigurationString) override;

    std::string GetName() override;

    static std::string getTypeString();

    std::string GetType() override;

    void GetVersion(LibMCDriver_OpenFOAM_uint32& nMajor, LibMCDriver_OpenFOAM_uint32& nMinor, LibMCDriver_OpenFOAM_uint32& nMicro, std::string& sBuild) override;

    void QueryParameters() override;

    void QueryParametersEx(LibMCEnv::PDriverStatusUpdateSession pDriverUpdateInstance) override;

    IOpenFOAMCase* CreateCase(const std::string& sIdentifier) override;

    bool CaseExists(const std::string& sIdentifier) override;

    IOpenFOAMCase* FindCase(const std::string& sIdentifier) override;

    void ReleaseCase(const std::string& sIdentifier) override;

};

} // namespace Impl
} // namespace LibMCDriver_OpenFOAM

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDRIVER_OPENFOAM_DRIVER_OPENFOAM
