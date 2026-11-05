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


Abstract: This is the class declaration of CDataCapture

*/


#ifndef __LIBMCDATA_DATACAPTURE
#define __LIBMCDATA_DATACAPTURE

#include "libmcdata_interfaces.hpp"

// Parent classes
#include "libmcdata_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "amcdata_sqlhandler.hpp"
#include "amcdata_storagestate.hpp"

namespace LibMCData {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CDataCapture

 A data capture manifest lives in three tables of the main database:
   datacaptures          one row per capture, owned by a build job execution
   datacapture_columns   the schema of the capture
   datacapture_segments  one row per finished segment stream, modelled on telemetry_chunks
 A segment row survives the deletion of its stream: retention marks it as purged instead of removing it,
 so the manifest keeps telling which row range has existed.
**************************************************************************************************************************/

class CDataCapture : public virtual IDataCapture, public virtual CBase {
private:

	AMCData::PSQLHandler m_pSQLHandler;
	AMCData::PStorageState m_pStorageState;

	std::string m_sUUID;
	std::string m_sExecutionUUID;
	std::string m_sIdentifier;
	std::string m_sName;
	std::string m_sTimeStamp;

	LibMCData::eDataCaptureStatus readStatus(AMCData::CSQLTransaction* pTransaction);

public:

	static std::string convertStatusToString(const LibMCData::eDataCaptureStatus eStatus);

	static LibMCData::eDataCaptureStatus convertStringToStatus(const std::string& sStatus);

	static CDataCapture* createInDatabase(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sExecutionUUID, const std::string& sIdentifier, const std::string& sName, const std::string& sUserUUID, uint64_t nAbsoluteTimeStamp);

	static CDataCapture* makeFromDatabase(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sExecutionUUID, const std::string& sIdentifier);

	static bool existsInDatabase(AMCData::PSQLHandler pSQLHandler, const std::string& sExecutionUUID, const std::string& sIdentifier);

	static CDataCapture* makeFromDatabaseByUUID(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sCaptureUUID);

	static uint32_t countInExecution(AMCData::PSQLHandler pSQLHandler, const std::string& sExecutionUUID);

	// Captures of an execution are ordered by their creation time.
	static CDataCapture* makeFromExecutionIndex(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sExecutionUUID, uint32_t nIndex);

	// Deletes the stream of a stored segment from disk and marks the segment as purged.
	// Returns false and changes nothing if the file could not be deleted, for example because it is open.
	static bool purgeSegment(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sSegmentUUID, const std::string& sStreamUUID);

	// Returns the size of all segments of all captures that are stored on disk.
	static uint64_t getTotalStoredSize(AMCData::PSQLHandler pSQLHandler);

	// Purges the oldest stored segments of all captures until the total stored size is within the limit.
	static uint32_t enforceStorageLimit(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, uint64_t nLimitInBytes);

	CDataCapture(AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState, const std::string& sUUID, const std::string& sExecutionUUID, const std::string& sIdentifier, const std::string& sName, const std::string& sTimeStamp);

	virtual ~CDataCapture();

	std::string GetUUID() override;

	std::string GetExecutionUUID() override;

	std::string GetIdentifier() override;

	std::string GetName() override;

	LibMCData::eDataCaptureStatus GetStatus() override;

	std::string GetTimeStamp() override;

	void AddColumn(const LibMCData_uint32 nColumnIndex, const std::string & sIdentifier, const std::string & sDescription, const LibMCData_uint32 nColumnType) override;

	LibMCData_uint32 GetColumnCount() override;

	void GetColumnInformation(const LibMCData_uint32 nColumnIndex, std::string & sIdentifier, std::string & sDescription, LibMCData_uint32 & nColumnType) override;

	void AddSegment(const LibMCData_uint32 nSegmentIndex, const std::string & sStreamUUID, const LibMCData_uint64 nFirstRow, const LibMCData_uint64 nRowCount, const LibMCData_uint32 nChunkCount, const LibMCData_uint64 nSizeInBytes, const LibMCData_uint64 nStartTimeStamp, const LibMCData_uint64 nEndTimeStamp) override;

	LibMCData_uint32 GetSegmentCount() override;

	void GetSegmentInformation(const LibMCData_uint32 nSegmentIndex, std::string & sStreamUUID, LibMCData_uint64 & nFirstRow, LibMCData_uint64 & nRowCount, LibMCData_uint64 & nSizeInBytes, bool & bIsPurged) override;

	LibMCData_uint32 GetSegmentChunkCount(const LibMCData_uint32 nSegmentIndex) override;

	LibMCData_uint64 GetRowCount() override;

	LibMCData_uint64 GetStoredSizeInBytes() override;

	void Finish(const LibMCData_uint64 nAbsoluteTimeStamp) override;

	void Abort(const LibMCData_uint64 nAbsoluteTimeStamp) override;

};

} // namespace Impl
} // namespace LibMCData

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDATA_DATACAPTURE
