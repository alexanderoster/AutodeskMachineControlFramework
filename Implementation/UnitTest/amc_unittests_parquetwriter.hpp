/*++

Copyright (C) 2026 Autodesk Inc.

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

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
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


#ifndef __AMCTEST_UNITTEST_PARQUETWRITER
#define __AMCTEST_UNITTEST_PARQUETWRITER


#include "amc_unittests.hpp"
#include "common_parquetwriter.hpp"
#include "amc_scatterplot.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>


namespace AMCUnitTest {

	class CUnitTestGroup_ParquetWriter : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "ParquetWriter";
		}

		void registerTests() override {
			registerTest("FileFrame", "The file starts and ends with PAR1 and the footer length is consistent", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ParquetWriter::testFileFrame, this));
			registerTest("PlainValues", "Uncompressed pages contain the PLAIN little endian values", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ParquetWriter::testPlainValues, this));
			registerTest("GZIPPages", "GZIP pages carry a gzip member instead of the raw values", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ParquetWriter::testGZIPPages, this));
			registerTest("RowGroups", "Rows are split into row groups of the given size", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ParquetWriter::testRowGroups, this));
			registerTest("InvalidColumns", "Empty, duplicate and mismatching columns are rejected", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ParquetWriter::testInvalidColumns, this));
			registerTest("ScatterplotCSV", "Scatterplot CSV export writes all channel columns and pads missing values", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ParquetWriter::testScatterplotCSV, this));
			registerTest("ScatterplotParquet", "Scatterplot Parquet export writes all channel columns", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ParquetWriter::testScatterplotParquet, this));
		}

		void initializeTests() override {
		}

	private:

		typedef AMCCommon::CParquetWriter CParquetWriter;
		typedef AMCCommon::eParquetCompressionCodec eCodec;

		static bool containsSequence(const std::vector<uint8_t>& buffer, const std::vector<uint8_t>& sequence)
		{
			return std::search(buffer.begin(), buffer.end(), sequence.begin(), sequence.end()) != buffer.end();
		}

		static size_t countSequence(const std::vector<uint8_t>& buffer, const std::vector<uint8_t>& sequence)
		{
			size_t nCount = 0;
			auto iIter = buffer.begin();
			while (true) {
				iIter = std::search(iIter, buffer.end(), sequence.begin(), sequence.end());
				if (iIter == buffer.end())
					return nCount;
				nCount++;
				iIter++;
			}
		}

		static std::vector<uint8_t> plainDoubles(const std::vector<double>& values)
		{
			std::vector<uint8_t> bytes;
			for (double dValue : values) {
				uint64_t nBits;
				memcpy(&nBits, &dValue, sizeof(nBits));
				for (uint32_t nIndex = 0; nIndex < 8; nIndex++)
					bytes.push_back((uint8_t)(nBits >> (nIndex * 8)));
			}
			return bytes;
		}

		static uint32_t footerLength(const std::vector<uint8_t>& buffer)
		{
			size_t nOffset = buffer.size() - 8;
			return (uint32_t)buffer[nOffset] | ((uint32_t)buffer[nOffset + 1] << 8) | ((uint32_t)buffer[nOffset + 2] << 16) | ((uint32_t)buffer[nOffset + 3] << 24);
		}

		void assertFileFrame(const std::vector<uint8_t>& buffer)
		{
			assertTrue(buffer.size() > 12, "file has header and footer");
			assertTrue(memcmp(buffer.data(), "PAR1", 4) == 0, "header magic");
			assertTrue(memcmp(buffer.data() + buffer.size() - 4, "PAR1", 4) == 0, "footer magic");

			uint32_t nFooterLength = footerLength(buffer);
			assertTrue((size_t)nFooterLength + 12 <= buffer.size(), "footer length fits into the file");

			// FileMetaData starts with field 1 (i32, short form 0x15) holding version 1 (zigzag 0x02).
			size_t nFooterStart = buffer.size() - 8 - nFooterLength;
			assertTrue(buffer[nFooterStart] == 0x15, "footer starts with the version field");
			assertTrue(buffer[nFooterStart + 1] == 0x02, "format version 1");
			assertTrue(buffer[buffer.size() - 9] == 0x00, "footer struct is terminated");
		}

		void testFileFrame()
		{
			CParquetWriter writer(eCodec::Uncompressed);
			writer.addDoubleColumn("value", { 1.0, 2.0, 3.0 });
			writer.addInt32Column("index", { 0, 1, 2 });
			writer.addKeyValueMetaData("origin", "unittest");

			std::vector<uint8_t> buffer;
			writer.writeToBuffer(buffer);

			assertFileFrame(buffer);
			assertTrue(writer.getRowCount() == 3, "row count");
			assertTrue(writer.getColumnCount() == 2, "column count");

			std::string sFile(buffer.begin(), buffer.end());
			assertTrue(sFile.find("value") != std::string::npos, "column name in schema");
			assertTrue(sFile.find("unittest") != std::string::npos, "key value metadata");
		}

		void testPlainValues()
		{
			std::vector<double> values = { 1.5, -2.25, 1.0e300, 0.0 };
			CParquetWriter writer(eCodec::Uncompressed);
			writer.addDoubleColumn("value", values);

			std::vector<uint8_t> buffer;
			writer.writeToBuffer(buffer);

			assertFileFrame(buffer);
			assertTrue(containsSequence(buffer, plainDoubles(values)), "plain values are stored contiguously");
		}

		void testGZIPPages()
		{
			std::vector<double> values(1000);
			for (size_t nIndex = 0; nIndex < values.size(); nIndex++)
				values[nIndex] = (double)(nIndex % 10);

			CParquetWriter writer(eCodec::GZIP);
			writer.addDoubleColumn("value", values);

			std::vector<uint8_t> buffer;
			writer.writeToBuffer(buffer);

			assertFileFrame(buffer);
			assertTrue(containsSequence(buffer, { 0x1f, 0x8b, 0x08 }), "gzip member present");
			assertFalse(containsSequence(buffer, plainDoubles(values)), "raw values are not stored");
			assertTrue(buffer.size() < values.size() * sizeof(double), "repetitive data is compressed");
		}

		void testRowGroups()
		{
			std::vector<double> values = { 10.0, 11.0, 12.0, 13.0, 14.0 };
			CParquetWriter writer(eCodec::Uncompressed, 2);
			writer.addDoubleColumn("value", values);

			std::vector<uint8_t> buffer;
			writer.writeToBuffer(buffer);

			assertFileFrame(buffer);
			assertTrue(containsSequence(buffer, plainDoubles({ 10.0, 11.0 })), "first row group");
			assertTrue(containsSequence(buffer, plainDoubles({ 12.0, 13.0 })), "second row group");
			assertTrue(containsSequence(buffer, plainDoubles({ 14.0 })), "last row group");
			assertFalse(containsSequence(buffer, plainDoubles({ 11.0, 12.0 })), "row groups are separate pages");

			// The column name is stored in the schema and in the path_in_schema of every column chunk.
			std::vector<uint8_t> columnName = { 'v', 'a', 'l', 'u', 'e' };
			assertTrue(countSequence(buffer, columnName) == 4, "one schema entry and three column chunks");
		}

		void assertThrows(std::function<void()> callback, const std::string& sContext)
		{
			bool bThrown = false;
			try {
				callback();
			}
			catch (std::exception&) {
				bThrown = true;
			}
			assertTrue(bThrown, sContext);
		}

		void testInvalidColumns()
		{
			assertThrows([]() { CParquetWriter writer; writer.addDoubleColumn("", { 1.0 }); }, "empty name");
			assertThrows([]() { CParquetWriter writer; writer.addDoubleColumn("a", { 1.0 }); writer.addDoubleColumn("a", { 2.0 }); }, "duplicate name");
			assertThrows([]() { CParquetWriter writer; writer.addDoubleColumn("a", { 1.0 }); writer.addDoubleColumn("b", { 1.0, 2.0 }); }, "row count mismatch");
			assertThrows([]() { CParquetWriter writer; std::vector<uint8_t> buffer; writer.writeToBuffer(buffer); }, "no columns");
			assertThrows([]() { CParquetWriter writer(eCodec::Uncompressed, 0); }, "zero row group size");
		}

		AMC::PScatterplot createScatterplot()
		{
			auto pScatterplot = std::make_shared<AMC::CScatterplot>("6f0d3b0e-2a9c-4c55-9a51-6f2b07a1c001");
			auto& entries = pScatterplot->getEntries();
			entries.push_back({ 1.5, 2.0 });
			entries.push_back({ -0.125, 1.0e-7 });
			entries.push_back({ 3.0, 4.0 });

			auto& channels = pScatterplot->getChannelEntries();
			channels["laser"]["power"] = { 100.0, 0.1, 250.5 };
			channels["laser"]["on"] = { 1.0, 0.0 };
			return pScatterplot;
		}

		void testScatterplotCSV()
		{
			auto pScatterplot = createScatterplot();
			std::vector<uint8_t> buffer;
			pScatterplot->writeCSVToBuffer(buffer);

			std::string sCSV(buffer.begin(), buffer.end());
			std::string sExpected =
				"x,y,laser_on,laser_power\n"
				"1.5,2,1,100\n"
				"-0.125,1e-07,0,0.1\n"
				"3,4,,250.5\n";
			assertTrue(sCSV == sExpected, "csv content: " + sCSV);
		}

		void testScatterplotParquet()
		{
			auto pScatterplot = createScatterplot();
			std::vector<uint8_t> buffer;
			pScatterplot->writeParquetToBuffer(buffer);

			assertFileFrame(buffer);
			std::string sFile(buffer.begin(), buffer.end());
			assertTrue(sFile.find("laser_power") != std::string::npos, "power column");
			assertTrue(sFile.find("laser_on") != std::string::npos, "padded column");
			assertTrue(sFile.find("6f0d3b0e-2a9c-4c55-9a51-6f2b07a1c001") != std::string::npos, "scatterplot uuid metadata");
		}

	};

}

#endif //__AMCTEST_UNITTEST_PARQUETWRITER
