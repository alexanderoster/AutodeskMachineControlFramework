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

#ifndef __AMCTEST_UNITTEST_DATACAPTURE
#define __AMCTEST_UNITTEST_DATACAPTURE

#include "amc_unittests.hpp"
#include "amc_unittests_libmcdata.hpp"
#include "amc_constants.hpp"
#include "amc_datatableformat.hpp"
#include "amc_meshhandler.hpp"
#include "amc_statejournal.hpp"
#include "amc_statejournalstream.hpp"
#include "amc_toolpathhandler.hpp"
#include "common_chrono.hpp"
#include "common_utils.hpp"

#include "libmcenv_buildexecution.hpp"
#include "libmcenv_datacapture.hpp"
#include "libmcenv_datacaptureoptions.hpp"
#include "libmcenv_datatable.hpp"
#include "libmcenv_tempstreamwriter.hpp"
#include "libmcenv_streamreader.hpp"
#include "libmcenv_interfaceexception.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace AMCUnitTest {

	class CUnitTestGroup_DataCapture : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "DataCapture";
		}

		void registerTests() override {
			registerTest("OptionsDefaultsAndValidation", "Capture options have sane defaults and reject invalid values", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testOptionsDefaultsAndValidation, this));
			registerTest("CreateValidation", "Creating a capture in a build execution is validated", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testCreateValidation, this));
			registerTest("RoundTripAllTypes", "A chunk of all column types is captured and loaded back", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testRoundTripAllTypes, this));
			registerTest("ChunkBoundaries", "Full chunks, a partial final chunk and an empty chunk", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testChunkBoundaries, this));
			registerTest("EmptyCapture", "Captures without rows and without columns", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testEmptyCapture, this));
			registerTest("SegmentRollover", "Rollover exactly on and on either side of a chunk boundary", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testSegmentRollover, this));
			registerTest("RowWiseAPI", "Row-wise convenience path for low-rate producers", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testRowWiseAPI, this));
			registerTest("ChunkValidation", "Inconsistent chunks and calls in the wrong state are rejected", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testChunkValidation, this));
			registerTest("AbortDeletesEverything", "Abort deletes finalized, pending and open segments", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testAbortDeletesEverything, this));
			registerTest("BackpressureBlock", "Block mode bounds the queue and loses nothing", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testBackpressureBlock, this));
			registerTest("BackpressureDropOldest", "DropOldest mode discards queued chunks and counts the rows", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testBackpressureDropOldest, this));
			registerTest("BackpressureFail", "Fail mode raises an error when the queue is full", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testBackpressureFail, this));
			registerTest("RetentionPurgesOldest", "The storage limit deletes the oldest segments while recording", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testRetentionPurgesOldest, this));
			registerTest("CountersAndFlush", "Counters follow the capture and Flush drains the queue", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testCountersAndFlush, this));
			registerTest("AbandonedCaptureIsFinished", "A capture that is released without Finish keeps its data", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testAbandonedCaptureIsFinished, this));
			registerTest("TruncatedSegment", "An interrupted segment is reported cleanly on load", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testTruncatedSegment, this));
			registerTest("AppendDataTable", "A capture takes its schema and its rows from data tables", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCapture::testAppendDataTable, this));
		}

		void initializeTests() override {
		}

	protected:

		typedef std::unique_ptr<LibMCEnv::Impl::IDataCaptureOptions> UOptions;
		typedef std::unique_ptr<LibMCEnv::Impl::IDataCapture> UCapture;
		typedef std::unique_ptr<LibMCEnv::Impl::IDataCaptureChunk> UChunk;
		typedef std::unique_ptr<LibMCEnv::Impl::IDataTable> UTable;

		struct SFixture {
			LibMCData::PWrapper m_pDataWrapper;
			LibMCData::PDataModel m_pDataModel;
			LibMCData::PBuildJobHandler m_pBuildJobHandler;
			LibMCData::PStorage m_pStorage;
			LibMCData::PLoginHandler m_pLoginHandler;
			LibMCData::PJournalSession m_pJournalSession;
			LibMCData::PBuildJobExecution m_pExecution;
			AMCCommon::PChrono m_pChrono;
			AMC::PToolpathHandler m_pToolpathHandler;
			AMC::PMeshHandler m_pMeshHandler;
			AMC::PStateJournalStream m_pStateJournalStream;
			AMC::PStateJournal m_pStateJournal;
			std::shared_ptr<LibMCEnv::Impl::CBuildExecution> m_pBuildExecution;
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

			fixture.m_sBasePath = sRootPath + "/datacapture_" + sSuffix + "_" + AMCCommon::CUtils::createUUID();
			if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(fixture.m_sBasePath))
				AMCCommon::CUtils::createDirectoryOnDisk(fixture.m_sBasePath);

			std::string sDatabaseFile = fixture.m_sBasePath + "/datacapture.db";

			fixture.m_pDataWrapper = AMCUnitTest::loadLibMCDataInProcess();
			fixture.m_pDataModel = fixture.m_pDataWrapper->CreateDataModelInstance();
			fixture.m_pDataModel->InitialiseDatabase(fixture.m_sBasePath, LibMCData::eDataBaseType::SqLite, sDatabaseFile);
			fixture.m_pBuildJobHandler = fixture.m_pDataModel->CreateBuildJobHandler();
			fixture.m_pStorage = fixture.m_pDataModel->CreateStorage();
			fixture.m_pLoginHandler = fixture.m_pDataModel->CreateLoginHandler();
			fixture.m_pJournalSession = fixture.m_pDataModel->CreateJournalSession();

			std::string sSalt = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
			std::string sHash = "fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210";
			fixture.m_sUserUUID = fixture.m_pLoginHandler->CreateUser("testuser", "operator", sSalt, sHash, "Test User");

			// A validated job with a running execution
			std::string sContent = "job content";
			std::vector<uint8_t> buffer(sContent.begin(), sContent.end());
			std::string sStreamUUID = AMCCommon::CUtils::createUUID();
			fixture.m_pStorage->StoreNewStream(sStreamUUID, "job.bin", "application/octet-stream", buffer, fixture.m_sUserUUID, getCurrentTimestamp());

			auto pJob = fixture.m_pBuildJobHandler->CreateJob(AMCCommon::CUtils::createUUID(), "Capture Job", fixture.m_sUserUUID, sStreamUUID, getCurrentTimestamp());
			pJob->StartValidating();
			pJob->FinishValidating(10);
			fixture.m_pExecution = pJob->CreateBuildJobExecution("Execution", fixture.m_sUserUUID, getCurrentTimestamp());

			fixture.m_pChrono = std::make_shared<AMCCommon::CChrono>();
			fixture.m_pToolpathHandler = std::make_shared<AMC::CToolpathHandler>(fixture.m_pDataModel);
			fixture.m_pMeshHandler = std::make_shared<AMC::CMeshHandler>();
			fixture.m_pStateJournalStream = std::make_shared<AMC::CStateJournalStream>(fixture.m_pJournalSession, nullptr, false);
			fixture.m_pStateJournal = std::make_shared<AMC::CStateJournal>(fixture.m_pStateJournalStream, fixture.m_pChrono);

			fixture.m_pBuildExecution = std::make_shared<LibMCEnv::Impl::CBuildExecution>(fixture.m_pExecution, fixture.m_pDataModel, fixture.m_pToolpathHandler, fixture.m_pMeshHandler, fixture.m_pChrono, fixture.m_pStateJournal);

			return fixture;
		}

		UOptions createOptions(SFixture& fixture, uint32_t nChunkRowCount, uint64_t nSegmentSizeInBytes, uint32_t nMaxQueuedChunks = DATACAPTURE_DEFAULTMAXQUEUEDCHUNKS, LibMCEnv::eDataCaptureBackpressureMode eMode = LibMCEnv::eDataCaptureBackpressureMode::Block, uint64_t nStorageLimitInBytes = 0)
		{
			UOptions pOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());
			pOptions->SetChunkRowCount(nChunkRowCount);
			pOptions->SetSegmentSizeInBytes(nSegmentSizeInBytes);
			pOptions->SetMaxQueuedChunks(nMaxQueuedChunks);
			pOptions->SetBackpressureMode(eMode);
			pOptions->SetStorageLimitInBytes(nStorageLimitInBytes);

			// The tests of the capture structure work on uncompressed pages.
			pOptions->SetCompression(LibMCEnv::eDataCaptureCompression::None);
			return pOptions;
		}

		// Options for the counter capture: its column stays plain, so that the size of a segment is known exactly.
		UOptions createCounterOptions(SFixture& fixture, uint32_t nChunkRowCount, uint64_t nSegmentSizeInBytes, uint32_t nMaxQueuedChunks = DATACAPTURE_DEFAULTMAXQUEUEDCHUNKS, LibMCEnv::eDataCaptureBackpressureMode eMode = LibMCEnv::eDataCaptureBackpressureMode::Block, uint64_t nStorageLimitInBytes = 0)
		{
			auto pOptions = createOptions(fixture, nChunkRowCount, nSegmentSizeInBytes, nMaxQueuedChunks, eMode, nStorageLimitInBytes);
			pOptions->SetColumnEncoding("n", LibMCEnv::eDataCaptureEncoding::Plain);
			return pOptions;
		}

		UCapture createCapture(SFixture& fixture, const std::string& sIdentifier, LibMCEnv::Impl::IDataCaptureOptions* pOptions)
		{
			return UCapture(fixture.m_pBuildExecution->CreateDataCapture(sIdentifier, "Capture " + sIdentifier, pOptions, fixture.m_sUserUUID));
		}

		// The counter capture has a single uint64 column that counts the rows of the capture.
		void addCounterColumn(LibMCEnv::Impl::IDataCapture* pCapture)
		{
			pCapture->AddColumn("n", "Counter", LibMCEnv::eDataTableColumnType::Uint64Column);
		}

		void commitCounterChunk(LibMCEnv::Impl::IDataCapture* pCapture, uint64_t& nCounter, uint32_t nRowCount)
		{
			std::vector<uint64_t> values(nRowCount);
			for (uint32_t nIndex = 0; nIndex < nRowCount; nIndex++)
				values[nIndex] = nCounter + nIndex;

			UChunk pChunk(pCapture->BeginChunk());
			pChunk->SetUint64ColumnValues(0, values.size(), values.data());
			pCapture->CommitChunk(pChunk.get());

			nCounter += nRowCount;
		}

		// Size of a segment of the counter capture that holds nChunkCount chunks of nRowCount rows.
		uint64_t counterSegmentSize(uint32_t nChunkCount, uint32_t nRowCount)
		{
			std::vector<AMC::sDataTableV2Column> columns;
			columns.push_back({ "n", "Counter", AMC::eDataTableV2DataType::Uint64 });
			AMC::CDataTableV2SegmentBuilder sizer(columns, 0, 0, nRowCount);

			return sizeof(AMC::sDataTableV2SegmentHeader) + (uint64_t)nChunkCount * sizer.getChunkSizeInBytes(nRowCount) + sizer.getFooterSizeInBytes(nChunkCount);
		}

		std::vector<uint64_t> loadCounterSegment(SFixture& fixture, const std::string& sIdentifier, uint32_t nSegmentIndex)
		{
			UTable pTable(fixture.m_pBuildExecution->LoadDataCaptureSegment(sIdentifier, nSegmentIndex));

			std::vector<uint64_t> values((size_t)pTable->GetRowCount64());
			if (!values.empty())
				pTable->GetUint64ColumnValues("n", values.size(), nullptr, values.data());

			return values;
		}

		size_t countStreamFiles(SFixture& fixture)
		{
			size_t nCount = 0;
			for (auto& entry : std::filesystem::directory_iterator(fixture.m_sBasePath)) {
				if (entry.path().extension() == ".stream")
					nCount++;
			}
			return nCount;
		}

		template <typename F>
		int32_t errorCodeOf(F&& fn)
		{
			try {
				fn();
			}
			catch (ELibMCEnvInterfaceException& envException) {
				return envException.getErrorCode();
			}
			return 0;
		}

		template <typename F>
		int32_t dataErrorCodeOf(F&& fn)
		{
			try {
				fn();
			}
			catch (LibMCData::ELibMCDataException& dataException) {
				return dataException.getErrorCode();
			}
			return 0;
		}

		void testOptionsDefaultsAndValidation()
		{
			auto fixture = createFixture("options");
			UOptions pOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());

			assertTrue(pOptions->GetChunkRowCount() == 65536, "default chunk row count");
			assertTrue(pOptions->GetSegmentSizeInBytes() == 1024ULL * 1024ULL * 1024ULL, "default segment size is 1 GB");
			assertTrue(pOptions->GetMaxQueuedChunks() == 16, "default queue depth");
			assertTrue(pOptions->GetBackpressureMode() == LibMCEnv::eDataCaptureBackpressureMode::Block, "default backpressure mode is Block");
			assertTrue(pOptions->GetStorageLimitInBytes() == 0, "retention is disabled by default");
			assertTrue(pOptions->GetCompression() == LibMCEnv::eDataCaptureCompression::LZ4, "default compression is LZ4");

			pOptions->SetChunkRowCount(1000);
			pOptions->SetSegmentSizeInBytes(0);
			pOptions->SetMaxQueuedChunks(4);
			pOptions->SetBackpressureMode(LibMCEnv::eDataCaptureBackpressureMode::DropOldest);
			pOptions->SetStorageLimitInBytes(123456);
			assertTrue(pOptions->GetChunkRowCount() == 1000, "chunk row count is stored");
			assertTrue(pOptions->GetSegmentSizeInBytes() == 0, "segment size 0 disables the rollover");
			assertTrue(pOptions->GetMaxQueuedChunks() == 4, "queue depth is stored");
			assertTrue(pOptions->GetBackpressureMode() == LibMCEnv::eDataCaptureBackpressureMode::DropOldest, "backpressure mode is stored");
			assertTrue(pOptions->GetStorageLimitInBytes() == 123456, "storage limit is stored");

			assertTrue(errorCodeOf([&]() { pOptions->SetChunkRowCount(0); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "zero chunk row count is rejected");
			assertTrue(errorCodeOf([&]() { pOptions->SetChunkRowCount(DATACAPTURE_MAXCHUNKROWCOUNT + 1); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "too large chunk row count is rejected");
			assertTrue(errorCodeOf([&]() { pOptions->SetSegmentSizeInBytes(10); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "tiny segment size is rejected");
			assertTrue(errorCodeOf([&]() { pOptions->SetMaxQueuedChunks(0); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "zero queue depth is rejected");
			assertTrue(errorCodeOf([&]() { pOptions->SetMaxQueuedChunks(DATACAPTURE_MAXMAXQUEUEDCHUNKS + 1); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "too large queue depth is rejected");
			assertTrue(errorCodeOf([&]() { pOptions->SetBackpressureMode(LibMCEnv::eDataCaptureBackpressureMode::Unknown); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "unknown backpressure mode is rejected");
			assertTrue(pOptions->GetChunkRowCount() == 1000, "rejected values do not change the options");
		}

		void testCreateValidation()
		{
			auto fixture = createFixture("create");

			assertTrue(errorCodeOf([&]() { UCapture pCapture(fixture.m_pBuildExecution->CreateDataCapture("", "Name", nullptr, "")); }) == LIBMCENV_ERROR_EMPTYDATACAPTUREIDENTIFIER, "empty identifier is rejected");
			assertTrue(errorCodeOf([&]() { UCapture pCapture(fixture.m_pBuildExecution->CreateDataCapture("trace", "", nullptr, "")); }) == LIBMCENV_ERROR_EMPTYDATACAPTURENAME, "empty name is rejected");
			assertTrue(errorCodeOf([&]() { UCapture pCapture(fixture.m_pBuildExecution->CreateDataCapture("bad id!", "Name", nullptr, "")); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREIDENTIFIER, "invalid identifier is rejected");

			assertFalse(fixture.m_pBuildExecution->HasDataCapture("trace"), "capture does not exist yet");

			// Options are optional and an empty user UUID means no user.
			UCapture pCapture(fixture.m_pBuildExecution->CreateDataCapture("trace", "Trace", nullptr, ""));
			assertTrue(pCapture->GetIdentifier() == "trace", "capture identifier");
			assertTrue(pCapture->GetName() == "Trace", "capture name");
			assertTrue(pCapture->GetStatus() == LibMCEnv::eDataCaptureStatus::Configuring, "new capture is configuring");
			assertTrue(fixture.m_pBuildExecution->HasDataCapture("trace"), "capture exists in the execution");
			assertTrue(fixture.m_pBuildExecution->GetDataCaptureSegmentCount("trace") == 0, "new capture has no segments");
			assertTrue(fixture.m_pBuildExecution->GetDataCaptureRowCount("trace") == 0, "new capture has no rows");

			assertTrue(dataErrorCodeOf([&]() { UCapture pDuplicate(fixture.m_pBuildExecution->CreateDataCapture("trace", "Duplicate", nullptr, "")); }) == LIBMCDATA_ERROR_DUPLICATEDATACAPTUREIDENTIFIER, "duplicate identifier is rejected");
			assertTrue(dataErrorCodeOf([&]() { fixture.m_pBuildExecution->GetDataCaptureRowCount("missing"); }) == LIBMCDATA_ERROR_DATACAPTURENOTFOUND, "unknown capture is not found");
			assertTrue(errorCodeOf([&]() { UTable pTable(fixture.m_pBuildExecution->LoadDataCaptureSegment("trace", 0)); }) == LIBMCENV_ERROR_INVALIDDATACAPTURESEGMENTINDEX, "unknown segment index is rejected");

			pCapture->Abort();

			// A capture lives inside a running execution.
			fixture.m_pExecution->ChangeStatus(LibMCData::eBuildJobExecutionStatus::Finished, getCurrentTimestamp() + 1000000);
			assertTrue(errorCodeOf([&]() { UCapture pLate(fixture.m_pBuildExecution->CreateDataCapture("late", "Late", nullptr, "")); }) == LIBMCENV_ERROR_DATACAPTUREEXECUTIONNOTINPROCESS, "capture can not be created in a finished execution");
		}

		void testRoundTripAllTypes()
		{
			auto fixture = createFixture("alltypes");
			auto pOptions = createOptions(fixture, 16, 0);
			auto pCapture = createCapture(fixture, "all", pOptions.get());

			pCapture->AddColumn("d", "Double", LibMCEnv::eDataTableColumnType::DoubleColumn);
			pCapture->AddColumn("i32", "Int32", LibMCEnv::eDataTableColumnType::Int32Column);
			pCapture->AddColumn("u32", "Uint32", LibMCEnv::eDataTableColumnType::Uint32Column);
			pCapture->AddColumn("i64", "Int64", LibMCEnv::eDataTableColumnType::Int64Column);
			pCapture->AddColumn("u64", "Uint64", LibMCEnv::eDataTableColumnType::Uint64Column);
			pCapture->AddColumn("f", "Float", LibMCEnv::eDataTableColumnType::FloatColumn);
			pCapture->AddColumn("b", "Bool", LibMCEnv::eDataTableColumnType::BoolColumn);
			pCapture->AddColumn("t", "Timestamp", LibMCEnv::eDataTableColumnType::TimestampColumn);
			assertTrue(pCapture->GetColumnCount() == 8, "eight columns");

			std::vector<double> doubles = { 1.5, -2.25, 3.0, 1.0e6 };
			std::vector<int32_t> int32s = { -1, 2, -3, 4 };
			std::vector<uint32_t> uint32s = { 1, 2, 3, 4000000000U };
			std::vector<int64_t> int64s = { -10, 20, -5000000000LL, 40 };
			std::vector<uint64_t> uint64s = { 10, 20, 30, 18000000000000000000ULL };
			std::vector<float> floats = { 0.5f, 1.5f, 2.5f, 1800.25f };
			std::vector<uint8_t> bools = { 1, 0, 7, 0 };
			std::vector<uint64_t> timestamps = { 1000000ULL, 1000010ULL, 1000020ULL, 1000030ULL };

			{
				UChunk pChunk(pCapture->BeginChunk());
				assertTrue(pCapture->GetStatus() == LibMCEnv::eDataCaptureStatus::Recording, "capture records after the first chunk was borrowed");
				assertTrue(pChunk->GetCapacity() == 16, "chunk capacity follows the options");
				assertTrue(pChunk->GetRowCount() == 0, "new chunk is empty");
				assertFalse(pChunk->IsFull(), "new chunk is not full");

				pChunk->SetDoubleColumnValues(0, doubles.size(), doubles.data());
				pChunk->SetInt32ColumnValues(1, int32s.size(), int32s.data());
				pChunk->SetUint32ColumnValues(2, uint32s.size(), uint32s.data());
				pChunk->SetInt64ColumnValues(3, int64s.size(), int64s.data());
				pChunk->SetUint64ColumnValues(4, uint64s.size(), uint64s.data());
				pChunk->SetFloatColumnValues(5, floats.size(), floats.data());
				pChunk->SetBoolColumnValues(6, bools.size(), bools.data());
				pChunk->SetTimestampColumnValues(7, timestamps.size(), timestamps.data());
				assertTrue(pChunk->GetRowCount() == 4, "chunk holds four rows");

				pCapture->CommitChunk(pChunk.get());
			}

			std::string sCaptureUUID = pCapture->Finish();
			assertTrue(sCaptureUUID == pCapture->GetUUID(), "Finish returns the capture uuid");
			assertTrue(pCapture->GetStatus() == LibMCEnv::eDataCaptureStatus::Finished, "capture is finished");
			assertTrue(pCapture->Finish() == sCaptureUUID, "a second Finish is harmless");

			// The manifest holds the schema and the segment.
			auto pManifest = fixture.m_pExecution->RetrieveDataCapture("all");
			assertTrue(pManifest->GetUUID() == sCaptureUUID, "manifest uuid");
			assertTrue(pManifest->GetStatus() == LibMCData::eDataCaptureStatus::Finished, "manifest is finished");
			assertTrue(pManifest->GetColumnCount() == 8, "manifest schema has eight columns");
			assertTrue(pManifest->GetSegmentCount() == 1, "one segment");
			assertTrue(pManifest->GetRowCount() == 4, "four rows");

			std::string sIdentifier;
			std::string sDescription;
			uint32_t nColumnType = 0;
			pManifest->GetColumnInformation(5, sIdentifier, sDescription, nColumnType);
			assertTrue((sIdentifier == "f") && (sDescription == "Float") && (nColumnType == (uint32_t)LibMCEnv::eDataTableColumnType::FloatColumn), "manifest column information");

			assertTrue(fixture.m_pBuildExecution->GetDataCaptureRowCount("all") == 4, "row count through the build execution");
			assertTrue(fixture.m_pBuildExecution->GetDataCaptureSegmentCount("all") == 1, "segment count through the build execution");

			// The segment is an ordinary version 2 data table stream.
			UTable pTable(fixture.m_pBuildExecution->LoadDataCaptureSegment("all", 0));
			assertTrue(pTable->GetColumnCount() == 8, "loaded column count");
			assertTrue(pTable->GetRowCount64() == 4, "loaded row count");
			assertTrue(pTable->GetColumnIdentifier(7) == "t", "loaded column identifier");
			assertTrue(pTable->GetColumnDescription(7) == "Timestamp", "loaded column description");
			assertTrue(pTable->GetColumnType(6) == LibMCEnv::eDataTableColumnType::BoolColumn, "loaded column type");

			std::vector<double> doublesOut(4);
			pTable->GetDoubleColumnValues("d", doublesOut.size(), nullptr, doublesOut.data());
			assertTrue(doublesOut == doubles, "double column round trip");
			std::vector<int32_t> int32sOut(4);
			pTable->GetInt32ColumnValues("i32", int32sOut.size(), nullptr, int32sOut.data());
			assertTrue(int32sOut == int32s, "int32 column round trip");
			std::vector<uint32_t> uint32sOut(4);
			pTable->GetUint32ColumnValues("u32", uint32sOut.size(), nullptr, uint32sOut.data());
			assertTrue(uint32sOut == uint32s, "uint32 column round trip");
			std::vector<int64_t> int64sOut(4);
			pTable->GetInt64ColumnValues("i64", int64sOut.size(), nullptr, int64sOut.data());
			assertTrue(int64sOut == int64s, "int64 column round trip");
			std::vector<uint64_t> uint64sOut(4);
			pTable->GetUint64ColumnValues("u64", uint64sOut.size(), nullptr, uint64sOut.data());
			assertTrue(uint64sOut == uint64s, "uint64 column round trip");
			std::vector<float> floatsOut(4);
			pTable->GetFloatColumnValues("f", floatsOut.size(), nullptr, floatsOut.data());
			assertTrue(floatsOut == floats, "float column round trip");
			std::vector<uint8_t> boolsOut(4);
			pTable->GetBoolColumnValues("b", boolsOut.size(), nullptr, boolsOut.data());
			assertTrue((boolsOut[0] == 1) && (boolsOut[1] == 0) && (boolsOut[2] == 1) && (boolsOut[3] == 0), "bool column round trip with normalisation");
			std::vector<uint64_t> timestampsOut(4);
			pTable->GetTimestampColumnValues("t", timestampsOut.size(), nullptr, timestampsOut.data());
			assertTrue(timestampsOut == timestamps, "timestamp column round trip");
		}

		void testChunkBoundaries()
		{
			auto fixture = createFixture("boundaries");
			auto pOptions = createCounterOptions(fixture, 4, 0);
			auto pCapture = createCapture(fixture, "counter", pOptions.get());
			addCounterColumn(pCapture.get());

			uint64_t nCounter = 0;
			commitCounterChunk(pCapture.get(), nCounter, 4);
			commitCounterChunk(pCapture.get(), nCounter, 4);

			// A chunk without rows is not an error and does not reach the file.
			{
				UChunk pEmptyChunk(pCapture->BeginChunk());
				pCapture->CommitChunk(pEmptyChunk.get());
			}
			assertTrue(pCapture->GetCapturedRowCount() == 8, "empty chunk does not count");

			// The final chunk is partial.
			commitCounterChunk(pCapture.get(), nCounter, 2);
			assertTrue(pCapture->GetCapturedRowCount() == 10, "captured row count");

			pCapture->Finish();

			auto pManifest = fixture.m_pExecution->RetrieveDataCapture("counter");
			assertTrue(pManifest->GetSegmentCount() == 1, "no rollover without a segment size");
			assertTrue(pManifest->GetRowCount() == 10, "ten rows stored");

			auto values = loadCounterSegment(fixture, "counter", 0);
			assertTrue(values.size() == 10, "ten rows loaded");
			for (size_t nIndex = 0; nIndex < values.size(); nIndex++)
				assertTrue(values[nIndex] == nIndex, "row " + std::to_string(nIndex) + " is in order");
		}

		// A table with the columns of a recording piece: nRowCount rows that continue at nFirstValue.
		UTable createPieceTable(SFixture& fixture, uint32_t nRowCount, uint32_t nFirstValue, uint32_t nLayerIndex, bool bWithLayerColumn)
		{
			UTable pTable(new LibMCEnv::Impl::CDataTable(fixture.m_pToolpathHandler));
			pTable->AddColumn("timestamp", "Timestamp", LibMCEnv::eDataTableColumnType::DoubleColumn);
			pTable->AddColumn("x", "Target X", LibMCEnv::eDataTableColumnType::DoubleColumn);
			pTable->AddColumn("laseron", "LaserOn", LibMCEnv::eDataTableColumnType::Int32Column);
			pTable->AddColumn("tag", "Tag", LibMCEnv::eDataTableColumnType::Uint32Column);

			std::vector<double> timestamps(nRowCount);
			std::vector<double> positions(nRowCount);
			std::vector<int32_t> laserOn(nRowCount);
			std::vector<uint32_t> tags(nRowCount);
			for (uint32_t nIndex = 0; nIndex < nRowCount; nIndex++) {
				uint32_t nValue = nFirstValue + nIndex;
				timestamps[nIndex] = (double)nValue * 10.0;
				positions[nIndex] = (double)nValue * 0.125 - 3.0;
				laserOn[nIndex] = (int32_t)(nValue % 2);
				tags[nIndex] = 1000 + nValue;
			}

			if (nRowCount > 0) {
				pTable->SetDoubleColumnValues("timestamp", timestamps.size(), timestamps.data());
				pTable->SetDoubleColumnValues("x", positions.size(), positions.data());
				pTable->SetInt32ColumnValues("laseron", laserOn.size(), laserOn.data());
				pTable->SetUint32ColumnValues("tag", tags.size(), tags.data());
			}

			if (bWithLayerColumn) {
				std::vector<uint32_t> layers(nRowCount, nLayerIndex);
				pTable->AddColumn("layer", "Layer Index", LibMCEnv::eDataTableColumnType::Uint32Column);
				if (nRowCount > 0)
					pTable->SetUint32ColumnValues("layer", layers.size(), layers.data());
			}

			return pTable;
		}

		void testAppendDataTable()
		{
			auto fixture = createFixture("appendtable");
			auto pOptions = createOptions(fixture, 4, 0);
			auto pCapture = createCapture(fixture, "recording", pOptions.get());

			// The schema is a layer column followed by the columns of the first piece.
			auto pSchemaTable = createPieceTable(fixture, 0, 0, 0, false);
			pCapture->AddColumn("layer", "Layer Index", LibMCEnv::eDataTableColumnType::Uint32Column);
			pCapture->AddColumnsFromDataTable(pSchemaTable.get());
			assertTrue(pCapture->GetColumnCount() == 5, "one capture column per table column");

			assertTrue(errorCodeOf([&]() { pCapture->AddColumnsFromDataTable(pSchemaTable.get()); }) == LIBMCENV_ERROR_DUPLICATECOLUMNIDENTIFIER, "adding the same table twice is rejected");
			assertTrue(pCapture->GetColumnCount() == 5, "a rejected table leaves the schema untouched");
			assertTrue(errorCodeOf([&]() { pCapture->AddColumnsFromDataTable(nullptr); }) == LIBMCENV_ERROR_INVALIDCAST, "null table is rejected");

			// A table without the layer column can not be appended.
			auto pIncompleteTable = createPieceTable(fixture, 3, 0, 0, false);
			assertTrue(errorCodeOf([&]() { pCapture->AppendDataTable(pIncompleteTable.get()); }) == LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND, "table without a capture column is rejected");

			// Ten rows are split into chunks of four.
			auto pFirstPiece = createPieceTable(fixture, 10, 0, 7, true);
			assertTrue(pCapture->AppendDataTable(pFirstPiece.get()) == 10, "ten rows appended");
			assertTrue(pCapture->GetCapturedRowCount() == 10, "ten rows captured");

			assertTrue(errorCodeOf([&]() { pCapture->AddColumnsFromDataTable(pSchemaTable.get()); }) != 0, "schema is frozen after the first append");

			// A further column of the table is ignored.
			auto pSecondPiece = createPieceTable(fixture, 3, 10, 8, true);
			pSecondPiece->AddColumn("extra", "Extra", LibMCEnv::eDataTableColumnType::DoubleColumn);
			assertTrue(pCapture->AppendDataTable(pSecondPiece.get()) == 3, "three rows appended");

			// An empty table appends nothing.
			auto pEmptyPiece = createPieceTable(fixture, 0, 13, 9, true);
			assertTrue(pCapture->AppendDataTable(pEmptyPiece.get()) == 0, "empty table appends no rows");

			// Columns of a different type or length are rejected, and nothing of the table is written.
			{
				UTable pWrongType(new LibMCEnv::Impl::CDataTable(fixture.m_pToolpathHandler));
				pWrongType->AddColumn("layer", "Layer Index", LibMCEnv::eDataTableColumnType::Uint32Column);
				pWrongType->AddColumn("timestamp", "Timestamp", LibMCEnv::eDataTableColumnType::DoubleColumn);
				pWrongType->AddColumn("x", "Target X", LibMCEnv::eDataTableColumnType::FloatColumn);
				pWrongType->AddColumn("laseron", "LaserOn", LibMCEnv::eDataTableColumnType::Int32Column);
				pWrongType->AddColumn("tag", "Tag", LibMCEnv::eDataTableColumnType::Uint32Column);
				assertTrue(errorCodeOf([&]() { pCapture->AppendDataTable(pWrongType.get()); }) == LIBMCENV_ERROR_DATACAPTURECOLUMNTYPEMISMATCH, "column of another type is rejected");

				auto pRagged = createPieceTable(fixture, 5, 13, 9, true);
				std::vector<uint32_t> shortColumn(2, 9);
				pRagged->SetUint32ColumnValues("tag", shortColumn.size(), shortColumn.data());
				assertTrue(errorCodeOf([&]() { pCapture->AppendDataTable(pRagged.get()); }) == LIBMCENV_ERROR_DATACAPTURECHUNKINCOMPLETE, "columns of different length are rejected");
			}
			assertTrue(pCapture->GetCapturedRowCount() == 13, "rejected tables add no rows");

			pCapture->Finish();

			auto pManifest = fixture.m_pExecution->RetrieveDataCapture("recording");
			assertTrue(pManifest->GetRowCount() == 13, "thirteen rows stored");
			assertTrue(pManifest->GetColumnCount() == 5, "five columns stored");
			assertTrue(pManifest->GetSegmentChunkCount(0) == 4, "ten rows make three chunks, three rows one more");

			UTable pLoaded(fixture.m_pBuildExecution->LoadDataCaptureSegment("recording", 0));
			assertTrue(pLoaded->GetRowCount64() == 13, "thirteen rows loaded");

			std::vector<uint32_t> layers(13);
			std::vector<double> timestamps(13);
			std::vector<double> positions(13);
			std::vector<int32_t> laserOn(13);
			std::vector<uint32_t> tags(13);
			pLoaded->GetUint32ColumnValues("layer", layers.size(), nullptr, layers.data());
			pLoaded->GetDoubleColumnValues("timestamp", timestamps.size(), nullptr, timestamps.data());
			pLoaded->GetDoubleColumnValues("x", positions.size(), nullptr, positions.data());
			pLoaded->GetInt32ColumnValues("laseron", laserOn.size(), nullptr, laserOn.data());
			pLoaded->GetUint32ColumnValues("tag", tags.size(), nullptr, tags.data());

			for (uint32_t nIndex = 0; nIndex < 13; nIndex++) {
				std::string sRow = "row " + std::to_string(nIndex);
				assertTrue(layers[nIndex] == ((nIndex < 10) ? 7u : 8u), sRow + " layer");
				assertTrue(timestamps[nIndex] == (double)nIndex * 10.0, sRow + " timestamp");
				assertTrue(positions[nIndex] == (double)nIndex * 0.125 - 3.0, sRow + " position");
				assertTrue(laserOn[nIndex] == (int32_t)(nIndex % 2), sRow + " laser");
				assertTrue(tags[nIndex] == 1000 + nIndex, sRow + " tag");
			}
		}

		void testEmptyCapture()
		{
			auto fixture = createFixture("empty");

			// Columns but no chunk: the schema is still stored.
			{
				auto pCapture = createCapture(fixture, "norows", nullptr);
				pCapture->AddColumn("f", "Float", LibMCEnv::eDataTableColumnType::FloatColumn);
				pCapture->AddColumn("b", "Bool", LibMCEnv::eDataTableColumnType::BoolColumn);
				pCapture->Finish();

				assertTrue(pCapture->GetCapturedRowCount() == 0, "no rows captured");
				assertTrue(pCapture->GetSegmentCount() == 1, "an empty capture has one segment");

				auto pManifest = fixture.m_pExecution->RetrieveDataCapture("norows");
				assertTrue(pManifest->GetStatus() == LibMCData::eDataCaptureStatus::Finished, "empty capture is finished");
				assertTrue(pManifest->GetSegmentCount() == 1, "one segment registered");
				assertTrue(pManifest->GetRowCount() == 0, "no rows registered");
				assertTrue(pManifest->GetColumnCount() == 2, "schema registered");

				UTable pTable(fixture.m_pBuildExecution->LoadDataCaptureSegment("norows", 0));
				assertTrue(pTable->GetColumnCount() == 2, "empty segment keeps its columns");
				assertTrue(pTable->GetRowCount64() == 0, "empty segment has no rows");
				assertTrue(pTable->GetColumnType(0) == LibMCEnv::eDataTableColumnType::FloatColumn, "float column type survives");
				assertTrue(pTable->GetColumnType(1) == LibMCEnv::eDataTableColumnType::BoolColumn, "bool column type survives");
			}

			// No columns at all.
			{
				auto pCapture = createCapture(fixture, "nocolumns", nullptr);
				pCapture->Finish();

				UTable pTable(fixture.m_pBuildExecution->LoadDataCaptureSegment("nocolumns", 0));
				assertTrue(pTable->GetColumnCount() == 0, "capture without columns loads as an empty table");
				assertTrue(pTable->GetRowCount64() == 0, "capture without columns has no rows");
			}
		}

		struct SSegmentInfo {
			std::string m_sStreamUUID;
			uint64_t m_nFirstRow;
			uint64_t m_nRowCount;
			uint64_t m_nSizeInBytes;
			bool m_bIsPurged;
		};

		std::vector<SSegmentInfo> listSegments(SFixture& fixture, const std::string& sIdentifier)
		{
			std::vector<SSegmentInfo> segments;
			auto pManifest = fixture.m_pExecution->RetrieveDataCapture(sIdentifier);
			uint32_t nSegmentCount = pManifest->GetSegmentCount();
			for (uint32_t nIndex = 0; nIndex < nSegmentCount; nIndex++) {
				SSegmentInfo info;
				pManifest->GetSegmentInformation(nIndex, info.m_sStreamUUID, info.m_nFirstRow, info.m_nRowCount, info.m_nSizeInBytes, info.m_bIsPurged);
				segments.push_back(info);
			}
			return segments;
		}

		// Captures nChunkCount chunks of 100 rows with the given segment size and returns the segments.
		std::vector<SSegmentInfo> runRollover(SFixture& fixture, const std::string& sIdentifier, uint64_t nSegmentSize, uint32_t nChunkCount)
		{
			auto pOptions = createCounterOptions(fixture, 100, nSegmentSize);
			auto pCapture = createCapture(fixture, sIdentifier, pOptions.get());
			addCounterColumn(pCapture.get());

			uint64_t nCounter = 0;
			for (uint32_t nIndex = 0; nIndex < nChunkCount; nIndex++)
				commitCounterChunk(pCapture.get(), nCounter, 100);

			pCapture->Finish();
			assertTrue(pCapture->GetCapturedRowCount() == (uint64_t)nChunkCount * 100, "all rows are captured");

			auto segments = listSegments(fixture, sIdentifier);
			assertTrue(pCapture->GetSegmentCount() == segments.size(), "segment count of the capture matches the manifest");

			// The segments are contiguous, within the size limit, and the rows are in order.
			uint64_t nExpectedFirstRow = 0;
			for (uint32_t nSegmentIndex = 0; nSegmentIndex < segments.size(); nSegmentIndex++) {
				auto& segment = segments[nSegmentIndex];
				assertTrue(segment.m_nFirstRow == nExpectedFirstRow, "segment " + std::to_string(nSegmentIndex) + " continues the row numbering");
				assertTrue(segment.m_nSizeInBytes <= nSegmentSize, "segment " + std::to_string(nSegmentIndex) + " is within the size limit");
				assertTrue(fixture.m_pStorage->RetrieveStream(segment.m_sStreamUUID)->GetSize() == segment.m_nSizeInBytes, "manifest size is the stream size");
				assertTrue(fixture.m_pStorage->StreamIsReady(segment.m_sStreamUUID), "segment stream is finished");

				auto values = loadCounterSegment(fixture, sIdentifier, nSegmentIndex);
				assertTrue(values.size() == segment.m_nRowCount, "loaded row count matches the manifest");
				for (size_t nRowIndex = 0; nRowIndex < values.size(); nRowIndex++)
					assertTrue(values[nRowIndex] == nExpectedFirstRow + nRowIndex, "segment " + std::to_string(nSegmentIndex) + " row " + std::to_string(nRowIndex) + " is in order");

				nExpectedFirstRow += segment.m_nRowCount;
			}
			assertTrue(nExpectedFirstRow == (uint64_t)nChunkCount * 100, "segments hold all rows");

			return segments;
		}

		void testSegmentRollover()
		{
			auto fixture = createFixture("rollover");

			// A segment of three chunks is exactly this large.
			uint64_t nThreeChunkSize = counterSegmentSize(3, 100);

			// Exactly on the boundary: three chunks fit, the fourth rolls over.
			auto exact = runRollover(fixture, "exact", nThreeChunkSize, 7);
			assertTrue(exact.size() == 3, "seven chunks make three segments on the boundary");
			assertTrue((exact[0].m_nRowCount == 300) && (exact[1].m_nRowCount == 300) && (exact[2].m_nRowCount == 100), "three, three and one chunk");
			assertTrue(exact[0].m_nSizeInBytes == nThreeChunkSize, "a full segment has exactly the predicted size");

			// One byte below: only two chunks fit.
			auto below = runRollover(fixture, "below", nThreeChunkSize - 1, 7);
			assertTrue(below.size() == 4, "seven chunks make four segments below the boundary");
			assertTrue((below[0].m_nRowCount == 200) && (below[3].m_nRowCount == 100), "two chunks per segment");

			// One byte above: still three chunks.
			auto above = runRollover(fixture, "above", nThreeChunkSize + 1, 7);
			assertTrue(above.size() == 3, "seven chunks make three segments above the boundary");
			assertTrue(above[0].m_nRowCount == 300, "three chunks per segment");

			// A segment always holds at least one chunk, even if the chunk alone exceeds the limit.
			{
				auto pOptions = createCounterOptions(fixture, 100, DATACAPTURE_MINSEGMENTSIZE);
				auto pCapture = createCapture(fixture, "tiny", pOptions.get());
				addCounterColumn(pCapture.get());
				uint64_t nCounter = 0;
				for (uint32_t nIndex = 0; nIndex < 3; nIndex++)
					commitCounterChunk(pCapture.get(), nCounter, 100);
				pCapture->Finish();

				auto tiny = listSegments(fixture, "tiny");
				assertTrue(tiny.size() == 3, "one chunk per segment");
				assertTrue(tiny[0].m_nSizeInBytes == counterSegmentSize(1, 100), "segment of one chunk");
				assertTrue(tiny[0].m_nSizeInBytes > DATACAPTURE_MINSEGMENTSIZE, "a single chunk may exceed the segment size");
			}
		}

		void testRowWiseAPI()
		{
			auto fixture = createFixture("rows");
			auto pOptions = createOptions(fixture, 4, 0);
			auto pCapture = createCapture(fixture, "rows", pOptions.get());
			pCapture->AddColumn("t", "Time", LibMCEnv::eDataTableColumnType::TimestampColumn);
			pCapture->AddColumn("f", "Float", LibMCEnv::eDataTableColumnType::FloatColumn);
			pCapture->AddColumn("i", "Int32", LibMCEnv::eDataTableColumnType::Int32Column);
			pCapture->AddColumn("b", "Bool", LibMCEnv::eDataTableColumnType::BoolColumn);
			pCapture->AddColumn("d", "Double", LibMCEnv::eDataTableColumnType::DoubleColumn);
			pCapture->AddColumn("u", "Uint32", LibMCEnv::eDataTableColumnType::Uint32Column);

			UChunk pChunk(pCapture->BeginChunk());

			assertTrue(errorCodeOf([&]() { pChunk->SetRowDoubleValue(1, 1.0); }) == LIBMCENV_ERROR_DATACAPTUREROWNOTOPEN, "value without an open row is rejected");
			assertTrue(errorCodeOf([&]() { pChunk->EndRow(); }) == LIBMCENV_ERROR_DATACAPTUREROWNOTOPEN, "EndRow without an open row is rejected");

			pChunk->BeginRow();
			assertTrue(errorCodeOf([&]() { pChunk->BeginRow(); }) == LIBMCENV_ERROR_DATACAPTUREROWALREADYOPEN, "second BeginRow is rejected");
			pChunk->SetRowIntegerValue(0, 1000);
			pChunk->SetRowDoubleValue(1, 1.5);
			pChunk->SetRowIntegerValue(2, -7);
			pChunk->SetRowIntegerValue(3, 5);
			pChunk->SetRowDoubleValue(4, 2.5);
			pChunk->SetRowIntegerValue(5, 42);

			assertTrue(errorCodeOf([&]() { pChunk->SetRowDoubleValue(2, 1.0); }) == LIBMCENV_ERROR_DATACAPTURECOLUMNTYPEMISMATCH, "double value on an integer column is rejected");
			assertTrue(errorCodeOf([&]() { pChunk->SetRowIntegerValue(1, 1); }) == LIBMCENV_ERROR_DATACAPTURECOLUMNTYPEMISMATCH, "integer value on a float column is rejected");
			assertTrue(errorCodeOf([&]() { pChunk->SetRowIntegerValue(2, 5000000000LL); }) == LIBMCENV_ERROR_DATACAPTUREVALUEOUTOFRANGE, "int32 overflow is rejected");
			assertTrue(errorCodeOf([&]() { pChunk->SetRowIntegerValue(5, -1); }) == LIBMCENV_ERROR_DATACAPTUREVALUEOUTOFRANGE, "negative uint32 is rejected");
			assertTrue(errorCodeOf([&]() { pChunk->SetRowIntegerValue(0, -1); }) == LIBMCENV_ERROR_DATACAPTUREVALUEOUTOFRANGE, "negative timestamp is rejected");
			assertTrue(errorCodeOf([&]() { pChunk->SetRowIntegerValue(9, 1); }) == LIBMCENV_ERROR_INVALIDCOLUMNINDEX, "unknown column index is rejected");

			std::vector<uint64_t> bulk = { 1, 2 };
			assertTrue(errorCodeOf([&]() { pChunk->SetTimestampColumnValues(0, bulk.size(), bulk.data()); }) == LIBMCENV_ERROR_DATACAPTUREROWALREADYOPEN, "bulk setter while a row is open is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->CommitChunk(pChunk.get()); }) == LIBMCENV_ERROR_DATACAPTUREROWALREADYOPEN, "commit with an open row is rejected");

			pChunk->EndRow();
			assertTrue(pChunk->GetRowCount() == 1, "one row after EndRow");

			// Columns that are not set are zero.
			pChunk->BeginRow();
			pChunk->SetRowIntegerValue(0, 2000);
			pChunk->EndRow();

			pChunk->BeginRow();
			pChunk->SetRowIntegerValue(0, 3000);
			pChunk->SetRowDoubleValue(1, -0.25);
			pChunk->SetRowIntegerValue(2, 2147483647);
			pChunk->SetRowIntegerValue(3, 0);
			pChunk->SetRowDoubleValue(4, -1.0e9);
			pChunk->SetRowIntegerValue(5, 4000000000LL);
			pChunk->EndRow();

			pChunk->BeginRow();
			pChunk->SetRowIntegerValue(0, 4000);
			pChunk->EndRow();

			assertTrue(pChunk->GetRowCount() == 4, "four rows");
			assertTrue(pChunk->IsFull(), "chunk is full");
			assertTrue(errorCodeOf([&]() { pChunk->BeginRow(); }) == LIBMCENV_ERROR_DATACAPTURECHUNKOVERFLOW, "row beyond the capacity is rejected");

			pCapture->CommitChunk(pChunk.get());
			pCapture->Finish();

			UTable pTable(fixture.m_pBuildExecution->LoadDataCaptureSegment("rows", 0));
			assertTrue(pTable->GetRowCount64() == 4, "four rows loaded");

			std::vector<uint64_t> timestamps(4);
			pTable->GetTimestampColumnValues("t", timestamps.size(), nullptr, timestamps.data());
			assertTrue((timestamps[0] == 1000) && (timestamps[1] == 2000) && (timestamps[2] == 3000) && (timestamps[3] == 4000), "timestamp rows");

			std::vector<float> floats(4);
			pTable->GetFloatColumnValues("f", floats.size(), nullptr, floats.data());
			assertTrue((floats[0] == 1.5f) && (floats[1] == 0.0f) && (floats[2] == -0.25f) && (floats[3] == 0.0f), "float rows with zero for unset values");

			std::vector<int32_t> int32s(4);
			pTable->GetInt32ColumnValues("i", int32s.size(), nullptr, int32s.data());
			assertTrue((int32s[0] == -7) && (int32s[1] == 0) && (int32s[2] == 2147483647), "int32 rows");

			std::vector<uint8_t> bools(4);
			pTable->GetBoolColumnValues("b", bools.size(), nullptr, bools.data());
			assertTrue((bools[0] == 1) && (bools[1] == 0) && (bools[2] == 0), "bool rows are normalised");

			std::vector<double> doubles(4);
			pTable->GetDoubleColumnValues("d", doubles.size(), nullptr, doubles.data());
			assertTrue((doubles[0] == 2.5) && (doubles[1] == 0.0) && (doubles[2] == -1.0e9), "double rows");

			std::vector<uint32_t> uint32s(4);
			pTable->GetUint32ColumnValues("u", uint32s.size(), nullptr, uint32s.data());
			assertTrue((uint32s[0] == 42) && (uint32s[2] == 4000000000U), "uint32 rows");
		}

		void testChunkValidation()
		{
			auto fixture = createFixture("validation");
			auto pOptions = createOptions(fixture, 4, 0);
			auto pCapture = createCapture(fixture, "checked", pOptions.get());

			assertTrue(errorCodeOf([&]() { pCapture->AddColumn("", "Empty", LibMCEnv::eDataTableColumnType::DoubleColumn); }) == LIBMCENV_ERROR_COLUMNIDENTIFIEREMPTY, "empty column identifier is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->AddColumn("bad id!", "Invalid", LibMCEnv::eDataTableColumnType::DoubleColumn); }) == LIBMCENV_ERROR_INVALIDCOLUMNIDENTIFIER, "invalid column identifier is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->AddColumn("x", "Unknown", (LibMCEnv::eDataTableColumnType)99); }) == LIBMCENV_ERROR_INVALIDCOLUMNDATATYPE, "unknown column type is rejected");

			pCapture->AddColumn("n", "Counter", LibMCEnv::eDataTableColumnType::Uint64Column);
			pCapture->AddColumn("v", "Value", LibMCEnv::eDataTableColumnType::DoubleColumn);
			assertTrue(errorCodeOf([&]() { pCapture->AddColumn("n", "Duplicate", LibMCEnv::eDataTableColumnType::DoubleColumn); }) == LIBMCENV_ERROR_DUPLICATECOLUMNIDENTIFIER, "duplicate column identifier is rejected");

			std::vector<uint64_t> counters = { 1, 2, 3 };
			std::vector<double> values = { 0.5, 1.0, 1.5 };
			std::vector<uint64_t> tooMany = { 1, 2, 3, 4, 5 };

			UChunk pChunk(pCapture->BeginChunk());

			// The schema is frozen with the first chunk.
			assertTrue(errorCodeOf([&]() { pCapture->AddColumn("late", "Late", LibMCEnv::eDataTableColumnType::DoubleColumn); }) == LIBMCENV_ERROR_DATACAPTURENOTCONFIGURABLE, "column after the first chunk is rejected");
			assertTrue(errorCodeOf([&]() { UChunk pSecond(pCapture->BeginChunk()); }) == LIBMCENV_ERROR_DATACAPTURECHUNKALREADYOPEN, "second chunk while one is uncommitted is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->Finish(); }) == LIBMCENV_ERROR_DATACAPTURECHUNKOUTSTANDING, "Finish with an uncommitted chunk is rejected");

			assertTrue(errorCodeOf([&]() { pChunk->SetUint64ColumnValues(0, tooMany.size(), tooMany.data()); }) == LIBMCENV_ERROR_DATACAPTURECHUNKOVERFLOW, "more rows than the capacity are rejected");
			assertTrue(errorCodeOf([&]() { pChunk->SetDoubleColumnValues(0, values.size(), values.data()); }) == LIBMCENV_ERROR_DATACAPTURECOLUMNTYPEMISMATCH, "wrong column type is rejected");
			assertTrue(errorCodeOf([&]() { pChunk->SetTimestampColumnValues(0, counters.size(), counters.data()); }) == LIBMCENV_ERROR_DATACAPTURECOLUMNTYPEMISMATCH, "timestamp setter on a uint64 column is rejected");
			assertTrue(errorCodeOf([&]() { pChunk->SetUint64ColumnValues(5, counters.size(), counters.data()); }) == LIBMCENV_ERROR_INVALIDCOLUMNINDEX, "unknown column index is rejected");

			// A chunk whose columns have different lengths is rejected and stays with the producer.
			pChunk->SetUint64ColumnValues(0, counters.size(), counters.data());
			pChunk->SetDoubleColumnValues(1, 2, values.data());
			assertTrue(errorCodeOf([&]() { pCapture->CommitChunk(pChunk.get()); }) == LIBMCENV_ERROR_DATACAPTURECHUNKINCOMPLETE, "columns of different length are rejected");
			assertTrue(pCapture->GetCapturedRowCount() == 0, "a rejected chunk is not captured");

			pChunk->SetDoubleColumnValues(1, values.size(), values.data());
			pCapture->CommitChunk(pChunk.get());
			assertTrue(pCapture->GetCapturedRowCount() == 3, "the completed chunk is captured");

			// A committed chunk is not usable anymore.
			assertTrue(errorCodeOf([&]() { pChunk->GetRowCount(); }) == LIBMCENV_ERROR_DATACAPTURECHUNKINVALID, "committed chunk is invalid");
			assertTrue(errorCodeOf([&]() { pChunk->SetUint64ColumnValues(0, counters.size(), counters.data()); }) == LIBMCENV_ERROR_DATACAPTURECHUNKINVALID, "writing to a committed chunk is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->CommitChunk(pChunk.get()); }) == LIBMCENV_ERROR_DATACAPTURECHUNKINVALID, "committing a chunk twice is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->CommitChunk(nullptr); }) == LIBMCENV_ERROR_DATACAPTURECHUNKINVALID, "committing a null chunk is rejected");

			// A chunk of another capture is rejected.
			auto pOtherCapture = createCapture(fixture, "other", pOptions.get());
			pOtherCapture->AddColumn("n", "Counter", LibMCEnv::eDataTableColumnType::Uint64Column);
			{
				UChunk pOtherChunk(pOtherCapture->BeginChunk());
				assertTrue(errorCodeOf([&]() { pCapture->CommitChunk(pOtherChunk.get()); }) == LIBMCENV_ERROR_DATACAPTURECHUNKINVALID, "chunk of another capture is rejected");
			}
			pOtherCapture->Abort();

			pCapture->Finish();
			assertTrue(errorCodeOf([&]() { UChunk pLate(pCapture->BeginChunk()); }) == LIBMCENV_ERROR_DATACAPTURENOTRECORDING, "chunk after Finish is rejected");
			assertTrue(errorCodeOf([&]() { pCapture->Abort(); }) == LIBMCENV_ERROR_DATACAPTURENOTRECORDING, "a finished capture is not aborted");
		}

		void testAbortDeletesEverything()
		{
			auto fixture = createFixture("abort");
			size_t nBaselineStreamFiles = countStreamFiles(fixture);

			{
				// One chunk per segment: some segments are finalized, some wait for the finalizer, one is open.
				auto pOptions = createCounterOptions(fixture, 100, DATACAPTURE_MINSEGMENTSIZE);
				auto pCapture = createCapture(fixture, "aborted", pOptions.get());
				addCounterColumn(pCapture.get());

				uint64_t nCounter = 0;
				for (uint32_t nIndex = 0; nIndex < 6; nIndex++)
					commitCounterChunk(pCapture.get(), nCounter, 100);

				pCapture->Flush();
				assertTrue(countStreamFiles(fixture) > nBaselineStreamFiles, "segment files exist while recording");

				UChunk pOutstandingChunk(pCapture->BeginChunk());

				pCapture->Abort();
				assertTrue(pCapture->GetStatus() == LibMCEnv::eDataCaptureStatus::Aborted, "capture is aborted");

				assertTrue(countStreamFiles(fixture) == nBaselineStreamFiles, "no segment file is left after abort");
				assertTrue(fixture.m_pStorage->GetDataCaptureStorageSize() == 0, "no capture data is stored after abort");

				auto pManifest = fixture.m_pExecution->RetrieveDataCapture("aborted");
				assertTrue(pManifest->GetStatus() == LibMCData::eDataCaptureStatus::Aborted, "manifest is aborted");
				assertTrue(pManifest->GetStoredSizeInBytes() == 0, "manifest has nothing stored");

				pCapture->Abort();
				assertTrue(errorCodeOf([&]() { UChunk pLate(pCapture->BeginChunk()); }) == LIBMCENV_ERROR_DATACAPTURENOTRECORDING, "chunk after abort is rejected");
				assertTrue(errorCodeOf([&]() { pCapture->Finish(); }) == LIBMCENV_ERROR_DATACAPTURENOTRECORDING, "Finish after abort is rejected");
			}

			// A capture that never recorded is aborted without touching the storage.
			{
				auto pCapture = createCapture(fixture, "unused", nullptr);
				addCounterColumn(pCapture.get());
				pCapture->Abort();
				assertTrue(pCapture->GetStatus() == LibMCEnv::eDataCaptureStatus::Aborted, "unused capture is aborted");
				assertTrue(fixture.m_pExecution->RetrieveDataCapture("unused")->GetStatus() == LibMCData::eDataCaptureStatus::Aborted, "unused manifest is aborted");
				assertTrue(countStreamFiles(fixture) == nBaselineStreamFiles, "no segment file was created");
			}
		}

		void testBackpressureBlock()
		{
			auto fixture = createFixture("block");

			// One chunk per segment keeps the writer busy, so the producer runs into the queue limit.
			auto pOptions = createCounterOptions(fixture, 100, DATACAPTURE_MINSEGMENTSIZE, 2, LibMCEnv::eDataCaptureBackpressureMode::Block);
			auto pCapture = createCapture(fixture, "blocked", pOptions.get());
			addCounterColumn(pCapture.get());

			uint64_t nCounter = 0;
			for (uint32_t nIndex = 0; nIndex < 40; nIndex++) {
				commitCounterChunk(pCapture.get(), nCounter, 100);
				assertTrue(pCapture->GetQueuedChunkCount() <= 2, "queue never exceeds its depth");
			}

			pCapture->Finish();
			assertTrue(pCapture->GetDroppedRowCount() == 0, "Block mode drops nothing");
			assertTrue(pCapture->GetCapturedRowCount() == 4000, "all rows are captured");
			assertTrue(pCapture->GetQueuedChunkCount() == 0, "queue is empty after Finish");

			auto segments = listSegments(fixture, "blocked");
			assertTrue(segments.size() == 40, "forty segments");
			assertTrue(segments[39].m_nFirstRow == 3900, "last segment continues the row numbering");

			auto lastValues = loadCounterSegment(fixture, "blocked", 39);
			assertTrue((lastValues.size() == 100) && (lastValues[0] == 3900) && (lastValues[99] == 3999), "last segment holds the last rows");
		}

		void testBackpressureDropOldest()
		{
			auto fixture = createFixture("dropoldest");

			auto pOptions = createCounterOptions(fixture, 100, DATACAPTURE_MINSEGMENTSIZE, 1, LibMCEnv::eDataCaptureBackpressureMode::DropOldest);
			auto pCapture = createCapture(fixture, "dropping", pOptions.get());
			addCounterColumn(pCapture.get());

			// The producer is orders of magnitude faster than a writer that rolls over on every chunk.
			uint64_t nCounter = 0;
			for (uint32_t nIndex = 0; nIndex < 60; nIndex++) {
				commitCounterChunk(pCapture.get(), nCounter, 100);
				assertTrue(pCapture->GetQueuedChunkCount() <= 1, "queue never exceeds its depth");
			}

			pCapture->Finish();

			uint64_t nDroppedRows = pCapture->GetDroppedRowCount();
			uint64_t nCapturedRows = pCapture->GetCapturedRowCount();
			assertTrue(nDroppedRows > 0, "the producer outran the writer and rows were dropped");
			assertTrue(nDroppedRows % 100 == 0, "whole chunks are dropped");
			assertTrue(nCapturedRows + nDroppedRows == 6000, "every committed row is either captured or counted as dropped");

			auto pManifest = fixture.m_pExecution->RetrieveDataCapture("dropping");
			assertTrue(pManifest->GetRowCount() == nCapturedRows, "the stored rows are the captured rows");

			// The producer was never blocked and the newest chunk always survives.
			auto segments = listSegments(fixture, "dropping");
			auto lastValues = loadCounterSegment(fixture, "dropping", (uint32_t)segments.size() - 1);
			assertTrue((lastValues.size() == 100) && (lastValues[99] == 5999), "the last committed chunk is stored");
		}

		void testBackpressureFail()
		{
			auto fixture = createFixture("fail");

			auto pOptions = createCounterOptions(fixture, 100, DATACAPTURE_MINSEGMENTSIZE, 1, LibMCEnv::eDataCaptureBackpressureMode::Fail);
			auto pCapture = createCapture(fixture, "failing", pOptions.get());
			addCounterColumn(pCapture.get());

			uint32_t nQueueFullCount = 0;
			uint64_t nCounter = 0;
			for (uint32_t nIndex = 0; nIndex < 60; nIndex++) {

				// When the queue is full, the producer decides: here it waits and tries again.
				for (;;) {
					int32_t nErrorCode = errorCodeOf([&]() { commitCounterChunk(pCapture.get(), nCounter, 100); });
					if (nErrorCode == 0)
						break;

					assertTrue(nErrorCode == LIBMCENV_ERROR_DATACAPTUREQUEUEFULL, "a full queue raises DATACAPTUREQUEUEFULL, got " + std::to_string(nErrorCode));
					nQueueFullCount++;
					pCapture->Flush();
				}
			}

			pCapture->Finish();
			assertTrue(nQueueFullCount > 0, "the producer outran the writer and the queue was reported as full");
			assertTrue(pCapture->GetDroppedRowCount() == 0, "Fail mode drops nothing");
			assertTrue(pCapture->GetCapturedRowCount() == 6000, "all rows are captured after retrying");
			assertTrue(fixture.m_pExecution->RetrieveDataCapture("failing")->GetRowCount() == 6000, "all rows are stored");
		}

		void testRetentionPurgesOldest()
		{
			auto fixture = createFixture("retention");

			// One chunk per segment. The limit holds exactly three segments.
			uint64_t nSegmentSize = counterSegmentSize(1, 100);
			auto pOptions = createCounterOptions(fixture, 100, DATACAPTURE_MINSEGMENTSIZE, 4, LibMCEnv::eDataCaptureBackpressureMode::Block, 3 * nSegmentSize);
			auto pCapture = createCapture(fixture, "retained", pOptions.get());
			addCounterColumn(pCapture.get());

			uint64_t nCounter = 0;
			for (uint32_t nIndex = 0; nIndex < 8; nIndex++)
				commitCounterChunk(pCapture.get(), nCounter, 100);

			pCapture->Finish();

			auto segments = listSegments(fixture, "retained");
			assertTrue(segments.size() == 8, "all eight segments are in the manifest");

			for (uint32_t nIndex = 0; nIndex < 5; nIndex++)
				assertTrue(segments[nIndex].m_bIsPurged, "old segment " + std::to_string(nIndex) + " is purged");
			for (uint32_t nIndex = 5; nIndex < 8; nIndex++)
				assertFalse(segments[nIndex].m_bIsPurged, "new segment " + std::to_string(nIndex) + " is stored");

			assertTrue(fixture.m_pStorage->GetDataCaptureStorageSize() == 3 * nSegmentSize, "the stored size is at the limit");
			assertTrue(fixture.m_pBuildExecution->GetDataCaptureRowCount("retained") == 800, "the manifest keeps the full row count");
			assertTrue(fixture.m_pExecution->RetrieveDataCapture("retained")->GetStatus() == LibMCData::eDataCaptureStatus::Finished, "the capture finishes normally");

			assertTrue(errorCodeOf([&]() { UTable pTable(fixture.m_pBuildExecution->LoadDataCaptureSegment("retained", 0)); }) == LIBMCENV_ERROR_DATACAPTURESEGMENTPURGED, "a purged segment can not be loaded");

			auto values = loadCounterSegment(fixture, "retained", 7);
			assertTrue((values.size() == 100) && (values[0] == 700), "the newest segment is loadable");
		}

		void testCountersAndFlush()
		{
			auto fixture = createFixture("counters");
			auto pOptions = createCounterOptions(fixture, 100, DATACAPTURE_MINSEGMENTSIZE);
			auto pCapture = createCapture(fixture, "counted", pOptions.get());
			addCounterColumn(pCapture.get());

			assertTrue(pCapture->GetSegmentCount() == 0, "no segment before recording");
			assertTrue(pCapture->GetStorageSizeInBytes() == 0, "nothing written before recording");
			pCapture->Flush();

			uint64_t nCounter = 0;
			for (uint32_t nIndex = 0; nIndex < 3; nIndex++)
				commitCounterChunk(pCapture.get(), nCounter, 100);

			pCapture->Flush();
			assertTrue(pCapture->GetQueuedChunkCount() == 0, "queue is empty after Flush");
			assertTrue(pCapture->GetSegmentCount() == 3, "three segments have been opened");
			assertTrue(pCapture->GetCapturedRowCount() == 300, "three hundred rows captured");
			assertTrue(pCapture->GetDroppedRowCount() == 0, "nothing dropped");

			// Two segments are closed, the third one is open and has no footer yet.
			uint64_t nSizeBeforeFinish = pCapture->GetStorageSizeInBytes();
			uint64_t nOpenSegmentSize = counterSegmentSize(1, 100) - AMC::CDataTableV2SegmentBuilder({ { "n", "Counter", AMC::eDataTableV2DataType::Uint64 } }, 0, 0, 100).getFooterSizeInBytes(1);
			assertTrue(nSizeBeforeFinish == 2 * counterSegmentSize(1, 100) + nOpenSegmentSize, "storage size counts every written byte");

			pCapture->Finish();
			assertTrue(pCapture->GetStorageSizeInBytes() == 3 * counterSegmentSize(1, 100), "storage size after Finish is the size of all segments");
			assertTrue(fixture.m_pExecution->RetrieveDataCapture("counted")->GetStoredSizeInBytes() == pCapture->GetStorageSizeInBytes(), "manifest size matches the capture");
		}

		void testAbandonedCaptureIsFinished()
		{
			auto fixture = createFixture("abandoned");

			{
				auto pOptions = createOptions(fixture, 100, 0);
				auto pCapture = createCapture(fixture, "abandoned", pOptions.get());
				addCounterColumn(pCapture.get());

				uint64_t nCounter = 0;
				commitCounterChunk(pCapture.get(), nCounter, 100);
				commitCounterChunk(pCapture.get(), nCounter, 50);

				// The capture is released without Finish or Abort.
			}

			auto pManifest = fixture.m_pExecution->RetrieveDataCapture("abandoned");
			assertTrue(pManifest->GetStatus() == LibMCData::eDataCaptureStatus::Finished, "an abandoned capture is finished");
			assertTrue(pManifest->GetRowCount() == 150, "an abandoned capture keeps its committed rows");

			auto values = loadCounterSegment(fixture, "abandoned", 0);
			assertTrue((values.size() == 150) && (values[149] == 149), "the committed rows are loadable");
		}

		void testTruncatedSegment()
		{
			auto fixture = createFixture("truncated");
			auto pOptions = createOptions(fixture, 100, 0);
			auto pCapture = createCapture(fixture, "whole", pOptions.get());
			addCounterColumn(pCapture.get());

			uint64_t nCounter = 0;
			commitCounterChunk(pCapture.get(), nCounter, 100);
			commitCounterChunk(pCapture.get(), nCounter, 100);
			pCapture->Finish();

			auto segments = listSegments(fixture, "whole");
			std::vector<uint8_t> bytes;
			fixture.m_pStorage->RetrieveStream(segments[0].m_sStreamUUID)->GetContent(bytes);
			assertTrue(bytes.size() == segments[0].m_nSizeInBytes, "segment content is read");

			auto loadBytes = [&](const std::vector<uint8_t>& content, LibMCEnv::Impl::CDataTable* pTable) {
				auto pWriter = std::make_unique<LibMCEnv::Impl::CTempStreamWriter>(fixture.m_pDataModel, "copy.dat", "application/amcf-datatable", AMCCommon::CUtils::createEmptyUUID(), fixture.m_pChrono);
				pWriter->WriteData(content.size(), content.data());
				pWriter->Finish();
				std::unique_ptr<LibMCEnv::Impl::IStreamReader> pReader(pWriter->GetStreamReader());
				pTable->LoadFromStream(pReader.get());
			};

			auto pTable = std::make_unique<LibMCEnv::Impl::CDataTable>(fixture.m_pToolpathHandler);

			// The intact copy loads.
			loadBytes(bytes, pTable.get());
			assertTrue(pTable->GetRowCount64() == 200, "intact segment copy loads");

			// A capture that was interrupted before the footer was written: all chunk data is there, the footer is missing.
			AMC::sDataTableV2Trailer trailer;
			memcpy(&trailer, bytes.data() + bytes.size() - sizeof(trailer), sizeof(trailer));
			std::vector<uint8_t> interrupted(bytes.begin(), bytes.end() - sizeof(trailer) - (size_t)trailer.m_nFooterLength);

			int32_t nErrorCode = errorCodeOf([&]() { loadBytes(interrupted, pTable.get()); });
			assertTrue(nErrorCode == LIBMCENV_ERROR_DATATABLESEGMENTTRUNCATED, "interrupted segment is reported as truncated, got " + std::to_string(nErrorCode));
			assertTrue(pTable->GetColumnCount() == 0, "no garbage is loaded from an interrupted segment");

			// A segment cut in the middle of a page.
			std::vector<uint8_t> cut(bytes.begin(), bytes.begin() + bytes.size() / 2);
			nErrorCode = errorCodeOf([&]() { loadBytes(cut, pTable.get()); });
			assertTrue(nErrorCode == LIBMCENV_ERROR_DATATABLESEGMENTTRUNCATED, "cut segment is reported as truncated, got " + std::to_string(nErrorCode));
			assertTrue(pTable->GetColumnCount() == 0, "no garbage is loaded from a cut segment");
		}

	};

}

#endif // __AMCTEST_UNITTEST_DATACAPTURE
