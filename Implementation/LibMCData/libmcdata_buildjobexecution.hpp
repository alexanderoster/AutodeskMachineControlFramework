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


Abstract: This is the class declaration of CBuildJobExecution

*/


#ifndef __LIBMCDATA_BUILDJOBEXECUTION
#define __LIBMCDATA_BUILDJOBEXECUTION

#include <cstdint>
#include <optional>
#include "libmcdata_interfaces.hpp"
#include "libmcdata_buildjobexecutiondata.hpp"

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
 Parent build job values, read together with the execution (LEFT JOIN on buildjobs).
**************************************************************************************************************************/

struct sBuildJobExecutionLoadedJob {
	std::string m_sName;
	eBuildJobStatus m_Status;
	uint32_t m_nLayerCount;
	std::string m_sThumbnailStreamUUID;
};

/*************************************************************************************************************************
 Execution values that are loaded together with a build job execution and returned by the GetLoaded* getters
 without another database query. The other mutable getters (GetStatus, GetDescription, ...) keep reading fresh.
**************************************************************************************************************************/

struct sBuildJobExecutionLoadedValues {
	eBuildJobExecutionStatus m_Status;
	std::string m_sDescription;
	std::optional<uint64_t> m_EndTimeStamp; // empty if endjournaltimestamp is not a valid time stamp (as while in process)
};

/*************************************************************************************************************************
 Class declaration of CBuildJobExecution
**************************************************************************************************************************/

class CBuildJobExecution : public virtual IBuildJobExecution, public virtual CBase {
private:

	AMCData::PSQLHandler m_pSQLHandler;
	std::string m_sExecutionUUID;
	std::string m_sUserUUID;
	std::string m_sJournalUUID;
	uint64_t m_nStartJournalTimestamp;
	AMCData::PStorageState m_pStorageState;

	// Build Job related information
	std::string m_sJobUUID;
	sBuildJobExecutionLoadedJob m_LoadedJob;

	sBuildJobExecutionLoadedValues m_LoadedValues;

	CBuildJobExecutionData* makeJobExecutionDataEx(AMCData::CSQLStatement* pStatement);
	IBuildJobExecutionDataIterator* listJobExecutionDataEx(AMCData::CSQLStatement* pStatement);

	// Parses an endjournaltimestamp column; empty if it is not a valid time stamp.
	static std::optional<uint64_t> parseEndTimeStamp(const std::string& sEndTimeStamp);
	static uint64_t endTimeStampFromValues(const std::string& sExecutionUUID, uint64_t nStartTimeStamp, eBuildJobExecutionStatus eStatus, const std::optional<uint64_t>& endTimeStamp);
	static uint64_t elapsedTimeFromValues(const std::string& sExecutionUUID, uint64_t nStartTimeStamp, bool bIsFromCurrentJournal, eBuildJobExecutionStatus eStatus, const std::optional<uint64_t>& endTimeStamp, const LibMCData_uint64 nGlobalTimerInMicroseconds, const bool bThrowExceptionInFailure);

	bool isFromCurrentJournal();

public:

	static void bumpIncrementalID(AMCData::PSQLHandler pSQLHandler, const std::string& sExecutionUUID);

	// "SELECT <columns> FROM buildjobexecutions LEFT JOIN buildjobs" in the column order makeFromStatement reads,
	// without any condition. Callers append their WHERE and ORDER BY clauses.
	static std::string selectQueryWithoutConditions();

	static CBuildJobExecution* makeFrom(CBuildJobExecution * pBuildJobExecution);

	static std::shared_ptr<CBuildJobExecution> makeSharedFrom(CBuildJobExecution* pBuildJobExecution);

	static CBuildJobExecution* makeFromDatabase(AMCData::PSQLHandler pSQLHandler, const std::string& sExecutionUUID, AMCData::PStorageState pStorageState);

	static std::shared_ptr<CBuildJobExecution> makeSharedFromDatabase(AMCData::PSQLHandler pSQLHandler, const std::string& sExecutionUUID, AMCData::PStorageState pStorageState);

	static CBuildJobExecution* makeFromStatement(AMCData::PSQLHandler pSQLHandler, AMCData::PSQLStatement pStatement, AMCData::PStorageState pStorageState);

	static std::shared_ptr<CBuildJobExecution> makeSharedFromStatement(AMCData::PSQLHandler pSQLHandler, AMCData::PSQLStatement pStatement, AMCData::PStorageState pStorageState);

	CBuildJobExecution(AMCData::PSQLHandler pSQLHandler, const std::string& sExecutionUUID, const std::string& sJobUUID, const std::string& sJournalUUID, const std::string& sUserUUID, uint64_t nStartJournalTimeStamp, const sBuildJobExecutionLoadedJob& loadedJob, const sBuildJobExecutionLoadedValues& loadedValues, AMCData::PStorageState pStorageState);

	virtual ~CBuildJobExecution();

	std::string GetExecutionUUID() override;

	std::string GetJobUUID() override;

	std::string GetJobName() override;

	LibMCData::eBuildJobStatus GetJobStatus() override;

	std::string GetJobStatusString() override;

	LibMCData_uint32 GetJobLayerCount() override;

	LibMCData::eBuildJobExecutionStatus GetStatus() override;

	std::string GetStatusString() override;

	void ChangeStatus(const LibMCData::eBuildJobExecutionStatus eNewExecutionStatus, const LibMCData_uint64 nAbsoluteEndTimeStampInMicrosecondsSince1970) override;

	std::string GetDescription() override;

	void SetDescription(const std::string & sNewDescription) override;

	std::string GetJournalUUID() override;

	std::string GetUserUUID() override;

	LibMCData_uint64 GetStartTimeStampInMicroseconds() override;

	LibMCData_uint64 GetEndTimeStampInMicroseconds() override;

	LibMCData_uint64 ComputeElapsedTimeInMicroseconds(const LibMCData_uint64 nGlobalTimerInMicroseconds, const bool bThrowExceptionInFailure);

	LibMCData::eBuildJobExecutionStatus GetLoadedStatus() override;

	std::string GetLoadedStatusString() override;

	std::string GetLoadedDescription() override;

	LibMCData_uint64 GetLoadedEndTimeStampInMicroseconds() override;

	LibMCData_uint64 ComputeLoadedElapsedTimeInMicroseconds(const LibMCData_uint64 nGlobalTimerInMicroseconds, const bool bThrowExceptionInFailure) override;

	bool JobHasThumbnailStream() override;

	std::string GetJobThumbnailStreamUUID() override;

	std::string AddJobExecutionData(const std::string& sIdentifier, const std::string& sName, IStorageStream* pStream, const LibMCData::eCustomDataType eDataType, const std::string& sUserUUID, const LibMCData_uint64 nAbsoluteTimeStamp) override;

	IBuildJobExecutionDataIterator* ListJobExecutionDataByType(const LibMCData::eCustomDataType eDataType) override;

	IBuildJobExecutionDataIterator* ListJobExecutionData() override;

	IBuildJobExecutionData* RetrieveJobExecutionData(const std::string& sDataUUID) override;

	IBuildJobExecutionData* RetrieveJobExecutionDataByIdentifier(const std::string& sIdentifier) override;

	bool HasJobExecutionDataUUID(const std::string& sUUID) override;

	bool HasJobExecutionDataIdentifier(const std::string& sIdentifier) override;

	void StoreMetaDataString(const std::string& sKey, const std::string& sValue, const LibMCData_uint64 nAbsoluteTimeStamp) override;

	bool HasMetaDataString(const std::string& sKey) override;

	std::string GetMetaDataString(const std::string& sKey) override;

	static std::string convertBuildJobExecutionStatusToString(const LibMCData::eBuildJobExecutionStatus eStatus);
	static LibMCData::eBuildJobExecutionStatus convertStringToBuildJobExecutionStatus(const std::string& sValue);


};

} // namespace Impl
} // namespace LibMCData

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDATA_BUILDJOBEXECUTION
