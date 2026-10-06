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

#ifndef __AMCTEST_UNITTEST_DATATABLEENCODING
#define __AMCTEST_UNITTEST_DATATABLEENCODING

#include "amc_unittests.hpp"
#include "amc_constants.hpp"
#include "amc_datatableformat.hpp"
#include "amc_datatableencoding.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace AMCUnitTest {

	class CUnitTestGroup_DataTableEncoding : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "DataTableEncoding";
		}

		void registerTests() override {
			registerTest("DeltaKnownAnswers", "Delta encoding produces the byte layout of Parquet DELTA_BINARY_PACKED", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableEncoding::testDeltaKnownAnswers, this));
			registerTest("DeltaRoundTrip", "Delta encoding round trip across block boundaries and value extremes", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableEncoding::testDeltaRoundTrip, this));
			registerTest("DeltaFixedRateClock", "A fixed-rate clock deltas to almost nothing", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableEncoding::testDeltaFixedRateClock, this));
			registerTest("DeltaCorruptData", "Corrupt and truncated delta data is rejected", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableEncoding::testDeltaCorruptData, this));
			registerTest("BitPack", "Bool bit packing round trip", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableEncoding::testBitPack, this));
			registerTest("Codecs", "LZ4 and deflate page compression", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableEncoding::testCodecs, this));
			registerTest("EncodedChunkRoundTrip", "A chunk with all encodings and codecs is read back", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableEncoding::testEncodedChunkRoundTrip, this));
			registerTest("FallbackToPlain", "Pages that do not get smaller stay plain and uncompressed", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableEncoding::testFallbackToPlain, this));
			registerTest("EncodingValidation", "Encodings that do not fit the column type are rejected on write and read", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_DataTableEncoding::testEncodingValidation, this));
		}

		void initializeTests() override {
		}

	private:

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

		AMC::CDataTableV2SegmentReader::ReadCallback memoryReader(const std::vector<uint8_t>& bytes)
		{
			return [&bytes](uint64_t nOffset, uint64_t nSize, uint8_t* pBuffer) {
				if ((nOffset > bytes.size()) || (nSize > bytes.size() - nOffset))
					throw std::out_of_range("segment reader read outside of the stream");
				if (nSize > 0)
					memcpy(pBuffer, bytes.data() + nOffset, (size_t)nSize);
			};
		}

		// Deterministic pseudo random numbers, so that the tests are reproducible.
		uint64_t nextRandom(uint64_t& nState)
		{
			nState = nState * 6364136223846793005ULL + 1442695040888963407ULL;
			return nState ^ (nState >> 29);
		}

		void assertDeltaRoundTrip(const std::vector<int64_t>& values, const std::string& sContext)
		{
			std::vector<uint8_t> encoded;
			AMC::CDataTableEncoding::encodeDelta(values.data(), values.size(), encoded);

			std::vector<int64_t> decoded(values.size(), -1);
			AMC::CDataTableEncoding::decodeDelta(encoded.data(), encoded.size(), decoded.data(), decoded.size());

			assertTrue(decoded == values, "delta round trip: " + sContext);
		}

		void testDeltaKnownAnswers()
		{
			// The examples of the Parquet format specification, with the block layout of this implementation:
			// blocks of 128 values in 4 miniblocks.
			{
				std::vector<int64_t> values = { 1, 2, 3, 4, 5 };
				std::vector<uint8_t> encoded;
				AMC::CDataTableEncoding::encodeDelta(values.data(), values.size(), encoded);

				// block size 128, 4 miniblocks, 5 values, first value 1 (zigzag 2),
				// block: min delta 1 (zigzag 2), bit widths 0 0 0 0, no miniblock data
				std::vector<uint8_t> expected = { 0x80, 0x01, 0x04, 0x05, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00 };
				assertTrue(encoded == expected, "constant delta sequence has the specified layout");
			}

			{
				std::vector<int64_t> values = { 7, 5, 3, 1, 2, 3, 4, 5 };
				std::vector<uint8_t> encoded;
				AMC::CDataTableEncoding::encodeDelta(values.data(), values.size(), encoded);

				// deltas -2 -2 -2 1 1 1 1, min delta -2 (zigzag 3), relative deltas 0 0 0 3 3 3 3 on 2 bits,
				// packed least significant bit first and padded to a full miniblock of 32 values
				std::vector<uint8_t> expected = { 0x80, 0x01, 0x04, 0x08, 0x0E, 0x03, 0x02, 0x00, 0x00, 0x00, 0xC0, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
				assertTrue(encoded == expected, "mixed delta sequence has the specified layout");
			}
		}

		void testDeltaRoundTrip()
		{
			assertDeltaRoundTrip({}, "empty");
			assertDeltaRoundTrip({ 42 }, "single value");
			assertDeltaRoundTrip({ -5, -5 }, "two equal values");
			assertDeltaRoundTrip({ (std::numeric_limits<int64_t>::min)(), (std::numeric_limits<int64_t>::max)(), 0, -1, 1 }, "value extremes with wrapping deltas");

			// Lengths around miniblock and block boundaries
			for (size_t nCount : { 2, 31, 32, 33, 34, 64, 65, 128, 129, 130, 161, 256, 257, 258, 1000, 4097 }) {
				std::vector<int64_t> ramp(nCount);
				for (size_t nIndex = 0; nIndex < nCount; nIndex++)
					ramp[nIndex] = 1000 + (int64_t)nIndex * 7;
				assertDeltaRoundTrip(ramp, "ramp of " + std::to_string(nCount));

				uint64_t nState = 12345 + nCount;
				std::vector<int64_t> noisy(nCount);
				int64_t nValue = 0;
				for (size_t nIndex = 0; nIndex < nCount; nIndex++) {
					nValue += (int64_t)(nextRandom(nState) % 2001) - 1000;
					noisy[nIndex] = nValue;
				}
				assertDeltaRoundTrip(noisy, "random walk of " + std::to_string(nCount));

				std::vector<int64_t> random(nCount);
				for (size_t nIndex = 0; nIndex < nCount; nIndex++)
					random[nIndex] = (int64_t)nextRandom(nState);
				assertDeltaRoundTrip(random, "full range random values of " + std::to_string(nCount));
			}
		}

		void testDeltaFixedRateClock()
		{
			// 65536 timestamps of a 100 kHz clock in microseconds
			std::vector<int64_t> timestamps(65536);
			for (size_t nIndex = 0; nIndex < timestamps.size(); nIndex++)
				timestamps[nIndex] = 1700000000000000LL + (int64_t)nIndex * 10;

			std::vector<uint8_t> encoded;
			AMC::CDataTableEncoding::encodeDelta(timestamps.data(), timestamps.size(), encoded);

			size_t nPlainSize = timestamps.size() * sizeof(int64_t);
			assertTrue(encoded.size() * 100 < nPlainSize, "fixed-rate clock encodes to less than one percent, got " + std::to_string(encoded.size()) + " of " + std::to_string(nPlainSize) + " bytes");

			std::vector<int64_t> decoded(timestamps.size());
			AMC::CDataTableEncoding::decodeDelta(encoded.data(), encoded.size(), decoded.data(), decoded.size());
			assertTrue(decoded == timestamps, "fixed-rate clock round trip");
		}

		void testDeltaCorruptData()
		{
			uint64_t nState = 99;
			std::vector<int64_t> values(300);
			for (auto& nValue : values)
				nValue = (int64_t)(nextRandom(nState) % 100000);

			std::vector<uint8_t> encoded;
			AMC::CDataTableEncoding::encodeDelta(values.data(), values.size(), encoded);

			std::vector<int64_t> decoded(values.size());
			uint32_t nCorrupt = (uint32_t)AMC::eDataTableFormatError::Corrupt;

			// Every truncation is rejected.
			for (size_t nCutSize = 0; nCutSize < encoded.size(); nCutSize++) {
				std::vector<uint8_t> cut(encoded.begin(), encoded.begin() + nCutSize);
				cut.reserve(1);
				uint32_t nError = formatErrorOf([&]() { AMC::CDataTableEncoding::decodeDelta(cut.data(), cut.size(), decoded.data(), decoded.size()); });
				assertTrue((nError == nCorrupt) || ((nCutSize == 0) && (nError == (uint32_t)AMC::eDataTableFormatError::InvalidParam)), "delta data cut at " + std::to_string(nCutSize) + " is rejected, got " + std::to_string(nError));
			}

			// A different number of values than the page directory states is rejected.
			std::vector<int64_t> tooFew(values.size() - 1);
			assertTrue(formatErrorOf([&]() { AMC::CDataTableEncoding::decodeDelta(encoded.data(), encoded.size(), tooFew.data(), tooFew.size()); }) == nCorrupt, "value count mismatch is rejected");

			// An invalid bit width is rejected. The first width byte follows the header and the minimum delta.
			{
				std::vector<int64_t> fewValues = { 1, 2, 3, 4, 5 };
				std::vector<uint8_t> smallEncoded;
				AMC::CDataTableEncoding::encodeDelta(fewValues.data(), fewValues.size(), smallEncoded);
				smallEncoded[6] = 65;
				std::vector<int64_t> smallDecoded(5);
				assertTrue(formatErrorOf([&]() { AMC::CDataTableEncoding::decodeDelta(smallEncoded.data(), smallEncoded.size(), smallDecoded.data(), smallDecoded.size()); }) == nCorrupt, "bit width above 64 is rejected");
			}

			// An invalid block layout is rejected.
			{
				std::vector<uint8_t> invalidLayout = { 0x00, 0x04, 0x01, 0x02 };
				std::vector<int64_t> one(1);
				assertTrue(formatErrorOf([&]() { AMC::CDataTableEncoding::decodeDelta(invalidLayout.data(), invalidLayout.size(), one.data(), one.size()); }) == nCorrupt, "zero block size is rejected");
			}
		}

		void testBitPack()
		{
			for (size_t nCount : { 0, 1, 7, 8, 9, 63, 64, 65, 1000 }) {
				std::vector<uint8_t> values(nCount);
				for (size_t nIndex = 0; nIndex < nCount; nIndex++)
					values[nIndex] = ((nIndex % 3) == 0) ? (uint8_t)((nIndex % 200) + 1) : 0;

				std::vector<uint8_t> packed;
				AMC::CDataTableEncoding::encodeBitPack(values.data(), values.size(), packed);
				assertTrue(packed.size() == (nCount + 7) / 8, "bit packed size of " + std::to_string(nCount) + " values");
				assertTrue(packed.size() == AMC::CDataTableEncoding::getBitPackSize(nCount), "bit packed size matches the prediction");

				std::vector<uint8_t> unpacked(nCount, 7);
				unpacked.reserve(1);
				packed.reserve(1);
				AMC::CDataTableEncoding::decodeBitPack(packed.data(), packed.size(), unpacked.data(), unpacked.size());
				for (size_t nIndex = 0; nIndex < nCount; nIndex++)
					assertTrue(unpacked[nIndex] == (((nIndex % 3) == 0) ? 1 : 0), "bit " + std::to_string(nIndex) + " of " + std::to_string(nCount));
			}

			// Least significant bit first
			std::vector<uint8_t> pattern = { 1, 0, 0, 0, 0, 0, 0, 1, 1 };
			std::vector<uint8_t> packed;
			AMC::CDataTableEncoding::encodeBitPack(pattern.data(), pattern.size(), packed);
			assertTrue((packed.size() == 2) && (packed[0] == 0x81) && (packed[1] == 0x01), "bits are packed least significant bit first");

			std::vector<uint8_t> unpacked(9);
			assertTrue(formatErrorOf([&]() { AMC::CDataTableEncoding::decodeBitPack(packed.data(), 1, unpacked.data(), unpacked.size()); }) == (uint32_t)AMC::eDataTableFormatError::Corrupt, "bit packed data of the wrong size is rejected");
		}

		void testCodecs()
		{
			// Compressible data: a slowly changing byte pattern
			std::vector<uint8_t> compressible(100000);
			for (size_t nIndex = 0; nIndex < compressible.size(); nIndex++)
				compressible[nIndex] = (uint8_t)((nIndex / 100) & 0xff);

			// Incompressible data
			uint64_t nState = 7;
			std::vector<uint8_t> random(100000);
			for (auto& nByte : random)
				nByte = (uint8_t)(nextRandom(nState) >> 24);

			uint32_t nCorrupt = (uint32_t)AMC::eDataTableFormatError::Corrupt;

			for (uint32_t nCodec : { (uint32_t)DATATABLE_V2_CODEC_LZ4, (uint32_t)DATATABLE_V2_CODEC_DEFLATE }) {
				std::string sCodec = "codec " + std::to_string(nCodec);

				std::vector<uint8_t> compressed;
				assertTrue(AMC::CDataTableEncoding::compress(nCodec, compressible.data(), compressible.size(), compressed), sCodec + " compresses compressible data");
				assertTrue(compressed.size() < compressible.size() / 10, sCodec + " shrinks the pattern by an order of magnitude");

				std::vector<uint8_t> decompressed(compressible.size());
				AMC::CDataTableEncoding::decompress(nCodec, compressed.data(), compressed.size(), decompressed.data(), decompressed.size());
				assertTrue(decompressed == compressible, sCodec + " round trip");

				// The decompressed size must match exactly.
				std::vector<uint8_t> tooSmall(compressible.size() - 1);
				assertTrue(formatErrorOf([&]() { AMC::CDataTableEncoding::decompress(nCodec, compressed.data(), compressed.size(), tooSmall.data(), tooSmall.size()); }) == nCorrupt, sCodec + " rejects a target that is too small");
				std::vector<uint8_t> tooLarge(compressible.size() + 1);
				assertTrue(formatErrorOf([&]() { AMC::CDataTableEncoding::decompress(nCodec, compressed.data(), compressed.size(), tooLarge.data(), tooLarge.size()); }) == nCorrupt, sCodec + " rejects a target that is too large");

				// Truncated compressed data
				assertTrue(formatErrorOf([&]() { AMC::CDataTableEncoding::decompress(nCodec, compressed.data(), compressed.size() / 2, decompressed.data(), decompressed.size()); }) == nCorrupt, sCodec + " rejects truncated data");

				// Data that does not get smaller is reported, so that the page is stored uncompressed.
				std::vector<uint8_t> notSmaller;
				assertFalse(AMC::CDataTableEncoding::compress(nCodec, random.data(), random.size(), notSmaller), sCodec + " does not shrink random data");
				assertFalse(AMC::CDataTableEncoding::compress(nCodec, random.data(), 0, notSmaller), sCodec + " does not compress empty data");
			}

			std::vector<uint8_t> unused;
			assertFalse(AMC::CDataTableEncoding::compress(DATATABLE_V2_CODEC_NONE, compressible.data(), compressible.size(), unused), "codec none never compresses");

			std::vector<uint8_t> copy(compressible.size());
			AMC::CDataTableEncoding::decompress(DATATABLE_V2_CODEC_NONE, compressible.data(), compressible.size(), copy.data(), copy.size());
			assertTrue(copy == compressible, "codec none copies");

			assertTrue(formatErrorOf([&]() { AMC::CDataTableEncoding::compress(9, compressible.data(), compressible.size(), unused); }) == (uint32_t)AMC::eDataTableFormatError::UnsupportedCodec, "unknown codec is rejected on compression");
			assertTrue(formatErrorOf([&]() { AMC::CDataTableEncoding::decompress(9, compressible.data(), compressible.size(), copy.data(), copy.size()); }) == (uint32_t)AMC::eDataTableFormatError::UnsupportedCodec, "unknown codec is rejected on decompression");
		}

		struct SSampleData {
			std::vector<uint64_t> m_Timestamps;
			std::vector<int32_t> m_Int32s;
			std::vector<uint32_t> m_Uint32s;
			std::vector<int64_t> m_Int64s;
			std::vector<uint64_t> m_Uint64s;
			std::vector<uint8_t> m_Bools;
			std::vector<float> m_Floats;
			std::vector<double> m_Quantized;
			std::vector<double> m_Doubles;
		};

		std::vector<AMC::sDataTableV2Column> sampleColumns()
		{
			std::vector<AMC::sDataTableV2Column> columns;
			columns.push_back({ "t", "Time", AMC::eDataTableV2DataType::Timestamp, DATATABLE_V2_ENCODING_DELTA, 0.0 });
			columns.push_back({ "i32", "Int32", AMC::eDataTableV2DataType::Int32, DATATABLE_V2_ENCODING_DELTA, 0.0 });
			columns.push_back({ "u32", "Uint32", AMC::eDataTableV2DataType::Uint32, DATATABLE_V2_ENCODING_DELTA, 0.0 });
			columns.push_back({ "i64", "Int64", AMC::eDataTableV2DataType::Int64, DATATABLE_V2_ENCODING_DELTA, 0.0 });
			columns.push_back({ "u64", "Uint64", AMC::eDataTableV2DataType::Uint64, DATATABLE_V2_ENCODING_DELTA, 0.0 });
			columns.push_back({ "b", "Bool", AMC::eDataTableV2DataType::Bool, DATATABLE_V2_ENCODING_BITPACK, 0.0 });
			columns.push_back({ "f", "Float", AMC::eDataTableV2DataType::Float, DATATABLE_V2_ENCODING_PLAIN, 0.0 });
			columns.push_back({ "q", "Quantised double", AMC::eDataTableV2DataType::Double, DATATABLE_V2_ENCODING_DELTA, 0.001 });
			columns.push_back({ "d", "Double", AMC::eDataTableV2DataType::Double, DATATABLE_V2_ENCODING_PLAIN, 0.0 });
			return columns;
		}

		SSampleData sampleData(uint32_t nRowCount)
		{
			SSampleData data;
			for (uint32_t nIndex = 0; nIndex < nRowCount; nIndex++) {
				data.m_Timestamps.push_back(1700000000000000ULL + (uint64_t)nIndex * 10);
				data.m_Int32s.push_back(-500 + (int32_t)(nIndex % 17) - (int32_t)nIndex);
				data.m_Uint32s.push_back(4000000000U + nIndex);
				data.m_Int64s.push_back(-5000000000LL + (int64_t)nIndex * 3);
				data.m_Uint64s.push_back(18000000000000000000ULL + nIndex);
				data.m_Bools.push_back(((nIndex / 50) % 2) ? 1 : 0);
				data.m_Floats.push_back(20.0f + (float)std::sin(nIndex * 0.01) * 5.0f);
				data.m_Quantized.push_back(1500.0 + std::sin(nIndex * 0.01) * 300.0);
				data.m_Doubles.push_back(std::cos(nIndex * 0.013) * 1.0e-3);
			}
			return data;
		}

		std::vector<const uint8_t*> columnPointers(const SSampleData& data)
		{
			return {
				(const uint8_t*)data.m_Timestamps.data(), (const uint8_t*)data.m_Int32s.data(), (const uint8_t*)data.m_Uint32s.data(),
				(const uint8_t*)data.m_Int64s.data(), (const uint8_t*)data.m_Uint64s.data(), data.m_Bools.data(),
				(const uint8_t*)data.m_Floats.data(), (const uint8_t*)data.m_Quantized.data(), (const uint8_t*)data.m_Doubles.data()
			};
		}

		template <typename T>
		std::vector<T> readColumn(AMC::CDataTableV2SegmentReader& reader, uint32_t nColumnIndex)
		{
			std::vector<T> values((size_t)reader.getRowCount());
			size_t nRowOffset = 0;
			for (uint32_t nChunkIndex = 0; nChunkIndex < reader.getChunkCount(); nChunkIndex++) {
				reader.readPage(nChunkIndex, nColumnIndex, (uint8_t*)(values.data() + nRowOffset), (values.size() - nRowOffset) * sizeof(T));
				nRowOffset += reader.getChunkRowCount(nChunkIndex);
			}
			return values;
		}

		void testEncodedChunkRoundTrip()
		{
			auto columns = sampleColumns();
			auto data1 = sampleData(1000);
			auto data2 = sampleData(300);

			size_t nPlainChunkSize = 1000 * (8 + 4 + 4 + 8 + 8 + 1 + 4 + 8 + 8);

			for (uint32_t nCodec : { (uint32_t)DATATABLE_V2_CODEC_NONE, (uint32_t)DATATABLE_V2_CODEC_LZ4, (uint32_t)DATATABLE_V2_CODEC_DEFLATE }) {
				std::string sCodec = "codec " + std::to_string(nCodec) + ": ";

				AMC::CDataTableV2ChunkEncoder encoder(columns, nCodec);
				AMC::CDataTableV2SegmentBuilder builder(columns, 0, 0, 1000);
				AMC::sDataTableV2EncodedChunk chunk;

				std::vector<uint8_t> bytes;
				builder.appendHeader(bytes);

				encoder.encodeChunk(columnPointers(data1), 1000, chunk);
				assertTrue(chunk.m_Bytes.size() < nPlainChunkSize / 2, sCodec + "encoded chunk is less than half of the plain size, got " + std::to_string(chunk.m_Bytes.size()));
				assertTrue(chunk.m_Bytes.size() <= builder.getChunkSizeInBytes(1000), sCodec + "encoded chunk is never larger than the plain chunk");
				builder.appendEncodedChunk(chunk, bytes);

				encoder.encodeChunk(columnPointers(data2), 300, chunk);
				builder.appendEncodedChunk(chunk, bytes);

				builder.appendFooter(bytes);
				assertTrue(builder.getCurrentSize() == bytes.size(), sCodec + "builder size is the segment size");

				AMC::CDataTableV2SegmentReader reader(memoryReader(bytes), bytes.size());
				assertTrue(reader.getRowCount() == 1300, sCodec + "row count");
				assertTrue(reader.getChunkCount() == 2, sCodec + "chunk count");

				// The schema records encoding and quantisation.
				assertTrue(reader.getColumn(0).m_nEncoding == DATATABLE_V2_ENCODING_DELTA, sCodec + "timestamp column encoding in the schema");
				assertTrue(reader.getColumn(5).m_nEncoding == DATATABLE_V2_ENCODING_BITPACK, sCodec + "bool column encoding in the schema");
				assertTrue(reader.getColumn(7).m_dQuantizationStep == 0.001, sCodec + "quantisation step in the schema");
				assertTrue(reader.getColumn(8).m_dQuantizationStep == 0.0, sCodec + "plain double column is not quantised");

				// The page directory records what was actually stored.
				auto& timestampEntry = reader.getPageEntry(0, 0);
				assertTrue(timestampEntry.m_nEncoding == DATATABLE_V2_ENCODING_DELTA, sCodec + "timestamp page is delta encoded");
				assertTrue(timestampEntry.m_nUncompressedSize < 100, sCodec + "fixed-rate timestamp page is tiny, got " + std::to_string(timestampEntry.m_nUncompressedSize));
				assertTrue(timestampEntry.m_nMinValue == 1700000000000000ULL, sCodec + "timestamp page minimum");
				assertTrue(timestampEntry.m_nMaxValue == 1700000000000000ULL + 9990, sCodec + "timestamp page maximum");

				auto& boolEntry = reader.getPageEntry(0, 5);
				assertTrue(boolEntry.m_nEncoding == DATATABLE_V2_ENCODING_BITPACK, sCodec + "bool page is bit packed");
				assertTrue(boolEntry.m_nUncompressedSize == 125, sCodec + "bool page holds one bit per row");

				auto& floatEntry = reader.getPageEntry(0, 6);
				assertTrue(floatEntry.m_nEncoding == DATATABLE_V2_ENCODING_PLAIN, sCodec + "float page is plain");
				assertTrue(floatEntry.m_nUncompressedSize == 4000, sCodec + "float page size");
				if (nCodec == DATATABLE_V2_CODEC_NONE)
					assertTrue(floatEntry.m_nCodec == DATATABLE_V2_CODEC_NONE, sCodec + "float page is uncompressed");

				auto& quantizedEntry = reader.getPageEntry(0, 7);
				assertTrue(quantizedEntry.m_nEncoding == DATATABLE_V2_ENCODING_DELTA, sCodec + "quantised page is delta encoded");
				assertTrue(quantizedEntry.m_nUncompressedSize < 4000, sCodec + "quantised page is less than half of the plain doubles");

				// Exact columns
				std::vector<uint64_t> expectedTimestamps = data1.m_Timestamps;
				expectedTimestamps.insert(expectedTimestamps.end(), data2.m_Timestamps.begin(), data2.m_Timestamps.end());
				assertTrue(readColumn<uint64_t>(reader, 0) == expectedTimestamps, sCodec + "timestamp column is exact");

				std::vector<int32_t> expectedInt32s = data1.m_Int32s;
				expectedInt32s.insert(expectedInt32s.end(), data2.m_Int32s.begin(), data2.m_Int32s.end());
				assertTrue(readColumn<int32_t>(reader, 1) == expectedInt32s, sCodec + "int32 column is exact");

				std::vector<uint32_t> expectedUint32s = data1.m_Uint32s;
				expectedUint32s.insert(expectedUint32s.end(), data2.m_Uint32s.begin(), data2.m_Uint32s.end());
				assertTrue(readColumn<uint32_t>(reader, 2) == expectedUint32s, sCodec + "uint32 column is exact");

				std::vector<int64_t> expectedInt64s = data1.m_Int64s;
				expectedInt64s.insert(expectedInt64s.end(), data2.m_Int64s.begin(), data2.m_Int64s.end());
				assertTrue(readColumn<int64_t>(reader, 3) == expectedInt64s, sCodec + "int64 column is exact");

				std::vector<uint64_t> expectedUint64s = data1.m_Uint64s;
				expectedUint64s.insert(expectedUint64s.end(), data2.m_Uint64s.begin(), data2.m_Uint64s.end());
				assertTrue(readColumn<uint64_t>(reader, 4) == expectedUint64s, sCodec + "uint64 column is exact");

				std::vector<uint8_t> expectedBools = data1.m_Bools;
				expectedBools.insert(expectedBools.end(), data2.m_Bools.begin(), data2.m_Bools.end());
				assertTrue(readColumn<uint8_t>(reader, 5) == expectedBools, sCodec + "bool column is exact");

				std::vector<float> expectedFloats = data1.m_Floats;
				expectedFloats.insert(expectedFloats.end(), data2.m_Floats.begin(), data2.m_Floats.end());
				assertTrue(readColumn<float>(reader, 6) == expectedFloats, sCodec + "float column is bit-exact");

				std::vector<double> expectedDoubles = data1.m_Doubles;
				expectedDoubles.insert(expectedDoubles.end(), data2.m_Doubles.begin(), data2.m_Doubles.end());
				assertTrue(readColumn<double>(reader, 8) == expectedDoubles, sCodec + "unquantised double column is bit-exact");

				// The quantised column is exact to half a step and holds integer multiples of the step.
				auto quantized = readColumn<double>(reader, 7);
				for (size_t nIndex = 0; nIndex < 1000; nIndex++) {
					double dOriginal = data1.m_Quantized[nIndex];
					assertTrue(std::fabs(quantized[nIndex] - dOriginal) <= 0.0005 + 1.0e-9, sCodec + "quantised value " + std::to_string(nIndex) + " is within half a step");
					assertTrue(quantized[nIndex] == (double)std::llround(dOriginal / 0.001) * 0.001, sCodec + "quantised value " + std::to_string(nIndex) + " is a multiple of the step");
				}

				// The statistics of the quantised page describe the values as they are read back.
				double dPageMin = AMC::CDataTableV2::statisticsValueToDouble(AMC::eDataTableV2DataType::Double, quantizedEntry.m_nMinValue);
				double dPageMax = AMC::CDataTableV2::statisticsValueToDouble(AMC::eDataTableV2DataType::Double, quantizedEntry.m_nMaxValue);
				double dReadMin = quantized[0];
				double dReadMax = quantized[0];
				for (size_t nIndex = 0; nIndex < 1000; nIndex++) {
					if (quantized[nIndex] < dReadMin)
						dReadMin = quantized[nIndex];
					if (quantized[nIndex] > dReadMax)
						dReadMax = quantized[nIndex];
				}
				assertTrue((dPageMin == dReadMin) && (dPageMax == dReadMax), sCodec + "quantised page statistics match the values read back");

				assertTrue(reader.getPageReadCount() == 18, sCodec + "every page was read once");
			}
		}

		void testFallbackToPlain()
		{
			uint64_t nState = 4711;

			std::vector<AMC::sDataTableV2Column> columns;
			columns.push_back({ "r", "Random", AMC::eDataTableV2DataType::Int64, DATATABLE_V2_ENCODING_DELTA, 0.0 });
			columns.push_back({ "q", "Quantised with NaN", AMC::eDataTableV2DataType::Double, DATATABLE_V2_ENCODING_DELTA, 0.5 });
			columns.push_back({ "b", "Bool", AMC::eDataTableV2DataType::Bool, DATATABLE_V2_ENCODING_BITPACK, 0.0 });

			// Full range random integers have deltas as wide as the values.
			std::vector<int64_t> random(200);
			for (auto& nValue : random)
				nValue = (int64_t)nextRandom(nState);

			// A value that can not be quantised keeps the page plain and bit-exact.
			std::vector<double> withNaN(200, 1.26);
			withNaN[17] = std::numeric_limits<double>::quiet_NaN();

			std::vector<uint8_t> bools(200, 1);

			AMC::CDataTableV2ChunkEncoder encoder(columns, DATATABLE_V2_CODEC_LZ4);
			AMC::CDataTableV2SegmentBuilder builder(columns, 0, 0, 200);
			AMC::sDataTableV2EncodedChunk chunk;

			std::vector<uint8_t> bytes;
			builder.appendHeader(bytes);
			encoder.encodeChunk({ (const uint8_t*)random.data(), (const uint8_t*)withNaN.data(), bools.data() }, 200, chunk);
			builder.appendEncodedChunk(chunk, bytes);

			// A single bool does not get smaller by bit packing.
			encoder.encodeChunk({ (const uint8_t*)random.data(), (const uint8_t*)withNaN.data(), bools.data() }, 1, chunk);
			builder.appendEncodedChunk(chunk, bytes);
			builder.appendFooter(bytes);

			AMC::CDataTableV2SegmentReader reader(memoryReader(bytes), bytes.size());

			auto& randomEntry = reader.getPageEntry(0, 0);
			assertTrue(randomEntry.m_nEncoding == DATATABLE_V2_ENCODING_PLAIN, "random integers fall back to plain");
			assertTrue(randomEntry.m_nCodec == DATATABLE_V2_CODEC_NONE, "random integers stay uncompressed");
			assertTrue(randomEntry.m_nCompressedSize == 1600, "random integer page has the plain size");

			auto& nanEntry = reader.getPageEntry(0, 1);
			assertTrue(nanEntry.m_nEncoding == DATATABLE_V2_ENCODING_PLAIN, "a page with NaN is not quantised");

			assertTrue(reader.getPageEntry(0, 2).m_nEncoding == DATATABLE_V2_ENCODING_BITPACK, "bool page of 200 rows is bit packed");
			assertTrue(reader.getPageEntry(1, 2).m_nEncoding == DATATABLE_V2_ENCODING_PLAIN, "bool page of one row stays plain");

			auto randomOut = readColumn<int64_t>(reader, 0);
			assertTrue(std::vector<int64_t>(randomOut.begin(), randomOut.begin() + 200) == random, "plain fallback is exact");

			auto nanOut = readColumn<double>(reader, 1);
			assertTrue(nanOut[0] == 1.26, "unquantised fallback page is bit-exact");
			assertTrue(std::isnan(nanOut[17]), "NaN survives in the fallback page");

			// Without the NaN the same column is quantised.
			withNaN[17] = 1.26;
			std::vector<uint8_t> quantizedBytes;
			AMC::CDataTableV2SegmentBuilder quantizedBuilder(columns, 0, 0, 200);
			quantizedBuilder.appendHeader(quantizedBytes);
			quantizedBuilder.appendChunk({ (const uint8_t*)random.data(), (const uint8_t*)withNaN.data(), bools.data() }, 200, quantizedBytes);
			quantizedBuilder.appendFooter(quantizedBytes);

			AMC::CDataTableV2SegmentReader quantizedReader(memoryReader(quantizedBytes), quantizedBytes.size());
			assertTrue(quantizedReader.getPageEntry(0, 1).m_nEncoding == DATATABLE_V2_ENCODING_DELTA, "the page is quantised without the NaN");
			assertTrue(readColumn<double>(quantizedReader, 1)[0] == 1.5, "1.26 is stored as the nearest multiple of 0.5");
		}

		void testEncodingValidation()
		{
			uint32_t nUnsupportedEncoding = (uint32_t)AMC::eDataTableFormatError::UnsupportedEncoding;

			auto encoderError = [&](AMC::eDataTableV2DataType dataType, uint32_t nEncoding, double dStep) {
				std::vector<AMC::sDataTableV2Column> columns;
				columns.push_back({ "x", "X", dataType, nEncoding, dStep });
				return formatErrorOf([&]() { AMC::CDataTableV2ChunkEncoder encoder(columns, DATATABLE_V2_CODEC_NONE); });
			};

			assertTrue(encoderError(AMC::eDataTableV2DataType::Float, DATATABLE_V2_ENCODING_DELTA, 0.0) == nUnsupportedEncoding, "delta on a float column is rejected");
			assertTrue(encoderError(AMC::eDataTableV2DataType::Double, DATATABLE_V2_ENCODING_DELTA, 0.0) == nUnsupportedEncoding, "delta on an unquantised double column is rejected");
			assertTrue(encoderError(AMC::eDataTableV2DataType::Int32, DATATABLE_V2_ENCODING_BITPACK, 0.0) == nUnsupportedEncoding, "bit packing on an integer column is rejected");
			assertTrue(encoderError(AMC::eDataTableV2DataType::Bool, DATATABLE_V2_ENCODING_DELTA, 0.0) == nUnsupportedEncoding, "delta on a bool column is rejected");
			assertTrue(encoderError(AMC::eDataTableV2DataType::Int32, 42, 0.0) == nUnsupportedEncoding, "unknown encoding is rejected");
			assertTrue(encoderError(AMC::eDataTableV2DataType::Double, DATATABLE_V2_ENCODING_DELTA, -1.0) == (uint32_t)AMC::eDataTableFormatError::InvalidParam, "negative quantisation step is rejected");
			assertTrue(encoderError(AMC::eDataTableV2DataType::Double, DATATABLE_V2_ENCODING_DELTA, 0.01) == 0, "delta on a quantised double column is accepted");
			assertTrue(encoderError(AMC::eDataTableV2DataType::Timestamp, DATATABLE_V2_ENCODING_DELTA, 0.0) == 0, "delta on a timestamp column is accepted");

			assertTrue(AMC::CDataTableV2::getDefaultEncoding(AMC::eDataTableV2DataType::Timestamp) == DATATABLE_V2_ENCODING_DELTA, "timestamps default to delta");
			assertTrue(AMC::CDataTableV2::getDefaultEncoding(AMC::eDataTableV2DataType::Uint32) == DATATABLE_V2_ENCODING_DELTA, "integers default to delta");
			assertTrue(AMC::CDataTableV2::getDefaultEncoding(AMC::eDataTableV2DataType::Bool) == DATATABLE_V2_ENCODING_BITPACK, "bools default to bit packing");
			assertTrue(AMC::CDataTableV2::getDefaultEncoding(AMC::eDataTableV2DataType::Float) == DATATABLE_V2_ENCODING_PLAIN, "floats default to plain");
			assertTrue(AMC::CDataTableV2::getDefaultEncoding(AMC::eDataTableV2DataType::Double) == DATATABLE_V2_ENCODING_PLAIN, "doubles default to plain");

			// A directory that claims a delta page in a float column is rejected by the reader.
			std::vector<AMC::sDataTableV2Column> columns;
			columns.push_back({ "f", "Float", AMC::eDataTableV2DataType::Float });
			std::vector<float> floats(10, 1.0f);

			AMC::CDataTableV2SegmentBuilder builder(columns, 0, 0, 10);
			std::vector<uint8_t> bytes;
			builder.appendHeader(bytes);
			builder.appendChunk({ (const uint8_t*)floats.data() }, 10, bytes);
			builder.appendFooter(bytes);

			AMC::sDataTableV2Trailer trailer;
			memcpy(&trailer, bytes.data() + bytes.size() - sizeof(trailer), sizeof(trailer));
			size_t nEntryOffset = bytes.size() - sizeof(trailer) - sizeof(AMC::sDataTableV2PageEntry);

			AMC::sDataTableV2PageEntry entry;
			memcpy(&entry, bytes.data() + nEntryOffset, sizeof(entry));
			entry.m_nEncoding = DATATABLE_V2_ENCODING_DELTA;
			memcpy(bytes.data() + nEntryOffset, &entry, sizeof(entry));

			assertTrue(formatErrorOf([&]() { AMC::CDataTableV2SegmentReader reader(memoryReader(bytes), bytes.size()); }) == (uint32_t)AMC::eDataTableFormatError::Corrupt, "delta page in a float column is rejected on read");

			entry.m_nEncoding = DATATABLE_V2_ENCODING_BITPACK;
			memcpy(bytes.data() + nEntryOffset, &entry, sizeof(entry));
			assertTrue(formatErrorOf([&]() { AMC::CDataTableV2SegmentReader reader(memoryReader(bytes), bytes.size()); }) == (uint32_t)AMC::eDataTableFormatError::Corrupt, "bit packed page in a float column is rejected on read");
		}

	};

}

#endif // __AMCTEST_UNITTEST_DATATABLEENCODING
