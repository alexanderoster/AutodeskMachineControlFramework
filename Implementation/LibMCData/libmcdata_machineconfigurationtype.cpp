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

#include "libmcdata_machineconfigurationtype.hpp"
#include "libmcdata_interfaceexception.hpp"

// Include custom headers here.
#include "common_utils.hpp"

using namespace LibMCData::Impl;

/*************************************************************************************************************************
 Class definition of CMachineConfigurationType 
**************************************************************************************************************************/

CMachineConfigurationType* CMachineConfigurationType::makeBySchema(AMCData::PSQLHandler pSQLHandler, const std::string& sSchemaType)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

	auto pStatement = pSQLHandler->prepareStatement("SELECT uuid, name, schematype, timestamp FROM machineconfiguration_types WHERE schematype = ?");
	pStatement->setString(1, sSchemaType);
	if (pStatement->nextRow()) {
		std::string sDBUUID = pStatement->getColumnString(1);
		std::string sDBName = pStatement->getColumnString(2);
		std::string sDBSchemaType = pStatement->getColumnString(3);
		std::string sDBTimestamp = pStatement->getColumnString(4);

		return new CMachineConfigurationType(pSQLHandler, sDBUUID, sDBName, sDBSchemaType, sDBTimestamp);
	}
	else {
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_COULDNOTFINDMACHINECONFIGURATIONTYPE, sSchemaType);
	}

}

CMachineConfigurationType* CMachineConfigurationType::makeByUUID(AMCData::PSQLHandler pSQLHandler, const std::string& sUUID)
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

}

CMachineConfigurationType* CMachineConfigurationType::createNewConfigurationType(AMCData::PSQLHandler pSQLHandler, const std::string& sSchemaType, const std::string& sName)
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

}


CMachineConfigurationType::CMachineConfigurationType(AMCData::PSQLHandler pSQLHandler, const std::string& sUUID, const std::string& sName, const std::string& sSchemaType, const std::string& sTimestamp)
: m_pSQLHandler (pSQLHandler), 
	m_sUUID (AMCCommon::CUtils::normalizeUUIDString(sUUID)),
	m_sName (sName),
	m_sSchemaType (sSchemaType),
	m_sTimestamp(sTimestamp)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

}



CMachineConfigurationType::~CMachineConfigurationType()
{

}

std::string CMachineConfigurationType::GetUUID()
{
	return m_sUUID;
}

std::string CMachineConfigurationType::GetName()
{
	return m_sName;
}

std::string CMachineConfigurationType::GetSchemaType()
{
	return m_sSchemaType;
}

IMachineConfigurationXSD * CMachineConfigurationType::GetLatestXSD()
{
	auto pStatement = m_pSQLHandler->prepareStatement("SELECT uuid, schemaversionnumber, defaultconfigurationxml FROM machineconfiguration_types WHERE typeuuid = ?");
	pStatement->setString(1, m_sUUID);

	if (pStatement->nextRow()) {

	}
	else {
		//throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_COULDNOTFINDMACHINECONFIGURATIONTYPE, sSchemaType);
	}
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_NOTIMPLEMENTED);
}

LibMCData_uint32 CMachineConfigurationType::GetLatestXSDVersion()
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_NOTIMPLEMENTED);
}

IMachineConfigurationXSD * CMachineConfigurationType::FindXSDByUUID(const std::string & sXSDUUID)
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_NOTIMPLEMENTED);
}

IMachineConfigurationXSD * CMachineConfigurationType::CreateNewXSD(const std::string & sXSDString, const LibMCData_uint32 nXSDVersion, const std::string & sDefaultConfigurationXML)
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_NOTIMPLEMENTED);
}

IMachineConfigurationXSD * CMachineConfigurationType::GetXSDVersion(const LibMCData_uint32 nXSDVersion)
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_NOTIMPLEMENTED);
}

IMachineConfigurationVersionIterator * CMachineConfigurationType::ListVersions()
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_NOTIMPLEMENTED);
}

IMachineConfigurationVersion * CMachineConfigurationType::FindVersionByUUID(const std::string & sVersionUUID)
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_NOTIMPLEMENTED);
}

IMachineConfigurationVersion * CMachineConfigurationType::GetActiveVersion(const std::string & sVersionUUID)
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_NOTIMPLEMENTED);
}

IMachineConfigurationVersion * CMachineConfigurationType::GetLatestVersion()
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_NOTIMPLEMENTED);
}

