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


Abstract: This is the class declaration of CMachineConfigurationXSD

*/


#ifndef __LIBMCDATA_MACHINECONFIGURATIONXSD
#define __LIBMCDATA_MACHINECONFIGURATIONXSD

#include "libmcdata_interfaces.hpp"

// Parent classes
#include "libmcdata_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "amcdata_sqlhandler.hpp"


namespace LibMCData {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CMachineConfigurationXSD 
**************************************************************************************************************************/

class CMachineConfigurationXSD : public virtual IMachineConfigurationXSD, public virtual CBase {
private:

	/**
	* Put private members here.
	*/

	AMCData::PSQLHandler m_pSQLHandler;

	std::string m_sUUID;
	std::string m_sTypeUUID;
	LibMCData_uint32 m_nXSDVersion;
	std::string m_sXSDString;
	std::string m_sTimestamp;
	
protected:

	/**
	* Put protected members here.
	*/
	
public:

	/**
	* Put additional public members here. They will not be visible in the external API.
	*/

	CMachineConfigurationXSD(AMCData::PSQLHandler pSQLHandler, const std::string& sUUID, const std::string& sTypeUUID, const LibMCData_uint32 nXSDVersion, const std::string& sXSDString, const std::string& sTimestamp);

	virtual ~CMachineConfigurationXSD();

	/**
	* Public member functions to implement.
	*/

	static CMachineConfigurationXSD* makeFrom(CMachineConfigurationXSD* pMachineConfigurationVersion);

	static CMachineConfigurationXSD* createNewXSDForType(AMCData::PSQLHandler pSQLHandler, const std::string& sTypeUUID, const std::string& sXSDString, const LibMCData_uint32 nXSDNumericVersion);
		
	static CMachineConfigurationXSD* getLatestXSDForType(AMCData::PSQLHandler pSQLHandler, const std::string& sTypeUUID);

	static LibMCData_uint32 getLatestXSDNumericVersionForType(AMCData::PSQLHandler pSQLHandler, const std::string& sTypeUUID);

	static CMachineConfigurationXSD* findXSDByUUID(AMCData::PSQLHandler pSQLHandler, const std::string& sXSDUUID);

	static CMachineConfigurationXSD* findXSDByNumericVersion(AMCData::PSQLHandler pSQLHandler, LibMCData_uint32 nXSDNumericVersion);

	static IMachineConfigurationXSDIterator* listConfigurationXSDVersions(AMCData::PSQLHandler pSQLHandler, const std::string& sXSDUUID);

	AMCData::PSQLHandler GetSQLHandler();

	std::string GetUUID() override;

	std::string GetTypeUUID() override;

	LibMCData_uint32 GetXSDVersion() override;

	std::string GetXSDString() override;

	std::string GetTimestamp() override;
};

} // namespace Impl
} // namespace LibMCData

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDATA_MACHINECONFIGURATIONXSD
