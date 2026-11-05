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

#ifndef __AMCTEST_UNITTEST_DATACAPTUREQUERY
#define __AMCTEST_UNITTEST_DATACAPTUREQUERY

#include "amc_unittests_datatablereader.hpp"
#include "amc_datacapturequery.hpp"
#include "amc_jsonwriter.hpp"

#include <cmath>
#include <cstring>
#include <limits>

namespace AMCUnitTest {

	// The query class builds the response bodies of the datatable API handler.
	// Uses the fixture and the signal capture of the reader tests.
	class CUnitTestGroup_DataCaptureQuery : public CUnitTestGroup_DataTableReader {
	public:

		std::string getTestGroupName() override {
			return "DataCaptureQuery";
		}

		void registerTests() override {
			registerTest("Info", "Info document with schema, segments and value ranges", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCaptureQuery::testInfo, this));
			registerTest("JSONData", "Row range as JSON value arrays", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCaptureQuery::testJSONData, this));
			registerTest("JSONSpecialValues", "64 bit integers stay exact and NaN becomes null", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCaptureQuery::testJSONSpecialValues, this));
			registerTest("JSONEnvelope", "Downsampled data as bucket series", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCaptureQuery::testJSONEnvelope, this));
			registerTest("CSV", "Row range and downsampled data as CSV", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCaptureQuery::testCSV, this));
			registerTest("Binary", "Raw float64 and float32 arrays", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCaptureQuery::testBinary, this));
			registerTest("SizeCap", "Unbounded requests are rejected with an error that names the parameters", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCaptureQuery::testSizeCap, this));
			registerTest("DefaultsAndValidation", "Default range and parameter validation", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataCaptureQuery::testDefaultsAndValidation, this));
		}

	protected:

		AMC::PDataCaptureReader openCoreReader(SFixture& fixture, const std::string& sIdentifier)
		{
			return std::make_shared<AMC::CDataCaptureReader>(fixture.m_pDataModel, fixture.m_pExecution->RetrieveDataCapture(sIdentifier));
		}

		template <typename F>
		uint32_t formatErrorOf(F&& fn, std::string* pMessage = nullptr)
		{
			try {
				fn();
			}
			catch (AMC::EDataTableFormatException& formatException) {
				if (pMessage != nullptr)
					*pMessage = formatException.what();
				return (uint32_t)formatException.getError();
			}
			return 0;
		}

		// Wraps the members a query produces into a JSON object and parses it.
		void parseQueryJSON(AMC::CDataCaptureQuery& captureQuery, const AMC::sDataCaptureQuery& query, rapidjson::Document& document, std::string& sJSON)
		{
			sJSON = "{";
			captureQuery.appendJSONMembers(query, sJSON);
			sJSON += "}";

			document.Parse(sJSON.c_str());
			assertFalse(document.HasParseError(), "query output is valid JSON: " + sJSON.substr(0, 200));
			assertTrue(document.IsObject(), "query output is a JSON object");
		}

		void testInfo()
		{
			auto fixture = createFixture("queryinfo");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());

			AMC::CDataCaptureQuery captureQuery(openCoreReader(fixture, "signal"));

			AMC::CJSONWriter writer;
			captureQuery.writeInfo(writer);

			rapidjson::Document document;
			document.Parse(writer.saveToString().c_str());
			assertFalse(document.HasParseError(), "info is valid JSON");

			auto pManifest = fixture.m_pExecution->RetrieveDataCapture("signal");
			assertTrue(std::string(document["uuid"].GetString()) == pManifest->GetUUID(), "info uuid");
			assertTrue(std::string(document["identifier"].GetString()) == "signal", "info identifier");
			assertTrue(document["rowcount"].GetUint64() == SIGNAL_ROWCOUNT, "info row count");
			assertTrue(document["firstavailablerow"].GetUint64() == 0, "info first available row");
			assertTrue(document["columncount"].GetUint64() == 4, "info column count");
			assertTrue(document["segmentcount"].GetUint64() == 7, "info segment count");
			assertTrue(document["chunkcount"].GetUint64() == 25, "info chunk count");
			assertTrue(document["storedsize"].GetUint64() == pManifest->GetStoredSizeInBytes(), "info stored size");

			auto& columns = document["columns"];
			assertTrue(columns.IsArray() && (columns.Size() == 4), "info has four columns");
			assertTrue(std::string(columns[0]["name"].GetString()) == "t", "column 0 name");
			assertTrue(std::string(columns[0]["type"].GetString()) == "timestamp", "column 0 type");
			assertTrue(columns[0]["min"].GetDouble() == (double)signalTimestamp(0), "timestamp minimum");
			assertTrue(columns[0]["max"].GetDouble() == (double)signalTimestamp(SIGNAL_ROWCOUNT - 1), "timestamp maximum");

			assertTrue(std::string(columns[1]["name"].GetString()) == "v", "column 1 name");
			assertTrue(std::string(columns[1]["description"].GetString()) == "Value", "column 1 description");
			assertTrue(std::string(columns[1]["type"].GetString()) == "float", "column 1 type");
			assertTrue(columns[1]["hasrange"].GetBool(), "column 1 has a value range");
			assertTrue(columns[1]["min"].GetDouble() == 0.0, "value minimum");
			assertTrue(columns[1]["max"].GetDouble() == 5000.0, "value maximum is the spike");
			assertTrue(columns[1]["quantization"].GetDouble() == 0.0, "column is not quantised");
			assertTrue(std::string(columns[3]["type"].GetString()) == "bool", "column 3 type");

			auto& segments = document["segments"];
			assertTrue(segments.IsArray() && (segments.Size() == 7), "info has seven segments");
			assertTrue(segments[2]["firstrow"].GetUint64() == 800, "segment first row");
			assertTrue(segments[2]["rowcount"].GetUint64() == 400, "segment row count");
			assertTrue(segments[2]["chunkcount"].GetUint64() == 4, "segment chunk count");
			assertTrue(segments[2]["available"].GetBool(), "segment is available");
			assertTrue(segments[6]["rowcount"].GetUint64() == 100, "last segment row count");
		}

		void testJSONData()
		{
			auto fixture = createFixture("queryjson");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());

			AMC::CDataCaptureQuery captureQuery(openCoreReader(fixture, "signal"));

			AMC::sDataCaptureQuery query;
			query.m_ColumnIdentifiers = { "n", "b", "v", "t" };
			query.m_bHasFromRow = true;
			query.m_nFromRow = 1298;
			query.m_bHasRowCount = true;
			query.m_nRowCount = 4;

			rapidjson::Document document;
			std::string sJSON;
			parseQueryJSON(captureQuery, query, document, sJSON);

			assertTrue(document["fromrow"].GetUint64() == 1298, "fromrow");
			assertTrue(document["rowcount"].GetUint64() == 4, "rowcount");
			assertTrue(std::string(document["downsample"].GetString()) == "none", "downsample mode");

			auto& columns = document["columns"];
			assertTrue(columns.IsArray() && (columns.Size() == 4), "four columns in request order");

			assertTrue(std::string(columns[0]["name"].GetString()) == "n", "first column is n");
			assertTrue(std::string(columns[0]["type"].GetString()) == "uint64", "counter type");
			assertTrue(columns[0]["values"].Size() == 4, "four values");
			assertTrue((columns[0]["values"][0].GetUint64() == 1298) && (columns[0]["values"][3].GetUint64() == 1301), "counter values across a chunk boundary");

			// The flag toggles at row 1300.
			assertTrue(columns[1]["values"][1].IsBool() && (columns[1]["values"][1].GetBool() == false), "bool value before the boundary");
			assertTrue(columns[1]["values"][2].GetBool() == true, "bool value after the boundary");

			assertTrue(columns[2]["values"][0].GetDouble() == 48.0, "float value");
			assertTrue(columns[3]["values"][3].GetUint64() == signalTimestamp(1301), "timestamp value");

			// The spike row
			query.m_ColumnIdentifiers = { "v" };
			query.m_nFromRow = SIGNAL_SPIKEROW;
			query.m_nRowCount = 1;
			parseQueryJSON(captureQuery, query, document, sJSON);
			assertTrue(document["columns"][0]["values"][0].GetDouble() == 5000.0, "spike value");

			// An empty range is a valid response.
			query.m_nRowCount = 0;
			parseQueryJSON(captureQuery, query, document, sJSON);
			assertTrue(document["columns"][0]["values"].Size() == 0, "empty range has no values");
		}

		void testJSONSpecialValues()
		{
			auto fixture = createFixture("queryspecial");

			UOptions pOptions(fixture.m_pBuildExecution->CreateDataCaptureOptions());
			auto pCapture = createCapture(fixture, "special", pOptions.get());
			pCapture->AddColumn("u", "Large integer", LibMCEnv::eDataTableColumnType::Uint64Column);
			pCapture->AddColumn("i", "Negative integer", LibMCEnv::eDataTableColumnType::Int64Column);
			pCapture->AddColumn("d", "Double", LibMCEnv::eDataTableColumnType::DoubleColumn);

			std::vector<uint64_t> unsignedValues = { 18000000000000000001ULL, 9007199254740993ULL };
			std::vector<int64_t> signedValues = { -9007199254740993LL, 7 };
			std::vector<double> doubleValues = { std::numeric_limits<double>::quiet_NaN(), 0.1 };
			{
				UChunk pChunk(pCapture->BeginChunk());
				pChunk->SetUint64ColumnValues(0, unsignedValues.size(), unsignedValues.data());
				pChunk->SetInt64ColumnValues(1, signedValues.size(), signedValues.data());
				pChunk->SetDoubleColumnValues(2, doubleValues.size(), doubleValues.data());
				pCapture->CommitChunk(pChunk.get());
			}
			pCapture->Finish();

			AMC::CDataCaptureQuery captureQuery(openCoreReader(fixture, "special"));
			AMC::sDataCaptureQuery query;

			rapidjson::Document document;
			std::string sJSON;
			parseQueryJSON(captureQuery, query, document, sJSON);

			// Integers beyond 2^53 are written digit by digit and not through a double.
			assertTrue(sJSON.find("18000000000000000001") != std::string::npos, "large unsigned integer is exact");
			assertTrue(sJSON.find("9007199254740993") != std::string::npos, "integer beyond the double precision is exact");
			assertTrue(sJSON.find("-9007199254740993") != std::string::npos, "negative integer beyond the double precision is exact");

			auto& columns = document["columns"];
			assertTrue(columns[0]["values"][0].GetUint64() == 18000000000000000001ULL, "large unsigned integer parses back");
			assertTrue(columns[1]["values"][0].GetInt64() == -9007199254740993LL, "negative integer parses back");
			assertTrue(columns[2]["values"][0].IsNull(), "NaN is null in JSON");
			assertTrue(columns[2]["values"][1].GetDouble() == 0.1, "double value reads back unchanged");

			// CSV prints the same exact integers.
			std::string sCSV = captureQuery.buildCSV(query);
			assertTrue(sCSV == "row;u;i;d\n0;18000000000000000001;-9007199254740993;nan\n1;9007199254740993;7;0.10000000000000001\n", "CSV with exact integers, got: " + sCSV);
		}

		void testJSONEnvelope()
		{
			auto fixture = createFixture("queryenvelope");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());

			AMC::CDataCaptureQuery captureQuery(openCoreReader(fixture, "signal"));

			AMC::sDataCaptureQuery query;
			query.m_ColumnIdentifiers = { "v", "n" };
			query.m_Downsample = AMC::eDataCaptureQueryDownsample::MinMax;
			query.m_nTargetPoints = 10;

			rapidjson::Document document;
			std::string sJSON;
			parseQueryJSON(captureQuery, query, document, sJSON);

			assertTrue(std::string(document["downsample"].GetString()) == "minmax", "downsample mode");
			assertTrue(document["count"].GetUint64() == 10, "bucket count");
			assertTrue(document["fromrow"].GetUint64() == 0, "default first row");
			assertTrue(document["rowcount"].GetUint64() == SIGNAL_ROWCOUNT, "default row count");

			auto& series = document["series"];
			assertTrue(series.IsArray() && (series.Size() == 2), "one series per column");
			assertTrue(std::string(series[0]["name"].GetString()) == "v", "series name");
			assertTrue(series[0]["available"].GetBool(), "series is available");

			auto& buckets = series[0]["buckets"];
			assertTrue(buckets.Size() == 10, "ten buckets");
			assertTrue(buckets[0]["row"].GetUint64() == 0, "first bucket row");
			assertTrue(buckets[4]["row"].GetUint64() == 1000, "bucket row is the first row of the bucket");
			assertTrue(buckets[4]["min"].GetDouble() == 0.0, "bucket minimum");
			assertTrue(buckets[4]["max"].GetDouble() == 5000.0, "the envelope preserves the spike");
			assertTrue(buckets[5]["max"].GetDouble() == 249.0, "bucket maximum without spike");

			auto& counterBuckets = series[1]["buckets"];
			assertTrue((counterBuckets[9]["min"].GetDouble() == 2250.0) && (counterBuckets[9]["max"].GetDouble() == 2499.0), "counter envelope of the last bucket");

			// Stride and average return one value per bucket.
			query.m_Downsample = AMC::eDataCaptureQueryDownsample::Average;
			parseQueryJSON(captureQuery, query, document, sJSON);
			assertTrue(std::string(document["downsample"].GetString()) == "average", "average mode");
			assertTrue(std::fabs(document["series"][0]["buckets"][0]["value"].GetDouble() - 124.5) < 1.0e-9, "bucket average");

			query.m_Downsample = AMC::eDataCaptureQueryDownsample::Stride;
			query.m_bHasFromRow = true;
			query.m_nFromRow = 100;
			query.m_bHasRowCount = true;
			query.m_nRowCount = 400;
			query.m_nTargetPoints = 4;
			parseQueryJSON(captureQuery, query, document, sJSON);
			auto& strideBuckets = document["series"][1]["buckets"];
			assertTrue((strideBuckets.Size() == 4) && (strideBuckets[0]["row"].GetUint64() == 100) && (strideBuckets[3]["row"].GetUint64() == 400), "stride bucket rows");
			assertTrue((strideBuckets[0]["value"].GetDouble() == 100.0) && (strideBuckets[3]["value"].GetDouble() == 400.0), "stride values");
		}

		void testCSV()
		{
			auto fixture = createFixture("querycsv");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());

			AMC::CDataCaptureQuery captureQuery(openCoreReader(fixture, "signal"));

			AMC::sDataCaptureQuery query;
			query.m_ColumnIdentifiers = { "t", "v", "b" };
			query.m_bHasFromRow = true;
			query.m_nFromRow = 1233;
			query.m_bHasRowCount = true;
			query.m_nRowCount = 3;

			std::string sExpected = "row;t;v;b\n";
			sExpected += "1233;" + std::to_string(signalTimestamp(1233)) + ";233;0\n";
			sExpected += "1234;" + std::to_string(signalTimestamp(1234)) + ";5000;0\n";
			sExpected += "1235;" + std::to_string(signalTimestamp(1235)) + ";235;0\n";

			std::string sCSV = captureQuery.buildCSV(query);
			assertTrue(sCSV == sExpected, "CSV of a row range, got: " + sCSV);

			// Downsampled CSV has a minimum and a maximum column per data column.
			AMC::sDataCaptureQuery envelopeQuery;
			envelopeQuery.m_ColumnIdentifiers = { "v" };
			envelopeQuery.m_Downsample = AMC::eDataCaptureQueryDownsample::MinMax;
			envelopeQuery.m_nTargetPoints = 5;

			std::string sEnvelopeCSV = captureQuery.buildCSV(envelopeQuery);
			assertTrue(sEnvelopeCSV == "row;v_min;v_max\n0;0;249\n500;0;249\n1000;0;5000\n1500;0;249\n2000;0;249\n", "CSV envelope, got: " + sEnvelopeCSV);

			AMC::sDataCaptureQuery averageQuery;
			averageQuery.m_ColumnIdentifiers = { "n" };
			averageQuery.m_bHasFromRow = true;
			averageQuery.m_nFromRow = 0;
			averageQuery.m_bHasRowCount = true;
			averageQuery.m_nRowCount = 4;
			averageQuery.m_Downsample = AMC::eDataCaptureQueryDownsample::Average;
			averageQuery.m_nTargetPoints = 2;
			assertTrue(captureQuery.buildCSV(averageQuery) == "row;n\n0;0.5\n2;2.5\n", "CSV of bucket averages");
		}

		void testBinary()
		{
			auto fixture = createFixture("querybinary");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());

			AMC::CDataCaptureQuery captureQuery(openCoreReader(fixture, "signal"));

			AMC::sDataCaptureQuery query;
			query.m_ColumnIdentifiers = { "n", "v" };
			query.m_bHasFromRow = true;
			query.m_nFromRow = 1230;
			query.m_bHasRowCount = true;
			query.m_nRowCount = 10;

			// One float64 array per column, one after the other.
			std::vector<uint8_t> buffer;
			captureQuery.buildBinary(query, false, buffer);
			assertTrue(buffer.size() == 2 * 10 * sizeof(double), "float64 buffer size");

			std::vector<double> doubles(20);
			memcpy(doubles.data(), buffer.data(), buffer.size());
			assertTrue((doubles[0] == 1230.0) && (doubles[9] == 1239.0), "first array is the counter column");
			assertTrue((doubles[10] == 230.0) && (doubles[14] == 5000.0), "second array is the value column");

			captureQuery.buildBinary(query, true, buffer);
			assertTrue(buffer.size() == 2 * 10 * sizeof(float), "float32 buffer size");
			std::vector<float> floats(20);
			memcpy(floats.data(), buffer.data(), buffer.size());
			assertTrue((floats[0] == 1230.0f) && (floats[14] == 5000.0f), "float32 values");

			// Downsampled: two values per bucket in MinMax mode.
			AMC::sDataCaptureQuery envelopeQuery;
			envelopeQuery.m_ColumnIdentifiers = { "v" };
			envelopeQuery.m_Downsample = AMC::eDataCaptureQueryDownsample::MinMax;
			envelopeQuery.m_nTargetPoints = 10;
			captureQuery.buildBinary(envelopeQuery, true, buffer);
			assertTrue(buffer.size() == 20 * sizeof(float), "envelope buffer holds minimum and maximum per bucket");
			memcpy(floats.data(), buffer.data(), buffer.size());
			assertTrue((floats[8] == 0.0f) && (floats[9] == 5000.0f), "envelope of the bucket with the spike");

			// Empty range
			query.m_nRowCount = 0;
			captureQuery.buildBinary(query, false, buffer);
			assertTrue(buffer.empty(), "empty range is an empty buffer");
		}

		void testSizeCap()
		{
			auto fixture = createFixture("querycap");
			auto pOptions = signalOptions(fixture);
			captureSignal(fixture, "signal", pOptions.get());

			// A cap of 1000 values stands in for the production limit.
			AMC::CDataCaptureQuery captureQuery(openCoreReader(fixture, "signal"), 1000);
			uint32_t nInvalidRange = (uint32_t)AMC::eDataTableFormatError::InvalidRange;

			// The whole capture: 4 columns of 2500 rows.
			AMC::sDataCaptureQuery unbounded;
			std::string sMessage;
			std::string sJSON;
			assertTrue(formatErrorOf([&]() { captureQuery.appendJSONMembers(unbounded, sJSON); }, &sMessage) == nInvalidRange, "unbounded JSON request is rejected");
			assertTrue((sMessage.find("rowcount") != std::string::npos) && (sMessage.find("downsample") != std::string::npos) && (sMessage.find("targetpoints") != std::string::npos), "the error names the parameters that bound the request: " + sMessage);
			assertTrue(sJSON.empty(), "nothing is written for a rejected request");

			std::vector<uint8_t> buffer;
			assertTrue(formatErrorOf([&]() { captureQuery.buildCSV(unbounded); }) == nInvalidRange, "unbounded CSV request is rejected");
			assertTrue(formatErrorOf([&]() { captureQuery.buildBinary(unbounded, false, buffer); }) == nInvalidRange, "unbounded binary request is rejected");

			// The cap counts values, which is rows times columns.
			AMC::sDataCaptureQuery oneColumn;
			oneColumn.m_ColumnIdentifiers = { "n" };
			oneColumn.m_bHasFromRow = true;
			oneColumn.m_nFromRow = 0;
			oneColumn.m_bHasRowCount = true;
			oneColumn.m_nRowCount = 1000;
			captureQuery.buildBinary(oneColumn, false, buffer);
			assertTrue(buffer.size() == 1000 * sizeof(double), "a request at the limit is served");

			oneColumn.m_nRowCount = 1001;
			assertTrue(formatErrorOf([&]() { captureQuery.buildBinary(oneColumn, false, buffer); }) == nInvalidRange, "one value above the limit is rejected");

			AMC::sDataCaptureQuery twoColumns = oneColumn;
			twoColumns.m_ColumnIdentifiers = { "n", "v" };
			twoColumns.m_nRowCount = 501;
			assertTrue(formatErrorOf([&]() { captureQuery.buildBinary(twoColumns, false, buffer); }) == nInvalidRange, "two columns of 501 rows are above the limit");
			twoColumns.m_nRowCount = 500;
			captureQuery.buildBinary(twoColumns, false, buffer);
			assertTrue(buffer.size() == 1000 * sizeof(double), "two columns of 500 rows are at the limit");

			// Downsampling bounds the response by the target points, whatever the capture length.
			AMC::sDataCaptureQuery downsampled;
			downsampled.m_Downsample = AMC::eDataCaptureQueryDownsample::MinMax;
			downsampled.m_nTargetPoints = 100;
			captureQuery.buildBinary(downsampled, false, buffer);
			assertTrue(buffer.size() == 4 * 200 * sizeof(double), "the downsampled whole capture is served");

			downsampled.m_nTargetPoints = 200;
			assertTrue(formatErrorOf([&]() { captureQuery.buildBinary(downsampled, false, buffer); }) == nInvalidRange, "a downsampled request above the limit is rejected as well");
		}

		void testDefaultsAndValidation()
		{
			auto fixture = createFixture("querydefaults");

			// Retention has deleted the oldest segments of this capture.
			auto pProbeOptions = signalOptions(fixture);
			auto pOptions = signalOptions(fixture, 3 * pProbeOptions->GetSegmentSizeInBytes());
			captureSignal(fixture, "signal", pOptions.get());

			auto pReader = openCoreReader(fixture, "signal");
			uint64_t nFirstAvailableRow = pReader->getFirstAvailableRow();
			assertTrue(nFirstAvailableRow > 0, "the capture has deleted rows");

			AMC::CDataCaptureQuery captureQuery(pReader);

			// Without a range the query covers everything that is still stored.
			AMC::sDataCaptureQuery query;
			query.m_ColumnIdentifiers = { "n" };

			std::vector<uint8_t> buffer;
			captureQuery.buildBinary(query, false, buffer);
			assertTrue(buffer.size() == (SIGNAL_ROWCOUNT - nFirstAvailableRow) * sizeof(double), "default range is the stored part of the capture");

			double dFirstValue = 0.0;
			memcpy(&dFirstValue, buffer.data(), sizeof(double));
			assertTrue(dFirstValue == (double)nFirstAvailableRow, "default range starts at the first available row");

			// An explicit range over deleted rows is reported.
			query.m_bHasFromRow = true;
			query.m_nFromRow = 0;
			assertTrue(formatErrorOf([&]() { captureQuery.buildBinary(query, false, buffer); }) == (uint32_t)AMC::eDataTableFormatError::SegmentPurged, "a range over deleted rows is reported");

			// Parameter validation
			uint32_t nInvalidParam = (uint32_t)AMC::eDataTableFormatError::InvalidParam;

			AMC::sDataCaptureQuery unknownColumn;
			unknownColumn.m_ColumnIdentifiers = { "missing" };
			assertTrue(formatErrorOf([&]() { captureQuery.buildBinary(unknownColumn, false, buffer); }) == (uint32_t)AMC::eDataTableFormatError::ColumnNotFound, "unknown column is rejected");

			AMC::sDataCaptureQuery beyondEnd;
			beyondEnd.m_bHasFromRow = true;
			beyondEnd.m_nFromRow = SIGNAL_ROWCOUNT + 1;
			assertTrue(formatErrorOf([&]() { captureQuery.buildBinary(beyondEnd, false, buffer); }) == (uint32_t)AMC::eDataTableFormatError::InvalidRange, "fromrow beyond the end is rejected");

			AMC::sDataCaptureQuery tooManyRows;
			tooManyRows.m_bHasFromRow = true;
			tooManyRows.m_nFromRow = 2400;
			tooManyRows.m_bHasRowCount = true;
			tooManyRows.m_nRowCount = 101;
			assertTrue(formatErrorOf([&]() { captureQuery.buildBinary(tooManyRows, false, buffer); }) == (uint32_t)AMC::eDataTableFormatError::InvalidRange, "rowcount beyond the end is rejected");

			AMC::sDataCaptureQuery zeroTargetPoints;
			zeroTargetPoints.m_Downsample = AMC::eDataCaptureQueryDownsample::MinMax;
			zeroTargetPoints.m_nTargetPoints = 0;
			assertTrue(formatErrorOf([&]() { captureQuery.buildBinary(zeroTargetPoints, false, buffer); }) == nInvalidParam, "zero target points are rejected");
			zeroTargetPoints.m_nTargetPoints = DATACAPTUREQUERY_MAXTARGETPOINTS + 1;
			assertTrue(formatErrorOf([&]() { captureQuery.buildBinary(zeroTargetPoints, false, buffer); }) == nInvalidParam, "too many target points are rejected");

			assertTrue(AMC::CDataCaptureQuery::parseDownsample("") == AMC::eDataCaptureQueryDownsample::None, "empty downsample is none");
			assertTrue(AMC::CDataCaptureQuery::parseDownsample("none") == AMC::eDataCaptureQueryDownsample::None, "none");
			assertTrue(AMC::CDataCaptureQuery::parseDownsample("stride") == AMC::eDataCaptureQueryDownsample::Stride, "stride");
			assertTrue(AMC::CDataCaptureQuery::parseDownsample("minmax") == AMC::eDataCaptureQueryDownsample::MinMax, "minmax");
			assertTrue(AMC::CDataCaptureQuery::parseDownsample("average") == AMC::eDataCaptureQueryDownsample::Average, "average");
			assertTrue(formatErrorOf([&]() { AMC::CDataCaptureQuery::parseDownsample("bogus"); }) == nInvalidParam, "unknown downsample mode is rejected");
		}

	};

}

#endif // __AMCTEST_UNITTEST_DATACAPTUREQUERY
