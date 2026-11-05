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

#ifndef __AMCTEST_UNITTEST_DATATABLEREADER
#define __AMCTEST_UNITTEST_DATATABLEREADER

#include "amc_unittests_datacapture.hpp"
#include "libmcenv_datatablereader.hpp"

#include <chrono>
#include <cmath>
#include <limits>

namespace AMCUnitTest {

	// Uses the fixture and the helpers of the data capture tests.
	class CUnitTestGroup_DataTableReader : public CUnitTestGroup_DataCapture {
	public:

		std::string getTestGroupName() override {
			return "DataTableReader";
		}

		void registerTests() override {
			registerTest("DefaultEncodingsCompress", "Default options encode and compress a capture and keep it exact", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testDefaultEncodingsCompress, this));
			registerTest("CompressionModes", "None, LZ4 and deflate captures are read back identically", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testCompressionModes, this));
			registerTest("QuantizedDoubleColumn", "Opt-in quantisation of a double column is bounded and recorded", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testQuantizedDoubleColumn, this));
			registerTest("EncodingOptionValidation", "Encoding options that do not fit the schema fail when recording starts", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testEncodingOptionValidation, this));
			registerTest("SchemaAndChunks", "Reader reports schema, segments and chunks of a capture", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testSchemaAndChunks, this));
			registerTest("RangeReads", "Row ranges across chunk and segment boundaries", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testRangeReads, this));
			registerTest("ReadSelectivity", "A range read touches only the pages that overlap it", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testReadSelectivity, this));
			registerTest("ChunkStatisticsAndValueSearch", "Page statistics and chunk search by value range", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testChunkStatisticsAndValueSearch, this));
			registerTest("Downsampling", "Stride, MinMax and Average downsampling", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testDownsampling, this));
			registerTest("ExtractDataTable", "A row range is materialised as a data table", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testExtractDataTable, this));
			registerTest("PurgedSegments", "Reader reports segments that retention has deleted", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testPurgedSegments, this));
			registerTest("ThroughputReport", "Synthetic throughput of the write path per codec", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableReader::testThroughputReport, this));
		}

	protected:

		typedef std::unique_ptr<LibMCEnv::Impl::IDataTableReader> UReader;

		// The signal capture: 2500 rows in chunks of 100.
		//   t  timestamp, 10 microseconds per row
		//   v  float sawtooth 0..249 with a single spike of 5000 at row 1234
		//   n  row counter
		//   b  bool that toggles every 100 rows
		static constexpr uint32_t SIGNAL_ROWCOUNT = 2500;
		static constexpr uint32_t SIGNAL_CHUNKROWS = 100;
		static constexpr uint32_t SIGNAL_SPIKEROW = 1234;

		uint64_t signalTimestamp(uint64_t nRow) { return 1000000ULL + nRow * 10; }
		float signalValue(uint64_t nRow) { return (nRow == SIGNAL_SPIKEROW) ? 5000.0f : (float)(nRow % 250); }
		uint8_t signalBool(uint64_t nRow) { return (uint8_t)((nRow / 100) % 2); }

		void captureSignal(SFixture& fixture, const std::string& sIdentifier, LibMCEnv::Impl::IDataCaptureOptions* pOptions, uint32_t nRowCount = SIGNAL_ROWCOUNT)
		{
			auto pCapture = createCapture(fixture, sIdentifier, pOptions);
			pCapture->AddColumn("t", "Time", LibMCEnv::eDataTableColumnType::TimestampColumn);
			pCapture->AddColumn("v", "Value", LibMCEnv::eDataTableColumnType::FloatColumn);
			pCapture->AddColumn("n", "Counter", LibMCEnv::eDataTableColumnType::Uint64Column);
			pCapture->AddColumn("b", "Flag", LibMCEnv::eDataTableColumnType::BoolColumn);

			for (uint32_t nFirstRow = 0; nFirstRow < nRowCount; nFirstRow += SIGNAL_CHUNKROWS) {
				std::vector<uint64_t> timestamps(SIGNAL_CHUNKROWS);
				std::vector<float> values(SIGNAL_CHUNKROWS);
				std::vector<uint64_t> counters(SIGNAL_CHUNKROWS);
				std::vector<uint8_t> flags(SIGNAL_CHUNKROWS);
				for (uint32_t nIndex = 0; nIndex < SIGNAL_CHUNKROWS; nIndex++) {
					uint64_t nRow = (uint64_t)nFirstRow + nIndex;
					timestamps[nIndex] = signalTimestamp(nRow);
					values[nIndex] = signalValue(nRow);
					counters[nIndex] = nRow;
					flags[nIndex] = signalBool(nRow);
				}

				UChunk pChunk(pCapture->BeginChunk());
				pChunk->SetTimestampColumnValues(0, timestamps.size(), timestamps.data());
				pChunk->SetFloatColumnValues(1, values.size(), values.data());
				pChunk->SetUint64ColumnValues(2, counters.size(), counters.data());
				pChunk->SetBoolColumnValues(3, flags.size(), flags.data());
				pCapture->CommitChunk(pChunk.get());
			}

			pCapture->Finish();
		}

		// Segments of four chunks: the pages are plain and uncompressed, so that the segment layout is deterministic.
		UOptions signalOptions(SFixture& fixture, uint64_t nStorageLimitInBytes = 0)
		{
			std::vector<AMC::sDataTableV2Column> columns;
			columns.push_back({ "t", "Time", AMC::eDataTableV2DataType::Timestamp });
			columns.push_back({ "v", "Value", AMC::eDataTableV2DataType::Float });
			columns.push_back({ "n", "Counter", AMC::eDataTableV2DataType::Uint64 });
			columns.push_back({ "b", "Flag", AMC::eDataTableV2DataType::Bool });
			AMC::CDataTableV2SegmentBuilder sizer(columns, 0, 0, SIGNAL_CHUNKROWS);
			uint64_t nSegmentSize = sizeof(AMC::sDataTableV2SegmentHeader) + 4 * sizer.getChunkSizeInBytes(SIGNAL_CHUNKROWS) + sizer.getFooterSizeInBytes(4);

			auto pOptions = createOptions(fixture, SIGNAL_CHUNKROWS, nSegmentSize, DATACAPTURE_DEFAULTMAXQUEUEDCHUNKS, LibMCEnv::eDataCaptureBackpressureMode::Block, nStorageLimitInBytes);
			pOptions->SetColumnEncoding("t", LibMCEnv::eDataCaptureEncoding::Plain);
			pOptions->SetColumnEncoding("n", LibMCEnv::eDataCaptureEncoding::Plain);
			pOptions->SetColumnEncoding("b", LibMCEnv::eDataCaptureEncoding::Plain);
			return pOptions;
		}

		UReader openReader(SFixture& fixture, const std::string& sIdentifier)
		{
			return UReader(fixture.m_pBuildExecution->OpenDataCaptureReader(sIdentifier));
		}

		uint64_t pageReadCount(LibMCEnv::Impl::IDataTableReader* pReader)
		{
			auto pReaderImpl = dynamic_cast<LibMCEnv::Impl::CDataTableReader*>(pReader);
			assertAssigned(pReaderImpl, "reader implementation");
			return pReaderImpl->getPageReadCount();
		}

		std::vector<double> readDoubles(LibMCEnv::Impl::IDataTableReader* pReader, const std::string& sIdentifier, uint64_t nFromRow, uint64_t nRowCount)
		{
			uint64_t nNeededCount = 0;
			pReader->ReadDoubleColumnRange(sIdentifier, nFromRow, nRowCount, 0, &nNeededCount, nullptr);
			std::vector<double> values((size_t)nNeededCount);
			if (nNeededCount > 0)
				pReader->ReadDoubleColumnRange(sIdentifier, nFromRow, nRowCount, values.size(), nullptr, values.data());
			return values;
		}

		std::vector<double> readDownsampled(LibMCEnv::Impl::IDataTableReader* pReader, const std::string& sIdentifier, uint64_t nFromRow, uint64_t nRowCount, uint32_t nTargetPoints, LibMCEnv::eDataTableDownsampleMode eMode)
		{
			uint64_t nNeededCount = 0;
			pReader->ReadColumnDownsampled(sIdentifier, nFromRow, nRowCount, nTargetPoints, eMode, 0, &nNeededCount, nullptr);
			std::vector<double> values((size_t)nNeededCount);
			if (nNeededCount > 0)
				pReader->ReadColumnDownsampled(sIdentifier, nFromRow, nRowCount, nTargetPoints, eMode, values.size(), nullptr, values.data());
			return values;
		}

		std::vector<uint64_t> findChunks(LibMCEnv::Impl::IDataTableReader* pReader, const std::string& sIdentifier, double dMinValue, double dMaxValue)
		{
			uint64_t nNeededCount = 0;
			pReader->FindChunksInValueRange(sIdentifier, dMinValue, dMaxValue, 0, &nNeededCount, nullptr);
			std::vector<uint64_t> chunkIndices((size_t)nNeededCount);
			if (nNeededCount > 0)
				pReader->FindChunksInValueRange(sIdentifier, dMinValue, dMaxValue, chunkIndices.size(), nullptr, chunkIndices.data());
			return chunkIndices;
		}

		void assertSignalIsExact(SFixture& fixture, const std::string& sIdentifier, const std::string& sContext)
		{
			auto pReader = openReader(fixture, sIdentifier);
			assertTrue(pReader->GetRowCount() == SIGNAL_ROWCOUNT, sContext + ": row count");

			std::vector<int64_t> timestamps(SIGNAL_ROWCOUNT);
			pReader->ReadInt64ColumnRange("t", 0, SIGNAL_ROWCOUNT, timestamps.size(), nullptr, timestamps.data());
			std::vector<float> values(SIGNAL_ROWCOUNT);
			pReader->ReadFloatColumnRange("v", 0, SIGNAL_ROWCOUNT, values.size(), nullptr, values.data());
			std::vector<int64_t> counters(SIGNAL_ROWCOUNT);
			pReader->ReadInt64ColumnRange("n", 0, SIGNAL_ROWCOUNT, counters.size(), nullptr, counters.data());
			std::vector<uint8_t> flags(SIGNAL_ROWCOUNT);
			pReader->ReadBoolColumnRange("b", 0, SIGNAL_ROWCOUNT, flags.size(), nullptr, flags.data());

			bool bExact = true;
			for (uint64_t nRow = 0; nRow < SIGNAL_ROWCOUNT; nRow++) {
				bExact = bExact && (timestamps[(size_t)nRow] == (int64_t)signalTimestamp(nRow));
				bExact = bExact && (values[(size_t)nRow] == signalValue(nRow));
				bExact = bExact && (counters[(size_t)nRow] == (int64_t)nRow);
				bExact = bExact && (flags[(size_t)nRow] == signalBool(nRow));
			}
			assertTrue(bExact, sContext + ": all columns are exact");
		}

		void testDefaultEncodingsCompress()
		{
			auto fixture = createFixture("defaults");

			// Default options: LZ4, delta for timestamps and counters, bit packing for bools, plain floats.
			UOptions pOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());
			pOptions->SetChunkRowCount(SIGNAL_CHUNKROWS);
			captureSignal(fixture, "signal", pOptions.get());

			assertSignalIsExact(fixture, "signal", "default encodings");

			// 8 + 4 + 8 + 1 bytes per row in plain form
			uint64_t nPlainSize = (uint64_t)SIGNAL_ROWCOUNT * 21;
			uint64_t nStoredSize = fixture.m_pExecution->RetrieveDataCapture("signal")->GetStoredSizeInBytes();
			assertTrue(nStoredSize * 2 < nPlainSize, "encoded capture is less than half of the plain data, got " + std::to_string(nStoredSize) + " of " + std::to_string(nPlainSize) + " bytes");

			// The segment is still an ordinary data table stream.
			UTable pTable(fixture.m_pBuildExecution->LoadDataCaptureSegment("signal", 0));
			assertTrue(pTable->GetRowCount64() == SIGNAL_ROWCOUNT, "encoded segment loads as data table");
			std::vector<uint64_t> timestamps(SIGNAL_ROWCOUNT);
			pTable->GetTimestampColumnValues("t", timestamps.size(), nullptr, timestamps.data());
			assertTrue((timestamps[0] == signalTimestamp(0)) && (timestamps[SIGNAL_ROWCOUNT - 1] == signalTimestamp(SIGNAL_ROWCOUNT - 1)), "delta encoded timestamps load exactly");
			std::vector<uint8_t> flags(SIGNAL_ROWCOUNT);
			pTable->GetBoolColumnValues("b", flags.size(), nullptr, flags.data());
			assertTrue((flags[99] == 0) && (flags[100] == 1) && (flags[200] == 0), "bit packed bools load exactly");
		}

		void testCompressionModes()
		{
			auto fixture = createFixture("codecs");

			uint64_t nUncompressedSize = 0;
			for (auto eCompression : { LibMCEnv::eDataCaptureCompression::None, LibMCEnv::eDataCaptureCompression::LZ4, LibMCEnv::eDataCaptureCompression::Deflate }) {
				std::string sIdentifier = "codec" + std::to_string((uint32_t)eCompression);

				UOptions pOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());
				pOptions->SetChunkRowCount(SIGNAL_CHUNKROWS);
				pOptions->SetCompression(eCompression);
				assertTrue(pOptions->GetCompression() == eCompression, "compression option is stored");

				captureSignal(fixture, sIdentifier, pOptions.get());
				assertSignalIsExact(fixture, sIdentifier, sIdentifier);

				uint64_t nStoredSize = fixture.m_pExecution->RetrieveDataCapture(sIdentifier)->GetStoredSizeInBytes();
				// A page that a codec does not shrink is stored uncompressed, so a codec never makes the capture larger.
				if (eCompression == LibMCEnv::eDataCaptureCompression::None)
					nUncompressedSize = nStoredSize;
				else
					assertTrue(nStoredSize <= nUncompressedSize, sIdentifier + " is not larger than the uncompressed capture");

				if (eCompression == LibMCEnv::eDataCaptureCompression::Deflate)
					assertTrue(nStoredSize < nUncompressedSize, sIdentifier + " is smaller than the uncompressed capture");
			}

			UOptions pOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());
			assertTrue(pOptions->GetCompression() == LibMCEnv::eDataCaptureCompression::LZ4, "default compression is LZ4");
			assertTrue(errorCodeOf([&]() { pOptions->SetCompression(LibMCEnv::eDataCaptureCompression::Unknown); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "unknown compression is rejected");
		}

		void testQuantizedDoubleColumn()
		{
			auto fixture = createFixture("quantized");

			UOptions pOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());
			pOptions->SetChunkRowCount(500);
			pOptions->SetDoubleQuantization("q", 0.01);

			auto pCapture = createCapture(fixture, "quantized", pOptions.get());
			pCapture->AddColumn("q", "Quantised", LibMCEnv::eDataTableColumnType::DoubleColumn);
			pCapture->AddColumn("x", "Exact", LibMCEnv::eDataTableColumnType::DoubleColumn);

			std::vector<double> values(1000);
			for (size_t nIndex = 0; nIndex < values.size(); nIndex++)
				values[nIndex] = 1500.0 + std::sin(nIndex * 0.02) * 321.123456789;

			for (size_t nFirstRow = 0; nFirstRow < values.size(); nFirstRow += 500) {
				UChunk pChunk(pCapture->BeginChunk());
				pChunk->SetDoubleColumnValues(0, 500, values.data() + nFirstRow);
				pChunk->SetDoubleColumnValues(1, 500, values.data() + nFirstRow);
				pCapture->CommitChunk(pChunk.get());
			}
			pCapture->Finish();

			auto pReader = openReader(fixture, "quantized");
			assertTrue(pReader->GetColumnQuantization("q") == 0.01, "quantisation step is recorded");
			assertTrue(pReader->GetColumnQuantization("x") == 0.0, "a column that did not ask for it is not quantised");

			auto quantized = readDoubles(pReader.get(), "q", 0, 1000);
			auto exact = readDoubles(pReader.get(), "x", 0, 1000);
			assertTrue(exact == values, "unquantised double column is bit-exact");

			bool bWithinBound = true;
			bool bLossy = false;
			for (size_t nIndex = 0; nIndex < values.size(); nIndex++) {
				bWithinBound = bWithinBound && (std::fabs(quantized[nIndex] - values[nIndex]) <= 0.005 + 1.0e-9);
				bLossy = bLossy || (quantized[nIndex] != values[nIndex]);
			}
			assertTrue(bWithinBound, "quantised values are within half a step");
			assertTrue(bLossy, "quantisation is lossy for this signal");

			// The quantised column is much smaller than the exact one.
			UOptions pExactOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());
			auto pExactOnly = createCapture(fixture, "exactonly", pExactOptions.get());
			pExactOnly->AddColumn("x", "Exact", LibMCEnv::eDataTableColumnType::DoubleColumn);
			auto pQuantizedOptions = UOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());
			pQuantizedOptions->SetDoubleQuantization("q", 0.01);
			auto pQuantizedOnly = createCapture(fixture, "quantizedonly", pQuantizedOptions.get());
			pQuantizedOnly->AddColumn("q", "Quantised", LibMCEnv::eDataTableColumnType::DoubleColumn);
			{
				UChunk pChunk(pExactOnly->BeginChunk());
				pChunk->SetDoubleColumnValues(0, values.size(), values.data());
				pExactOnly->CommitChunk(pChunk.get());
				UChunk pQuantizedChunk(pQuantizedOnly->BeginChunk());
				pQuantizedChunk->SetDoubleColumnValues(0, values.size(), values.data());
				pQuantizedOnly->CommitChunk(pQuantizedChunk.get());
			}
			pExactOnly->Finish();
			pQuantizedOnly->Finish();
			assertTrue(pQuantizedOnly->GetStorageSizeInBytes() * 2 < pExactOnly->GetStorageSizeInBytes(), "quantised column takes less than half of the exact column, got " + std::to_string(pQuantizedOnly->GetStorageSizeInBytes()) + " of " + std::to_string(pExactOnly->GetStorageSizeInBytes()) + " bytes");
		}

		void testEncodingOptionValidation()
		{
			auto fixture = createFixture("encodingoptions");

			UOptions pOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());
			assertTrue(errorCodeOf([&]() { pOptions->SetDoubleQuantization("q", -0.1); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "negative quantisation step is rejected");
			assertTrue(errorCodeOf([&]() { pOptions->SetDoubleQuantization("q", std::numeric_limits<double>::quiet_NaN()); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "NaN quantisation step is rejected");
			assertTrue(errorCodeOf([&]() { pOptions->SetDoubleQuantization("", 0.1); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "empty column identifier is rejected");
			assertTrue(errorCodeOf([&]() { pOptions->SetColumnEncoding("n", LibMCEnv::eDataCaptureEncoding::Unknown); }) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "unknown encoding is rejected");

			// The schema is only known when recording starts, so that is where the options are checked.
			auto tryStart = [&](const std::string& sIdentifier, LibMCEnv::Impl::IDataCaptureOptions* pCaptureOptions) {
				auto pCapture = createCapture(fixture, sIdentifier, pCaptureOptions);
				pCapture->AddColumn("n", "Counter", LibMCEnv::eDataTableColumnType::Uint64Column);
				pCapture->AddColumn("f", "Float", LibMCEnv::eDataTableColumnType::FloatColumn);
				pCapture->AddColumn("d", "Double", LibMCEnv::eDataTableColumnType::DoubleColumn);
				int32_t nErrorCode = errorCodeOf([&]() { UChunk pChunk(pCapture->BeginChunk()); });
				pCapture->Abort();
				return nErrorCode;
			};

			{
				UOptions pUnknownColumn(fixture.m_pBuildExecution->CreateDataCaptureOptions());
				pUnknownColumn->SetColumnEncoding("missing", LibMCEnv::eDataCaptureEncoding::Plain);
				assertTrue(tryStart("unknowncolumn", pUnknownColumn.get()) == LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND, "encoding for an unknown column is rejected");
			}
			{
				UOptions pUnknownQuantization(fixture.m_pBuildExecution->CreateDataCaptureOptions());
				pUnknownQuantization->SetDoubleQuantization("missing", 0.1);
				assertTrue(tryStart("unknownquantization", pUnknownQuantization.get()) == LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND, "quantisation for an unknown column is rejected");
			}
			{
				UOptions pFloatQuantization(fixture.m_pBuildExecution->CreateDataCaptureOptions());
				pFloatQuantization->SetDoubleQuantization("f", 0.1);
				assertTrue(tryStart("floatquantization", pFloatQuantization.get()) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "quantisation of a float column is rejected");
			}
			{
				UOptions pBitPackInteger(fixture.m_pBuildExecution->CreateDataCaptureOptions());
				pBitPackInteger->SetColumnEncoding("n", LibMCEnv::eDataCaptureEncoding::BitPack);
				assertTrue(tryStart("bitpackinteger", pBitPackInteger.get()) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "bit packing of an integer column is rejected");
			}
			{
				UOptions pDeltaFloat(fixture.m_pBuildExecution->CreateDataCaptureOptions());
				pDeltaFloat->SetColumnEncoding("f", LibMCEnv::eDataCaptureEncoding::Delta);
				assertTrue(tryStart("deltafloat", pDeltaFloat.get()) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "delta encoding of a float column is rejected");
			}
			{
				UOptions pPlainQuantized(fixture.m_pBuildExecution->CreateDataCaptureOptions());
				pPlainQuantized->SetDoubleQuantization("d", 0.1);
				pPlainQuantized->SetColumnEncoding("d", LibMCEnv::eDataCaptureEncoding::Plain);
				assertTrue(tryStart("plainquantized", pPlainQuantized.get()) == LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "plain encoding of a quantised column is rejected");
			}
			{
				// Automatic removes an explicit encoding again, and a step of 0 removes the quantisation.
				UOptions pReset(fixture.m_pBuildExecution->CreateDataCaptureOptions());
				pReset->SetColumnEncoding("missing", LibMCEnv::eDataCaptureEncoding::Plain);
				pReset->SetColumnEncoding("missing", LibMCEnv::eDataCaptureEncoding::Automatic);
				pReset->SetDoubleQuantization("f", 0.1);
				pReset->SetDoubleQuantization("f", 0.0);
				assertTrue(tryStart("reset", pReset.get()) == 0, "options that have been reset are accepted");
			}
		}

		void testSchemaAndChunks()
		{
			auto fixture = createFixture("schema");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());

			auto pReader = openReader(fixture, "signal");
			assertTrue(pReader->GetRowCount() == SIGNAL_ROWCOUNT, "row count");
			assertTrue(pReader->GetFirstAvailableRow() == 0, "all rows are available");
			assertTrue(pReader->GetColumnCount() == 4, "column count");
			assertTrue(pReader->HasColumn("v"), "column v exists");
			assertFalse(pReader->HasColumn("missing"), "unknown column does not exist");

			std::string sIdentifier;
			std::string sDescription;
			LibMCEnv::eDataTableColumnType eColumnType = LibMCEnv::eDataTableColumnType::Unknown;
			pReader->GetColumnInformation(1, sIdentifier, sDescription, eColumnType);
			assertTrue((sIdentifier == "v") && (sDescription == "Value") && (eColumnType == LibMCEnv::eDataTableColumnType::FloatColumn), "column information");
			assertTrue(errorCodeOf([&]() { pReader->GetColumnInformation(9, sIdentifier, sDescription, eColumnType); }) == LIBMCENV_ERROR_INVALIDCOLUMNINDEX, "column index out of bounds is rejected");

			// 25 chunks in segments of four
			assertTrue(pReader->GetChunkCount() == 25, "chunk count");
			assertTrue(pReader->GetSegmentCount() == 7, "segment count");

			uint64_t nFirstRow = 0;
			uint64_t nRowCount = 0;
			uint64_t nSizeInBytes = 0;
			bool bIsAvailable = false;
			pReader->GetSegmentInformation(2, nFirstRow, nRowCount, nSizeInBytes, bIsAvailable);
			assertTrue((nFirstRow == 800) && (nRowCount == 400) && bIsAvailable && (nSizeInBytes > 0), "segment information");
			pReader->GetSegmentInformation(6, nFirstRow, nRowCount, nSizeInBytes, bIsAvailable);
			assertTrue((nFirstRow == 2400) && (nRowCount == 100), "the last segment holds the remaining chunk");
			assertTrue(errorCodeOf([&]() { pReader->GetSegmentInformation(7, nFirstRow, nRowCount, nSizeInBytes, bIsAvailable); }) == LIBMCENV_ERROR_INVALIDDATACAPTURESEGMENTINDEX, "segment index out of bounds is rejected");

			uint32_t nSegmentIndex = 0;
			pReader->GetChunkInformation(7, nSegmentIndex, nFirstRow, nRowCount, bIsAvailable);
			assertTrue((nSegmentIndex == 1) && (nFirstRow == 700) && (nRowCount == 100) && bIsAvailable, "chunk information");
			pReader->GetChunkInformation(24, nSegmentIndex, nFirstRow, nRowCount, bIsAvailable);
			assertTrue((nSegmentIndex == 6) && (nFirstRow == 2400) && (nRowCount == 100), "last chunk information");
			assertTrue(errorCodeOf([&]() { pReader->GetChunkInformation(25, nSegmentIndex, nFirstRow, nRowCount, bIsAvailable); }) == LIBMCENV_ERROR_INVALIDDATATABLECHUNKINDEX, "chunk index out of bounds is rejected");

			assertTrue(dataErrorCodeOf([&]() { UReader pMissing(fixture.m_pBuildExecution->OpenDataCaptureReader("missing")); }) == LIBMCDATA_ERROR_DATACAPTURENOTFOUND, "reader on an unknown capture is rejected");

			// Opening a reader reads no page.
			assertTrue(pageReadCount(pReader.get()) == 0, "schema and chunk queries read no page");
		}

		void testRangeReads()
		{
			auto fixture = createFixture("ranges");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());
			auto pReader = openReader(fixture, "signal");

			// Across chunk boundaries and the segment boundary at row 400
			auto counters = readDoubles(pReader.get(), "n", 95, 310);
			assertTrue(counters.size() == 310, "range size");
			bool bInOrder = true;
			for (size_t nIndex = 0; nIndex < counters.size(); nIndex++)
				bInOrder = bInOrder && (counters[nIndex] == (double)(95 + nIndex));
			assertTrue(bInOrder, "range across chunk and segment boundaries is in order");

			// The whole capture across all seven segments
			assertSignalIsExact(fixture, "signal", "plain capture");

			// The spike
			std::vector<float> values(10);
			pReader->ReadFloatColumnRange("v", 1230, 10, values.size(), nullptr, values.data());
			assertTrue((values[3] == 233.0f) && (values[4] == 5000.0f) && (values[5] == 235.0f), "float range holds the spike");

			// Every column type reads as double
			auto timestamps = readDoubles(pReader.get(), "t", 2499, 1);
			assertTrue(timestamps[0] == (double)signalTimestamp(2499), "timestamp as double");
			auto flags = readDoubles(pReader.get(), "b", 99, 2);
			assertTrue((flags[0] == 0.0) && (flags[1] == 1.0), "bool as double");
			auto valuesAsDouble = readDoubles(pReader.get(), "v", 1234, 1);
			assertTrue(valuesAsDouble[0] == 5000.0, "float as double");

			std::vector<uint8_t> flagBytes(20);
			pReader->ReadBoolColumnRange("b", 190, 20, flagBytes.size(), nullptr, flagBytes.data());
			assertTrue((flagBytes[9] == 1) && (flagBytes[10] == 0), "bool range across a chunk boundary");

			std::vector<int64_t> flagsAsInt(2);
			pReader->ReadInt64ColumnRange("b", 99, 2, flagsAsInt.size(), nullptr, flagsAsInt.data());
			assertTrue((flagsAsInt[0] == 0) && (flagsAsInt[1] == 1), "bool as integer");

			// An empty range at the end is valid.
			assertTrue(readDoubles(pReader.get(), "n", SIGNAL_ROWCOUNT, 0).empty(), "empty range at the end");

			assertTrue(errorCodeOf([&]() { readDoubles(pReader.get(), "n", 2400, 101); }) == LIBMCENV_ERROR_INVALIDDATATABLEROWRANGE, "range beyond the end is rejected");
			assertTrue(errorCodeOf([&]() { readDoubles(pReader.get(), "n", SIGNAL_ROWCOUNT + 1, 0); }) == LIBMCENV_ERROR_INVALIDDATATABLEROWRANGE, "start beyond the end is rejected");
			assertTrue(errorCodeOf([&]() { readDoubles(pReader.get(), "n", 10, UINT64_MAX); }) == LIBMCENV_ERROR_INVALIDDATATABLEROWRANGE, "overflowing range is rejected");
			assertTrue(errorCodeOf([&]() { readDoubles(pReader.get(), "missing", 0, 1); }) == LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND, "unknown column is rejected");

			std::vector<float> floatBuffer(10);
			assertTrue(errorCodeOf([&]() { pReader->ReadFloatColumnRange("n", 0, 10, floatBuffer.size(), nullptr, floatBuffer.data()); }) == LIBMCENV_ERROR_DATATABLECOLUMNTYPEMISMATCH, "integer column is not read as float");
			std::vector<int64_t> intBuffer(10);
			assertTrue(errorCodeOf([&]() { pReader->ReadInt64ColumnRange("v", 0, 10, intBuffer.size(), nullptr, intBuffer.data()); }) == LIBMCENV_ERROR_DATATABLECOLUMNTYPEMISMATCH, "float column is not read as integer");
			std::vector<uint8_t> boolBuffer(10);
			assertTrue(errorCodeOf([&]() { pReader->ReadBoolColumnRange("n", 0, 10, boolBuffer.size(), nullptr, boolBuffer.data()); }) == LIBMCENV_ERROR_DATATABLECOLUMNTYPEMISMATCH, "integer column is not read as bool");

			std::vector<double> tooSmall(5);
			assertTrue(errorCodeOf([&]() { pReader->ReadDoubleColumnRange("n", 0, 10, tooSmall.size(), nullptr, tooSmall.data()); }) == LIBMCENV_ERROR_BUFFERTOOSMALL, "small buffer is rejected");
		}

		void testReadSelectivity()
		{
			auto fixture = createFixture("selectivity");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());
			auto pReader = openReader(fixture, "signal");

			// One chunk of one column is one page, no matter how many columns and chunks the capture has.
			readDoubles(pReader.get(), "v", 200, 100);
			assertTrue(pageReadCount(pReader.get()) == 1, "a range inside one chunk reads one page, got " + std::to_string(pageReadCount(pReader.get())));

			// Rows 250 to 449 overlap the chunks 2, 3 and 4.
			uint64_t nBefore = pageReadCount(pReader.get());
			readDoubles(pReader.get(), "n", 250, 200);
			assertTrue(pageReadCount(pReader.get()) - nBefore == 3, "a range over three chunks reads three pages");

			// A single row
			nBefore = pageReadCount(pReader.get());
			readDoubles(pReader.get(), "t", 1999, 1);
			assertTrue(pageReadCount(pReader.get()) - nBefore == 1, "a single row reads one page");

			// The same page again is served from the page cache.
			nBefore = pageReadCount(pReader.get());
			readDoubles(pReader.get(), "t", 1950, 10);
			assertTrue(pageReadCount(pReader.get()) == nBefore, "the last page is not read twice");

			// A whole column reads one page per chunk: a quarter of the pages of the four column capture.
			auto pFreshReader = openReader(fixture, "signal");
			readDoubles(pFreshReader.get(), "v", 0, SIGNAL_ROWCOUNT);
			assertTrue(pageReadCount(pFreshReader.get()) == 25, "a whole column reads one page per chunk");

			// The size query of the two-call pattern reads nothing.
			auto pQueryReader = openReader(fixture, "signal");
			uint64_t nNeededCount = 0;
			pQueryReader->ReadDoubleColumnRange("v", 0, SIGNAL_ROWCOUNT, 0, &nNeededCount, nullptr);
			assertTrue((nNeededCount == SIGNAL_ROWCOUNT) && (pageReadCount(pQueryReader.get()) == 0), "the size query reads no page");
		}

		void testChunkStatisticsAndValueSearch()
		{
			auto fixture = createFixture("statistics");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());
			auto pReader = openReader(fixture, "signal");

			double dMinValue = 0.0;
			double dMaxValue = 0.0;

			assertTrue(pReader->GetChunkStatistics(0, "v", dMinValue, dMaxValue), "chunk 0 has statistics");
			assertTrue((dMinValue == 0.0) && (dMaxValue == 99.0), "chunk 0 value range");

			// Chunk 12 holds the rows 1200 to 1299: the sawtooth wraps and the spike is inside.
			assertTrue(pReader->GetChunkStatistics(12, "v", dMinValue, dMaxValue), "chunk 12 has statistics");
			assertTrue((dMinValue == 0.0) && (dMaxValue == 5000.0), "chunk 12 holds the spike");

			assertTrue(pReader->GetChunkStatistics(3, "t", dMinValue, dMaxValue), "timestamp statistics");
			assertTrue((dMinValue == (double)signalTimestamp(300)) && (dMaxValue == (double)signalTimestamp(399)), "timestamp range of chunk 3");

			assertTrue(pReader->GetChunkStatistics(1, "b", dMinValue, dMaxValue), "bool statistics");
			assertTrue((dMinValue == 1.0) && (dMaxValue == 1.0), "constant bool chunk");

			assertTrue(errorCodeOf([&]() { pReader->GetChunkStatistics(25, "v", dMinValue, dMaxValue); }) == LIBMCENV_ERROR_INVALIDDATATABLECHUNKINDEX, "chunk index out of bounds is rejected");
			assertTrue(errorCodeOf([&]() { pReader->GetChunkStatistics(0, "missing", dMinValue, dMaxValue); }) == LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND, "unknown column is rejected");

			// Where did the value exceed 1000? Only the chunk with the spike can hold a match.
			auto spikeChunks = findChunks(pReader.get(), "v", 1000.0, 10000.0);
			assertTrue((spikeChunks.size() == 1) && (spikeChunks[0] == 12), "only the spike chunk matches the value range");

			// Values 240 to 249 occur at the end of every sawtooth period.
			auto highChunks = findChunks(pReader.get(), "v", 240.0, 249.0);
			std::vector<uint64_t> expectedChunks = { 2, 4, 7, 9, 12, 14, 17, 19, 22, 24 };
			assertTrue(highChunks == expectedChunks, "chunks that reach the end of the sawtooth");

			assertTrue(findChunks(pReader.get(), "v", 6000.0, 7000.0).empty(), "no chunk above the spike");
			assertTrue(findChunks(pReader.get(), "v", 10.0, 5.0).empty(), "an inverted range matches nothing");
			assertTrue(findChunks(pReader.get(), "n", 700.0, 700.0).size() == 1, "a single counter value is in one chunk");

			// Statistics and search use the page directory only.
			assertTrue(pageReadCount(pReader.get()) == 0, "statistics and value search read no page");
		}

		void testDownsampling()
		{
			auto fixture = createFixture("downsample");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());
			auto pReader = openReader(fixture, "signal");

			// Ten buckets of 250 rows, which is one sawtooth period each.
			auto envelope = readDownsampled(pReader.get(), "v", 0, SIGNAL_ROWCOUNT, 10, LibMCEnv::eDataTableDownsampleMode::MinMax);
			assertTrue(envelope.size() == 20, "MinMax returns two values per bucket");
			for (size_t nBucket = 0; nBucket < 10; nBucket++) {
				assertTrue(envelope[2 * nBucket] == 0.0, "minimum of bucket " + std::to_string(nBucket));
				assertTrue(envelope[2 * nBucket + 1] == ((nBucket == 4) ? 5000.0 : 249.0), "maximum of bucket " + std::to_string(nBucket));
			}

			// Chunks that lie inside one bucket are answered from the page statistics. Only the five chunks
			// that straddle a bucket boundary are read: 250, 750, 1250, 1750 and 2250 fall inside a chunk.
			assertTrue(pageReadCount(pReader.get()) == 5, "MinMax reads only the chunks on bucket boundaries, got " + std::to_string(pageReadCount(pReader.get())));

			// Stride returns the first value of each bucket and misses the spike that MinMax preserves.
			auto pStrideReader = openReader(fixture, "signal");
			auto stride = readDownsampled(pStrideReader.get(), "v", 0, SIGNAL_ROWCOUNT, 10, LibMCEnv::eDataTableDownsampleMode::Stride);
			assertTrue(stride.size() == 10, "Stride returns one value per bucket");
			bool bSpikeFound = false;
			for (double dValue : stride) {
				assertTrue(dValue == 0.0, "every bucket starts at the beginning of a sawtooth period");
				bSpikeFound = bSpikeFound || (dValue == 5000.0);
			}
			assertFalse(bSpikeFound, "Stride hides the spike");
			assertTrue(pageReadCount(pStrideReader.get()) == 10, "Stride reads one page per bucket");

			auto average = readDownsampled(pReader.get(), "v", 0, SIGNAL_ROWCOUNT, 10, LibMCEnv::eDataTableDownsampleMode::Average);
			assertTrue(average.size() == 10, "Average returns one value per bucket");
			assertTrue(std::fabs(average[0] - 124.5) < 1.0e-9, "average of a sawtooth period");
			assertTrue(std::fabs(average[4] - (124.5 + (5000.0 - 234.0) / 250.0)) < 1.0e-9, "average of the period with the spike");

			// Fewer rows than target points: one bucket per row.
			auto fewValues = readDownsampled(pReader.get(), "n", 1000, 5, 100, LibMCEnv::eDataTableDownsampleMode::MinMax);
			assertTrue(fewValues.size() == 10, "five rows make five buckets");
			assertTrue((fewValues[0] == 1000.0) && (fewValues[1] == 1000.0) && (fewValues[8] == 1004.0) && (fewValues[9] == 1004.0), "a bucket of one row has equal minimum and maximum");

			// Buckets of uneven size cover the range without gaps: 1000 rows in three buckets start at 0, 333 and 666.
			auto uneven = readDownsampled(pReader.get(), "n", 0, 1000, 3, LibMCEnv::eDataTableDownsampleMode::Stride);
			assertTrue((uneven.size() == 3) && (uneven[0] == 0.0) && (uneven[1] == 333.0) && (uneven[2] == 666.0), "uneven bucket starts");
			auto unevenEnvelope = readDownsampled(pReader.get(), "n", 0, 1000, 3, LibMCEnv::eDataTableDownsampleMode::MinMax);
			assertTrue((unevenEnvelope[0] == 0.0) && (unevenEnvelope[1] == 332.0) && (unevenEnvelope[2] == 333.0) && (unevenEnvelope[3] == 665.0) && (unevenEnvelope[4] == 666.0) && (unevenEnvelope[5] == 999.0), "uneven buckets are contiguous");

			// A range that does not start at a chunk boundary
			auto offset = readDownsampled(pReader.get(), "n", 150, 600, 2, LibMCEnv::eDataTableDownsampleMode::MinMax);
			assertTrue((offset[0] == 150.0) && (offset[1] == 449.0) && (offset[2] == 450.0) && (offset[3] == 749.0), "envelope of a range inside the capture");

			assertTrue(readDownsampled(pReader.get(), "n", 100, 0, 10, LibMCEnv::eDataTableDownsampleMode::MinMax).empty(), "an empty range has no buckets");

			assertTrue(errorCodeOf([&]() { readDownsampled(pReader.get(), "v", 0, 100, 0, LibMCEnv::eDataTableDownsampleMode::MinMax); }) == LIBMCENV_ERROR_INVALIDDOWNSAMPLEPARAMETERS, "zero target points are rejected");
			assertTrue(errorCodeOf([&]() { readDownsampled(pReader.get(), "v", 0, 100, 10, LibMCEnv::eDataTableDownsampleMode::Unknown); }) == LIBMCENV_ERROR_INVALIDDOWNSAMPLEPARAMETERS, "unknown mode is rejected");
			assertTrue(errorCodeOf([&]() { readDownsampled(pReader.get(), "v", 2000, 1000, 10, LibMCEnv::eDataTableDownsampleMode::MinMax); }) == LIBMCENV_ERROR_INVALIDDATATABLEROWRANGE, "range beyond the end is rejected");
		}

		void testExtractDataTable()
		{
			auto fixture = createFixture("extract");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());
			auto pReader = openReader(fixture, "signal");

			UTable pTable(pReader->ExtractDataTable(1230, 10, "t, v"));
			assertTrue(pTable->GetColumnCount() == 2, "two columns extracted");
			assertTrue(pTable->GetRowCount64() == 10, "ten rows extracted");
			assertTrue(pTable->GetColumnIdentifier(0) == "t", "column order follows the request");
			assertTrue(pTable->GetColumnType(1) == LibMCEnv::eDataTableColumnType::FloatColumn, "column type is kept");
			assertTrue(pTable->GetColumnDescription(1) == "Value", "column description is kept");

			std::vector<uint64_t> timestamps(10);
			pTable->GetTimestampColumnValues("t", timestamps.size(), nullptr, timestamps.data());
			assertTrue((timestamps[0] == signalTimestamp(1230)) && (timestamps[9] == signalTimestamp(1239)), "extracted timestamps");
			std::vector<float> values(10);
			pTable->GetFloatColumnValues("v", values.size(), nullptr, values.data());
			assertTrue(values[4] == 5000.0f, "extracted values hold the spike");

			// An empty column list extracts all columns, across a segment boundary.
			UTable pAllColumns(pReader->ExtractDataTable(390, 20, ""));
			assertTrue(pAllColumns->GetColumnCount() == 4, "all columns extracted");
			std::vector<uint64_t> counters(20);
			pAllColumns->GetUint64ColumnValues("n", counters.size(), nullptr, counters.data());
			assertTrue((counters[0] == 390) && (counters[19] == 409), "extracted counters across the segment boundary");
			std::vector<uint8_t> flags(20);
			pAllColumns->GetBoolColumnValues("b", flags.size(), nullptr, flags.data());
			assertTrue((flags[9] == 1) && (flags[10] == 0), "extracted bools");

			UTable pEmpty(pReader->ExtractDataTable(100, 0, "n"));
			assertTrue((pEmpty->GetColumnCount() == 1) && (pEmpty->GetRowCount64() == 0), "an empty range extracts an empty table");

			assertTrue(errorCodeOf([&]() { UTable pInvalid(pReader->ExtractDataTable(0, 10, "t,missing")); }) == LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND, "unknown column is rejected");
			assertTrue(errorCodeOf([&]() { UTable pInvalid(pReader->ExtractDataTable(2495, 10, "t")); }) == LIBMCENV_ERROR_INVALIDDATATABLEROWRANGE, "range beyond the end is rejected");
		}

		void testPurgedSegments()
		{
			auto fixture = createFixture("purged");

			// The limit holds three of the seven segments: six full ones of four chunks and one of a single chunk.
			auto pProbeOptions = signalOptions(fixture);
			uint64_t nSegmentSize = pProbeOptions->GetSegmentSizeInBytes();
			auto pOptions = signalOptions(fixture, 3 * nSegmentSize);
			captureSignal(fixture, "signal", pOptions.get());

			auto pReader = openReader(fixture, "signal");
			assertTrue(pReader->GetSegmentCount() == 7, "all segments are in the manifest");
			assertTrue(pReader->GetRowCount() == SIGNAL_ROWCOUNT, "the row count includes the deleted rows");
			assertTrue(pReader->GetChunkCount() == 25, "the chunk count includes the deleted chunks");

			uint64_t nFirstAvailableRow = pReader->GetFirstAvailableRow();
			assertTrue((nFirstAvailableRow > 0) && (nFirstAvailableRow < SIGNAL_ROWCOUNT) && ((nFirstAvailableRow % 400) == 0), "the oldest segments have been deleted, first available row is " + std::to_string(nFirstAvailableRow));

			uint64_t nFirstRow = 0;
			uint64_t nRowCount = 0;
			uint64_t nSizeInBytes = 0;
			bool bIsAvailable = true;
			pReader->GetSegmentInformation(0, nFirstRow, nRowCount, nSizeInBytes, bIsAvailable);
			assertFalse(bIsAvailable, "the first segment is deleted");
			assertTrue((nFirstRow == 0) && (nRowCount == 400), "a deleted segment keeps its row range");
			pReader->GetSegmentInformation(6, nFirstRow, nRowCount, nSizeInBytes, bIsAvailable);
			assertTrue(bIsAvailable, "the newest segment is stored");

			uint32_t nSegmentIndex = 0;
			pReader->GetChunkInformation(1, nSegmentIndex, nFirstRow, nRowCount, bIsAvailable);
			assertTrue((nSegmentIndex == 0) && !bIsAvailable && (nRowCount == 0), "a chunk of a deleted segment is not available");
			pReader->GetChunkInformation(24, nSegmentIndex, nFirstRow, nRowCount, bIsAvailable);
			assertTrue((nSegmentIndex == 6) && bIsAvailable && (nFirstRow == 2400), "chunk indices are stable when old segments are deleted");

			double dMinValue = 0.0;
			double dMaxValue = 0.0;
			assertFalse(pReader->GetChunkStatistics(1, "v", dMinValue, dMaxValue), "a deleted chunk has no statistics");

			// Available rows read normally, deleted rows are reported.
			auto counters = readDoubles(pReader.get(), "n", nFirstAvailableRow, SIGNAL_ROWCOUNT - nFirstAvailableRow);
			assertTrue((counters.front() == (double)nFirstAvailableRow) && (counters.back() == (double)(SIGNAL_ROWCOUNT - 1)), "available rows are read");

			assertTrue(errorCodeOf([&]() { readDoubles(pReader.get(), "n", 0, 10); }) == LIBMCENV_ERROR_DATACAPTURESEGMENTPURGED, "deleted rows are reported");
			assertTrue(errorCodeOf([&]() { readDoubles(pReader.get(), "n", nFirstAvailableRow - 1, 10); }) == LIBMCENV_ERROR_DATACAPTURESEGMENTPURGED, "a range that starts in a deleted segment is reported");
			assertTrue(errorCodeOf([&]() { readDownsampled(pReader.get(), "v", 0, SIGNAL_ROWCOUNT, 10, LibMCEnv::eDataTableDownsampleMode::MinMax); }) == LIBMCENV_ERROR_DATACAPTURESEGMENTPURGED, "downsampling over deleted rows is reported");
			assertTrue(errorCodeOf([&]() { UTable pTable(pReader->ExtractDataTable(0, 10, "n")); }) == LIBMCENV_ERROR_DATACAPTURESEGMENTPURGED, "extracting deleted rows is reported");

			// The value search only returns chunks that can be read.
			auto chunks = findChunks(pReader.get(), "v", 0.0, 10000.0);
			for (uint64_t nChunkIndex : chunks) {
				pReader->GetChunkInformation(nChunkIndex, nSegmentIndex, nFirstRow, nRowCount, bIsAvailable);
				assertTrue(bIsAvailable && (nFirstRow >= nFirstAvailableRow), "value search returns only stored chunks");
			}
			assertTrue(chunks.size() == (SIGNAL_ROWCOUNT - nFirstAvailableRow) / 100, "value search finds every stored chunk");

			// A capture that was aborted has nothing to read.
			auto pAbortedOptions = createCounterOptions(fixture, 100, 0);
			auto pAborted = createCapture(fixture, "aborted", pAbortedOptions.get());
			addCounterColumn(pAborted.get());
			uint64_t nCounter = 0;
			commitCounterChunk(pAborted.get(), nCounter, 100);
			pAborted->Abort();
			auto pAbortedReader = openReader(fixture, "aborted");
			assertTrue(pAbortedReader->GetFirstAvailableRow() == pAbortedReader->GetRowCount(), "an aborted capture has no available row");
		}

		void testThroughputReport()
		{
			auto fixture = createFixture("throughput");

			// 20 channels of float32 plus a timestamp and a flag, the first scenario of the capacity budget.
			const uint32_t nChunkRows = 65536;
			const uint32_t nChunkCount = 4;
			const uint32_t nChannelCount = 20;

			std::vector<uint64_t> timestamps(nChunkRows);
			std::vector<uint8_t> flags(nChunkRows);
			std::vector<std::vector<float>> channels(nChannelCount, std::vector<float>(nChunkRows));

			// A 16 bit front-end: smooth signal plus a little noise, quantised like an ADC would.
			uint64_t nState = 2024;
			for (uint32_t nIndex = 0; nIndex < nChunkRows; nIndex++) {
				flags[nIndex] = (uint8_t)((nIndex / 5000) % 2);
				for (uint32_t nChannel = 0; nChannel < nChannelCount; nChannel++) {
					nState = nState * 6364136223846793005ULL + 1442695040888963407ULL;
					double dNoise = (double)((nState >> 33) % 64) - 32.0;
					double dSignal = 20000.0 + std::sin(nIndex * 0.0005 * (nChannel + 1)) * 10000.0 + dNoise;
					channels[nChannel][nIndex] = (float)std::floor(dSignal) * (10.0f / 65536.0f);
				}
			}

			uint64_t nPlainBytes = (uint64_t)nChunkRows * nChunkCount * (8 + 1 + 4 * nChannelCount);

			for (auto eCompression : { LibMCEnv::eDataCaptureCompression::None, LibMCEnv::eDataCaptureCompression::LZ4, LibMCEnv::eDataCaptureCompression::Deflate }) {
				std::string sIdentifier = "throughput" + std::to_string((uint32_t)eCompression);
				std::string sCodec = (eCompression == LibMCEnv::eDataCaptureCompression::None) ? "none" : ((eCompression == LibMCEnv::eDataCaptureCompression::LZ4) ? "lz4" : "deflate");

				UOptions pOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());
				pOptions->SetChunkRowCount(nChunkRows);
				pOptions->SetCompression(eCompression);
				pOptions->SetSegmentSizeInBytes(16ULL * 1024ULL * 1024ULL);

				auto pCapture = createCapture(fixture, sIdentifier, pOptions.get());
				pCapture->AddColumn("t", "Time", LibMCEnv::eDataTableColumnType::TimestampColumn);
				pCapture->AddColumn("flag", "Flag", LibMCEnv::eDataTableColumnType::BoolColumn);
				for (uint32_t nChannel = 0; nChannel < nChannelCount; nChannel++)
					pCapture->AddColumn("ch" + std::to_string(nChannel), "Channel " + std::to_string(nChannel), LibMCEnv::eDataTableColumnType::FloatColumn);

				auto startTime = std::chrono::steady_clock::now();

				for (uint32_t nChunkIndex = 0; nChunkIndex < nChunkCount; nChunkIndex++) {
					for (uint32_t nIndex = 0; nIndex < nChunkRows; nIndex++)
						timestamps[nIndex] = 1700000000000000ULL + ((uint64_t)nChunkIndex * nChunkRows + nIndex) * 10;

					UChunk pChunk(pCapture->BeginChunk());
					pChunk->SetTimestampColumnValues(0, timestamps.size(), timestamps.data());
					pChunk->SetBoolColumnValues(1, flags.size(), flags.data());
					for (uint32_t nChannel = 0; nChannel < nChannelCount; nChannel++)
						pChunk->SetFloatColumnValues(2 + nChannel, channels[nChannel].size(), channels[nChannel].data());
					pCapture->CommitChunk(pChunk.get());
				}

				pCapture->Flush();
				auto flushTime = std::chrono::steady_clock::now();

				// Finish includes finalizing the last segment, which hashes the file.
				pCapture->Finish();
				auto finishTime = std::chrono::steady_clock::now();

				double dWriteSeconds = std::chrono::duration<double>(flushTime - startTime).count();
				double dFinishSeconds = std::chrono::duration<double>(finishTime - flushTime).count();
				uint64_t nStoredBytes = pCapture->GetStorageSizeInBytes();

				assertTrue(pCapture->GetCapturedRowCount() == (uint64_t)nChunkRows * nChunkCount, sCodec + ": all rows are captured");
				assertTrue(pCapture->GetDroppedRowCount() == 0, sCodec + ": nothing is dropped");
				assertTrue(nStoredBytes < nPlainBytes, sCodec + ": the stored capture is smaller than the plain data");

				// The test console prints short lines only.
				double dMegabytes = (double)nPlainBytes / (1024.0 * 1024.0);
				uint32_t nRate = (uint32_t)(dMegabytes / ((dWriteSeconds > 0.0) ? dWriteSeconds : 1.0e-9));
				uint32_t nStoredPercent = (uint32_t)(100.0 * (double)nStoredBytes / (double)nPlainBytes);
				uint32_t nFinishMilliseconds = (uint32_t)(dFinishSeconds * 1000.0);
				logInfo("capture " + sCodec + ": " + std::to_string(nRate) + " MB/s, stored " + std::to_string(nStoredPercent) + " %, finish " + std::to_string(nFinishMilliseconds) + " ms");
			}
		}

	};

}

#endif // __AMCTEST_UNITTEST_DATATABLEREADER
