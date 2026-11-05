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

#ifndef __AMCTEST_UNITTEST_DATATABLE
#define __AMCTEST_UNITTEST_DATATABLE

#include "amc_unittests.hpp"
#include "amc_unittests_libmcdata.hpp"
#include "amc_constants.hpp"
#include "amc_toolpathhandler.hpp"
#include "common_chrono.hpp"
#include "common_utils.hpp"

#include "libmcenv_datatable.hpp"
#include "libmcenv_tempstreamwriter.hpp"
#include "libmcenv_streamreader.hpp"
#include "libmcenv_interfaceexception.hpp"

#include "lz4.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace AMCUnitTest {

#pragma pack(push, 1)

	// Byte-exact mirror of the version 1 stream layout. Used to hand-craft legacy (unversioned)
	// and corrupted streams independently of the writer under test.
	struct sDataTableTestStreamHeader {
		uint32_t m_nSignature;
		uint32_t m_nVersion;
		uint64_t m_nColumnTableStart;
		uint32_t m_nColumnCount;
		uint32_t m_nReserved[8];
	};

	struct sDataTableTestColumnHeader {
		uint32_t m_nColumnDataType;
		uint32_t m_nEncodingType;
		uint64_t m_nColumnDataStart;
		uint64_t m_nIdentifierStart;
		uint32_t m_nIdentifierLength;
		uint64_t m_nDescriptionStart;
		uint32_t m_nDescriptionLength;
		uint64_t m_nEntryCount;
		uint32_t m_nReserved[8];
	};

#pragma pack(pop)

	static_assert(sizeof(sDataTableTestStreamHeader) == 52, "datatable stream header must be 52 bytes");
	static_assert(sizeof(sDataTableTestColumnHeader) == 80, "datatable column header must be 80 bytes");

	class CUnitTestGroup_DataTable : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "DataTable";
		}

		void registerTests() override {
			registerTest("ColumnManagement", "Adding, querying and removing columns of all types", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testColumnManagement, this));
			registerTest("ColumnValuesInMemory", "Whole-column setters and getters, padding and type checks", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testColumnValuesInMemory, this));
			registerTest("BinaryRoundTripAllTypes", "Format version 1 round trip through a storage stream", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testBinaryRoundTripAllTypes, this));
			registerTest("LegacyUnversionedFileLoads", "Streams with version field 0 still load", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testLegacyUnversionedFileLoads, this));
			registerTest("RejectsUnknownVersion", "Reader rejects unknown format versions", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testRejectsUnknownVersion, this));
			registerTest("RejectsUnknownEncoding", "Reader rejects unknown column encodings", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testRejectsUnknownEncoding, this));
			registerTest("RejectsInvalidSignature", "Reader rejects foreign signatures and invalid streams under the version 2 signature", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testRejectsInvalidSignature, this));
			registerTest("RejectsTruncatedStream", "Reader reports truncated column data cleanly", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testRejectsTruncatedStream, this));
			registerTest("CSVExport", "CSV export of all column types", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testCSVExport, this));
			registerTest("EmptyTableRoundTrip", "Round trip of tables without columns or without rows", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testEmptyTableRoundTrip, this));
			registerTest("LZ4RoundTrip", "Vendored LZ4 is linked and compresses/decompresses correctly", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTable::testLZ4RoundTrip, this));
		}

		void initializeTests() override {
		}

	private:

		struct SFixture {
			LibMCData::PWrapper m_pDataWrapper;
			LibMCData::PDataModel m_pDataModel;
			AMC::PToolpathHandler m_pToolpathHandler;
			AMCCommon::PChrono m_pChrono;
			std::string m_sBasePath;
		};

		SFixture createFixture(const std::string& sSuffix)
		{
			SFixture fixture;

			std::string sRootPath = "temp";
			if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(sRootPath))
				AMCCommon::CUtils::createDirectoryOnDisk(sRootPath);

			fixture.m_sBasePath = sRootPath + "/datatable_" + sSuffix + "_" + AMCCommon::CUtils::createUUID();
			if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(fixture.m_sBasePath))
				AMCCommon::CUtils::createDirectoryOnDisk(fixture.m_sBasePath);

			std::string sDatabaseFile = fixture.m_sBasePath + "/storage.db";

			fixture.m_pDataWrapper = AMCUnitTest::loadLibMCDataInProcess();
			fixture.m_pDataModel = fixture.m_pDataWrapper->CreateDataModelInstance();
			fixture.m_pDataModel->InitialiseDatabase(fixture.m_sBasePath, LibMCData::eDataBaseType::SqLite, sDatabaseFile);
			fixture.m_pToolpathHandler = std::make_shared<AMC::CToolpathHandler>(fixture.m_pDataModel);
			fixture.m_pChrono = std::make_shared<AMCCommon::CChrono>();

			return fixture;
		}

		std::unique_ptr<LibMCEnv::Impl::CDataTable> createTable(SFixture& fixture)
		{
			return std::make_unique<LibMCEnv::Impl::CDataTable>(fixture.m_pToolpathHandler);
		}

		std::unique_ptr<LibMCEnv::Impl::CTempStreamWriter> createWriter(SFixture& fixture, const std::string& sMIMEType)
		{
			return std::make_unique<LibMCEnv::Impl::CTempStreamWriter>(fixture.m_pDataModel, "datatable_test.dat", sMIMEType, AMCCommon::CUtils::createEmptyUUID(), fixture.m_pChrono);
		}

		std::unique_ptr<LibMCEnv::Impl::IStreamReader> writeBytes(SFixture& fixture, const std::vector<uint8_t>& bytes)
		{
			auto pWriter = createWriter(fixture, "application/amcf-datatable");
			if (!bytes.empty())
				pWriter->WriteData(bytes.size(), bytes.data());
			pWriter->Finish();
			return std::unique_ptr<LibMCEnv::Impl::IStreamReader>(pWriter->GetStreamReader());
		}

		std::vector<uint8_t> readAll(LibMCEnv::Impl::IStreamReader* pReader)
		{
			std::vector<uint8_t> bytes;
			uint64_t nSize = pReader->GetSize();
			bytes.resize((size_t)nSize);
			if (nSize > 0) {
				pReader->Seek(0);
				pReader->ReadData(nSize, nSize, nullptr, bytes.data());
			}
			return bytes;
		}

		template <typename F>
		int32_t errorCodeOf(F&& fn)
		{
			try {
				fn();
			}
			catch (ELibMCEnvInterfaceException& e) {
				return e.getErrorCode();
			}
			return 0;
		}

		bool nearlyEqual(double a, double b, double epsilon = 1.0e-9)
		{
			return std::fabs(a - b) < epsilon;
		}

		template <typename T>
		void appendBytes(std::vector<uint8_t>& bytes, const T& value)
		{
			const uint8_t* pData = (const uint8_t*)&value;
			bytes.insert(bytes.end(), pData, pData + sizeof(T));
		}

		// Builds a single double column stream in the version 1 layout with arbitrary header fields,
		// so that legacy and corrupted variants can be produced without going through the writer.
		std::vector<uint8_t> buildDoubleColumnStream(uint32_t nSignature, uint32_t nVersion, uint32_t nEncoding, const std::vector<double>& values, uint64_t nEntryCountOverride = UINT64_MAX)
		{
			std::string sIdentifier = "x";
			std::string sDescription = "X value";

			sDataTableTestStreamHeader header;
			memset(&header, 0, sizeof(header));
			header.m_nSignature = nSignature;
			header.m_nVersion = nVersion;
			header.m_nColumnTableStart = sizeof(header);
			header.m_nColumnCount = 1;

			sDataTableTestColumnHeader column;
			memset(&column, 0, sizeof(column));
			column.m_nColumnDataType = (uint32_t)LibMCEnv::eDataTableColumnType::DoubleColumn;
			column.m_nEncodingType = nEncoding;

			uint64_t nOffset = sizeof(header) + sizeof(column);
			column.m_nIdentifierStart = nOffset;
			column.m_nIdentifierLength = (uint32_t)sIdentifier.length();
			nOffset += sIdentifier.length();
			column.m_nDescriptionStart = nOffset;
			column.m_nDescriptionLength = (uint32_t)sDescription.length();
			nOffset += sDescription.length();
			column.m_nColumnDataStart = nOffset;
			column.m_nEntryCount = (nEntryCountOverride == UINT64_MAX) ? values.size() : nEntryCountOverride;

			std::vector<uint8_t> bytes;
			appendBytes(bytes, header);
			appendBytes(bytes, column);
			bytes.insert(bytes.end(), sIdentifier.begin(), sIdentifier.end());
			bytes.insert(bytes.end(), sDescription.begin(), sDescription.end());
			for (double dValue : values)
				appendBytes(bytes, dValue);

			return bytes;
		}

		void addAllColumnTypes(LibMCEnv::Impl::CDataTable* pTable)
		{
			pTable->AddColumn("d", "Double", LibMCEnv::eDataTableColumnType::DoubleColumn);
			pTable->AddColumn("i32", "Int32", LibMCEnv::eDataTableColumnType::Int32Column);
			pTable->AddColumn("u32", "Uint32", LibMCEnv::eDataTableColumnType::Uint32Column);
			pTable->AddColumn("i64", "Int64", LibMCEnv::eDataTableColumnType::Int64Column);
			pTable->AddColumn("u64", "Uint64", LibMCEnv::eDataTableColumnType::Uint64Column);
			pTable->AddColumn("f", "Float", LibMCEnv::eDataTableColumnType::FloatColumn);
			pTable->AddColumn("b", "Bool", LibMCEnv::eDataTableColumnType::BoolColumn);
			pTable->AddColumn("t", "Timestamp", LibMCEnv::eDataTableColumnType::TimestampColumn);
		}

		void testColumnManagement()
		{
			auto fixture = createFixture("columns");
			auto pTable = createTable(fixture);

			assertTrue(pTable->GetColumnCount() == 0, "new table has no columns");
			assertTrue(pTable->GetRowCount() == 0, "new table has no rows");
			assertTrue(pTable->GetRowCount64() == 0, "new table has no rows (64 bit)");

			addAllColumnTypes(pTable.get());

			assertTrue(pTable->GetColumnCount() == 8, "eight columns added");
			assertTrue(pTable->HasColumn("f"), "float column exists");
			assertTrue(pTable->HasColumn("b"), "bool column exists");
			assertTrue(pTable->HasColumn("t"), "timestamp column exists");
			assertFalse(pTable->HasColumn("missing"), "unknown column does not exist");

			assertTrue(pTable->GetColumnType(0) == LibMCEnv::eDataTableColumnType::DoubleColumn, "column 0 is double");
			assertTrue(pTable->GetColumnType(5) == LibMCEnv::eDataTableColumnType::FloatColumn, "column 5 is float");
			assertTrue(pTable->GetColumnType(6) == LibMCEnv::eDataTableColumnType::BoolColumn, "column 6 is bool");
			assertTrue(pTable->GetColumnType(7) == LibMCEnv::eDataTableColumnType::TimestampColumn, "column 7 is timestamp");
			assertTrue(pTable->GetColumnIdentifier(7) == "t", "column 7 identifier");
			assertTrue(pTable->GetColumnDescription(7) == "Timestamp", "column 7 description");

			std::string sDescription;
			LibMCEnv::eDataTableColumnType eType = LibMCEnv::eDataTableColumnType::Unknown;
			pTable->GetColumnInformation("b", sDescription, eType);
			assertTrue(sDescription == "Bool", "column information description");
			assertTrue(eType == LibMCEnv::eDataTableColumnType::BoolColumn, "column information type");

			assertTrue(errorCodeOf([&]() { pTable->AddColumn("", "empty", LibMCEnv::eDataTableColumnType::DoubleColumn); }) == LIBMCENV_ERROR_COLUMNIDENTIFIEREMPTY, "empty identifier rejected");
			assertTrue(errorCodeOf([&]() { pTable->AddColumn("bad id!", "invalid", LibMCEnv::eDataTableColumnType::DoubleColumn); }) == LIBMCENV_ERROR_INVALIDCOLUMNIDENTIFIER, "invalid identifier rejected");
			assertTrue(errorCodeOf([&]() { pTable->AddColumn("d", "duplicate", LibMCEnv::eDataTableColumnType::DoubleColumn); }) == LIBMCENV_ERROR_DUPLICATECOLUMNIDENTIFIER, "duplicate identifier rejected");
			assertTrue(errorCodeOf([&]() { pTable->AddColumn("unknown", "unknown type", (LibMCEnv::eDataTableColumnType)99); }) == LIBMCENV_ERROR_INVALIDCOLUMNDATATYPE, "unknown column type rejected");
			assertTrue(errorCodeOf([&]() { pTable->GetColumnIdentifier(99); }) == LIBMCENV_ERROR_INVALIDCOLUMNINDEX, "column index out of range rejected");
			assertTrue(errorCodeOf([&]() { pTable->RemoveColumn("missing"); }) == LIBMCENV_ERROR_COLUMNIDENTIFIERNOTFOUND, "removing unknown column rejected");

			pTable->RemoveColumn("i32");
			assertTrue(pTable->GetColumnCount() == 7, "column removed");
			assertFalse(pTable->HasColumn("i32"), "removed column is gone");
			assertTrue(pTable->GetColumnIdentifier(1) == "u32", "columns shift after removal");

			pTable->Clear();
			assertTrue(pTable->GetColumnCount() == 0, "clear removes all columns");
		}

		void testColumnValuesInMemory()
		{
			auto fixture = createFixture("values");
			auto pTable = createTable(fixture);
			addAllColumnTypes(pTable.get());

			std::vector<double> doubles = { 1.5, -2.25, 3.0 };
			std::vector<float> floats = { 0.5f, 100.25f };
			std::vector<uint8_t> bools = { 0, 5, 1, 0 };
			std::vector<uint64_t> timestamps = { 1000000ULL, 2000000ULL, 3000000ULL, 4000000ULL };
			std::vector<int32_t> int32s = { -1, 2 };

			pTable->SetDoubleColumnValues("d", doubles.size(), doubles.data());
			assertTrue(pTable->GetRowCount() == 3, "row count follows the longest column");
			pTable->SetFloatColumnValues("f", floats.size(), floats.data());
			assertTrue(pTable->GetRowCount() == 3, "shorter column does not shrink the row count");
			pTable->SetBoolColumnValues("b", bools.size(), bools.data());
			pTable->SetTimestampColumnValues("t", timestamps.size(), timestamps.data());
			pTable->SetInt32ColumnValues("i32", int32s.size(), int32s.data());
			assertTrue(pTable->GetRowCount() == 4, "row count grows with the longest column");
			assertTrue(pTable->GetRowCount64() == 4, "64 bit row count matches");

			uint64_t nNeeded = 0;
			pTable->GetFloatColumnValues("f", 0, &nNeeded, nullptr);
			assertTrue(nNeeded == 4, "needed count is the table row count");

			std::vector<float> floatsOut(4, -1.0f);
			pTable->GetFloatColumnValues("f", floatsOut.size(), nullptr, floatsOut.data());
			assertTrue(floatsOut[0] == 0.5f && floatsOut[1] == 100.25f, "float values round trip");
			assertTrue(floatsOut[2] == 0.0f && floatsOut[3] == 0.0f, "missing float rows are padded with zero");

			std::vector<uint8_t> boolsOut(4, 7);
			pTable->GetBoolColumnValues("b", boolsOut.size(), nullptr, boolsOut.data());
			assertTrue(boolsOut[0] == 0 && boolsOut[1] == 1 && boolsOut[2] == 1 && boolsOut[3] == 0, "bool values are normalised to 0 and 1");

			std::vector<uint64_t> timestampsOut(4, 0);
			pTable->GetTimestampColumnValues("t", timestampsOut.size(), nullptr, timestampsOut.data());
			assertTrue(timestampsOut == timestamps, "timestamp values round trip");

			std::vector<double> doublesOut(4, -1.0);
			pTable->GetDoubleColumnValues("d", doublesOut.size(), nullptr, doublesOut.data());
			assertTrue(nearlyEqual(doublesOut[1], -2.25) && nearlyEqual(doublesOut[3], 0.0), "double values round trip with padding");

			std::vector<float> tooSmall(2);
			assertTrue(errorCodeOf([&]() { pTable->GetFloatColumnValues("f", tooSmall.size(), nullptr, tooSmall.data()); }) == LIBMCENV_ERROR_BUFFERTOOSMALL, "small buffer rejected");
			assertTrue(errorCodeOf([&]() { pTable->GetFloatColumnValues("d", floatsOut.size(), nullptr, floatsOut.data()); }) == LIBMCENV_ERROR_COLUMNISNOTOFTYPEFLOAT, "float getter on double column rejected");
			assertTrue(errorCodeOf([&]() { pTable->GetBoolColumnValues("d", boolsOut.size(), nullptr, boolsOut.data()); }) == LIBMCENV_ERROR_COLUMNISNOTOFTYPEBOOL, "bool getter on double column rejected");
			assertTrue(errorCodeOf([&]() { pTable->GetTimestampColumnValues("d", timestampsOut.size(), nullptr, timestampsOut.data()); }) == LIBMCENV_ERROR_COLUMNISNOTOFTYPETIMESTAMP, "timestamp getter on double column rejected");
			assertTrue(errorCodeOf([&]() { pTable->SetDoubleColumnValues("f", doubles.size(), doubles.data()); }) == LIBMCENV_ERROR_COLUMNISNOTOFTYPEDOUBLE, "double setter on float column rejected");
			assertTrue(errorCodeOf([&]() { pTable->SetFloatColumnValues("t", floats.size(), floats.data()); }) == LIBMCENV_ERROR_COLUMNISNOTOFTYPEFLOAT, "float setter on timestamp column rejected");
			assertTrue(errorCodeOf([&]() { pTable->SetBoolColumnValues("missing", bools.size(), bools.data()); }) == LIBMCENV_ERROR_COLUMNIDENTIFIERNOTFOUND, "setter on unknown column rejected");

			pTable->Clear();
			assertTrue(pTable->GetRowCount64() == 0, "clear resets the row count");
		}

		void testBinaryRoundTripAllTypes()
		{
			auto fixture = createFixture("roundtrip");
			auto pTable = createTable(fixture);
			addAllColumnTypes(pTable.get());

			std::vector<double> doubles = { 1.5, -2.25, 3.0, 4.125, 1.0e6 };
			std::vector<int32_t> int32s = { -1, 2, -3, 4, 5 };
			std::vector<uint32_t> uint32s = { 1, 2, 3, 4, 4000000000U };
			std::vector<int64_t> int64s = { -10, 20, -30, 40, 50 };
			std::vector<uint64_t> uint64s = { 10, 20, 30, 40, 50 };
			std::vector<float> floats = { 0.5f, 1.5f, 2.5f, 3.5f, 4.5f };
			std::vector<uint8_t> bools = { 1, 0, 1, 1, 0 };
			std::vector<uint64_t> timestamps = { 1000000ULL, 2000000ULL, 3000000ULL, 4000000ULL, 5000000ULL };

			pTable->SetDoubleColumnValues("d", doubles.size(), doubles.data());
			pTable->SetInt32ColumnValues("i32", int32s.size(), int32s.data());
			pTable->SetUint32ColumnValues("u32", uint32s.size(), uint32s.data());
			pTable->SetInt64ColumnValues("i64", int64s.size(), int64s.data());
			pTable->SetUint64ColumnValues("u64", uint64s.size(), uint64s.data());
			pTable->SetFloatColumnValues("f", floats.size(), floats.data());
			pTable->SetBoolColumnValues("b", bools.size(), bools.data());
			pTable->SetTimestampColumnValues("t", timestamps.size(), timestamps.data());

			auto pWriter = createWriter(fixture, "application/amcf-datatable");
			pTable->WriteDataToStream(pWriter.get(), nullptr);
			pWriter->Finish();

			std::unique_ptr<LibMCEnv::Impl::IStreamReader> pReader(pWriter->GetStreamReader());

			// The writer populates the version field; the signature is unchanged for format version 1.
			auto bytes = readAll(pReader.get());
			assertTrue(bytes.size() > sizeof(sDataTableTestStreamHeader), "stream contains a header");
			sDataTableTestStreamHeader header;
			memcpy(&header, bytes.data(), sizeof(header));
			assertTrue(header.m_nSignature == DATATABLE_HEADERSIGNATURE, "written signature is the v1 signature");
			assertTrue(header.m_nVersion == DATATABLE_FORMATVERSION_1, "written version field is 1");
			assertTrue(header.m_nColumnCount == 8, "written column count");
			assertTrue(header.m_nColumnTableStart == sizeof(sDataTableTestStreamHeader), "column table follows the header");

			sDataTableTestColumnHeader firstColumn;
			memcpy(&firstColumn, bytes.data() + header.m_nColumnTableStart, sizeof(firstColumn));
			assertTrue(firstColumn.m_nEncodingType == DATATABLE_ENCODINGTYPE_RAW, "columns are written with RAW encoding");
			assertTrue(firstColumn.m_nEntryCount == 5, "first column entry count");

			auto pLoaded = createTable(fixture);
			pLoaded->LoadFromStream(pReader.get());

			assertTrue(pLoaded->GetColumnCount() == 8, "loaded column count");
			assertTrue(pLoaded->GetRowCount64() == 5, "loaded row count");
			for (uint32_t nIndex = 0; nIndex < 8; nIndex++) {
				assertTrue(pLoaded->GetColumnIdentifier(nIndex) == pTable->GetColumnIdentifier(nIndex), "loaded identifier " + std::to_string(nIndex));
				assertTrue(pLoaded->GetColumnDescription(nIndex) == pTable->GetColumnDescription(nIndex), "loaded description " + std::to_string(nIndex));
				assertTrue(pLoaded->GetColumnType(nIndex) == pTable->GetColumnType(nIndex), "loaded type " + std::to_string(nIndex));
			}

			std::vector<double> doublesOut(5);
			pLoaded->GetDoubleColumnValues("d", doublesOut.size(), nullptr, doublesOut.data());
			assertTrue(doublesOut == doubles, "double column round trip");
			std::vector<int32_t> int32sOut(5);
			pLoaded->GetInt32ColumnValues("i32", int32sOut.size(), nullptr, int32sOut.data());
			assertTrue(int32sOut == int32s, "int32 column round trip");
			std::vector<uint32_t> uint32sOut(5);
			pLoaded->GetUint32ColumnValues("u32", uint32sOut.size(), nullptr, uint32sOut.data());
			assertTrue(uint32sOut == uint32s, "uint32 column round trip");
			std::vector<int64_t> int64sOut(5);
			pLoaded->GetInt64ColumnValues("i64", int64sOut.size(), nullptr, int64sOut.data());
			assertTrue(int64sOut == int64s, "int64 column round trip");
			std::vector<uint64_t> uint64sOut(5);
			pLoaded->GetUint64ColumnValues("u64", uint64sOut.size(), nullptr, uint64sOut.data());
			assertTrue(uint64sOut == uint64s, "uint64 column round trip");
			std::vector<float> floatsOut(5);
			pLoaded->GetFloatColumnValues("f", floatsOut.size(), nullptr, floatsOut.data());
			assertTrue(floatsOut == floats, "float column round trip");
			std::vector<uint8_t> boolsOut(5);
			pLoaded->GetBoolColumnValues("b", boolsOut.size(), nullptr, boolsOut.data());
			assertTrue(boolsOut == bools, "bool column round trip");
			std::vector<uint64_t> timestampsOut(5);
			pLoaded->GetTimestampColumnValues("t", timestampsOut.size(), nullptr, timestampsOut.data());
			assertTrue(timestampsOut == timestamps, "timestamp column round trip");
		}

		void testLegacyUnversionedFileLoads()
		{
			auto fixture = createFixture("legacy");
			std::vector<double> values = { 1.5, -2.25, 3.0 };

			auto bytes = buildDoubleColumnStream(DATATABLE_HEADERSIGNATURE, DATATABLE_FORMATVERSION_UNVERSIONED, DATATABLE_ENCODINGTYPE_RAW, values);
			auto pReader = writeBytes(fixture, bytes);

			auto pTable = createTable(fixture);
			pTable->LoadFromStream(pReader.get());

			assertTrue(pTable->GetColumnCount() == 1, "legacy stream has one column");
			assertTrue(pTable->GetRowCount64() == 3, "legacy stream has three rows");
			assertTrue(pTable->GetColumnIdentifier(0) == "x", "legacy column identifier");
			assertTrue(pTable->GetColumnDescription(0) == "X value", "legacy column description");
			assertTrue(pTable->GetColumnType(0) == LibMCEnv::eDataTableColumnType::DoubleColumn, "legacy column type");

			std::vector<double> valuesOut(3);
			pTable->GetDoubleColumnValues("x", valuesOut.size(), nullptr, valuesOut.data());
			assertTrue(valuesOut == values, "legacy values load unchanged");

			// Version 1 with an explicit version field loads identically.
			auto bytesV1 = buildDoubleColumnStream(DATATABLE_HEADERSIGNATURE, DATATABLE_FORMATVERSION_1, DATATABLE_ENCODINGTYPE_RAW, values);
			auto pReaderV1 = writeBytes(fixture, bytesV1);
			pTable->LoadFromStream(pReaderV1.get());
			assertTrue(pTable->GetRowCount64() == 3, "explicit version 1 stream loads");
		}

		void testRejectsUnknownVersion()
		{
			auto fixture = createFixture("version");
			std::vector<double> values = { 1.0, 2.0 };

			auto bytes = buildDoubleColumnStream(DATATABLE_HEADERSIGNATURE, 7, DATATABLE_ENCODINGTYPE_RAW, values);
			auto pReader = writeBytes(fixture, bytes);

			auto pTable = createTable(fixture);
			pTable->AddColumn("stale", "Stale column", LibMCEnv::eDataTableColumnType::Int32Column);

			int32_t nErrorCode = errorCodeOf([&]() { pTable->LoadFromStream(pReader.get()); });
			assertTrue(nErrorCode == LIBMCENV_ERROR_UNSUPPORTEDDATATABLEVERSION, "unknown version rejected with UNSUPPORTEDDATATABLEVERSION, got " + std::to_string(nErrorCode));
			assertTrue(pTable->GetColumnCount() == 0, "table is empty after a rejected load");
		}

		void testRejectsUnknownEncoding()
		{
			auto fixture = createFixture("encoding");
			std::vector<double> values = { 1.0, 2.0 };

			auto bytes = buildDoubleColumnStream(DATATABLE_HEADERSIGNATURE, DATATABLE_FORMATVERSION_1, 2, values);
			auto pReader = writeBytes(fixture, bytes);

			auto pTable = createTable(fixture);
			int32_t nErrorCode = errorCodeOf([&]() { pTable->LoadFromStream(pReader.get()); });
			assertTrue(nErrorCode == LIBMCENV_ERROR_UNSUPPORTEDDATATABLEENCODING, "unknown encoding rejected with UNSUPPORTEDDATATABLEENCODING, got " + std::to_string(nErrorCode));
			assertTrue(pTable->GetColumnCount() == 0, "no partially loaded columns remain");
		}

		void testRejectsInvalidSignature()
		{
			auto fixture = createFixture("signature");
			std::vector<double> values = { 1.0 };

			auto bytesForeign = buildDoubleColumnStream(0x12345678, DATATABLE_FORMATVERSION_1, DATATABLE_ENCODINGTYPE_RAW, values);
			auto pReaderForeign = writeBytes(fixture, bytesForeign);
			auto pTable = createTable(fixture);
			int32_t nErrorCode = errorCodeOf([&]() { pTable->LoadFromStream(pReaderForeign.get()); });
			assertTrue(nErrorCode == LIBMCENV_ERROR_INVALIDDATATABLESIGNATURE, "foreign signature rejected, got " + std::to_string(nErrorCode));

			// The version 2 signature selects the chunked format. A stream that carries it with a version 1 layout
			// is not a valid segment, and a later format version is reported as unsupported. Neither is read as garbage.
			auto bytesV2 = buildDoubleColumnStream(DATATABLE_HEADERSIGNATURE_V2, 2, DATATABLE_ENCODINGTYPE_RAW, values);
			auto pReaderV2 = writeBytes(fixture, bytesV2);
			nErrorCode = errorCodeOf([&]() { pTable->LoadFromStream(pReaderV2.get()); });
			assertTrue(nErrorCode == LIBMCENV_ERROR_INVALIDDATATABLESEGMENT, "version 1 layout under the version 2 signature is rejected as invalid segment, got " + std::to_string(nErrorCode));
			assertTrue(pTable->GetColumnCount() == 0, "table stays empty");

			auto bytesV3 = buildDoubleColumnStream(DATATABLE_HEADERSIGNATURE_V2, 3, DATATABLE_ENCODINGTYPE_RAW, values);
			auto pReaderV3 = writeBytes(fixture, bytesV3);
			nErrorCode = errorCodeOf([&]() { pTable->LoadFromStream(pReaderV3.get()); });
			assertTrue(nErrorCode == LIBMCENV_ERROR_UNSUPPORTEDDATATABLEVERSION, "later format version is rejected as unsupported version, got " + std::to_string(nErrorCode));
			assertTrue(pTable->GetColumnCount() == 0, "table stays empty");
		}

		void testRejectsTruncatedStream()
		{
			auto fixture = createFixture("truncated");
			std::vector<double> values = { 1.0, 2.0, 3.0 };

			// The column header claims far more entries than the payload holds.
			auto bytes = buildDoubleColumnStream(DATATABLE_HEADERSIGNATURE, DATATABLE_FORMATVERSION_1, DATATABLE_ENCODINGTYPE_RAW, values, 1000);
			auto pReader = writeBytes(fixture, bytes);

			auto pTable = createTable(fixture);
			int32_t nErrorCode = errorCodeOf([&]() { pTable->LoadFromStream(pReader.get()); });
			assertTrue(nErrorCode == LIBMCENV_ERROR_INVALIDDATATABLECOLUMNHEADER, "truncated column data rejected cleanly, got " + std::to_string(nErrorCode));
			assertTrue(pTable->GetColumnCount() == 0, "no partially loaded columns remain after truncation");

			// A stream that ends inside the column table is rejected by the stream reader, not by reading garbage.
			std::vector<uint8_t> cut(bytes.begin(), bytes.begin() + sizeof(sDataTableTestStreamHeader) + 10);
			auto pReaderCut = writeBytes(fixture, cut);
			nErrorCode = errorCodeOf([&]() { pTable->LoadFromStream(pReaderCut.get()); });
			assertTrue(nErrorCode == LIBMCENV_ERROR_STREAMREADEXCEEDSSTREAMSIZE, "stream cut inside the column table is rejected by the stream reader, got " + std::to_string(nErrorCode));
			assertTrue(pTable->GetColumnCount() == 0, "table stays empty after a cut stream");
		}

		void testCSVExport()
		{
			auto fixture = createFixture("csv");
			auto pTable = createTable(fixture);
			pTable->AddColumn("d", "Double", LibMCEnv::eDataTableColumnType::DoubleColumn);
			pTable->AddColumn("f", "Float", LibMCEnv::eDataTableColumnType::FloatColumn);
			pTable->AddColumn("b", "Bool", LibMCEnv::eDataTableColumnType::BoolColumn);
			pTable->AddColumn("t", "Timestamp", LibMCEnv::eDataTableColumnType::TimestampColumn);
			pTable->AddColumn("u", "Uint32", LibMCEnv::eDataTableColumnType::Uint32Column);

			std::vector<double> doubles = { 1.5, -0.5 };
			std::vector<float> floats = { 2.25f, 0.125f };
			std::vector<uint8_t> bools = { 1, 0 };
			std::vector<uint64_t> timestamps = { 1000000ULL, 2000000ULL };
			std::vector<uint32_t> uint32s = { 7, 8 };
			pTable->SetDoubleColumnValues("d", doubles.size(), doubles.data());
			pTable->SetFloatColumnValues("f", floats.size(), floats.data());
			pTable->SetBoolColumnValues("b", bools.size(), bools.data());
			pTable->SetTimestampColumnValues("t", timestamps.size(), timestamps.data());
			pTable->SetUint32ColumnValues("u", uint32s.size(), uint32s.data());

			auto pWriter = createWriter(fixture, "text/csv");
			pTable->WriteCSVToStream(pWriter.get(), nullptr);
			pWriter->Finish();

			std::unique_ptr<LibMCEnv::Impl::IStreamReader> pReader(pWriter->GetStreamReader());
			auto bytes = readAll(pReader.get());
			std::string sCSV(bytes.begin(), bytes.end());

			std::string sExpected = "Double;Float;Bool;Timestamp;Uint32\n1.500;2.250;1;1000000;7\n-0.500;0.125;0;2000000;8\n";
			assertTrue(sCSV == sExpected, "CSV export matches, got: " + sCSV);
		}

		void testEmptyTableRoundTrip()
		{
			auto fixture = createFixture("empty");

			auto pEmpty = createTable(fixture);
			auto pWriter = createWriter(fixture, "application/amcf-datatable");
			pEmpty->WriteDataToStream(pWriter.get(), nullptr);
			pWriter->Finish();
			std::unique_ptr<LibMCEnv::Impl::IStreamReader> pReader(pWriter->GetStreamReader());
			assertTrue(pReader->GetSize() == sizeof(sDataTableTestStreamHeader), "empty table is a bare header");

			auto pLoaded = createTable(fixture);
			pLoaded->AddColumn("stale", "Stale column", LibMCEnv::eDataTableColumnType::Int32Column);
			pLoaded->LoadFromStream(pReader.get());
			assertTrue(pLoaded->GetColumnCount() == 0, "loading an empty stream clears the table");
			assertTrue(pLoaded->GetRowCount64() == 0, "empty table has no rows");

			auto pColumnsOnly = createTable(fixture);
			pColumnsOnly->AddColumn("f", "Float", LibMCEnv::eDataTableColumnType::FloatColumn);
			pColumnsOnly->AddColumn("b", "Bool", LibMCEnv::eDataTableColumnType::BoolColumn);
			auto pWriter2 = createWriter(fixture, "application/amcf-datatable");
			pColumnsOnly->WriteDataToStream(pWriter2.get(), nullptr);
			pWriter2->Finish();
			std::unique_ptr<LibMCEnv::Impl::IStreamReader> pReader2(pWriter2->GetStreamReader());

			pLoaded->LoadFromStream(pReader2.get());
			assertTrue(pLoaded->GetColumnCount() == 2, "columns without rows round trip");
			assertTrue(pLoaded->GetRowCount64() == 0, "columns without rows have zero rows");
			assertTrue(pLoaded->GetColumnType(0) == LibMCEnv::eDataTableColumnType::FloatColumn, "float column type survives");
			assertTrue(pLoaded->GetColumnType(1) == LibMCEnv::eDataTableColumnType::BoolColumn, "bool column type survives");
		}

		void testLZ4RoundTrip()
		{
			std::vector<uint8_t> input(64 * 1024);
			for (size_t nIndex = 0; nIndex < input.size(); nIndex++)
				input[nIndex] = (uint8_t)((nIndex / 64) & 0xff);

			int nBound = LZ4_compressBound((int)input.size());
			assertTrue(nBound > 0, "LZ4 compress bound is positive");

			std::vector<char> compressed((size_t)nBound);
			int nCompressedSize = LZ4_compress_default((const char*)input.data(), compressed.data(), (int)input.size(), nBound);
			assertTrue(nCompressedSize > 0, "LZ4 compression succeeds");
			assertTrue((size_t)nCompressedSize < input.size(), "LZ4 shrinks the repetitive input");

			std::vector<uint8_t> output(input.size());
			int nDecompressedSize = LZ4_decompress_safe(compressed.data(), (char*)output.data(), nCompressedSize, (int)output.size());
			assertTrue(nDecompressedSize == (int)input.size(), "LZ4 decompression restores the original size");
			assertTrue(output == input, "LZ4 decompression restores the original bytes");
		}

	};

}

#endif // __AMCTEST_UNITTEST_DATATABLE
