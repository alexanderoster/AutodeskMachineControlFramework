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

*/

#ifndef __AMCTEST_UNITTEST_LIBMCDATA_DATACAPTURE
#define __AMCTEST_UNITTEST_LIBMCDATA_DATACAPTURE

#include "amc_unittests.hpp"
#include "amc_unittests_libmcdata.hpp"
#include "common_utils.hpp"
#include "libmcdata_dynamic.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace AMCUnitTest {

	class CUnitTestGroup_LibMCData_DataCapture : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "LibMCData_DataCapture";
		}

		void registerTests() override {
			registerTest("SchemaVersion", "Data model schema contains the data capture tables", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_LibMCData_DataCapture::testSchemaVersion, this));
			registerTest("CreateAndRetrieve", "Create, find and retrieve a capture manifest", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_LibMCData_DataCapture::testCreateAndRetrieve, this));
			registerTest("Columns", "Register and read back the capture schema", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_LibMCData_DataCapture::testColumns, this));
			registerTest("SegmentsAndFinish", "Register segments and finish the capture", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_LibMCData_DataCapture::testSegmentsAndFinish, this));
			registerTest("AbortPurgesSegments", "Abort deletes the stored segments and keeps the manifest", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_LibMCData_DataCapture::testAbortPurgesSegments, this));
			registerTest("StorageLimit", "Retention deletes the oldest segments across captures", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_LibMCData_DataCapture::testStorageLimit, this));
			registerTest("DiscardRandomWriteStream", "Discard an unfinished random write stream", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_LibMCData_DataCapture::testDiscardRandomWriteStream, this));
			registerTest("ListingAndLookup", "List the captures of an execution and find a capture by uuid", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_LibMCData_DataCapture::testListingAndLookup, this));
		}

		void initializeTests() override {
		}

	private:

		struct SFixture {
			LibMCData::PWrapper m_pDataWrapper;
			LibMCData::PDataModel m_pDataModel;
			LibMCData::PBuildJobHandler m_pBuildJobHandler;
			LibMCData::PStorage m_pStorage;
			LibMCData::PLoginHandler m_pLoginHandler;
			LibMCData::PBuildJobExecution m_pExecution;
			std::string m_sBasePath;
			std::string m_sUserUUID;
		};

		uint64_t getCurrentTimestamp()
		{
			return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
				std::chrono::system_clock::now().time_since_epoch()).count());
		}

		SFixture createFixture(const std::string& sSuffix)
		{
			SFixture fixture;

			std::string sRootPath = "temp";
			if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(sRootPath))
				AMCCommon::CUtils::createDirectoryOnDisk(sRootPath);

			fixture.m_sBasePath = sRootPath + "/libmcdata_datacapture_" + sSuffix + "_" + AMCCommon::CUtils::createUUID();
			if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(fixture.m_sBasePath))
				AMCCommon::CUtils::createDirectoryOnDisk(fixture.m_sBasePath);

			std::string sDatabaseFile = fixture.m_sBasePath + "/datacapture.db";

			fixture.m_pDataWrapper = AMCUnitTest::loadLibMCDataInProcess();
			fixture.m_pDataModel = fixture.m_pDataWrapper->CreateDataModelInstance();
			fixture.m_pDataModel->InitialiseDatabase(fixture.m_sBasePath, LibMCData::eDataBaseType::SqLite, sDatabaseFile);
			fixture.m_pBuildJobHandler = fixture.m_pDataModel->CreateBuildJobHandler();
			fixture.m_pStorage = fixture.m_pDataModel->CreateStorage();
			fixture.m_pLoginHandler = fixture.m_pDataModel->CreateLoginHandler();

			std::string sSalt = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
			std::string sHash = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
			fixture.m_sUserUUID = fixture.m_pLoginHandler->CreateUser("testuser", "operator", sSalt, sHash, "Test User");

			fixture.m_pExecution = createExecution(fixture, "Capture Job");

			return fixture;
		}

		LibMCData::PBuildJobExecution createExecution(SFixture& fixture, const std::string& sJobName)
		{
			std::string sContent = "job content for " + sJobName;
			std::vector<uint8_t> buffer(sContent.begin(), sContent.end());
			std::string sStreamUUID = AMCCommon::CUtils::createUUID();
			fixture.m_pStorage->StoreNewStream(sStreamUUID, "job.bin", "application/octet-stream", buffer, fixture.m_sUserUUID, getCurrentTimestamp());

			auto pJob = fixture.m_pBuildJobHandler->CreateJob(AMCCommon::CUtils::createUUID(), sJobName, fixture.m_sUserUUID, sStreamUUID, getCurrentTimestamp());
			pJob->StartValidating();
			pJob->FinishValidating(10);

			return pJob->CreateBuildJobExecution("Execution", fixture.m_sUserUUID, getCurrentTimestamp());
		}

		// Stores a finished random write stream of the given size and returns its UUID.
		std::string createSegmentStream(SFixture& fixture, size_t nSize)
		{
			std::string sUUID = AMCCommon::CUtils::createUUID();
			std::vector<uint8_t> buffer(nSize, 0x5A);

			fixture.m_pStorage->BeginRandomWriteStream(sUUID, "segment", "application/amcf-datatable", fixture.m_sUserUUID, getCurrentTimestamp());
			fixture.m_pStorage->StoreRandomWriteStream(sUUID, 0, buffer);
			fixture.m_pStorage->FinishRandomWriteStream(sUUID);

			return sUUID;
		}

		std::string streamPath(SFixture& fixture, const std::string& sStreamUUID)
		{
			return fixture.m_sBasePath + "/" + AMCCommon::CUtils::normalizeUUIDString(sStreamUUID) + ".stream";
		}

		template <typename F>
		int32_t errorCodeOf(F&& fn)
		{
			try {
				fn();
			}
			catch (LibMCData::ELibMCDataException& dataException) {
				return dataException.getErrorCode();
			}
			return 0;
		}

		void testSchemaVersion()
		{
			auto fixture = createFixture("schema");
			assertTrue(fixture.m_pDataModel->GetDataModelVersion() >= 23, "schema version includes the data capture tables");

			// The tables exist: an unknown capture is reported as not found and not as an SQL error.
			assertFalse(fixture.m_pExecution->HasDataCapture("missing"), "unknown capture does not exist");
			assertTrue(fixture.m_pStorage->GetDataCaptureStorageSize() == 0, "no capture data is stored initially");
		}

		void testCreateAndRetrieve()
		{
			auto fixture = createFixture("create");

			auto pCapture = fixture.m_pExecution->CreateDataCapture("pyrometer", "Build pyrometer trace", fixture.m_sUserUUID, getCurrentTimestamp());
			assertAssigned(pCapture.get(), "capture is created");
			assertTrue(pCapture->GetIdentifier() == "pyrometer", "capture identifier");
			assertTrue(pCapture->GetName() == "Build pyrometer trace", "capture name");
			assertTrue(pCapture->GetExecutionUUID() == fixture.m_pExecution->GetExecutionUUID(), "capture belongs to the execution");
			assertTrue(pCapture->GetStatus() == LibMCData::eDataCaptureStatus::Recording, "new capture is recording");
			assertTrue(pCapture->GetSegmentCount() == 0, "new capture has no segments");
			assertTrue(pCapture->GetRowCount() == 0, "new capture has no rows");
			assertTrue(pCapture->GetColumnCount() == 0, "new capture has no columns");

			assertTrue(fixture.m_pExecution->HasDataCapture("pyrometer"), "capture is found by identifier");
			assertFalse(fixture.m_pExecution->HasDataCapture("other"), "other identifier is not found");

			auto pRetrieved = fixture.m_pExecution->RetrieveDataCapture("pyrometer");
			assertTrue(pRetrieved->GetUUID() == pCapture->GetUUID(), "retrieved capture has the same uuid");
			assertTrue(pRetrieved->GetName() == "Build pyrometer trace", "retrieved capture name");

			assertTrue(errorCodeOf([&]() { fixture.m_pExecution->CreateDataCapture("pyrometer", "Duplicate", fixture.m_sUserUUID, getCurrentTimestamp()); }) == LIBMCDATA_ERROR_DUPLICATEDATACAPTUREIDENTIFIER, "duplicate identifier is rejected");
			assertTrue(errorCodeOf([&]() { fixture.m_pExecution->CreateDataCapture("bad id!", "Invalid", fixture.m_sUserUUID, getCurrentTimestamp()); }) == LIBMCDATA_ERROR_INVALIDDATACAPTUREIDENTIFIER, "invalid identifier is rejected");
			assertTrue(errorCodeOf([&]() { fixture.m_pExecution->CreateDataCapture("", "Empty", fixture.m_sUserUUID, getCurrentTimestamp()); }) == LIBMCDATA_ERROR_INVALIDDATACAPTUREIDENTIFIER, "empty identifier is rejected");
			assertTrue(errorCodeOf([&]() { fixture.m_pExecution->CreateDataCapture("unnamed", "", fixture.m_sUserUUID, getCurrentTimestamp()); }) == LIBMCDATA_ERROR_INVALIDDATACAPTURENAME, "empty name is rejected");
			assertTrue(errorCodeOf([&]() { fixture.m_pExecution->RetrieveDataCapture("missing"); }) == LIBMCDATA_ERROR_DATACAPTURENOTFOUND, "unknown capture is not found");

			// The identifier is unique per execution, not globally.
			auto pOtherExecution = createExecution(fixture, "Other Job");
			assertFalse(pOtherExecution->HasDataCapture("pyrometer"), "capture is not visible in another execution");
			auto pOtherCapture = pOtherExecution->CreateDataCapture("pyrometer", "Other trace", fixture.m_sUserUUID, getCurrentTimestamp());
			assertTrue(pOtherCapture->GetUUID() != pCapture->GetUUID(), "same identifier in another execution is a different capture");
		}

		void testColumns()
		{
			auto fixture = createFixture("columns");
			auto pCapture = fixture.m_pExecution->CreateDataCapture("trace", "Trace", fixture.m_sUserUUID, getCurrentTimestamp());

			pCapture->AddColumn(0, "t", "Time", 8);
			pCapture->AddColumn(1, "temp", "Temperature in C", 6);
			pCapture->AddColumn(2, "laser", "Laser active", 7);
			assertTrue(pCapture->GetColumnCount() == 3, "three columns registered");

			std::string sIdentifier;
			std::string sDescription;
			uint32_t nColumnType = 0;
			pCapture->GetColumnInformation(1, sIdentifier, sDescription, nColumnType);
			assertTrue(sIdentifier == "temp", "column identifier");
			assertTrue(sDescription == "Temperature in C", "column description");
			assertTrue(nColumnType == 6, "column type");

			assertTrue(errorCodeOf([&]() { pCapture->AddColumn(1, "other", "Other", 1); }) == LIBMCDATA_ERROR_DUPLICATEDATACAPTURECOLUMN, "duplicate column index is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->AddColumn(3, "temp", "Again", 1); }) == LIBMCDATA_ERROR_DUPLICATEDATACAPTURECOLUMN, "duplicate column identifier is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->GetColumnInformation(9, sIdentifier, sDescription, nColumnType); }) == LIBMCDATA_ERROR_INVALIDDATACAPTURECOLUMNINDEX, "unknown column index is rejected");

			pCapture->Finish(getCurrentTimestamp());
			assertTrue(errorCodeOf([&]() { pCapture->AddColumn(3, "late", "Late", 1); }) == LIBMCDATA_ERROR_DATACAPTURENOTRECORDING, "schema is frozen after finish");
		}

		void testSegmentsAndFinish()
		{
			auto fixture = createFixture("segments");
			auto pCapture = fixture.m_pExecution->CreateDataCapture("trace", "Trace", fixture.m_sUserUUID, getCurrentTimestamp());

			std::string sStream0 = createSegmentStream(fixture, 1000);
			std::string sStream1 = createSegmentStream(fixture, 600);

			uint64_t nTime = getCurrentTimestamp();
			pCapture->AddSegment(0, sStream0, 0, 500, 5, 1000, nTime, nTime + 10);
			pCapture->AddSegment(1, sStream1, 500, 300, 3, 600, nTime + 10, nTime + 20);

			assertTrue(pCapture->GetSegmentCount() == 2, "two segments registered");
			assertTrue(pCapture->GetRowCount() == 800, "row count is the sum of the segments");
			assertTrue(pCapture->GetStoredSizeInBytes() == 1600, "stored size is the sum of the segments");
			assertTrue(fixture.m_pStorage->GetDataCaptureStorageSize() == 1600, "global stored size");

			std::string sStreamUUID;
			uint64_t nFirstRow = 0;
			uint64_t nRowCount = 0;
			uint64_t nSizeInBytes = 0;
			bool bIsPurged = true;
			pCapture->GetSegmentInformation(1, sStreamUUID, nFirstRow, nRowCount, nSizeInBytes, bIsPurged);
			assertTrue(AMCCommon::CUtils::normalizeUUIDString(sStreamUUID) == AMCCommon::CUtils::normalizeUUIDString(sStream1), "segment stream uuid");
			assertTrue(nFirstRow == 500, "segment first row");
			assertTrue(nRowCount == 300, "segment row count");
			assertTrue(nSizeInBytes == 600, "segment size");
			assertFalse(bIsPurged, "segment is stored");

			assertTrue(errorCodeOf([&]() { pCapture->AddSegment(1, sStream1, 800, 1, 1, 1, nTime, nTime); }) == LIBMCDATA_ERROR_DUPLICATEDATACAPTURESEGMENT, "duplicate segment index is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->AddSegment(2, AMCCommon::CUtils::createUUID(), 800, 1, 1, 1, nTime, nTime); }) == LIBMCDATA_ERROR_STORAGESTREAMNOTFOUND, "unknown stream is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->GetSegmentInformation(7, sStreamUUID, nFirstRow, nRowCount, nSizeInBytes, bIsPurged); }) == LIBMCDATA_ERROR_INVALIDDATACAPTURESEGMENTINDEX, "unknown segment index is rejected");

			pCapture->Finish(getCurrentTimestamp());
			assertTrue(pCapture->GetStatus() == LibMCData::eDataCaptureStatus::Finished, "capture is finished");

			std::string sStream2 = createSegmentStream(fixture, 100);
			assertTrue(errorCodeOf([&]() { pCapture->AddSegment(2, sStream2, 800, 1, 1, 100, nTime, nTime); }) == LIBMCDATA_ERROR_DATACAPTURENOTRECORDING, "no segments after finish");
			assertTrue(errorCodeOf([&]() { pCapture->Finish(getCurrentTimestamp()); }) == LIBMCDATA_ERROR_DATACAPTURENOTRECORDING, "second finish is rejected");

			// The status is read from the database, so another manifest instance sees it.
			auto pRetrieved = fixture.m_pExecution->RetrieveDataCapture("trace");
			assertTrue(pRetrieved->GetStatus() == LibMCData::eDataCaptureStatus::Finished, "retrieved capture is finished");
			assertTrue(pRetrieved->GetSegmentCount() == 2, "retrieved capture has its segments");
		}

		void testAbortPurgesSegments()
		{
			auto fixture = createFixture("abort");
			auto pCapture = fixture.m_pExecution->CreateDataCapture("trace", "Trace", fixture.m_sUserUUID, getCurrentTimestamp());

			std::string sStream0 = createSegmentStream(fixture, 1000);
			std::string sStream1 = createSegmentStream(fixture, 600);
			uint64_t nTime = getCurrentTimestamp();
			pCapture->AddSegment(0, sStream0, 0, 500, 5, 1000, nTime, nTime + 10);
			pCapture->AddSegment(1, sStream1, 500, 300, 3, 600, nTime + 10, nTime + 20);

			assertTrue(AMCCommon::CUtils::fileOrPathExistsOnDisk(streamPath(fixture, sStream0)), "segment file exists before abort");

			pCapture->Abort(getCurrentTimestamp());

			assertTrue(pCapture->GetStatus() == LibMCData::eDataCaptureStatus::Aborted, "capture is aborted");
			assertFalse(AMCCommon::CUtils::fileOrPathExistsOnDisk(streamPath(fixture, sStream0)), "first segment file is deleted");
			assertFalse(AMCCommon::CUtils::fileOrPathExistsOnDisk(streamPath(fixture, sStream1)), "second segment file is deleted");
			assertTrue(pCapture->GetStoredSizeInBytes() == 0, "nothing is stored after abort");
			assertTrue(fixture.m_pStorage->GetDataCaptureStorageSize() == 0, "global stored size is zero after abort");

			// The manifest keeps telling which rows have existed.
			assertTrue(pCapture->GetSegmentCount() == 2, "manifest keeps the purged segments");
			assertTrue(pCapture->GetRowCount() == 800, "manifest keeps the row count");

			std::string sStreamUUID;
			uint64_t nFirstRow = 0;
			uint64_t nRowCount = 0;
			uint64_t nSizeInBytes = 0;
			bool bIsPurged = false;
			pCapture->GetSegmentInformation(0, sStreamUUID, nFirstRow, nRowCount, nSizeInBytes, bIsPurged);
			assertTrue(bIsPurged, "segment is marked as purged");

			assertTrue(errorCodeOf([&]() { fixture.m_pStorage->RetrieveStream(sStream0); }) == LIBMCDATA_ERROR_STORAGESTREAMNOTFOUND, "purged stream is not found anymore");
			assertTrue(errorCodeOf([&]() { pCapture->Abort(getCurrentTimestamp()); }) == LIBMCDATA_ERROR_DATACAPTUREALREADYABORTED, "second abort is rejected");
		}

		void testStorageLimit()
		{
			auto fixture = createFixture("limit");
			auto pCaptureA = fixture.m_pExecution->CreateDataCapture("first", "First", fixture.m_sUserUUID, getCurrentTimestamp());
			auto pCaptureB = fixture.m_pExecution->CreateDataCapture("second", "Second", fixture.m_sUserUUID, getCurrentTimestamp());

			std::string sStreamA0 = createSegmentStream(fixture, 1000);
			std::string sStreamB0 = createSegmentStream(fixture, 1000);
			std::string sStreamA1 = createSegmentStream(fixture, 1000);

			// Age is given by the end time stamp of the segment: A0 is the oldest, A1 the newest.
			uint64_t nTime = getCurrentTimestamp();
			pCaptureA->AddSegment(0, sStreamA0, 0, 100, 1, 1000, nTime, nTime + 10);
			pCaptureB->AddSegment(0, sStreamB0, 0, 100, 1, 1000, nTime + 10, nTime + 20);
			pCaptureA->AddSegment(1, sStreamA1, 100, 100, 1, 1000, nTime + 20, nTime + 30);

			assertTrue(fixture.m_pStorage->GetDataCaptureStorageSize() == 3000, "three segments stored");
			assertTrue(fixture.m_pStorage->EnforceDataCaptureStorageLimit(3000) == 0, "nothing is purged within the limit");

			uint32_t nPurged = fixture.m_pStorage->EnforceDataCaptureStorageLimit(2500);
			assertTrue(nPurged == 1, "one segment is purged to get below the limit");
			assertTrue(fixture.m_pStorage->GetDataCaptureStorageSize() == 2000, "two segments remain");
			assertFalse(AMCCommon::CUtils::fileOrPathExistsOnDisk(streamPath(fixture, sStreamA0)), "the oldest segment file is deleted");
			assertTrue(AMCCommon::CUtils::fileOrPathExistsOnDisk(streamPath(fixture, sStreamB0)), "the second oldest segment file is kept");
			assertTrue(AMCCommon::CUtils::fileOrPathExistsOnDisk(streamPath(fixture, sStreamA1)), "the newest segment file is kept");

			std::string sStreamUUID;
			uint64_t nFirstRow = 0;
			uint64_t nRowCount = 0;
			uint64_t nSizeInBytes = 0;
			bool bIsPurged = false;
			pCaptureA->GetSegmentInformation(0, sStreamUUID, nFirstRow, nRowCount, nSizeInBytes, bIsPurged);
			assertTrue(bIsPurged, "the oldest segment is marked as purged");
			pCaptureA->GetSegmentInformation(1, sStreamUUID, nFirstRow, nRowCount, nSizeInBytes, bIsPurged);
			assertFalse(bIsPurged, "the newest segment is still stored");
			assertTrue(pCaptureA->GetStoredSizeInBytes() == 1000, "capture stored size follows the purge");
			assertTrue(pCaptureA->GetRowCount() == 200, "capture row count is unchanged by the purge");
			assertTrue(pCaptureA->GetStatus() == LibMCData::eDataCaptureStatus::Recording, "retention does not change the capture status");

			// Oldest first across captures: B0 goes before A1.
			nPurged = fixture.m_pStorage->EnforceDataCaptureStorageLimit(1000);
			assertTrue(nPurged == 1, "one more segment is purged");
			assertFalse(AMCCommon::CUtils::fileOrPathExistsOnDisk(streamPath(fixture, sStreamB0)), "the second oldest segment file is deleted");
			assertTrue(AMCCommon::CUtils::fileOrPathExistsOnDisk(streamPath(fixture, sStreamA1)), "the newest segment file is still kept");

			nPurged = fixture.m_pStorage->EnforceDataCaptureStorageLimit(999);
			assertTrue(nPurged == 1, "the last segment is purged");
			assertTrue(fixture.m_pStorage->GetDataCaptureStorageSize() == 0, "nothing remains");
			assertTrue(fixture.m_pStorage->EnforceDataCaptureStorageLimit(0) == 0, "nothing left to purge");
		}

		void testListingAndLookup()
		{
			auto fixture = createFixture("listing");

			assertTrue(fixture.m_pExecution->GetDataCaptureCount() == 0, "new execution has no captures");

			uint64_t nTime = getCurrentTimestamp();
			auto pFirst = fixture.m_pExecution->CreateDataCapture("alpha", "First capture", fixture.m_sUserUUID, nTime);
			auto pSecond = fixture.m_pExecution->CreateDataCapture("beta", "Second capture", fixture.m_sUserUUID, nTime + 1000000);

			assertTrue(fixture.m_pExecution->GetDataCaptureCount() == 2, "two captures in the execution");

			// Captures are ordered by their creation time.
			auto pByIndex0 = fixture.m_pExecution->RetrieveDataCaptureByIndex(0);
			auto pByIndex1 = fixture.m_pExecution->RetrieveDataCaptureByIndex(1);
			assertTrue(pByIndex0->GetUUID() == pFirst->GetUUID(), "first capture by index");
			assertTrue(pByIndex1->GetIdentifier() == "beta", "second capture by index");
			assertTrue(!pByIndex0->GetTimeStamp().empty(), "capture has a creation time stamp");
			assertTrue(pByIndex0->GetTimeStamp() < pByIndex1->GetTimeStamp(), "creation time stamps are ordered");
			assertTrue(errorCodeOf([&]() { fixture.m_pExecution->RetrieveDataCaptureByIndex(2); }) == LIBMCDATA_ERROR_DATACAPTURENOTFOUND, "index out of bounds is rejected");

			// A capture is found by its uuid without knowing the execution.
			auto pByUUID = fixture.m_pBuildJobHandler->RetrieveDataCapture(pSecond->GetUUID());
			assertTrue(pByUUID->GetIdentifier() == "beta", "capture found by uuid");
			assertTrue(pByUUID->GetName() == "Second capture", "capture name by uuid");
			assertTrue(pByUUID->GetExecutionUUID() == fixture.m_pExecution->GetExecutionUUID(), "execution of the capture found by uuid");
			assertTrue(errorCodeOf([&]() { fixture.m_pBuildJobHandler->RetrieveDataCapture(AMCCommon::CUtils::createUUID()); }) == LIBMCDATA_ERROR_DATACAPTURENOTFOUND, "unknown uuid is not found");

			// Captures of another execution are not listed.
			auto pOtherExecution = createExecution(fixture, "Other Job");
			assertTrue(pOtherExecution->GetDataCaptureCount() == 0, "another execution has its own captures");

			// The chunk count of a segment is part of the manifest.
			std::string sStream = createSegmentStream(fixture, 500);
			pFirst->AddSegment(0, sStream, 0, 300, 3, 500, nTime, nTime + 10);
			assertTrue(pFirst->GetSegmentChunkCount(0) == 3, "segment chunk count");
			assertTrue(errorCodeOf([&]() { pFirst->GetSegmentChunkCount(1); }) == LIBMCDATA_ERROR_INVALIDDATACAPTURESEGMENTINDEX, "unknown segment index is rejected");
		}

		void testDiscardRandomWriteStream()
		{
			auto fixture = createFixture("discard");

			std::string sUUID = AMCCommon::CUtils::createUUID();
			std::vector<uint8_t> buffer(500, 0x11);
			fixture.m_pStorage->BeginRandomWriteStream(sUUID, "unfinished", "application/amcf-datatable", fixture.m_sUserUUID, getCurrentTimestamp());
			fixture.m_pStorage->StoreRandomWriteStream(sUUID, 0, buffer);
			assertTrue(fixture.m_pStorage->GetRandomWriteStreamSize(sUUID) == 500, "unfinished stream holds the data");
			assertTrue(AMCCommon::CUtils::fileOrPathExistsOnDisk(streamPath(fixture, sUUID)), "unfinished stream file exists");

			fixture.m_pStorage->DiscardRandomWriteStream(sUUID);

			assertFalse(AMCCommon::CUtils::fileOrPathExistsOnDisk(streamPath(fixture, sUUID)), "discarded stream file is deleted");
			assertFalse(fixture.m_pStorage->StreamIsReady(sUUID), "discarded stream is not ready");
			assertTrue(errorCodeOf([&]() { fixture.m_pStorage->RetrieveStream(sUUID); }) == LIBMCDATA_ERROR_STORAGESTREAMNOTFOUND, "discarded stream entry is deleted");
			assertTrue(errorCodeOf([&]() { fixture.m_pStorage->DiscardRandomWriteStream(sUUID); }) == LIBMCDATA_ERROR_RANDOMWRITESTREAMNOTFOUND, "second discard is rejected");
			assertTrue(errorCodeOf([&]() { fixture.m_pStorage->DiscardRandomWriteStream(AMCCommon::CUtils::createUUID()); }) == LIBMCDATA_ERROR_RANDOMWRITESTREAMNOTFOUND, "unknown stream is rejected");

			// A finished stream is not a random write stream anymore and is left untouched.
			std::string sFinished = createSegmentStream(fixture, 100);
			assertTrue(errorCodeOf([&]() { fixture.m_pStorage->DiscardRandomWriteStream(sFinished); }) == LIBMCDATA_ERROR_RANDOMWRITESTREAMNOTFOUND, "finished stream can not be discarded");
			assertTrue(fixture.m_pStorage->StreamIsReady(sFinished), "finished stream is still ready");
		}

	};

}

#endif // __AMCTEST_UNITTEST_LIBMCDATA_DATACAPTURE
