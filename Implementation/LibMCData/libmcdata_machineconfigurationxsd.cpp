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


Abstract: This is a stub class definition of CMachineConfigurationXSD

*/

#include "libmcdata_machineconfigurationxsd.hpp"
#include "libmcdata_interfaceexception.hpp"

// Include custom headers here.
#include "common_utils.hpp"
#include "common_chrono.hpp"
#include "libmcdata_machineconfigurationxsditerator.hpp"

using namespace LibMCData::Impl;

/*************************************************************************************************************************
 Class definition of CMachineConfigurationXSD 
**************************************************************************************************************************/

CMachineConfigurationXSD::CMachineConfigurationXSD( AMCData::PSQLHandler pSQLHandler, const std::string& sUUID, const std::string& sTypeUUID, const LibMCData_uint32 nXSDVersion, const std::string& sXSDString, const std::string& sTimestamp)
    : m_pSQLHandler(pSQLHandler)
    , m_sUUID(AMCCommon::CUtils::normalizeUUIDString(sUUID))
    , m_sTypeUUID(AMCCommon::CUtils::normalizeUUIDString(sTypeUUID))
    , m_nXSDVersion(nXSDVersion)
    , m_sXSDString(sXSDString)
    , m_sTimestamp(sTimestamp)
{
    if (m_pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    if (m_sUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty machine configuration XSD UUID.");

    if (m_sTypeUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty machine configuration type UUID.");

    if (m_sXSDString.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty machine configuration XSD String");
}

CMachineConfigurationXSD::~CMachineConfigurationXSD()
{
}

CMachineConfigurationXSD* CMachineConfigurationXSD::makeFrom(CMachineConfigurationXSD* pMachineConfigurationVersion)
{
    if (pMachineConfigurationVersion == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    return new CMachineConfigurationXSD(
        pMachineConfigurationVersion->GetSQLHandler(),
        pMachineConfigurationVersion->GetUUID(),
        pMachineConfigurationVersion->GetTypeUUID(),
        pMachineConfigurationVersion->GetXSDVersion(),
        pMachineConfigurationVersion->GetXSDString(),
        pMachineConfigurationVersion->GetTimestamp());
}

CMachineConfigurationXSD* CMachineConfigurationXSD::createNewXSDForType(AMCData::PSQLHandler pSQLHandler, const std::string& sTypeUUID, const std::string& sXSDString, const LibMCData_uint32 nXSDNumericVersion)
{
    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    if (sTypeUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty machine configuration type UUID.");

    if (sXSDString.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "Empty machine configuration XSD String");

    std::string sNormalizedTypeUUID = AMCCommon::CUtils::normalizeUUIDString(sTypeUUID);

    // Ensure the configuration type exists
    {
        auto pCheckTypeUUIDStatement = pSQLHandler->prepareStatement("SELECT uuid FROM machineconfiguration_types WHERE uuid = ?");
        pCheckTypeUUIDStatement->setString(1, sNormalizedTypeUUID);
        if (!pCheckTypeUUIDStatement->nextRow())
            throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDMACHINECONFIGURATIONTYPEUUID);
    }

    // Ensure the new version is strictly greater than the latest one
    LibMCData_uint32 nLatestVersion = getLatestXSDNumericVersionForType(pSQLHandler, sNormalizedTypeUUID);
    if (nXSDNumericVersion <= nLatestVersion)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_MACHINECONFIGURATIONXSDVERSIONNOTINCREMENTING);

    // Generate new UUID and timestamp
    std::string sUUID = AMCCommon::CUtils::createUUID();
                        
    std::string sTimestamp = AMCCommon::CChrono().getUTCTimeInISO8601();

    // Insert new XSD record into the database
    auto pInsertStmt = pSQLHandler->prepareStatement(
        "INSERT INTO machineconfiguration_xsds (uuid, typeuuid, schemaversionnumber, configurationxsdstring, timestamp) "
        "VALUES (?, ?, ?, ?, ?)"
    );

    pInsertStmt->setString(1, sUUID);
    pInsertStmt->setString(2, sNormalizedTypeUUID);
    pInsertStmt->setInt(3, nXSDNumericVersion);
    pInsertStmt->setString(4, sXSDString);
    pInsertStmt->setString(5, sTimestamp);
    pInsertStmt->execute();

    // Return new XSD instance
    return new CMachineConfigurationXSD(pSQLHandler, sUUID, sNormalizedTypeUUID, nXSDNumericVersion, sXSDString, sTimestamp);
}

CMachineConfigurationXSD* CMachineConfigurationXSD::getLatestXSDForType(AMCData::PSQLHandler pSQLHandler, const std::string& sTypeUUID)
{
    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedTypeUUID = AMCCommon::CUtils::normalizeUUIDString(sTypeUUID);

    auto pStatement = pSQLHandler->prepareStatement(
        "SELECT uuid, schemaversionnumber, configurationxsdstring, timestamp "
        "FROM machineconfiguration_xsds WHERE typeuuid = ? "
        "ORDER BY schemaversionnumber DESC LIMIT 1"
    );
    pStatement->setString(1, sNormalizedTypeUUID);

    if (pStatement->nextRow()) {
        std::string sUUID = pStatement->getColumnString(1);
        LibMCData_uint32 nVersion = static_cast<LibMCData_uint32>(pStatement->getColumnInt(2));
        std::string sXSDString = pStatement->getColumnString(3);
        std::string sTimestamp = pStatement->getColumnString(4);

        return new CMachineConfigurationXSD(pSQLHandler, sUUID, sNormalizedTypeUUID, nVersion, sXSDString, sTimestamp);
    }

    return nullptr;
}

LibMCData_uint32 CMachineConfigurationXSD::getLatestXSDNumericVersionForType(AMCData::PSQLHandler pSQLHandler, const std::string& sTypeUUID)
{
    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedTypeUUID = AMCCommon::CUtils::normalizeUUIDString(sTypeUUID);

    auto pStatement = pSQLHandler->prepareStatement(
        "SELECT MAX(schemaversionnumber) FROM machineconfiguration_xsds WHERE typeuuid = ?"
    );
    pStatement->setString(1, sNormalizedTypeUUID);

    if (pStatement->nextRow()) {
        if (pStatement->columnIsNull(1))
            return 0;

        return static_cast<LibMCData_uint32>(pStatement->getColumnInt(1));
    }

    throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_COULDNOTGETLATESTMACHINECONFIGURATIONXSDVERSION);
}

CMachineConfigurationXSD* CMachineConfigurationXSD::findXSDByUUID(AMCData::PSQLHandler pSQLHandler, const std::string& sXSDUUID)
{
    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedXSDUUID = AMCCommon::CUtils::normalizeUUIDString(sXSDUUID);

    auto pStatement = pSQLHandler->prepareStatement(
        "SELECT uuid, typeuuid, schemaversionnumber, configurationxsdstring, timestamp "
        "FROM machineconfiguration_xsds WHERE uuid = ?"
    );
    pStatement->setString(1, sNormalizedXSDUUID);

    if (!pStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_COULDNOTFINDMACHINECONFIGURATIONXSDBYUUID, "XSD not forund for UUID = " + sNormalizedXSDUUID);

    std::string sUUID = pStatement->getColumnString(1);
    std::string sTypeUUID = pStatement->getColumnString(2);
    LibMCData_uint32 nVersion = static_cast<LibMCData_uint32>(pStatement->getColumnInt(3));
    std::string sXSDString = pStatement->getColumnString(4);
    std::string sTimestamp = pStatement->getColumnString(5);

    return new CMachineConfigurationXSD(pSQLHandler, sUUID, sTypeUUID, nVersion, sXSDString, sTimestamp);
}


CMachineConfigurationXSD* CMachineConfigurationXSD::findXSDByNumericVersion(AMCData::PSQLHandler pSQLHandler, LibMCData_uint32 nXSDNumericVersion)
{
    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    auto pStatement = pSQLHandler->prepareStatement(
        "SELECT uuid, typeuuid, schemaversionnumber, configurationxsdstring, timestamp "
        "FROM machineconfiguration_xsds WHERE schemaversionnumber = ?"
    );
    pStatement->setInt(1, nXSDNumericVersion);

    if (!pStatement->nextRow())
        return nullptr; // XSD not found

    std::string sUUID = pStatement->getColumnString(1);
    std::string sTypeUUID = pStatement->getColumnString(2);
    LibMCData_uint32 nVersion = static_cast<LibMCData_uint32>(pStatement->getColumnInt(3));
    std::string sXSDString = pStatement->getColumnString(4);
    std::string sTimestamp = pStatement->getColumnString(5);

    return new CMachineConfigurationXSD(pSQLHandler, sUUID, sTypeUUID, nVersion, sXSDString, sTimestamp);
}

IMachineConfigurationXSDIterator* CMachineConfigurationXSD::listConfigurationXSDVersions(AMCData::PSQLHandler pSQLHandler, const std::string& sXSDUUID)
{
    if (!pSQLHandler)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    if (sXSDUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedXSDUUID = AMCCommon::CUtils::normalizeUUIDString(sXSDUUID);

    // Begin a transaction to ensure a consistent view of the data
    auto pTransaction = pSQLHandler->beginTransaction();

    // Prepare the SQL query to retrieve all XSD versions matching the given type UUID
    std::string sQuery =
        "SELECT uuid, schemaversionnumber, configurationxsdstring, timestamp "
        "FROM machineconfiguration_xsds "
        "WHERE typeuuid = ? "
        "ORDER BY schemaversionnumber ASC;";

    auto pStatement = pTransaction->prepareStatement(sQuery);
    pStatement->setString(1, sNormalizedXSDUUID);

    // Create the iterator that will store the XSD version objects
    auto pIterator = std::make_unique<CMachineConfigurationXSDIterator>();

    // Fetch each row and construct corresponding XSD objects
    while (pStatement->nextRow()) {
        std::string sUUID = pStatement->getColumnString(1);
        LibMCData_uint32 nSchemaVersion = static_cast<LibMCData_uint32>(pStatement->getColumnInt(2));
        std::string sXSDString = pStatement->getColumnString(3);
        std::string sTimestamp = pStatement->getColumnString(4);

        // Create a new CMachineConfigurationXSD instance
        auto pXSD = std::make_shared<CMachineConfigurationXSD>(pSQLHandler, sUUID, sNormalizedXSDUUID, nSchemaVersion, sXSDString, sTimestamp);

        // Add the object to the iterator
        pIterator->AddXSDVersion(pXSD);
    }

    // Commit the transaction after all rows have been processed
    pTransaction->commit();

    // Return the populated iterator
    return pIterator.release();
}

AMCData::PSQLHandler CMachineConfigurationXSD::GetSQLHandler()
{
    return m_pSQLHandler;
}

std::string CMachineConfigurationXSD::GetUUID()
{
    return m_sUUID;
}

std::string CMachineConfigurationXSD::GetTypeUUID()
{
    return m_sTypeUUID;
}

LibMCData_uint32 CMachineConfigurationXSD::GetXSDVersion()
{
    return m_nXSDVersion;
}

std::string CMachineConfigurationXSD::GetXSDString()
{
    return m_sXSDString;
}

std::string CMachineConfigurationXSD::GetTimestamp()
{
    return m_sTimestamp;
}
