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


Abstract: This is a stub class definition of CMachineConfigurationVersion

*/

#include "libmcdata_machineconfigurationversion.hpp"
#include "libmcdata_interfaceexception.hpp"

// Include custom headers here.
#include "common_utils.hpp"
#include "common_chrono.hpp"
#include "libmcdata_machineconfigurationversioniterator.hpp"

using namespace LibMCData::Impl;

/*************************************************************************************************************************
 Class definition of CMachineConfigurationVersion 
**************************************************************************************************************************/

CMachineConfigurationVersion::CMachineConfigurationVersion(AMCData::PSQLHandler pSQLHandler, const std::string& sUUID, const std::string& sXSDUUID, LibMCData_uint32 nConfigurationVersionNumber, const std::string& sParentUUID, const std::string& sConfigurationXML, const std::string& sUserUUID, const std::string& sTimestampUTC)
    : m_pSQLHandler(pSQLHandler)
    , m_sUUID(AMCCommon::CUtils::normalizeUUIDString(sUUID))
    , m_sXSDUUID(AMCCommon::CUtils::normalizeUUIDString(sXSDUUID))
    , m_nConfigurationVersionNumber(nConfigurationVersionNumber)
    , m_sParentUUID(AMCCommon::CUtils::normalizeUUIDString(sParentUUID))
    , m_sConfigurationXML(sConfigurationXML)
    , m_sUserUUID(sUserUUID)
    , m_sTimestampUTC(sTimestampUTC)
{
    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    if (sXSDUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty machine configuration XSDUUID");

    if (sParentUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty parent configuration UUID");

    if (sConfigurationXML.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty configuration XML string");

    if (sUserUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty active user UUID");

    if (sTimestampUTC.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty creation timestamp");
}

CMachineConfigurationVersion::~CMachineConfigurationVersion()
{

}

CMachineConfigurationVersion* CMachineConfigurationVersion::makeFrom(CMachineConfigurationVersion* pMachineConfigurationVersion)
{
    if (pMachineConfigurationVersion == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    return new CMachineConfigurationVersion(
                    pMachineConfigurationVersion->GetSQLHandler(),
                    pMachineConfigurationVersion->GetVersionUUID(), 
                    pMachineConfigurationVersion->GetXSDUUID(), 
                    pMachineConfigurationVersion->GetNumericVersion(), 
                    pMachineConfigurationVersion->GetParentUUID(), 
                    pMachineConfigurationVersion->GetConfigurationXMLString(), 
                    pMachineConfigurationVersion->GetUserUUID(), 
                    pMachineConfigurationVersion->GetTimestamp());
}

std::shared_ptr<CMachineConfigurationVersion> CMachineConfigurationVersion::makeSharedFrom(CMachineConfigurationVersion* pMachineConfigurationVersion)
{
    if (pMachineConfigurationVersion == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    return std::make_shared<CMachineConfigurationVersion>(
        pMachineConfigurationVersion->GetSQLHandler(),
        pMachineConfigurationVersion->GetVersionUUID(),
        pMachineConfigurationVersion->GetXSDUUID(),
        pMachineConfigurationVersion->GetNumericVersion(),
        pMachineConfigurationVersion->GetParentUUID(),
        pMachineConfigurationVersion->GetConfigurationXMLString(),
        pMachineConfigurationVersion->GetUserUUID(),
        pMachineConfigurationVersion->GetTimestamp());
}

CMachineConfigurationVersion* CMachineConfigurationVersion::createDefaultConfigurationForXSD( AMCData::PSQLHandler pSQLHandler, const std::string& sXSDUUID, const std::string& sConfigurationXML, const std::string& sTimestampUTC)
{
    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    if (sXSDUUID.empty() || sConfigurationXML.empty() || sTimestampUTC.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedXSDUUID = AMCCommon::CUtils::normalizeUUIDString(sXSDUUID);   

    // Create UUID and timestamp
    std::string sNewUUID = AMCCommon::CUtils::createUUID();

    // Insert new configuration version with 0 version number
    uint32_t nConfigurationVersionNumber = 0;

    // Insert new configuration version with null parent UUID
    std::string sParentUUID = "00000000-0000-0000-0000-000000000000";

    // Insert new configuration version with null user UUID
    std::string sUserUUID = "00000000-0000-0000-0000-000000000000";

    auto pInsert = pSQLHandler->prepareStatement(
        "INSERT INTO machineconfiguration_versions "
        "(uuid, xsduuid, configurationversionnumber, parentversionuuid, configurationxmlstring, useruuid, timestamp) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"
    );

    pInsert->setString(1, sNewUUID);
    pInsert->setString(2, sNormalizedXSDUUID);
    pInsert->setInt(3, nConfigurationVersionNumber);
    pInsert->setString(4, sParentUUID);
    pInsert->setString(5, sConfigurationXML);
    pInsert->setString(6, sUserUUID);
    pInsert->setString(7, sTimestampUTC);

    pInsert->execute();

    // Construct object and return
    return new CMachineConfigurationVersion(pSQLHandler, sNewUUID, sNormalizedXSDUUID, nConfigurationVersionNumber, sParentUUID, sConfigurationXML, sUserUUID, sTimestampUTC);
}

LibMCData_uint32 CMachineConfigurationVersion::getLatestConfigurationNumericVersionForXSD(AMCData::PSQLHandler pSQLHandler, const std::string& sXSDUUID)
{
    LibMCData_uint32 nLatestVersion = 0;

    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedXSDUUID = AMCCommon::CUtils::normalizeUUIDString(sXSDUUID);

    auto pStatement = pSQLHandler->prepareStatement(
        "SELECT MAX(configurationversionnumber) FROM machineconfiguration_versions WHERE xsduuid = ?"
    );
    pStatement->setString(1, sNormalizedXSDUUID);

    if (pStatement->nextRow())
        nLatestVersion = static_cast<LibMCData_uint32>(pStatement->getColumnInt(1));

    return nLatestVersion;
}

CMachineConfigurationVersion* CMachineConfigurationVersion::getLatestConfigurationVersionForXSD(AMCData::PSQLHandler pSQLHandler, const std::string& sXSDUUID)
{
    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedXSDUUID = AMCCommon::CUtils::normalizeUUIDString(sXSDUUID);

    // Query the latest version entry for given XSD
    auto pStatement = pSQLHandler->prepareStatement(
        "SELECT uuid, xsduuid, configurationversionnumber, parentversionuuid, "
        "configurationxmlstring, useruuid, timestamp "
        "FROM machineconfiguration_versions "
        "WHERE xsduuid = ? "
        "ORDER BY configurationversionnumber DESC LIMIT 1"
    );
    pStatement->setString(1, AMCCommon::CUtils::normalizeUUIDString(sNormalizedXSDUUID));

    if (!pStatement->nextRow())
        return nullptr; // No versions exist

    std::string sUUID = pStatement->getColumnString(1);
    std::string sXSDUUID_DB = pStatement->getColumnString(2);
    LibMCData_uint32 nVersionNumber = static_cast<LibMCData_uint32>(pStatement->getColumnInt(3));
    std::string sParentUUID = pStatement->getColumnString(4);
    std::string sXML = pStatement->getColumnString(5);
    std::string sUserUUID = pStatement->getColumnString(6);
    std::string sTimestamp = pStatement->getColumnString(7);

    return new CMachineConfigurationVersion(pSQLHandler, sUUID, sXSDUUID_DB, nVersionNumber, sParentUUID, sXML, sUserUUID, sTimestamp);
}

CMachineConfigurationVersion* CMachineConfigurationVersion::findConfigurationVersionByUUID(AMCData::PSQLHandler pSQLHandler, const std::string& sVersionUUID)
{
    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedVersionUUID = AMCCommon::CUtils::normalizeUUIDString(sVersionUUID);

    auto pStatement = pSQLHandler->prepareStatement(
        "SELECT uuid, xsduuid, configurationversionnumber, parentversionuuid, "
        "configurationxmlstring, useruuid, timestamp "
        "FROM machineconfiguration_versions "
        "WHERE uuid = ?"
    );
    pStatement->setString(1, sNormalizedVersionUUID);

    if (!pStatement->nextRow())
        return nullptr; // No such configuration version found

    std::string sUUID = pStatement->getColumnString(1);
    std::string sXSDUUID = pStatement->getColumnString(2);
    LibMCData_uint32 nVersionNumber = static_cast<LibMCData_uint32>(pStatement->getColumnInt(3));
    std::string sParentUUID = pStatement->getColumnString(4);
    std::string sXML = pStatement->getColumnString(5);
    std::string sUserUUID = pStatement->getColumnString(6);
    std::string sTimestamp = pStatement->getColumnString(7);

    return new CMachineConfigurationVersion(pSQLHandler, sUUID, sXSDUUID, nVersionNumber, sParentUUID, sXML, sUserUUID, sTimestamp);
}

IMachineConfigurationVersionIterator* CMachineConfigurationVersion::listConfigurationVersionsForXSD(
    AMCData::PSQLHandler pSQLHandler,
    const std::string& sXSDUUID)
{
    if (!pSQLHandler)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    if (sXSDUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    auto pTransaction = pSQLHandler->beginTransaction();

    std::string sQuery =
        "SELECT uuid, configurationversionnumber, parentversionuuid, configurationxmlstring, useruuid, timestamp "
        "FROM machineconfiguration_versions "
        "WHERE xsduuid = ? "
        "ORDER BY configurationversionnumber ASC;";

    auto pStatement = pTransaction->prepareStatement(sQuery);
    pStatement->setString(1, sXSDUUID);

    auto pIterator = std::make_unique<CMachineConfigurationVersionIterator>();

    while (pStatement->nextRow()) {
        std::string sUUID = pStatement->getColumnString(1);
        LibMCData_uint32 nVersionNumber = pStatement->getColumnInt(2);
        std::string sParentUUID = pStatement->getColumnString(3);
        std::string sXML = pStatement->getColumnString(4);
        std::string sUserUUID = pStatement->getColumnString(5);
        std::string sTimestamp = pStatement->getColumnString(6);

        auto pVersion = std::make_shared<CMachineConfigurationVersion>(
            pSQLHandler, sUUID, sXSDUUID, nVersionNumber, sParentUUID, sXML, sUserUUID, sTimestamp);

        pIterator->AddVersion(pVersion);
    }

    pTransaction->commit();

    return pIterator.release();
}

AMCData::PSQLHandler CMachineConfigurationVersion::GetSQLHandler()
{
	return m_pSQLHandler;
}

std::string CMachineConfigurationVersion::GetVersionUUID()
{
    return m_sUUID;
}

std::string CMachineConfigurationVersion::GetXSDUUID()
{
    return m_sXSDUUID;
}

LibMCData_uint32 CMachineConfigurationVersion::GetNumericVersion()
{
    return m_nConfigurationVersionNumber;
}

std::string CMachineConfigurationVersion::GetParentUUID()
{
    return m_sParentUUID;
}

std::string CMachineConfigurationVersion::GetConfigurationXMLString()
{
    return m_sConfigurationXML;
}

std::string CMachineConfigurationVersion::GetUserUUID()
{
	return m_sUserUUID;
}

std::string CMachineConfigurationVersion::GetTimestamp()
{
    return m_sTimestampUTC;
}

IMachineConfigurationVersion* CMachineConfigurationVersion::CreateNewVersion(const std::string & sXMLString, const std::string & sUserUUID)
{
    if (m_pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    if (sXMLString.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty configuration XML string");
    
    if (sUserUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty user UUID");

    std::string sNormalizedUserUUID = AMCCommon::CUtils::normalizeUUIDString(sUserUUID);

    // Create new version with incremented version number
    LibMCData_uint32 nNewVersionNumber = m_nConfigurationVersionNumber + 1;
    
    // Create new UUID
    std::string sNewUUID = AMCCommon::CUtils::createUUID();
    
    // Insert new configuration version
    auto pInsert = m_pSQLHandler->prepareStatement(
        "INSERT INTO machineconfiguration_versions "
        "(uuid, xsduuid, configurationversionnumber, parentversionuuid, configurationxmlstring, useruuid, timestamp) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)"
    );
    pInsert->setString(1, sNewUUID);
    pInsert->setString(2, m_sXSDUUID);
    pInsert->setInt(3, nNewVersionNumber);
    pInsert->setString(4, m_sUUID); // Parent is the current version
    pInsert->setString(5, sXMLString);
    pInsert->setString(6, sNormalizedUserUUID);
    
    AMCCommon::CChrono chrono;
    auto sTimeStampUTC = chrono.getUTCTimeInISO8601();
    
    pInsert->setString(7, sTimeStampUTC);
    pInsert->execute();
    return new CMachineConfigurationVersion(m_pSQLHandler, sNewUUID, m_sXSDUUID, nNewVersionNumber, m_sUUID, sXMLString, sNormalizedUserUUID, sTimeStampUTC);
}

IMachineConfigurationVersion* CMachineConfigurationVersion::MigrateToNewXSD(IMachineConfigurationXSD* pNewXSD, const std::string & sXMLString, const std::string & sUserUUID)
{
	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_NOTIMPLEMENTED);
}

