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


Abstract: This is the class definition of CDataCapture

*/

#include "libmcdata_datacapture.hpp"
#include "libmcdata_interfaceexception.hpp"

// Include custom headers here.
#include "common_utils.hpp"
#include "common_chrono.hpp"

#include <vector>

using namespace LibMCData::Impl;

#define DATACAPTURE_SEGMENTSTATUS_STORED "stored"
#define DATACAPTURE_SEGMENTSTATUS_PURGED "purged"

/*************************************************************************************************************************
 Class definition of CDataCapture
**************************************************************************************************************************/

namespace {

	struct sStoredSegment {
		std::string m_sSegmentUUID;
		std::string m_sStreamUUID;
		uint64_t m_nSizeInBytes;
	};

	// Lists stored segments, oldest first. An empty capture UUID lists the segments of all captures.
	std::vector<sStoredSegment> listStoredSegments(AMCData::PSQLHandler pSQLHandler, const std::string& sCaptureUUID)
	{
		std::vector<sStoredSegment> segments;

		std::string sQuery = "SELECT uuid, storagestreamuuid, sizeinbytes FROM datacapture_segments WHERE status=?";
		if (!sCaptureUUID.empty())
			sQuery += " AND captureuuid=?";
		sQuery += " ORDER BY endtimestamp ASC, segmentindex ASC";

		auto pStatement = pSQLHandler->prepareStatement(sQuery);
		pStatement->setString(1, DATACAPTURE_SEGMENTSTATUS_STORED);
		if (!sCaptureUUID.empty())
			pStatement->setString(2, sCaptureUUID);

		while (pStatement->nextRow()) {
			sStoredSegment segment;
			segment.m_sSegmentUUID = pStatement->getColumnString(1);
			segment.m_sStreamUUID = pStatement->getColumnString(2);
			segment.m_nSizeInBytes = (uint64_t)pStatement->getColumnInt64(3);
			segments.push_back(segment);
		}

		return segments;
	}

}


CDataCapture::CDataCapture(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sUUID, const std::string& sExecutionUUID, const std::string& sIdentifier, const std::string& sName, const std::string& sTimeStamp)
	: m_pSQLHandler(pSQLHandler),
	m_pStorageState(pStorageState),
	m_sUUID(AMCCommon::CUtils::normalizeUUIDString(sUUID)),
	m_sExecutionUUID(AMCCommon::CUtils::normalizeUUIDString(sExecutionUUID)),
	m_sIdentifier(sIdentifier),
	m_sName(sName),
	m_sTimeStamp(sTimeStamp)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
	if (pStorageState.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
}

CDataCapture::~CDataCapture()
{

}

std::string CDataCapture::convertStatusToString(const LibMCData::eDataCaptureStatus eStatus)
{
	switch (eStatus) {
	case LibMCData::eDataCaptureStatus::Recording: return "recording";
	case LibMCData::eDataCaptureStatus::Finished: return "finished";
	case LibMCData::eDataCaptureStatus::Aborted: return "aborted";
	default:
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDDATACAPTURESTATUS);
	}
}

LibMCData::eDataCaptureStatus CDataCapture::convertStringToStatus(const std::string& sStatus)
{
	if (sStatus == "recording")
		return LibMCData::eDataCaptureStatus::Recording;
	if (sStatus == "finished")
		return LibMCData::eDataCaptureStatus::Finished;
	if (sStatus == "aborted")
		return LibMCData::eDataCaptureStatus::Aborted;

	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDDATACAPTURESTATUS, "invalid data capture status: " + sStatus);
}

CDataCapture* CDataCapture::createInDatabase(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sExecutionUUID, const std::string& sIdentifier, const std::string& sName, const std::string& sUserUUID, uint64_t nAbsoluteTimeStamp)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

	if (sIdentifier.empty() || !AMCCommon::CUtils::stringIsValidAlphanumericNameString(sIdentifier))
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDDATACAPTUREIDENTIFIER, "invalid data capture identifier: " + sIdentifier);
	if (sName.empty())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDDATACAPTURENAME);

	std::string sNormalizedExecutionUUID = AMCCommon::CUtils::normalizeUUIDString(sExecutionUUID);
	std::string sNormalizedUserUUID = AMCCommon::CUtils::normalizeUUIDString(sUserUUID);
	std::string sUUID = AMCCommon::CUtils::createUUID();
	std::string sTimeStamp = AMCCommon::CChrono::convertToISO8601TimeUTC(nAbsoluteTimeStamp);

	{
		auto pTransaction = pSQLHandler->beginTransaction();

		auto pDuplicateStatement = pTransaction->prepareStatement("SELECT uuid FROM datacaptures WHERE executionuuid=? AND identifier=? AND active=?");
		pDuplicateStatement->setString(1, sNormalizedExecutionUUID);
		pDuplicateStatement->setString(2, sIdentifier);
		pDuplicateStatement->setInt(3, 1);
		if (pDuplicateStatement->nextRow())
			throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DUPLICATEDATACAPTUREIDENTIFIER, "duplicate data capture identifier: " + sIdentifier);
		pDuplicateStatement = nullptr;

		auto pInsertStatement = pTransaction->prepareStatement("INSERT INTO datacaptures (uuid, executionuuid, identifier, name, status, useruuid, active, timestamp, endtimestamp) VALUES (?, ?, ?, ?, ?, ?, 1, ?, ?)");
		pInsertStatement->setString(1, sUUID);
		pInsertStatement->setString(2, sNormalizedExecutionUUID);
		pInsertStatement->setString(3, sIdentifier);
		pInsertStatement->setString(4, sName);
		pInsertStatement->setString(5, convertStatusToString(LibMCData::eDataCaptureStatus::Recording));
		pInsertStatement->setString(6, sNormalizedUserUUID);
		pInsertStatement->setString(7, sTimeStamp);
		pInsertStatement->setString(8, "");
		pInsertStatement->execute();
		pInsertStatement = nullptr;

		pTransaction->commit();
	}

	return new CDataCapture(pSQLHandler, pStorageState, sUUID, sNormalizedExecutionUUID, sIdentifier, sName, sTimeStamp);
}

CDataCapture* CDataCapture::makeFromDatabase(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sExecutionUUID, const std::string& sIdentifier)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

	std::string sNormalizedExecutionUUID = AMCCommon::CUtils::normalizeUUIDString(sExecutionUUID);

	auto pStatement = pSQLHandler->prepareStatement("SELECT uuid, name, timestamp FROM datacaptures WHERE executionuuid=? AND identifier=? AND active=?");
	pStatement->setString(1, sNormalizedExecutionUUID);
	pStatement->setString(2, sIdentifier);
	pStatement->setInt(3, 1);
	if (!pStatement->nextRow())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DATACAPTURENOTFOUND, "data capture not found: " + sIdentifier);

	std::string sUUID = pStatement->getColumnString(1);
	std::string sName = pStatement->getColumnString(2);
	std::string sTimeStamp = pStatement->getColumnString(3);
	pStatement = nullptr;

	return new CDataCapture(pSQLHandler, pStorageState, sUUID, sNormalizedExecutionUUID, sIdentifier, sName, sTimeStamp);
}

CDataCapture* CDataCapture::makeFromDatabaseByUUID(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sCaptureUUID)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

	std::string sNormalizedCaptureUUID = AMCCommon::CUtils::normalizeUUIDString(sCaptureUUID);

	auto pStatement = pSQLHandler->prepareStatement("SELECT executionuuid, identifier, name, timestamp FROM datacaptures WHERE uuid=? AND active=?");
	pStatement->setString(1, sNormalizedCaptureUUID);
	pStatement->setInt(2, 1);
	if (!pStatement->nextRow())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DATACAPTURENOTFOUND, "data capture not found: " + sNormalizedCaptureUUID);

	std::string sExecutionUUID = pStatement->getColumnString(1);
	std::string sIdentifier = pStatement->getColumnString(2);
	std::string sName = pStatement->getColumnString(3);
	std::string sTimeStamp = pStatement->getColumnString(4);
	pStatement = nullptr;

	return new CDataCapture(pSQLHandler, pStorageState, sNormalizedCaptureUUID, sExecutionUUID, sIdentifier, sName, sTimeStamp);
}

uint32_t CDataCapture::countInExecution(AMCData::PSQLHandler pSQLHandler, const std::string& sExecutionUUID)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

	auto pStatement = pSQLHandler->prepareStatement("SELECT COUNT(*) FROM datacaptures WHERE executionuuid=? AND active=?");
	pStatement->setString(1, AMCCommon::CUtils::normalizeUUIDString(sExecutionUUID));
	pStatement->setInt(2, 1);
	if (!pStatement->nextRow())
		return 0;

	return (uint32_t)pStatement->getColumnInt64(1);
}

CDataCapture* CDataCapture::makeFromExecutionIndex(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sExecutionUUID, uint32_t nIndex)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

	std::string sNormalizedExecutionUUID = AMCCommon::CUtils::normalizeUUIDString(sExecutionUUID);

	auto pStatement = pSQLHandler->prepareStatement("SELECT uuid, identifier, name, timestamp FROM datacaptures WHERE executionuuid=? AND active=? ORDER BY timestamp ASC, identifier ASC");
	pStatement->setString(1, sNormalizedExecutionUUID);
	pStatement->setInt(2, 1);

	uint32_t nRowIndex = 0;
	while (pStatement->nextRow()) {
		if (nRowIndex == nIndex) {
			std::string sUUID = pStatement->getColumnString(1);
			std::string sIdentifier = pStatement->getColumnString(2);
			std::string sName = pStatement->getColumnString(3);
			std::string sTimeStamp = pStatement->getColumnString(4);
			pStatement = nullptr;

			return new CDataCapture(pSQLHandler, pStorageState, sUUID, sNormalizedExecutionUUID, sIdentifier, sName, sTimeStamp);
		}
		nRowIndex++;
	}

	throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DATACAPTURENOTFOUND, "data capture index out of bounds: " + std::to_string(nIndex));
}

bool CDataCapture::existsInDatabase(AMCData::PSQLHandler pSQLHandler, const std::string& sExecutionUUID, const std::string& sIdentifier)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

	auto pStatement = pSQLHandler->prepareStatement("SELECT uuid FROM datacaptures WHERE executionuuid=? AND identifier=? AND active=?");
	pStatement->setString(1, AMCCommon::CUtils::normalizeUUIDString(sExecutionUUID));
	pStatement->setString(2, sIdentifier);
	pStatement->setInt(3, 1);
	return pStatement->nextRow();
}

bool CDataCapture::purgeSegment(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sSegmentUUID, const std::string& sStreamUUID)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
	if (pStorageState.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

	std::string sNormalizedStreamUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
	std::string sPath = pStorageState->getStreamPath(sNormalizedStreamUUID);

	// The file goes first: a segment that is still on disk must stay visible in the manifest.
	if (AMCCommon::CUtils::fileOrPathExistsOnDisk(sPath)) {
		AMCCommon::CUtils::deleteFileFromDisk(sPath, false);
		if (AMCCommon::CUtils::fileOrPathExistsOnDisk(sPath))
			return false;
	}

	auto pTransaction = pSQLHandler->beginTransaction();

	auto pSegmentStatement = pTransaction->prepareStatement("UPDATE datacapture_segments SET status=? WHERE uuid=?");
	pSegmentStatement->setString(1, DATACAPTURE_SEGMENTSTATUS_PURGED);
	pSegmentStatement->setString(2, sSegmentUUID);
	pSegmentStatement->execute();
	pSegmentStatement = nullptr;

	auto pLinkStatement = pTransaction->prepareStatement("DELETE FROM journal_tempstreams WHERE streamuuid=?");
	pLinkStatement->setString(1, sNormalizedStreamUUID);
	pLinkStatement->execute();
	pLinkStatement = nullptr;

	auto pStreamStatement = pTransaction->prepareStatement("DELETE FROM storage_streams WHERE uuid=?");
	pStreamStatement->setString(1, sNormalizedStreamUUID);
	pStreamStatement->execute();
	pStreamStatement = nullptr;

	pTransaction->commit();

	return true;
}

uint64_t CDataCapture::getTotalStoredSize(AMCData::PSQLHandler pSQLHandler)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

	auto pStatement = pSQLHandler->prepareStatement("SELECT COALESCE(SUM(sizeinbytes), 0) FROM datacapture_segments WHERE status=?");
	pStatement->setString(1, DATACAPTURE_SEGMENTSTATUS_STORED);
	if (!pStatement->nextRow())
		return 0;

	return (uint64_t)pStatement->getColumnInt64(1);
}

uint32_t CDataCapture::enforceStorageLimit(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, uint64_t nLimitInBytes)
{
	auto segments = listStoredSegments(pSQLHandler, "");

	uint64_t nTotalSize = 0;
	for (auto& segment : segments)
		nTotalSize += segment.m_nSizeInBytes;

	uint32_t nPurgedCount = 0;
	for (auto& segment : segments) {
		if (nTotalSize <= nLimitInBytes)
			break;

		// A segment whose file is in use is skipped and tried again on the next enforcement.
		if (purgeSegment(pSQLHandler, pStorageState, segment.m_sSegmentUUID, segment.m_sStreamUUID)) {
			nTotalSize -= segment.m_nSizeInBytes;
			nPurgedCount++;
		}
	}

	return nPurgedCount;
}

LibMCData::eDataCaptureStatus CDataCapture::readStatus(AMCData::CSQLTransaction* pTransaction)
{
	AMCData::PSQLStatement pStatement;
	if (pTransaction != nullptr)
		pStatement = pTransaction->prepareStatement("SELECT status FROM datacaptures WHERE uuid=?");
	else
		pStatement = m_pSQLHandler->prepareStatement("SELECT status FROM datacaptures WHERE uuid=?");

	pStatement->setString(1, m_sUUID);
	if (!pStatement->nextRow())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DATACAPTURENOTFOUND, "data capture not found: " + m_sUUID);

	return convertStringToStatus(pStatement->getColumnString(1));
}

std::string CDataCapture::GetUUID()
{
	return m_sUUID;
}

std::string CDataCapture::GetExecutionUUID()
{
	return m_sExecutionUUID;
}

std::string CDataCapture::GetIdentifier()
{
	return m_sIdentifier;
}

std::string CDataCapture::GetName()
{
	return m_sName;
}

LibMCData::eDataCaptureStatus CDataCapture::GetStatus()
{
	return readStatus(nullptr);
}

std::string CDataCapture::GetTimeStamp()
{
	return m_sTimeStamp;
}

void CDataCapture::AddColumn(const LibMCData_uint32 nColumnIndex, const std::string & sIdentifier, const std::string & sDescription, const LibMCData_uint32 nColumnType)
{
	if (sIdentifier.empty())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

	auto pTransaction = m_pSQLHandler->beginTransaction();

	if (readStatus(pTransaction.get()) != LibMCData::eDataCaptureStatus::Recording)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DATACAPTURENOTRECORDING);

	auto pDuplicateStatement = pTransaction->prepareStatement("SELECT columnindex FROM datacapture_columns WHERE captureuuid=? AND (columnindex=? OR identifier=?)");
	pDuplicateStatement->setString(1, m_sUUID);
	pDuplicateStatement->setInt64(2, nColumnIndex);
	pDuplicateStatement->setString(3, sIdentifier);
	if (pDuplicateStatement->nextRow())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DUPLICATEDATACAPTURECOLUMN, "duplicate data capture column: " + sIdentifier);
	pDuplicateStatement = nullptr;

	auto pInsertStatement = pTransaction->prepareStatement("INSERT INTO datacapture_columns (captureuuid, columnindex, identifier, description, columntype) VALUES (?, ?, ?, ?, ?)");
	pInsertStatement->setString(1, m_sUUID);
	pInsertStatement->setInt64(2, nColumnIndex);
	pInsertStatement->setString(3, sIdentifier);
	pInsertStatement->setString(4, sDescription);
	pInsertStatement->setInt64(5, nColumnType);
	pInsertStatement->execute();
	pInsertStatement = nullptr;

	pTransaction->commit();
}

LibMCData_uint32 CDataCapture::GetColumnCount()
{
	auto pStatement = m_pSQLHandler->prepareStatement("SELECT COUNT(*) FROM datacapture_columns WHERE captureuuid=?");
	pStatement->setString(1, m_sUUID);
	if (!pStatement->nextRow())
		return 0;

	return (uint32_t)pStatement->getColumnInt64(1);
}

void CDataCapture::GetColumnInformation(const LibMCData_uint32 nColumnIndex, std::string & sIdentifier, std::string & sDescription, LibMCData_uint32 & nColumnType)
{
	auto pStatement = m_pSQLHandler->prepareStatement("SELECT identifier, description, columntype FROM datacapture_columns WHERE captureuuid=? AND columnindex=?");
	pStatement->setString(1, m_sUUID);
	pStatement->setInt64(2, nColumnIndex);
	if (!pStatement->nextRow())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDDATACAPTURECOLUMNINDEX, "invalid data capture column index: " + std::to_string(nColumnIndex));

	sIdentifier = pStatement->getColumnString(1);
	sDescription = pStatement->getColumnString(2);
	nColumnType = (uint32_t)pStatement->getColumnInt64(3);
}

void CDataCapture::AddSegment(const LibMCData_uint32 nSegmentIndex, const std::string & sStreamUUID, const LibMCData_uint64 nFirstRow, const LibMCData_uint64 nRowCount, const LibMCData_uint32 nChunkCount, const LibMCData_uint64 nSizeInBytes, const LibMCData_uint64 nStartTimeStamp, const LibMCData_uint64 nEndTimeStamp)
{
	std::string sNormalizedStreamUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
	std::string sSegmentUUID = AMCCommon::CUtils::createUUID();
	std::string sTimeStamp = AMCCommon::CChrono::convertToISO8601TimeUTC(nEndTimeStamp);

	auto pTransaction = m_pSQLHandler->beginTransaction();

	if (readStatus(pTransaction.get()) != LibMCData::eDataCaptureStatus::Recording)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DATACAPTURENOTRECORDING);

	auto pStreamStatement = pTransaction->prepareStatement("SELECT uuid FROM storage_streams WHERE uuid=?");
	pStreamStatement->setString(1, sNormalizedStreamUUID);
	if (!pStreamStatement->nextRow())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_STORAGESTREAMNOTFOUND, "storage stream not found for data capture segment: " + sNormalizedStreamUUID);
	pStreamStatement = nullptr;

	auto pDuplicateStatement = pTransaction->prepareStatement("SELECT uuid FROM datacapture_segments WHERE captureuuid=? AND segmentindex=?");
	pDuplicateStatement->setString(1, m_sUUID);
	pDuplicateStatement->setInt64(2, nSegmentIndex);
	if (pDuplicateStatement->nextRow())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DUPLICATEDATACAPTURESEGMENT, "duplicate data capture segment: " + std::to_string(nSegmentIndex));
	pDuplicateStatement = nullptr;

	auto pInsertStatement = pTransaction->prepareStatement("INSERT INTO datacapture_segments (uuid, captureuuid, segmentindex, storagestreamuuid, firstrow, rowcount, chunkcount, sizeinbytes, starttimestamp, endtimestamp, status, timestamp) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
	pInsertStatement->setString(1, sSegmentUUID);
	pInsertStatement->setString(2, m_sUUID);
	pInsertStatement->setInt64(3, nSegmentIndex);
	pInsertStatement->setString(4, sNormalizedStreamUUID);
	pInsertStatement->setInt64(5, (int64_t)nFirstRow);
	pInsertStatement->setInt64(6, (int64_t)nRowCount);
	pInsertStatement->setInt64(7, nChunkCount);
	pInsertStatement->setInt64(8, (int64_t)nSizeInBytes);
	pInsertStatement->setInt64(9, (int64_t)nStartTimeStamp);
	pInsertStatement->setInt64(10, (int64_t)nEndTimeStamp);
	pInsertStatement->setString(11, DATACAPTURE_SEGMENTSTATUS_STORED);
	pInsertStatement->setString(12, sTimeStamp);
	pInsertStatement->execute();
	pInsertStatement = nullptr;

	pTransaction->commit();
}

LibMCData_uint32 CDataCapture::GetSegmentCount()
{
	auto pStatement = m_pSQLHandler->prepareStatement("SELECT COUNT(*) FROM datacapture_segments WHERE captureuuid=?");
	pStatement->setString(1, m_sUUID);
	if (!pStatement->nextRow())
		return 0;

	return (uint32_t)pStatement->getColumnInt64(1);
}

void CDataCapture::GetSegmentInformation(const LibMCData_uint32 nSegmentIndex, std::string & sStreamUUID, LibMCData_uint64 & nFirstRow, LibMCData_uint64 & nRowCount, LibMCData_uint64 & nSizeInBytes, bool & bIsPurged)
{
	auto pStatement = m_pSQLHandler->prepareStatement("SELECT storagestreamuuid, firstrow, rowcount, sizeinbytes, status FROM datacapture_segments WHERE captureuuid=? AND segmentindex=?");
	pStatement->setString(1, m_sUUID);
	pStatement->setInt64(2, nSegmentIndex);
	if (!pStatement->nextRow())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDDATACAPTURESEGMENTINDEX, "invalid data capture segment index: " + std::to_string(nSegmentIndex));

	sStreamUUID = pStatement->getColumnString(1);
	nFirstRow = (uint64_t)pStatement->getColumnInt64(2);
	nRowCount = (uint64_t)pStatement->getColumnInt64(3);
	nSizeInBytes = (uint64_t)pStatement->getColumnInt64(4);
	bIsPurged = (pStatement->getColumnString(5) != DATACAPTURE_SEGMENTSTATUS_STORED);
}

LibMCData_uint32 CDataCapture::GetSegmentChunkCount(const LibMCData_uint32 nSegmentIndex)
{
	auto pStatement = m_pSQLHandler->prepareStatement("SELECT chunkcount FROM datacapture_segments WHERE captureuuid=? AND segmentindex=?");
	pStatement->setString(1, m_sUUID);
	pStatement->setInt64(2, nSegmentIndex);
	if (!pStatement->nextRow())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDDATACAPTURESEGMENTINDEX, "invalid data capture segment index: " + std::to_string(nSegmentIndex));

	return (uint32_t)pStatement->getColumnInt64(1);
}

LibMCData_uint64 CDataCapture::GetRowCount()
{
	auto pStatement = m_pSQLHandler->prepareStatement("SELECT COALESCE(SUM(rowcount), 0) FROM datacapture_segments WHERE captureuuid=?");
	pStatement->setString(1, m_sUUID);
	if (!pStatement->nextRow())
		return 0;

	return (uint64_t)pStatement->getColumnInt64(1);
}

LibMCData_uint64 CDataCapture::GetStoredSizeInBytes()
{
	auto pStatement = m_pSQLHandler->prepareStatement("SELECT COALESCE(SUM(sizeinbytes), 0) FROM datacapture_segments WHERE captureuuid=? AND status=?");
	pStatement->setString(1, m_sUUID);
	pStatement->setString(2, DATACAPTURE_SEGMENTSTATUS_STORED);
	if (!pStatement->nextRow())
		return 0;

	return (uint64_t)pStatement->getColumnInt64(1);
}

void CDataCapture::Finish(const LibMCData_uint64 nAbsoluteTimeStamp)
{
	std::string sTimeStamp = AMCCommon::CChrono::convertToISO8601TimeUTC(nAbsoluteTimeStamp);

	auto pTransaction = m_pSQLHandler->beginTransaction();

	if (readStatus(pTransaction.get()) != LibMCData::eDataCaptureStatus::Recording)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DATACAPTURENOTRECORDING);

	auto pStatement = pTransaction->prepareStatement("UPDATE datacaptures SET status=?, endtimestamp=? WHERE uuid=?");
	pStatement->setString(1, convertStatusToString(LibMCData::eDataCaptureStatus::Finished));
	pStatement->setString(2, sTimeStamp);
	pStatement->setString(3, m_sUUID);
	pStatement->execute();
	pStatement = nullptr;

	pTransaction->commit();
}

void CDataCapture::Abort(const LibMCData_uint64 nAbsoluteTimeStamp)
{
	std::string sTimeStamp = AMCCommon::CChrono::convertToISO8601TimeUTC(nAbsoluteTimeStamp);

	if (readStatus(nullptr) == LibMCData::eDataCaptureStatus::Aborted)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_DATACAPTUREALREADYABORTED);

	// The status changes first, so that no further segment can be registered while the stored ones are deleted.
	{
		auto pStatement = m_pSQLHandler->prepareStatement("UPDATE datacaptures SET status=?, endtimestamp=? WHERE uuid=?");
		pStatement->setString(1, convertStatusToString(LibMCData::eDataCaptureStatus::Aborted));
		pStatement->setString(2, sTimeStamp);
		pStatement->setString(3, m_sUUID);
		pStatement->execute();
	}

	auto segments = listStoredSegments(m_pSQLHandler, m_sUUID);
	for (auto& segment : segments)
		purgeSegment(m_pSQLHandler, m_pStorageState, segment.m_sSegmentUUID, segment.m_sStreamUUID);
}
