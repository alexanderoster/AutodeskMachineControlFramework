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

#ifndef __AMCTEST_UNITTEST_DATATABLEFORMAT
#define __AMCTEST_UNITTEST_DATATABLEFORMAT

#include "amc_unittests.hpp"
#include "amc_constants.hpp"
#include "amc_datatableformat.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace AMCUnitTest {

	class CUnitTestGroup_DataTableFormatV2 : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "DataTableFormatV2";
		}

		void registerTests() override {
			registerTest("Statistics", "Typed page statistics for all column types", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableFormatV2::testStatistics, this));
			registerTest("SegmentRoundTrip", "Segment builder output is read back by the segment reader", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableFormatV2::testSegmentRoundTrip, this));
			registerTest("EmptySegments", "Segments without chunks and without columns", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableFormatV2::testEmptySegments, this));
			registerTest("SizePrediction", "Predicted chunk and footer sizes match the produced bytes", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableFormatV2::testSizePrediction, this));
			registerTest("TruncatedSegments", "Segments cut at any position are reported as truncated or corrupt", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableFormatV2::testTruncatedSegments, this));
			registerTest("CorruptSegments", "Corrupted headers, footers and directories are rejected", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableFormatV2::testCorruptSegments, this));
			registerTest("BuilderMisuse", "Builder rejects calls in the wrong order and invalid chunks", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableFormatV2::testBuilderMisuse, this));
		}

		void initializeTests() override {
		}

	private:

		// Returns the format error a call fails with, or 0 if it does not fail.
		template <typename F>
		uint32_t formatErrorOf(F&& fn)
		{
			try {
				fn();
			}
			catch (AMC::EDataTableFormatException& formatException) {
				return (uint32_t)formatException.getError();
			}
			return 0;
		}

		// A read callback over a byte vector. Reading outside of the vector is a test failure:
		// the reader must validate every offset against the stream size first.
		AMC::CDataTableV2SegmentReader::ReadCallback memoryReader(const std::vector<uint8_t>& bytes)
		{
			return [&bytes](uint64_t nOffset, uint64_t nSize, uint8_t* pBuffer) {
				if ((nOffset > bytes.size()) || (nSize > bytes.size() - nOffset))
					throw std::out_of_range("segment reader read outside of the stream");
				if (nSize > 0)
					memcpy(pBuffer, bytes.data() + nOffset, (size_t)nSize);
			};
		}

		std::vector<AMC::sDataTableV2Column> sampleColumns()
		{
			std::vector<AMC::sDataTableV2Column> columns;
			columns.push_back({ "t", "Time", AMC::eDataTableV2DataType::Timestamp });
			columns.push_back({ "temp", "Temperature in C", AMC::eDataTableV2DataType::Float });
			columns.push_back({ "laser", "Laser active", AMC::eDataTableV2DataType::Bool });
			return columns;
		}

		// Builds a segment with two chunks of 5 and 3 rows.
		std::vector<uint8_t> buildSampleSegment(uint32_t nSegmentIndex = 2, uint64_t nFirstRow = 1000)
		{
			std::vector<uint64_t> timestamps1 = { 100, 200, 300, 400, 500 };
			std::vector<float> temperatures1 = { 20.5f, 21.0f, 1800.25f, -5.0f, 22.0f };
			std::vector<uint8_t> laser1 = { 0, 1, 1, 0, 0 };

			std::vector<uint64_t> timestamps2 = { 600, 700, 800 };
			std::vector<float> temperatures2 = { 30.0f, 31.0f, 32.0f };
			std::vector<uint8_t> laser2 = { 1, 1, 1 };

			AMC::CDataTableV2SegmentBuilder builder(sampleColumns(), nSegmentIndex, nFirstRow, 5);

			std::vector<uint8_t> bytes;
			builder.appendHeader(bytes);
			builder.appendChunk({ (const uint8_t*)timestamps1.data(), (const uint8_t*)temperatures1.data(), laser1.data() }, 5, bytes);
			builder.appendChunk({ (const uint8_t*)timestamps2.data(), (const uint8_t*)temperatures2.data(), laser2.data() }, 3, bytes);
			builder.appendFooter(bytes);

			return bytes;
		}

		double bitsToDouble(uint64_t nBits)
		{
			double dValue = 0.0;
			memcpy(&dValue, &nBits, sizeof(dValue));
			return dValue;
		}

		// Offset of the page directory inside a finished segment.
		size_t directoryOffset(const std::vector<uint8_t>& bytes)
		{
			AMC::sDataTableV2Trailer trailer;
			memcpy(&trailer, bytes.data() + bytes.size() - sizeof(trailer), sizeof(trailer));
			size_t nFooterStart = bytes.size() - sizeof(trailer) - (size_t)trailer.m_nFooterLength;

			AMC::sDataTableV2FooterHeader footerHeader;
			memcpy(&footerHeader, bytes.data() + nFooterStart, sizeof(footerHeader));

			return nFooterStart + sizeof(footerHeader) + footerHeader.m_nSchemaSize;
		}

		void testStatistics()
		{
			uint64_t nMin = 0;
			uint64_t nMax = 0;

			std::vector<double> doubles = { 1.5, std::numeric_limits<double>::quiet_NaN(), -2.25, 1800.0 };
			assertTrue(AMC::CDataTableV2::computeStatistics(AMC::eDataTableV2DataType::Double, (const uint8_t*)doubles.data(), (uint32_t)doubles.size(), nMin, nMax), "double statistics are valid");
			assertTrue(bitsToDouble(nMin) == -2.25, "double minimum ignores NaN");
			assertTrue(bitsToDouble(nMax) == 1800.0, "double maximum ignores NaN");

			std::vector<float> floats = { 0.5f, -100.25f, 3.0f };
			assertTrue(AMC::CDataTableV2::computeStatistics(AMC::eDataTableV2DataType::Float, (const uint8_t*)floats.data(), (uint32_t)floats.size(), nMin, nMax), "float statistics are valid");
			assertTrue(bitsToDouble(nMin) == -100.25, "float minimum is widened to double");
			assertTrue(bitsToDouble(nMax) == 3.0, "float maximum is widened to double");

			std::vector<double> nans = { std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN() };
			assertFalse(AMC::CDataTableV2::computeStatistics(AMC::eDataTableV2DataType::Double, (const uint8_t*)nans.data(), (uint32_t)nans.size(), nMin, nMax), "NaN only page has no statistics");

			std::vector<int32_t> int32s = { 5, -7, 3 };
			assertTrue(AMC::CDataTableV2::computeStatistics(AMC::eDataTableV2DataType::Int32, (const uint8_t*)int32s.data(), (uint32_t)int32s.size(), nMin, nMax), "int32 statistics are valid");
			assertTrue((int64_t)nMin == -7, "int32 minimum is signed");
			assertTrue((int64_t)nMax == 5, "int32 maximum is signed");

			std::vector<int64_t> int64s = { -5000000000LL, 12 };
			assertTrue(AMC::CDataTableV2::computeStatistics(AMC::eDataTableV2DataType::Int64, (const uint8_t*)int64s.data(), (uint32_t)int64s.size(), nMin, nMax), "int64 statistics are valid");
			assertTrue((int64_t)nMin == -5000000000LL, "int64 minimum");
			assertTrue((int64_t)nMax == 12, "int64 maximum");

			std::vector<uint32_t> uint32s = { 4000000000U, 1 };
			assertTrue(AMC::CDataTableV2::computeStatistics(AMC::eDataTableV2DataType::Uint32, (const uint8_t*)uint32s.data(), (uint32_t)uint32s.size(), nMin, nMax), "uint32 statistics are valid");
			assertTrue(nMin == 1, "uint32 minimum");
			assertTrue(nMax == 4000000000ULL, "uint32 maximum is unsigned");

			std::vector<uint64_t> uint64s = { 18000000000000000000ULL, 7 };
			assertTrue(AMC::CDataTableV2::computeStatistics(AMC::eDataTableV2DataType::Uint64, (const uint8_t*)uint64s.data(), (uint32_t)uint64s.size(), nMin, nMax), "uint64 statistics are valid");
			assertTrue(nMin == 7, "uint64 minimum");
			assertTrue(nMax == 18000000000000000000ULL, "uint64 maximum is unsigned");

			std::vector<uint8_t> bools = { 0, 0, 0 };
			assertTrue(AMC::CDataTableV2::computeStatistics(AMC::eDataTableV2DataType::Bool, bools.data(), (uint32_t)bools.size(), nMin, nMax), "bool statistics are valid");
			assertTrue((nMin == 0) && (nMax == 0), "constant bool page has equal minimum and maximum");

			assertFalse(AMC::CDataTableV2::computeStatistics(AMC::eDataTableV2DataType::Timestamp, (const uint8_t*)uint64s.data(), 0, nMin, nMax), "empty page has no statistics");

			assertTrue(AMC::CDataTableV2::getEntrySize(AMC::eDataTableV2DataType::Bool) == 1, "bool entry size");
			assertTrue(AMC::CDataTableV2::getEntrySize(AMC::eDataTableV2DataType::Float) == 4, "float entry size");
			assertTrue(AMC::CDataTableV2::getEntrySize(AMC::eDataTableV2DataType::Timestamp) == 8, "timestamp entry size");
			assertTrue(formatErrorOf([&]() { AMC::CDataTableV2::getEntrySize((AMC::eDataTableV2DataType)99); }) == (uint32_t)AMC::eDataTableFormatError::InvalidDataType, "unknown data type is rejected");
		}

		void testSegmentRoundTrip()
		{
			auto bytes = buildSampleSegment();

			AMC::sDataTableV2SegmentHeader header;
			memcpy(&header, bytes.data(), sizeof(header));
			assertTrue(header.m_nSignature == DATATABLE_HEADERSIGNATURE_V2, "segment carries the version 2 signature");
			assertTrue(header.m_nSignature != DATATABLE_HEADERSIGNATURE, "version 2 signature differs from version 1");
			assertTrue(header.m_nVersion == DATATABLE_FORMATVERSION_2, "segment version is 2");
			assertTrue(header.m_nHeaderSize == sizeof(header), "header size field");

			AMC::CDataTableV2SegmentReader reader(memoryReader(bytes), bytes.size());

			assertTrue(reader.getSegmentIndex() == 2, "segment index round trip");
			assertTrue(reader.getFirstRowIndex() == 1000, "first row index round trip");
			assertTrue(reader.getRowCount() == 8, "row count round trip");
			assertTrue(reader.getChunkCount() == 2, "chunk count round trip");
			assertTrue(reader.getColumnCount() == 3, "column count round trip");
			assertTrue(reader.getChunkRowCount(0) == 5, "first chunk row count");
			assertTrue(reader.getChunkRowCount(1) == 3, "second chunk row count");

			assertTrue(reader.getColumn(0).m_sIdentifier == "t", "column 0 identifier");
			assertTrue(reader.getColumn(1).m_sDescription == "Temperature in C", "column 1 description");
			assertTrue(reader.getColumn(2).m_DataType == AMC::eDataTableV2DataType::Bool, "column 2 type");

			// Typed statistics: the temperature page reports its real range, which a directory with integer bounds could not.
			auto& temperatureEntry = reader.getPageEntry(0, 1);
			assertTrue((temperatureEntry.m_nFlags & DATATABLE_V2_PAGEFLAG_STATISTICSVALID) != 0, "temperature page has statistics");
			assertTrue(bitsToDouble(temperatureEntry.m_nMinValue) == -5.0, "temperature page minimum");
			assertTrue(bitsToDouble(temperatureEntry.m_nMaxValue) == 1800.25, "temperature page maximum");
			assertTrue(temperatureEntry.m_nRowCount == 5, "temperature page row count");
			assertTrue(temperatureEntry.m_nUncompressedSize == 20, "temperature page size");
			assertTrue(temperatureEntry.m_nCodec == DATATABLE_V2_CODEC_NONE, "page codec is none");
			assertTrue(temperatureEntry.m_nEncoding == DATATABLE_V2_ENCODING_PLAIN, "page encoding is plain");

			auto& timestampEntry = reader.getPageEntry(1, 0);
			assertTrue(timestampEntry.m_nMinValue == 600, "timestamp page minimum");
			assertTrue(timestampEntry.m_nMaxValue == 800, "timestamp page maximum");

			// Every page is preceded by a self-describing page header.
			AMC::sDataTableV2PageHeader pageHeader;
			memcpy(&pageHeader, bytes.data() + temperatureEntry.m_nFileOffset - sizeof(pageHeader), sizeof(pageHeader));
			assertTrue(pageHeader.m_nMagic == DATATABLE_V2_PAGEMAGIC, "page header magic");
			assertTrue(pageHeader.m_nColumnIndex == 1, "page header column index");
			assertTrue(pageHeader.m_nRowCount == 5, "page header row count");
			assertTrue(pageHeader.m_nDataType == (uint32_t)AMC::eDataTableV2DataType::Float, "page header data type");

			std::vector<float> temperatures(5);
			reader.readPage(0, 1, (uint8_t*)temperatures.data(), temperatures.size() * sizeof(float));
			assertTrue((temperatures[0] == 20.5f) && (temperatures[2] == 1800.25f) && (temperatures[4] == 22.0f), "temperature page content");

			std::vector<uint64_t> timestamps(3);
			reader.readPage(1, 0, (uint8_t*)timestamps.data(), timestamps.size() * sizeof(uint64_t));
			assertTrue((timestamps[0] == 600) && (timestamps[2] == 800), "timestamp page content");

			std::vector<uint8_t> laser(3);
			reader.readPage(1, 2, laser.data(), laser.size());
			assertTrue((laser[0] == 1) && (laser[2] == 1), "bool page content");

			std::vector<uint8_t> tooSmall(2);
			assertTrue(formatErrorOf([&]() { reader.readPage(0, 0, tooSmall.data(), tooSmall.size()); }) == (uint32_t)AMC::eDataTableFormatError::InvalidParam, "small target buffer is rejected");
			assertTrue(formatErrorOf([&]() { reader.getPageEntry(2, 0); }) == (uint32_t)AMC::eDataTableFormatError::InvalidParam, "chunk index out of range is rejected");
			assertTrue(formatErrorOf([&]() { reader.getColumn(3); }) == (uint32_t)AMC::eDataTableFormatError::InvalidParam, "column index out of range is rejected");
		}

		void testEmptySegments()
		{
			// A segment without chunks keeps the schema.
			AMC::CDataTableV2SegmentBuilder builder(sampleColumns(), 0, 0, 100);
			std::vector<uint8_t> bytes;
			builder.appendHeader(bytes);
			builder.appendFooter(bytes);

			AMC::CDataTableV2SegmentReader reader(memoryReader(bytes), bytes.size());
			assertTrue(reader.getColumnCount() == 3, "empty segment keeps its columns");
			assertTrue(reader.getChunkCount() == 0, "empty segment has no chunks");
			assertTrue(reader.getRowCount() == 0, "empty segment has no rows");

			// A segment without columns is the smallest valid segment.
			std::vector<AMC::sDataTableV2Column> noColumns;
			AMC::CDataTableV2SegmentBuilder emptyBuilder(noColumns, 0, 0, 100);
			std::vector<uint8_t> emptyBytes;
			emptyBuilder.appendHeader(emptyBytes);
			emptyBuilder.appendFooter(emptyBytes);
			assertTrue(emptyBytes.size() == sizeof(AMC::sDataTableV2SegmentHeader) + sizeof(AMC::sDataTableV2FooterHeader) + sizeof(AMC::sDataTableV2Trailer), "smallest segment is header, footer header and trailer");

			AMC::CDataTableV2SegmentReader emptyReader(memoryReader(emptyBytes), emptyBytes.size());
			assertTrue(emptyReader.getColumnCount() == 0, "segment without columns");
			assertTrue(emptyReader.getRowCount() == 0, "segment without columns has no rows");
		}

		void testSizePrediction()
		{
			auto columns = sampleColumns();
			AMC::CDataTableV2SegmentBuilder builder(columns, 0, 0, 10);

			std::vector<uint64_t> timestamps(10, 1);
			std::vector<float> temperatures(10, 2.0f);
			std::vector<uint8_t> laser(10, 1);

			std::vector<uint8_t> bytes;
			builder.appendHeader(bytes);
			assertTrue(bytes.size() == sizeof(AMC::sDataTableV2SegmentHeader), "header size");
			assertTrue(builder.getCurrentSize() == bytes.size(), "current size after header");

			size_t nBefore = bytes.size();
			builder.appendChunk({ (const uint8_t*)timestamps.data(), (const uint8_t*)temperatures.data(), laser.data() }, 10, bytes);
			assertTrue(bytes.size() - nBefore == builder.getChunkSizeInBytes(10), "predicted chunk size matches");
			// 3 page headers and 10 rows of 8 + 4 + 1 bytes
			assertTrue(builder.getChunkSizeInBytes(10) == 3 * sizeof(AMC::sDataTableV2PageHeader) + 10 * 13, "chunk size formula");

			nBefore = bytes.size();
			builder.appendChunk({ (const uint8_t*)timestamps.data(), (const uint8_t*)temperatures.data(), laser.data() }, 4, bytes);
			assertTrue(bytes.size() - nBefore == builder.getChunkSizeInBytes(4), "predicted partial chunk size matches");

			uint64_t nPredictedFooterSize = builder.getFooterSizeInBytes(builder.getChunkCount());
			nBefore = bytes.size();
			builder.appendFooter(bytes);
			assertTrue(bytes.size() - nBefore == nPredictedFooterSize, "predicted footer size matches");
			assertTrue(builder.getCurrentSize() == bytes.size(), "current size is the segment size");
			assertTrue(builder.getRowCount() == 14, "builder row count");
			assertTrue(builder.getChunkCount() == 2, "builder chunk count");
		}

		void testTruncatedSegments()
		{
			auto bytes = buildSampleSegment();

			// A segment that was interrupted anywhere before its trailer is complete has no footer.
			// No cut may make the reader read outside of the stream or return data.
			for (size_t nCutSize = 0; nCutSize < bytes.size(); nCutSize++) {
				std::vector<uint8_t> cut(bytes.begin(), bytes.begin() + nCutSize);

				uint32_t nError = formatErrorOf([&]() {
					AMC::CDataTableV2SegmentReader reader(memoryReader(cut), cut.size());
				});

				bool bRejected = (nError == (uint32_t)AMC::eDataTableFormatError::Truncated) || (nError == (uint32_t)AMC::eDataTableFormatError::Corrupt);
				assertTrue(bRejected, "segment cut at " + std::to_string(nCutSize) + " bytes is rejected, got error " + std::to_string(nError));
			}

			// The typical interruption: the chunk data is complete, the footer is missing.
			std::vector<uint8_t> noFooter(bytes.begin(), bytes.begin() + directoryOffset(bytes) - 200);
			assertTrue(formatErrorOf([&]() {
				AMC::CDataTableV2SegmentReader reader(memoryReader(noFooter), noFooter.size());
			}) == (uint32_t)AMC::eDataTableFormatError::Truncated, "segment without footer is reported as truncated");
		}

		void testCorruptSegments()
		{
			auto bytes = buildSampleSegment();
			size_t nDirectoryOffset = directoryOffset(bytes);

			auto errorOfVariant = [&](std::vector<uint8_t>& variant) {
				return formatErrorOf([&]() {
					AMC::CDataTableV2SegmentReader reader(memoryReader(variant), variant.size());
				});
			};

			{
				auto variant = bytes;
				uint32_t nSignature = 0x12345678;
				memcpy(variant.data(), &nSignature, sizeof(nSignature));
				assertTrue(errorOfVariant(variant) == (uint32_t)AMC::eDataTableFormatError::InvalidSignature, "foreign signature is rejected");
			}

			{
				auto variant = bytes;
				uint32_t nVersion = 3;
				memcpy(variant.data() + 4, &nVersion, sizeof(nVersion));
				assertTrue(errorOfVariant(variant) == (uint32_t)AMC::eDataTableFormatError::UnsupportedVersion, "unknown version is rejected");
			}

			{
				auto variant = bytes;
				AMC::sDataTableV2Trailer trailer;
				memcpy(&trailer, variant.data() + variant.size() - sizeof(trailer), sizeof(trailer));
				trailer.m_nFooterLength = variant.size();
				memcpy(variant.data() + variant.size() - sizeof(trailer), &trailer, sizeof(trailer));
				assertTrue(errorOfVariant(variant) == (uint32_t)AMC::eDataTableFormatError::Corrupt, "footer length beyond the stream is rejected");
			}

			{
				auto variant = bytes;
				AMC::sDataTableV2PageEntry entry;
				memcpy(&entry, variant.data() + nDirectoryOffset, sizeof(entry));
				entry.m_nCodec = 7;
				memcpy(variant.data() + nDirectoryOffset, &entry, sizeof(entry));
				assertTrue(errorOfVariant(variant) == (uint32_t)AMC::eDataTableFormatError::UnsupportedCodec, "unknown page codec is rejected");
			}

			{
				auto variant = bytes;
				AMC::sDataTableV2PageEntry entry;
				memcpy(&entry, variant.data() + nDirectoryOffset, sizeof(entry));
				entry.m_nEncoding = 9;
				memcpy(variant.data() + nDirectoryOffset, &entry, sizeof(entry));
				assertTrue(errorOfVariant(variant) == (uint32_t)AMC::eDataTableFormatError::UnsupportedEncoding, "unknown page encoding is rejected");
			}

			{
				auto variant = bytes;
				AMC::sDataTableV2PageEntry entry;
				memcpy(&entry, variant.data() + nDirectoryOffset, sizeof(entry));
				entry.m_nFileOffset = variant.size() * 2;
				memcpy(variant.data() + nDirectoryOffset, &entry, sizeof(entry));
				assertTrue(errorOfVariant(variant) == (uint32_t)AMC::eDataTableFormatError::Corrupt, "page offset outside of the segment is rejected");
			}

			{
				// The second column of the first chunk claims a different row count.
				auto variant = bytes;
				size_t nEntryOffset = nDirectoryOffset + sizeof(AMC::sDataTableV2PageEntry);
				AMC::sDataTableV2PageEntry entry;
				memcpy(&entry, variant.data() + nEntryOffset, sizeof(entry));
				entry.m_nRowCount = 4;
				entry.m_nCompressedSize = 16;
				entry.m_nUncompressedSize = 16;
				memcpy(variant.data() + nEntryOffset, &entry, sizeof(entry));
				assertTrue(errorOfVariant(variant) == (uint32_t)AMC::eDataTableFormatError::Corrupt, "different row counts within a chunk are rejected");
			}

			{
				// The page size does not match the row count.
				auto variant = bytes;
				AMC::sDataTableV2PageEntry entry;
				memcpy(&entry, variant.data() + nDirectoryOffset, sizeof(entry));
				entry.m_nUncompressedSize = 7;
				memcpy(variant.data() + nDirectoryOffset, &entry, sizeof(entry));
				assertTrue(errorOfVariant(variant) == (uint32_t)AMC::eDataTableFormatError::Corrupt, "page size mismatch is rejected");
			}

			{
				auto variant = bytes;
				AMC::sDataTableV2SegmentHeader header;
				memcpy(&header, variant.data(), sizeof(header));
				header.m_nColumnCount = 5;
				memcpy(variant.data(), &header, sizeof(header));
				assertTrue(errorOfVariant(variant) == (uint32_t)AMC::eDataTableFormatError::Corrupt, "column count mismatch between header and footer is rejected");
			}
		}

		void testBuilderMisuse()
		{
			std::vector<uint64_t> timestamps(4, 1);
			std::vector<float> temperatures(4, 2.0f);
			std::vector<uint8_t> laser(4, 1);
			std::vector<const uint8_t*> columnData = { (const uint8_t*)timestamps.data(), (const uint8_t*)temperatures.data(), laser.data() };

			uint32_t nInvalidParam = (uint32_t)AMC::eDataTableFormatError::InvalidParam;

			{
				AMC::CDataTableV2SegmentBuilder builder(sampleColumns(), 0, 0, 4);
				std::vector<uint8_t> bytes;
				assertTrue(formatErrorOf([&]() { builder.appendChunk(columnData, 4, bytes); }) == nInvalidParam, "chunk before header is rejected");
				assertTrue(formatErrorOf([&]() { builder.appendFooter(bytes); }) == nInvalidParam, "footer before header is rejected");

				builder.appendHeader(bytes);
				assertTrue(formatErrorOf([&]() { builder.appendHeader(bytes); }) == nInvalidParam, "second header is rejected");
				assertTrue(formatErrorOf([&]() { builder.appendChunk(columnData, 0, bytes); }) == nInvalidParam, "empty chunk is rejected");

				std::vector<const uint8_t*> tooFewColumns = { (const uint8_t*)timestamps.data() };
				assertTrue(formatErrorOf([&]() { builder.appendChunk(tooFewColumns, 4, bytes); }) == nInvalidParam, "chunk with missing columns is rejected");

				std::vector<const uint8_t*> nullColumn = { (const uint8_t*)timestamps.data(), nullptr, laser.data() };
				assertTrue(formatErrorOf([&]() { builder.appendChunk(nullColumn, 4, bytes); }) == nInvalidParam, "chunk with a null column is rejected");

				builder.appendFooter(bytes);
				assertTrue(formatErrorOf([&]() { builder.appendChunk(columnData, 4, bytes); }) == nInvalidParam, "chunk after footer is rejected");
				assertTrue(formatErrorOf([&]() { builder.appendFooter(bytes); }) == nInvalidParam, "second footer is rejected");
			}

			{
				std::vector<AMC::sDataTableV2Column> invalidColumns;
				invalidColumns.push_back({ "x", "X", (AMC::eDataTableV2DataType)42 });
				assertTrue(formatErrorOf([&]() { AMC::CDataTableV2SegmentBuilder builder(invalidColumns, 0, 0, 4); }) == (uint32_t)AMC::eDataTableFormatError::InvalidDataType, "unknown column type is rejected");
			}
		}

	};

}

#endif // __AMCTEST_UNITTEST_DATATABLEFORMAT
