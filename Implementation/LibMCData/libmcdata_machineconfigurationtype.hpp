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


Abstract: This is the class declaration of CMachineConfigurationType

*/


#ifndef __LIBMCDATA_MACHINECONFIGURATIONTYPE
#define __LIBMCDATA_MACHINECONFIGURATIONTYPE

#include "libmcdata_interfaces.hpp"

// Parent classes
#include "libmcdata_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "amcdata_sqlhandler.hpp"
//#include "libmcdata_machineconfigurationtypeiterator.hpp"

namespace LibMCData {
namespace Impl {
	

/*************************************************************************************************************************
 Class declaration of CMachineConfigurationType 
**************************************************************************************************************************/

class CMachineConfigurationType : public virtual IMachineConfigurationType, public virtual CBase {
private:
	
	AMCData::PSQLHandler m_pSQLHandler;
	std::string m_sUUID;
	std::string m_sName;
	std::string m_sSchemaType;
	std::string m_sTimestamp;

public:

	static CMachineConfigurationType* makeFrom(CMachineConfigurationType* pMachineConfigurationType);

	static CMachineConfigurationType* makeBySchema (AMCData::PSQLHandler pSQLHandler, const std::string& sSchemaType);

	static CMachineConfigurationType* makeByUUID (AMCData::PSQLHandler pSQLHandler, const std::string& sUUID);

	static CMachineConfigurationType* createNewConfigurationType(AMCData::PSQLHandler pSQLHandler, const std::string& sSchemaType, const std::string & sName);

	static IMachineConfigurationTypeIterator* listRegisteredConfigurationTypes(AMCData::PSQLHandler pSQLHandler);

	CMachineConfigurationType(AMCData::PSQLHandler pSQLHandler, const std::string & sUUID, const std::string& sName, const std::string& sSchemaType, const std::string& sTimestamp);

	virtual ~CMachineConfigurationType();

	AMCData::PSQLHandler GetSQLHandler();

	std::string GetUUID() override;

	std::string GetName() override;

	std::string GetSchemaType() override;

	std::string GetTimestamp() override;

	IMachineConfigurationXSD* GetLatestXSD() override;
									  
	IMachineConfigurationXSDIterator* ListXSDVersions() override;

	LibMCData_uint32 GetLatestXSDNumericVersion() override;

	IMachineConfigurationXSD* FindXSDByUUID(const std::string & sXSDUUID) override;

	IMachineConfigurationXSD* FindXSDByNumericVersion(const LibMCData_uint32 nXSDNumericVersion) override;

	IMachineConfigurationXSD* RegisterNewXSD(const std::string& sXSDString, const LibMCData_uint32 nXSDVersion) override;

	IMachineConfigurationVersion* CreateDefaultConfiguration(const std::string& sXSDUUID, const std::string& sDefaultXML, const std::string& sTimeStampUTC) override;
		
	IMachineConfigurationVersionIterator* ListAllConfigurationVersions() override;
		
	IMachineConfigurationVersionIterator* ListConfigurationVersionsForXSD(const std::string& sXSDUUID) override;

	IMachineConfigurationVersion* FindConfigurationVersionByUUID(const std::string & sVersionUUID) override;

	IMachineConfigurationVersion* GetActiveConfigurationVersion()  override;

	IMachineConfigurationVersion* GetLatestConfigurationVersion() override;

	void SetActiveConfigurationVersion(const std::string& sVersionUUID) override;
};

} // namespace Impl
} // namespace LibMCData

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDATA_MACHINECONFIGURATIONTYPE
