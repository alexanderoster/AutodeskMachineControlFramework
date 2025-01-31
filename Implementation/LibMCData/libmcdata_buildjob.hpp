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


Abstract: This is the class declaration of CBuildJob

*/


#ifndef __LIBMCDATA_BUILDJOB
#define __LIBMCDATA_BUILDJOB

#include "libmcdata_interfaces.hpp"

// Parent classes
#include "libmcdata_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "amcdata_storagestate.hpp"
#include "amcdata_sqlhandler.hpp"
#include "amcdata_journal.hpp"

namespace LibMCData {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CBuildJob 
**************************************************************************************************************************/

class CBuildJob;
typedef std::shared_ptr<CBuildJob> PBuildJob;

class CBuildJobData;
typedef std::shared_ptr<CBuildJobData> PBuildJobData;

class CBuildJob : public virtual IBuildJob, public virtual CBase {
private:

    std::string m_sUUID;
    std::string m_sName;
    LibMCData::eBuildJobStatus m_eJobStatus;
    std::string m_sTimeStamp;
    std::string m_sStorageStreamUUID;
    std::string m_sUserUUID;
    std::string m_sUserName;
    std::string m_sThumbnailStreamUUID;
    uint32_t m_nLayerCount;
    uint32_t m_nExecutionCount;
    AMCData::PStorageState m_pStorageState;
    AMCData::PSQLHandler m_pSQLHandler;

    void ensureUpdate (const std::string sUpdateUUID, uint32_t nErrorCode);

    CBuildJobData * makeJobDataEx(AMCData::CSQLStatement* pStatement);
    IBuildJobDataIterator* listJobDataEx(AMCData::CSQLStatement * pStatement);

protected:

    CBuildJob(const std::string& sUUID, const std::string & sName, LibMCData::eBuildJobStatus eJobStatus, const std::string & sTimeStamp, const std::string & sStorageStreamUUID, const std::string & sUserUUID, const std::string & sUserName, uint32_t nLayerCount, uint32_t nExecutionCount, const std::string & sThumbnailStreamUUID, AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState);

public:

    static CBuildJob* make(const std::string& sUUID, const std::string & sName, LibMCData::eBuildJobStatus eJobStatus, const std::string & sTimeStamp, const std::string & sStorageStreamUUID, const std::string & sUserUUID, const std::string& sUserName, uint32_t nLayerCount, uint32_t nExecutionCount, const std::string& sThumbnailStreamUUID, AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState);
    static CBuildJob* makeFromDatabase(const std::string& sJobUUID, AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState);
    static CBuildJob* makeFrom(CBuildJob* pBuildJob);

    static PBuildJob makeShared(const std::string& sUUID, const std::string& sName, LibMCData::eBuildJobStatus eJobStatus, const std::string& sTimeStamp, const std::string& sStorageStreamUUID, const std::string& sUserUUID, const std::string& sUserName, uint32_t nLayerCount, uint32_t nExecutionCount, const std::string& sThumbnailStreamUUID, AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState);
    static PBuildJob makeSharedFromDatabase(const std::string& sJobUUID, AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState);
    static PBuildJob makeSharedFrom(CBuildJob* pBuildJob);

    std::string GetUUID() override;

	std::string GetName() override;

    LibMCData_uint32 GetLayerCount() override;

    LibMCData_uint32 GetExecutionCount() override;

	LibMCData::eBuildJobStatus GetStatus() override;

	std::string GetTimeStamp() override;

    std::string GetCreatorUUID() override;

    std::string GetCreatorName() override;

    bool HasThumbnailStream() override;

    std::string GetThumbnailStreamUUID() override;

    void SetThumbnailStreamUUID(const std::string& sStreamUUID) override;

	IStorageStream * GetStorageStream() override;

    std::string GetStorageStreamUUID() override;

    void StartValidating() override;

    void FinishValidating(const LibMCData_uint32 nLayerCount) override;

    void ArchiveJob() override;

    void UnArchiveJob() override;

    void DeleteJob() override;
    
    bool JobCanBeArchived() override;
    
    std::string AddJobData(const std::string& sIdentifier, const std::string& sName, IStorageStream* pStream, const LibMCData::eCustomDataType eDataType, const std::string& sUserUUID, const LibMCData_uint64 nAbsoluteTimeStamp) override;
    
    IBuildJobDataIterator* ListJobDataByType(const LibMCData::eCustomDataType eDataType) override;

    IBuildJobDataIterator* ListJobData() override;

    IBuildJobData* RetrieveJobData(const std::string& sDataUUID) override;

    IBuildJobData* RetrieveJobDataByIdentifier(const std::string& sIdentifier) override;

    bool HasJobDataUUID(const std::string& sUUID) override;

    bool HasJobDataIdentifier(const std::string& sIdentifier) override;

    void StoreMetaDataString(const std::string& sKey, const std::string& sValue, const LibMCData_uint64 nAbsoluteTimeStamp) override;

    bool HasMetaDataString(const std::string& sKey) override;

    std::string GetMetaDataString(const std::string& sKey) override;

    IBuildJobExecution* CreateBuildJobExecution(const std::string& sDescription, const std::string& sUserUUID, const LibMCData_uint64 nAbsoluteStartTimeStampInMicrosecondsSince1970) override;

    IBuildJobExecution* RetrieveBuildJobExecution(const std::string& sExecutionUUID) override;

    IBuildJobExecutionIterator* RetrieveBuildJobExecutions(const std::string& sJournalUUIDFilter) override;

    IBuildJobExecutionIterator* RetrieveBuildJobExecutionsByStatus(const LibMCData::eBuildJobExecutionStatus eStatusFilter, const std::string& sJournalUUIDFilter) override;

    static std::string convertBuildJobStatusToString(const LibMCData::eBuildJobStatus eStatus);
    static LibMCData::eBuildJobStatus convertStringToBuildJobStatus(const std::string& sValue);

    

};

} // namespace Impl
} // namespace LibMCData

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDATA_BUILDJOB
