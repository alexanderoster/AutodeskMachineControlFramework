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


Abstract: This is a stub class definition of CStorage

*/

#include "libmcdata_storage.hpp"
#include "libmcdata_storagestream.hpp"
#include "libmcdata_interfaceexception.hpp"

#include "libmcdata_storagezipwriter.hpp"
#include "libmcdata_datacapture.hpp"

// Include custom headers here.
#include "common_utils.hpp"
#include "amcdata_storagestate.hpp"

#include "common_chrono.hpp"
#include <vector>
#include "PicoSHA2/picosha2.h"

using namespace LibMCData::Impl;

/*************************************************************************************************************************
 Class definition of CStorage 
**************************************************************************************************************************/

CStorage::CStorage(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState)
    : m_pSQLHandler (pSQLHandler), m_pStorageState(pStorageState)
{
    if (pStorageState.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
    if (pSQLHandler.get() == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

}


void CStorage::insertDBEntry(const std::string& sUUID, const std::string& sName, const std::string& sMimeType, const LibMCData_uint64 nSize, const std::string& sSHA2, uint64_t nAbsoluteTimeStamp, const std::string& sUserUUID)
{

    auto pTransaction = m_pSQLHandler->beginTransaction();

    std::string sNormalizedUserUUID = AMCCommon::CUtils::normalizeUUIDString(sUserUUID);

    std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);

    std::string sTimestamp = AMCCommon::CChrono::convertToISO8601TimeUTC (nAbsoluteTimeStamp);

    std::string sQuery = "SELECT uuid FROM storage_streams WHERE uuid=?";
    auto pStatement = pTransaction->prepareStatement(sQuery);
    pStatement->setString(1, sParsedUUID);
    if (pStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DUPLICATESTORAGESTREAM);
    pStatement = nullptr;

    std::string sInsertQuery = "INSERT INTO storage_streams (uuid, name, mimetype, sha2, size, userid, timestamp, status, useruuid) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
    auto pInsertStatement = pTransaction->prepareStatement(sInsertQuery);
    pInsertStatement->setString(1, sParsedUUID);
    pInsertStatement->setString(2, sName);
    pInsertStatement->setString(3, sMimeType);
    pInsertStatement->setString(4, sSHA2);
    pInsertStatement->setInt64(5, nSize);
    pInsertStatement->setString(6, "");
    pInsertStatement->setString(7, sTimestamp);
    pInsertStatement->setString(8, AMCData::CStorageState::storageStreamStatusToString (AMCData::eStorageStreamStatus::sssNew));
    pInsertStatement->setString(9, sNormalizedUserUUID);
    pInsertStatement->execute();
    pInsertStatement = nullptr;

    pTransaction->commit();
}


bool CStorage::StreamIsReady(const std::string& sUUID)
{
    std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);

    std::string sQuery = "SELECT uuid FROM storage_streams WHERE uuid=? AND status=?";
    auto pStatement = m_pSQLHandler->prepareStatement(sQuery);
    pStatement->setString(1, sParsedUUID);
    pStatement->setString(2, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssValidated));
    return (pStatement->nextRow());
}

IStorageStream* CStorage::RetrieveStream(const std::string& sUUID)
{
    return CStorageStream::makeFromDatabase(sUUID, m_pSQLHandler, m_pStorageState);
}

void CStorage::StoreNewStream(const std::string& sUUID, const std::string& sName, const std::string& sMimeType, const LibMCData_uint64 nContentBufferSize, const LibMCData_uint8* pContentBuffer, const std::string& sUserUUID, const LibMCData_uint64 nAbsoluteTimeStamp)
{
    if (nContentBufferSize == 0)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDBUFFERSIZE);
    if (pContentBuffer == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
    if (sName.empty ())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
    if (sMimeType.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
    if (sUserUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedUserUUID = AMCCommon::CUtils::normalizeUUIDString(sUserUUID);

    // sContextUUID is depreciated and not used anymore!

    // Calculate SHA Hash (not under mutex lock)
    auto beginIter = static_cast<const unsigned char*>(pContentBuffer);
    auto endIter = static_cast<const unsigned char*>(pContentBuffer) + nContentBufferSize;

    picosha2::hash256_one_by_one hasher;
    hasher.process(beginIter, endIter);
    hasher.finish();

    std::vector<unsigned char> hash(picosha2::k_digest_size);
    hasher.get_hash_bytes(hash.begin(), hash.end());
    std::string sSHA256 = picosha2::get_hash_hex_string(hasher);
	
    // From here, we lock storage database write access
    {       
        insertDBEntry(sUUID, sName, sMimeType, nContentBufferSize, sSHA256, nAbsoluteTimeStamp, sNormalizedUserUUID);
    }

    std::string sCalculatedSHA256, sCalculatedBlockSHA256;

    // Store data asynchroniously on disk and finalize when writing has finished
    auto pWriter = std::make_shared<AMCData::CStorageWriter_Partial>(sUUID, m_pStorageState->getStreamPath (sUUID), nContentBufferSize);
    pWriter->writeChunkAsync(pContentBuffer, nContentBufferSize, 0);
    pWriter->finalize(sSHA256, "", sCalculatedSHA256, sCalculatedBlockSHA256);
 
    std::string sUpdateQuery = "UPDATE storage_streams SET status=?, sha2=?, sha256_block64k=? WHERE uuid=? AND status=?";
    auto pUpdateStatement = m_pSQLHandler->prepareStatement(sUpdateQuery);
    pUpdateStatement->setString(1, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssValidated));
    pUpdateStatement->setString(2, sCalculatedSHA256);
    pUpdateStatement->setString(3, sCalculatedBlockSHA256);
    pUpdateStatement->setString(4, sUUID);
    pUpdateStatement->setString(5, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssNew));
    pUpdateStatement->execute();
    pUpdateStatement = nullptr;

}

void CStorage::BeginPartialStream(const std::string& sUUID, const std::string& sName, const std::string& sMimeType, const LibMCData_uint64 nSize, const std::string& sUserUUID, const LibMCData_uint64 nAbsoluteTimeStamp)
{

    if (nSize == 0)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDBUFFERSIZE);
    if (sName.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
    if (sMimeType.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
    if (sUserUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedUserUUID = AMCCommon::CUtils::normalizeUUIDString(sUserUUID);

    // sContextUUID is depreciated and not used anymore!

    {
        std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);
        insertDBEntry(sParsedUUID, sName, sMimeType, nSize, "", nAbsoluteTimeStamp, sNormalizedUserUUID);

        auto pWriter = std::make_shared<AMCData::CStorageWriter_Partial>(sParsedUUID, m_pStorageState->getStreamPath(sUUID), nSize);
        m_pStorageState->addPartialWriter(pWriter);
    }
    
}

void CStorage::StorePartialStream(const std::string & sUUID, const LibMCData_uint64 nOffset, const LibMCData_uint64 nContentBufferSize, const LibMCData_uint8 * pContentBuffer)
{

    if (nContentBufferSize == 0)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDBUFFERSIZE);
    if (pContentBuffer == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);
    auto pWriter = m_pStorageState->findPartialWriter(sParsedUUID, false);
    if (pWriter == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARTIALUPLOAD, "invalid partial upload: " + sParsedUUID);

    auto pPartialWriter = dynamic_cast<AMCData::CStorageWriter_Partial*> (pWriter.get());
    if (pPartialWriter == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTPARTIAL, "storage stream not partial: " + sParsedUUID);

    pWriter->writeChunkAsync(pContentBuffer, nContentBufferSize, nOffset);
}

void CStorage::FinishPartialStream(const std::string& sUUID, const std::string& sSHA2)
{
    FinishPartialStreamEx(sUUID, sSHA2, "");
}

void CStorage::FinishPartialStreamBlockwiseSHA256(const std::string& sUUID, const std::string& sBlockwiseSHA2)
{
    FinishPartialStreamEx(sUUID, "", sBlockwiseSHA2);
}


void CStorage::FinishPartialStreamEx(const std::string& sUUID, const std::string& sSHA2, const std::string& sBlockwiseSHA2)
{

    std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);

    std::string sNormalizedSHA2;
    if (!sSHA2.empty ())
        sNormalizedSHA2 = AMCCommon::CUtils::normalizeSHA256String(sSHA2);

    std::string sNormalizedBlockwiseSHA2;
    if (!sBlockwiseSHA2.empty())
        sNormalizedBlockwiseSHA2 = AMCCommon::CUtils::normalizeSHA256String(sBlockwiseSHA2);

    auto pWriter = m_pStorageState->findPartialWriter (sParsedUUID, true);
    auto pPartialWriter = dynamic_cast<AMCData::CStorageWriter_Partial*> (pWriter.get());
    if (pPartialWriter == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTPARTIAL, "storage stream not partial: " + sParsedUUID);

    m_pStorageState->deletePartialWriter (sParsedUUID);

    // Write data and calculate checksums
    std::string sCalculatedSHA256, sCalculatedBlockSHA256;
    pPartialWriter->finalize (sNormalizedSHA2, sNormalizedBlockwiseSHA2, sCalculatedSHA256, sCalculatedBlockSHA256);

    std::string sUpdateQuery = "UPDATE storage_streams SET status=?, sha2=?, sha256_block64k=? WHERE uuid=? AND status=?";
    auto pUpdateStatement = m_pSQLHandler->prepareStatement(sUpdateQuery);
    pUpdateStatement->setString(1, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssValidated));
    pUpdateStatement->setString(2, sCalculatedSHA256);
    pUpdateStatement->setString(3, sCalculatedBlockSHA256);
    pUpdateStatement->setString(4, sUUID);
    pUpdateStatement->setString(5, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssNew));
    pUpdateStatement->execute();
    pUpdateStatement = nullptr;

}

void CStorage::BeginRandomWriteStream(const std::string& sUUID, const std::string& sName, const std::string& sMimeType, const std::string& sUserUUID, const LibMCData_uint64 nAbsoluteTimeStamp)
{
    if (sName.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
    if (sMimeType.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
    if (sUserUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedUserUUID = AMCCommon::CUtils::normalizeUUIDString(sUserUUID);

    {
        std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);
        insertDBEntry(sParsedUUID, sName, sMimeType, 0, "", nAbsoluteTimeStamp, sNormalizedUserUUID);

        auto pWriter = std::make_shared<AMCData::CStorageWriter_RandomAccess>(sParsedUUID, m_pStorageState->getStreamPath(sUUID));
        m_pStorageState->addPartialWriter(pWriter);
    }

}

void CStorage::StoreRandomWriteStream(const std::string& sUUID, const LibMCData_uint64 nOffset, const LibMCData_uint64 nContentBufferSize, const LibMCData_uint8* pContentBuffer)
{
    if (nContentBufferSize == 0)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDBUFFERSIZE);
    if (pContentBuffer == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);
    auto pWriter = m_pStorageState->findPartialWriter(sParsedUUID, false);
    if (pWriter == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARTIALUPLOAD, "invalid partial upload: " + sParsedUUID);

    auto pRandomAccessWriter = dynamic_cast<AMCData::CStorageWriter_RandomAccess*> (pWriter.get());
    if (pRandomAccessWriter == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTRANDOMACCESS, "storage stream is not random access: " + sParsedUUID);

    pWriter->writeChunkAsync(pContentBuffer, nContentBufferSize, nOffset);

}

IStorageZIPWriter* CStorage::CreateZIPStream(const std::string& sUUID, const std::string& sName, const std::string& sUserUUID, const LibMCData_uint64 nAbsoluteTimeStamp)
{
    if (sName.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
    if (sUserUUID.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sNormalizedUserUUID = AMCCommon::CUtils::normalizeUUIDString(sUserUUID);

    {
        std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);
        insertDBEntry(sParsedUUID, sName, "application/zip", 0, "", nAbsoluteTimeStamp, sNormalizedUserUUID);

        auto pWriter = std::make_shared<AMCData::CStorageWriter_ZIPStream>(sParsedUUID, m_pStorageState->getStreamPath(sUUID));
        m_pStorageState->addPartialWriter(pWriter);

        return new CStorageZIPWriter(pWriter, m_pSQLHandler, m_pStorageState);
    }

}

LibMCData_uint64 CStorage::GetRandomWriteStreamSize(const std::string& sUUID)
{
    std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);
    auto pWriter = m_pStorageState->findPartialWriter(sParsedUUID, false);
    if (pWriter == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARTIALUPLOAD, "invalid partial upload: " + sParsedUUID);

    auto pRandomAccessWriter = dynamic_cast<AMCData::CStorageWriter_RandomAccess*> (pWriter.get());
    if (pRandomAccessWriter == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTRANDOMACCESS, "storage stream is not random access: " + sParsedUUID);

    return pRandomAccessWriter->getCurrentSize();

}

void CStorage::FinishRandomWriteStream(const std::string& sUUID)
{
    std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);

    auto pWriter = m_pStorageState->findPartialWriter(sParsedUUID, true);
    auto pRandomAccessWriter = dynamic_cast<AMCData::CStorageWriter_RandomAccess*> (pWriter.get());
    if (pRandomAccessWriter == nullptr)
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTRANDOMACCESS, "storage stream is not random access: " + sParsedUUID);

    m_pStorageState->deletePartialWriter(sParsedUUID);

    // Write data and calculate checksums
    std::string sCalculatedSHA256, sCalculatedBlockSHA256;
    uint64_t nSize = pRandomAccessWriter->getCurrentSize();

    pRandomAccessWriter->finalize(sCalculatedSHA256, sCalculatedBlockSHA256);

    std::string sUpdateQuery = "UPDATE storage_streams SET size=?, status=?, sha2=?, sha256_block64k=? WHERE uuid=? AND status=?";
    auto pUpdateStatement = m_pSQLHandler->prepareStatement(sUpdateQuery);
    pUpdateStatement->setString(1, std::to_string (nSize));
    pUpdateStatement->setString(2, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssValidated));
    pUpdateStatement->setString(3, sCalculatedSHA256);
    pUpdateStatement->setString(4, sCalculatedBlockSHA256);
    pUpdateStatement->setString(5, sUUID);
    pUpdateStatement->setString(6, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssNew));
    pUpdateStatement->execute();
    pUpdateStatement = nullptr;

}



LibMCData_uint64 CStorage::GetMaxStreamSize()
{
    return 1024ULL * 1024ULL * 1024ULL * 32ULL; // 32GB Maximum stream data size
}

bool CStorage::ContentTypeIsAccepted(const std::string& sContentType)
{
    auto sLowerContentType = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(sContentType));

    return m_pStorageState->isAcceptedContent(sLowerContentType);
}

bool CStorage::StreamIsImage(const std::string& sUUID) 
{
    std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);

    std::string sQuery = "SELECT mimetype FROM storage_streams WHERE uuid=?";
    auto pStatement = m_pSQLHandler->prepareStatement(sQuery);
    pStatement->setString(1, sParsedUUID);
    if (!pStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTFOUND, "storage stream not found: " + sParsedUUID);

    auto sMimeType = pStatement->getColumnString(1);
    auto sLowerMimeType = AMCCommon::CUtils::toLowerString(AMCCommon::CUtils::trimString(sMimeType));
    pStatement = nullptr;

    return m_pStorageState->isImageContent(sLowerMimeType);

}


void CStorage::CreateDownloadTicket(const std::string& sTicketUUID, const std::string& sStreamUUID, const std::string& sClientFileName, const std::string& sSessionUUID, const std::string& sUserUUID, const LibMCData_uint64 nAbsoluteTimeStamp)
{
    std::string sNormalizedTicketUUID = AMCCommon::CUtils::normalizeUUIDString(sTicketUUID);
    std::string sNormalizedStreamUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
    std::string sNormalizedSessionUUID = AMCCommon::CUtils::normalizeUUIDString(sSessionUUID);
    std::string sNormalizedUserUUID = AMCCommon::CUtils::normalizeUUIDString(sUserUUID);

    if (sClientFileName.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_EMPTYCLIENTFILENAME);

    if (!AMCCommon::CUtils::stringIsValidFileName(sClientFileName))
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDCLIENTFILENAME, "invalid client file name: " + sClientFileName);

    auto pTransaction = m_pSQLHandler->beginTransaction();
    
    std::string sTimestamp = AMCCommon::CChrono::convertToISO8601TimeUTC (nAbsoluteTimeStamp);

    std::string sStreamQuery = "SELECT uuid FROM storage_streams WHERE uuid=?";
    auto pStreamStatement = pTransaction->prepareStatement(sStreamQuery);
    pStreamStatement->setString(1, sNormalizedStreamUUID);
    if (!pStreamStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTFOUND, "storage stream not found for download: " + sNormalizedStreamUUID);
    pStreamStatement = nullptr;

    std::string sUserQuery = "SELECT uuid FROM users WHERE active=? AND uuid=?";
    auto pUserStatement = pTransaction->prepareStatement(sUserQuery);
    pUserStatement->setInt(1, 1);
    pUserStatement->setString(2, sNormalizedUserUUID);
    if (!pUserStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_USERNOTFOUND, "user not found for download: " + sNormalizedUserUUID);
    pUserStatement = nullptr;

    std::string sTicketQuery = "INSERT INTO storage_downloadtickets (ticketuuid, streamuuid, clientfilename, sessionuuid, useruuid, timestamp) VALUES (?, ?, ?, ?, ?, ?)";
    auto pTicketStatement = pTransaction->prepareStatement(sTicketQuery);
    pTicketStatement->setString(1, sNormalizedTicketUUID);
    pTicketStatement->setString(2, sNormalizedStreamUUID);
    pTicketStatement->setString(3, sClientFileName);
    pTicketStatement->setString(4, sNormalizedSessionUUID);
    pTicketStatement->setString(5, sNormalizedUserUUID);
    pTicketStatement->setString(6, sTimestamp);
    pTicketStatement->execute();
    pTicketStatement = nullptr;

    pTransaction->commit();


}


void CStorage::RequestDownloadTicket(const std::string& sTicketUUID, const std::string& sIPAddress, const LibMCData_uint64 nAbsoluteTimeStamp, std::string& sStreamUUID, std::string& sClientFileName, std::string& sSessionUUID, std::string& sUserUUID)
{
    std::string sNormalizedTicketUUID = AMCCommon::CUtils::normalizeUUIDString(sTicketUUID);

    auto pTransaction = m_pSQLHandler->beginTransaction();
    AMCCommon::CChrono chrono;
    std::string sTimestamp = AMCCommon::CChrono::convertToISO8601TimeUTC(nAbsoluteTimeStamp);

    std::string sTicketQuery = "SELECT streamuuid, clientfilename, sessionuuid, useruuid FROM storage_downloadtickets WHERE ticketuuid=?";
    auto pTicketStatement = pTransaction->prepareStatement(sTicketQuery);
    pTicketStatement->setString(1, sNormalizedTicketUUID);
    if (!pTicketStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DOWNLOADTICKETNOTFOUND, "download ticket not found: " + sNormalizedTicketUUID);

    sStreamUUID = pTicketStatement->getColumnString(1);
    sClientFileName = pTicketStatement->getColumnString(2);
    sSessionUUID = pTicketStatement->getColumnString(3);
    sUserUUID = pTicketStatement->getColumnString(4);

    pTicketStatement = nullptr;

    std::string sDownloadUUID = AMCCommon::CUtils::createUUID();

    std::string sDownloadQuery = "INSERT INTO storage_downloads (downloaduuid, ticketuuid, ipaddress, timestamp) VALUES (?, ?, ?, ?)";
    auto pDownloadStatement = pTransaction->prepareStatement(sDownloadQuery);
    pDownloadStatement->setString(1, sDownloadUUID);
    pDownloadStatement->setString(2, sNormalizedTicketUUID);
    pDownloadStatement->setString(3, sIPAddress);
    pDownloadStatement->setString(4, sTimestamp);
    pDownloadStatement->execute();
    pDownloadStatement = nullptr;

    pTransaction->commit();

}


void CStorage::AttachStreamToJournal(const std::string& sStreamUUID, const std::string& sJournalUUID)
{
    std::string sNormalizedStreamUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
    std::string sNormalizedJournalUUID = AMCCommon::CUtils::normalizeUUIDString(sJournalUUID);

    auto pTransaction = m_pSQLHandler->beginTransaction();

    std::string sStreamQuery = "SELECT uuid FROM storage_streams WHERE uuid=?";
    auto pStreamStatement = pTransaction->prepareStatement(sStreamQuery);
    pStreamStatement->setString(1, sNormalizedStreamUUID);
    if (!pStreamStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTFOUND, "storage stream not found for journal attachment: " + sNormalizedStreamUUID);
    pStreamStatement = nullptr;


    std::string sJournalQuery = "SELECT uuid FROM journals WHERE uuid=?";
    auto pJournalStatement = pTransaction->prepareStatement(sJournalQuery);
    pJournalStatement->setString(1, sNormalizedJournalUUID);
    if (!pJournalStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTFOUND, "journal not found for journal attachment: " + sNormalizedStreamUUID);
    pJournalStatement = nullptr;


    std::string sLinkQuery = "SELECT uuid FROM journal_tempstreams WHERE journaluuid=? AND streamuuid=?";
    auto pLinkStatement = pTransaction->prepareStatement(sLinkQuery);
    pLinkStatement->setString(1, sNormalizedJournalUUID);
    pLinkStatement->setString(2, sNormalizedStreamUUID);
    bool bLinkExists = pLinkStatement->nextRow();
    pLinkStatement = nullptr;

    if (!bLinkExists) {
        std::string sInsertQuery = "INSERT INTO journal_tempstreams (uuid, journaluuid, streamuuid) VALUES (?, ?, ?)";
        auto pInsertStatement = pTransaction->prepareStatement(sInsertQuery);
        pInsertStatement->setString(1, AMCCommon::CUtils::createUUID ());
        pInsertStatement->setString(2, sNormalizedJournalUUID);
        pInsertStatement->setString(3, sNormalizedStreamUUID);
        pInsertStatement->execute();
        pInsertStatement = nullptr;

    }

    pTransaction->commit();
}


void CStorage::DiscardRandomWriteStream(const std::string& sUUID)
{
    std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);

    {
        auto pWriter = m_pStorageState->findPartialWriter(sParsedUUID, false);
        if (pWriter == nullptr)
            throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_RANDOMWRITESTREAMNOTFOUND, "random write stream not found: " + sParsedUUID);

        auto pRandomAccessWriter = dynamic_cast<AMCData::CStorageWriter_RandomAccess*> (pWriter.get());
        if (pRandomAccessWriter == nullptr)
            throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTRANDOMACCESS, "storage stream is not random access: " + sParsedUUID);

        m_pStorageState->deletePartialWriter(sParsedUUID);

        // An unfinished writer deletes its file when the last reference is released at the end of this scope.
    }

    auto pTransaction = m_pSQLHandler->beginTransaction();

    auto pLinkStatement = pTransaction->prepareStatement("DELETE FROM journal_tempstreams WHERE streamuuid=?");
    pLinkStatement->setString(1, sParsedUUID);
    pLinkStatement->execute();
    pLinkStatement = nullptr;

    auto pStreamStatement = pTransaction->prepareStatement("DELETE FROM storage_streams WHERE uuid=? AND status=?");
    pStreamStatement->setString(1, sParsedUUID);
    pStreamStatement->setString(2, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssNew));
    pStreamStatement->execute();
    pStreamStatement = nullptr;

    pTransaction->commit();
}

LibMCData_uint64 CStorage::GetDataCaptureStorageSize()
{
    return CDataCapture::getTotalStoredSize(m_pSQLHandler);
}

LibMCData_uint32 CStorage::EnforceDataCaptureStorageLimit(const LibMCData_uint64 nLimitInBytes)
{
    return CDataCapture::enforceStorageLimit(m_pSQLHandler, m_pStorageState, nLimitInBytes);
}

void CStorage::GetDownloadTicketDetails(const std::string& sTicketUUID, std::string& sStreamUUID, std::string& sClientFileName, std::string& sSessionUUID, std::string& sUserUUID, LibMCData_uint64& nCreationTimeStamp, bool& bRevoked)
{
    std::string sNormalizedTicketUUID = AMCCommon::CUtils::normalizeUUIDString(sTicketUUID);

    std::string sTicketQuery = "SELECT streamuuid, clientfilename, sessionuuid, useruuid, timestamp, revoked FROM storage_downloadtickets WHERE ticketuuid=?";
    auto pTicketStatement = m_pSQLHandler->prepareStatement(sTicketQuery);
    pTicketStatement->setString(1, sNormalizedTicketUUID);
    if (!pTicketStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DOWNLOADTICKETNOTFOUND, "download ticket not found: " + sNormalizedTicketUUID);

    sStreamUUID = pTicketStatement->getColumnString(1);
    sClientFileName = pTicketStatement->getColumnString(2);
    sSessionUUID = pTicketStatement->getColumnString(3);
    sUserUUID = pTicketStatement->getColumnString(4);
    std::string sTimestamp = pTicketStatement->getColumnString(5);
    bRevoked = (pTicketStatement->getColumnInt(6) != 0);
    pTicketStatement = nullptr;

    nCreationTimeStamp = 0;
    if (!sTimestamp.empty())
        nCreationTimeStamp = AMCCommon::CChrono::parseISO8601TimeUTC(sTimestamp);
}

LibMCData_uint32 CStorage::RevokeDownloadTicketsOfStream(const std::string& sStreamUUID)
{
    std::string sNormalizedStreamUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);

    auto pTransaction = m_pSQLHandler->beginTransaction();

    uint32_t nCount = 0;
    auto pCountStatement = pTransaction->prepareStatement("SELECT COUNT(*) FROM storage_downloadtickets WHERE streamuuid=? AND revoked=0");
    pCountStatement->setString(1, sNormalizedStreamUUID);
    if (pCountStatement->nextRow())
        nCount = (uint32_t)pCountStatement->getColumnInt64(1);
    pCountStatement = nullptr;

    auto pUpdateStatement = pTransaction->prepareStatement("UPDATE storage_downloadtickets SET revoked=1 WHERE streamuuid=? AND revoked=0");
    pUpdateStatement->setString(1, sNormalizedStreamUUID);
    pUpdateStatement->execute();
    pUpdateStatement = nullptr;

    pTransaction->commit();

    return nCount;
}

std::string CStorage::LogDownloadStart(const std::string& sTicketUUID, const std::string& sIPAddress, const LibMCData_uint64 nAbsoluteTimeStamp)
{
    std::string sNormalizedTicketUUID = AMCCommon::CUtils::normalizeUUIDString(sTicketUUID);
    std::string sTimestamp = AMCCommon::CChrono::convertToISO8601TimeUTC(nAbsoluteTimeStamp);
    std::string sDownloadUUID = AMCCommon::CUtils::createUUID();

    auto pTransaction = m_pSQLHandler->beginTransaction();

    auto pTicketStatement = pTransaction->prepareStatement("SELECT ticketuuid FROM storage_downloadtickets WHERE ticketuuid=?");
    pTicketStatement->setString(1, sNormalizedTicketUUID);
    if (!pTicketStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DOWNLOADTICKETNOTFOUND, "download ticket not found: " + sNormalizedTicketUUID);
    pTicketStatement = nullptr;

    auto pDownloadStatement = pTransaction->prepareStatement("INSERT INTO storage_downloads (downloaduuid, ticketuuid, ipaddress, timestamp) VALUES (?, ?, ?, ?)");
    pDownloadStatement->setString(1, sDownloadUUID);
    pDownloadStatement->setString(2, sNormalizedTicketUUID);
    pDownloadStatement->setString(3, sIPAddress);
    pDownloadStatement->setString(4, sTimestamp);
    pDownloadStatement->execute();
    pDownloadStatement = nullptr;

    pTransaction->commit();

    return sDownloadUUID;
}

void CStorage::LogDownloadFinish(const std::string& sDownloadUUID, const LibMCData_uint64 nSentBytes, const bool bSuccess, const LibMCData_uint64 nAbsoluteTimeStamp)
{
    std::string sNormalizedDownloadUUID = AMCCommon::CUtils::normalizeUUIDString(sDownloadUUID);
    std::string sTimestamp = AMCCommon::CChrono::convertToISO8601TimeUTC(nAbsoluteTimeStamp);

    auto pTransaction = m_pSQLHandler->beginTransaction();

    auto pSelectStatement = pTransaction->prepareStatement("SELECT downloaduuid FROM storage_downloads WHERE downloaduuid=?");
    pSelectStatement->setString(1, sNormalizedDownloadUUID);
    if (!pSelectStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DOWNLOADENTRYNOTFOUND, "download entry not found: " + sNormalizedDownloadUUID);
    pSelectStatement = nullptr;

    auto pUpdateStatement = pTransaction->prepareStatement("UPDATE storage_downloads SET sentbytes=?, success=?, finishedtimestamp=? WHERE downloaduuid=?");
    pUpdateStatement->setInt64(1, (int64_t)nSentBytes);
    pUpdateStatement->setInt(2, bSuccess ? 1 : 0);
    pUpdateStatement->setString(3, sTimestamp);
    pUpdateStatement->setString(4, sNormalizedDownloadUUID);
    pUpdateStatement->execute();
    pUpdateStatement = nullptr;

    pTransaction->commit();
}

void CStorage::deleteStreamInternal(const std::string& sNormalizedUUID, bool bOnlyIfNew)
{
    auto pTransaction = m_pSQLHandler->beginTransaction();

    auto pLinkStatement = pTransaction->prepareStatement("DELETE FROM journal_tempstreams WHERE streamuuid=?");
    pLinkStatement->setString(1, sNormalizedUUID);
    pLinkStatement->execute();
    pLinkStatement = nullptr;

    auto pTicketStatement = pTransaction->prepareStatement("UPDATE storage_downloadtickets SET revoked=1 WHERE streamuuid=?");
    pTicketStatement->setString(1, sNormalizedUUID);
    pTicketStatement->execute();
    pTicketStatement = nullptr;

    if (bOnlyIfNew) {
        auto pStreamStatement = pTransaction->prepareStatement("DELETE FROM storage_streams WHERE uuid=? AND status=?");
        pStreamStatement->setString(1, sNormalizedUUID);
        pStreamStatement->setString(2, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssNew));
        pStreamStatement->execute();
        pStreamStatement = nullptr;
    }
    else {
        auto pStreamStatement = pTransaction->prepareStatement("DELETE FROM storage_streams WHERE uuid=?");
        pStreamStatement->setString(1, sNormalizedUUID);
        pStreamStatement->execute();
        pStreamStatement = nullptr;
    }

    pTransaction->commit();

    AMCCommon::CUtils::deleteFileFromDisk(m_pStorageState->getStreamPath(sNormalizedUUID), false);
}

void CStorage::DeleteStream(const std::string& sUUID)
{
    std::string sParsedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);

    if (m_pStorageState->hasPartialWriter(sParsedUUID))
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STREAMHASACTIVEWRITER, "storage stream is still being written: " + sParsedUUID);

    auto pStreamStatement = m_pSQLHandler->prepareStatement("SELECT uuid FROM storage_streams WHERE uuid=?");
    pStreamStatement->setString(1, sParsedUUID);
    if (!pStreamStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTFOUND, "storage stream not found: " + sParsedUUID);
    pStreamStatement = nullptr;

    // Streams that belong to a build job or a build execution are part of that data and are never deleted here.
    auto pJobDataStatement = m_pSQLHandler->prepareStatement("SELECT uuid FROM buildjobdata WHERE storagestreamuuid=?");
    pJobDataStatement->setString(1, sParsedUUID);
    if (pJobDataStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "storage stream is attached to a build job: " + sParsedUUID);
    pJobDataStatement = nullptr;

    auto pExecutionDataStatement = m_pSQLHandler->prepareStatement("SELECT uuid FROM buildjobexecutiondata WHERE storagestreamuuid=?");
    pExecutionDataStatement->setString(1, sParsedUUID);
    if (pExecutionDataStatement->nextRow())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM, "storage stream is attached to a build execution: " + sParsedUUID);
    pExecutionDataStatement = nullptr;

    deleteStreamInternal(sParsedUUID, false);
}

LibMCData_uint32 CStorage::DiscardUnfinishedZIPStreams()
{
    std::vector<std::string> streamUUIDs;

    auto pStatement = m_pSQLHandler->prepareStatement("SELECT uuid FROM storage_streams WHERE mimetype=? AND status=?");
    pStatement->setString(1, "application/zip");
    pStatement->setString(2, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssNew));
    while (pStatement->nextRow()) {
        std::string sStreamUUID = pStatement->getColumnString(1);
        if (!m_pStorageState->hasPartialWriter(sStreamUUID))
            streamUUIDs.push_back(sStreamUUID);
    }
    pStatement = nullptr;

    for (auto& sStreamUUID : streamUUIDs)
        deleteStreamInternal(sStreamUUID, true);

    return (uint32_t)streamUUIDs.size();
}

std::string CStorage::ListStreamUUIDsByName(const std::string& sName)
{
    if (sName.empty())
        throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

    std::string sResult;

    auto pStatement = m_pSQLHandler->prepareStatement("SELECT uuid FROM storage_streams WHERE name=? AND status=? ORDER BY timestamp DESC");
    pStatement->setString(1, sName);
    pStatement->setString(2, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssValidated));
    while (pStatement->nextRow()) {
        if (!sResult.empty())
            sResult += ",";
        sResult += pStatement->getColumnString(1);
    }

    return sResult;
}
