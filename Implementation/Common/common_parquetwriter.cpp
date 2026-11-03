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

Abstract:

common_parquetwriter.cpp implements a writer for flat Apache Parquet files.
The file metadata is serialized with the Thrift compact protocol, see
https://github.com/apache/parquet-format/blob/master/src/main/thrift/parquet.thrift

*/

#include "common_parquetwriter.hpp"
#include "Libraries/zlib/zlib.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#define PARQUETWRITER_MAGIC "PAR1"
#define PARQUETWRITER_MAGICLENGTH 4
#define PARQUETWRITER_FORMATVERSION 1
#define PARQUETWRITER_DEFAULTCREATEDBY "Autodesk Machine Control Framework version 1.0.0"

// Thrift compact protocol element types
#define THRIFTCOMPACT_TYPE_I32 5
#define THRIFTCOMPACT_TYPE_I64 6
#define THRIFTCOMPACT_TYPE_BINARY 8
#define THRIFTCOMPACT_TYPE_LIST 9
#define THRIFTCOMPACT_TYPE_STRUCT 12

// Parquet format enums
#define PARQUET_PAGETYPE_DATAPAGE 0
#define PARQUET_ENCODING_PLAIN 0
#define PARQUET_ENCODING_RLE 3
#define PARQUET_REPETITION_REQUIRED 0

namespace AMCCommon {

	namespace {

		class CThriftCompactWriter {
		private:
			std::vector<uint8_t>& m_Buffer;
			std::vector<int16_t> m_FieldIDStack;
			int16_t m_nLastFieldID;

		public:

			CThriftCompactWriter(std::vector<uint8_t>& buffer)
				: m_Buffer(buffer), m_nLastFieldID(0)
			{
			}

			void writeVarint(uint64_t nValue)
			{
				while (nValue >= 0x80) {
					m_Buffer.push_back((uint8_t)((nValue & 0x7f) | 0x80));
					nValue >>= 7;
				}
				m_Buffer.push_back((uint8_t)nValue);
			}

			void writeI32(int32_t nValue)
			{
				writeVarint((uint32_t)(((uint32_t)nValue << 1) ^ (uint32_t)(nValue >> 31)));
			}

			void writeI64(int64_t nValue)
			{
				writeVarint((((uint64_t)nValue) << 1) ^ (uint64_t)(nValue >> 63));
			}

			void writeBinary(const uint8_t* pData, size_t nSize)
			{
				writeVarint(nSize);
				if (nSize > 0)
					m_Buffer.insert(m_Buffer.end(), pData, pData + nSize);
			}

			void writeString(const std::string& sValue)
			{
				writeBinary((const uint8_t*)sValue.data(), sValue.size());
			}

			void writeFieldHeader(int16_t nFieldID, uint8_t nType)
			{
				int32_t nDelta = (int32_t)nFieldID - (int32_t)m_nLastFieldID;
				if ((nDelta > 0) && (nDelta <= 15)) {
					m_Buffer.push_back((uint8_t)((nDelta << 4) | nType));
				}
				else {
					m_Buffer.push_back(nType);
					writeI32(nFieldID);
				}
				m_nLastFieldID = nFieldID;
			}

			void writeListHeader(uint8_t nElementType, size_t nSize)
			{
				if (nSize < 15) {
					m_Buffer.push_back((uint8_t)((nSize << 4) | nElementType));
				}
				else {
					m_Buffer.push_back((uint8_t)(0xf0 | nElementType));
					writeVarint(nSize);
				}
			}

			void beginStruct()
			{
				m_FieldIDStack.push_back(m_nLastFieldID);
				m_nLastFieldID = 0;
			}

			void endStruct()
			{
				if (m_FieldIDStack.empty())
					throw std::runtime_error("thrift struct stack underflow");

				m_Buffer.push_back(0);
				m_nLastFieldID = m_FieldIDStack.back();
				m_FieldIDStack.pop_back();
			}

			void writeFieldI32(int16_t nFieldID, int32_t nValue)
			{
				writeFieldHeader(nFieldID, THRIFTCOMPACT_TYPE_I32);
				writeI32(nValue);
			}

			void writeFieldI64(int16_t nFieldID, int64_t nValue)
			{
				writeFieldHeader(nFieldID, THRIFTCOMPACT_TYPE_I64);
				writeI64(nValue);
			}

			void writeFieldBinary(int16_t nFieldID, const std::vector<uint8_t>& data)
			{
				writeFieldHeader(nFieldID, THRIFTCOMPACT_TYPE_BINARY);
				writeBinary(data.data(), data.size());
			}

			void writeFieldString(int16_t nFieldID, const std::string& sValue)
			{
				writeFieldHeader(nFieldID, THRIFTCOMPACT_TYPE_BINARY);
				writeString(sValue);
			}

			void writeFieldListHeader(int16_t nFieldID, uint8_t nElementType, size_t nSize)
			{
				writeFieldHeader(nFieldID, THRIFTCOMPACT_TYPE_LIST);
				writeListHeader(nElementType, nSize);
			}

			void beginFieldStruct(int16_t nFieldID)
			{
				writeFieldHeader(nFieldID, THRIFTCOMPACT_TYPE_STRUCT);
				beginStruct();
			}
		};

		void encodeUint32LE(uint8_t* pTarget, uint32_t nValue)
		{
			for (uint32_t nIndex = 0; nIndex < 4; nIndex++)
				pTarget[nIndex] = (uint8_t)(nValue >> (nIndex * 8));
		}

		void encodeUint64LE(uint8_t* pTarget, uint64_t nValue)
		{
			for (uint32_t nIndex = 0; nIndex < 8; nIndex++)
				pTarget[nIndex] = (uint8_t)(nValue >> (nIndex * 8));
		}

		uint32_t decodeUint32LE(const uint8_t* pSource)
		{
			uint32_t nValue = 0;
			for (uint32_t nIndex = 0; nIndex < 4; nIndex++)
				nValue |= ((uint32_t)pSource[nIndex]) << (nIndex * 8);
			return nValue;
		}

		uint64_t decodeUint64LE(const uint8_t* pSource)
		{
			uint64_t nValue = 0;
			for (uint32_t nIndex = 0; nIndex < 8; nIndex++)
				nValue |= ((uint64_t)pSource[nIndex]) << (nIndex * 8);
			return nValue;
		}

		void appendMagic(std::vector<uint8_t>& buffer)
		{
			const char* pMagic = PARQUETWRITER_MAGIC;
			buffer.insert(buffer.end(), pMagic, pMagic + PARQUETWRITER_MAGICLENGTH);
		}

		void compressGZIP(const uint8_t* pData, size_t nSize, std::vector<uint8_t>& compressedData)
		{
			if (nSize > (size_t)std::numeric_limits<uInt>::max())
				throw std::runtime_error("parquet page too large for compression");

			z_stream stream;
			memset(&stream, 0, sizeof(stream));
			// windowBits 15 + 16 selects the gzip wrapper, which is what the Parquet GZIP codec expects.
			if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
				throw std::runtime_error("could not initialize gzip compression");

			compressedData.resize((size_t)deflateBound(&stream, (uLong)nSize) + 64);
			stream.next_in = (Bytef*)pData;
			stream.avail_in = (uInt)nSize;

			int nResult = Z_OK;
			while (nResult == Z_OK) {
				if (stream.total_out >= compressedData.size())
					compressedData.resize(compressedData.size() * 2);

				stream.next_out = compressedData.data() + stream.total_out;
				stream.avail_out = (uInt)(compressedData.size() - stream.total_out);
				nResult = deflate(&stream, Z_FINISH);
			}

			size_t nCompressedSize = (size_t)stream.total_out;
			deflateEnd(&stream);

			if (nResult != Z_STREAM_END)
				throw std::runtime_error("gzip compression failed");

			compressedData.resize(nCompressedSize);
		}

		template <typename T> void updateMinMax(T value, bool& bHasValue, T& minValue, T& maxValue)
		{
			if (!bHasValue) {
				minValue = value;
				maxValue = value;
				bHasValue = true;
			}
			else {
				if (value < minValue)
					minValue = value;
				if (value > maxValue)
					maxValue = value;
			}
		}

		// Min/max of a chunk in PLAIN encoding. NaN values are ignored; a chunk with only NaN values has
		// no statistics. Zero bounds are written as -0.0 (min) and +0.0 (max), as the format requires.
		bool computeStatistics(eParquetPhysicalType physicalType, const uint8_t* pData, uint64_t nCount, std::vector<uint8_t>& minValue, std::vector<uint8_t>& maxValue)
		{
			uint32_t nValueSize = CParquetWriter::getValueSize(physicalType);
			minValue.resize(nValueSize);
			maxValue.resize(nValueSize);
			bool bHasValue = false;

			switch (physicalType) {
			case eParquetPhysicalType::Int32: {
				int32_t nMin = 0, nMax = 0;
				for (uint64_t nIndex = 0; nIndex < nCount; nIndex++)
					updateMinMax((int32_t)decodeUint32LE(pData + nIndex * 4), bHasValue, nMin, nMax);
				encodeUint32LE(minValue.data(), (uint32_t)nMin);
				encodeUint32LE(maxValue.data(), (uint32_t)nMax);
				break;
			}

			case eParquetPhysicalType::Int64: {
				int64_t nMin = 0, nMax = 0;
				for (uint64_t nIndex = 0; nIndex < nCount; nIndex++)
					updateMinMax((int64_t)decodeUint64LE(pData + nIndex * 8), bHasValue, nMin, nMax);
				encodeUint64LE(minValue.data(), (uint64_t)nMin);
				encodeUint64LE(maxValue.data(), (uint64_t)nMax);
				break;
			}

			case eParquetPhysicalType::Float: {
				float fMin = 0.0f, fMax = 0.0f;
				for (uint64_t nIndex = 0; nIndex < nCount; nIndex++) {
					uint32_t nBits = decodeUint32LE(pData + nIndex * 4);
					float fValue;
					memcpy(&fValue, &nBits, sizeof(fValue));
					if (!std::isnan(fValue))
						updateMinMax(fValue, bHasValue, fMin, fMax);
				}
				if (fMin == 0.0f)
					fMin = -0.0f;
				if (fMax == 0.0f)
					fMax = 0.0f;

				uint32_t nMinBits, nMaxBits;
				memcpy(&nMinBits, &fMin, sizeof(nMinBits));
				memcpy(&nMaxBits, &fMax, sizeof(nMaxBits));
				encodeUint32LE(minValue.data(), nMinBits);
				encodeUint32LE(maxValue.data(), nMaxBits);
				break;
			}

			case eParquetPhysicalType::Double: {
				double dMin = 0.0, dMax = 0.0;
				for (uint64_t nIndex = 0; nIndex < nCount; nIndex++) {
					uint64_t nBits = decodeUint64LE(pData + nIndex * 8);
					double dValue;
					memcpy(&dValue, &nBits, sizeof(dValue));
					if (!std::isnan(dValue))
						updateMinMax(dValue, bHasValue, dMin, dMax);
				}
				if (dMin == 0.0)
					dMin = -0.0;
				if (dMax == 0.0)
					dMax = 0.0;

				uint64_t nMinBits, nMaxBits;
				memcpy(&nMinBits, &dMin, sizeof(nMinBits));
				memcpy(&nMaxBits, &dMax, sizeof(nMaxBits));
				encodeUint64LE(minValue.data(), nMinBits);
				encodeUint64LE(maxValue.data(), nMaxBits);
				break;
			}

			default:
				throw std::runtime_error("invalid parquet physical type");
			}

			return bHasValue;
		}

		struct sParquetChunkInfo {
			uint64_t m_nDataPageOffset;
			uint64_t m_nUncompressedSize;
			uint64_t m_nCompressedSize;
			uint64_t m_nValueCount;
			bool m_bHasStatistics;
			std::vector<uint8_t> m_MinValue;
			std::vector<uint8_t> m_MaxValue;
		};

		struct sParquetRowGroupInfo {
			uint64_t m_nRowCount;
			uint64_t m_nFileOffset;
			uint64_t m_nTotalByteSize;
			uint64_t m_nTotalCompressedSize;
			std::vector<sParquetChunkInfo> m_Chunks;
		};

	}


	CParquetWriter::CParquetWriter(eParquetCompressionCodec codec, uint64_t nRowGroupSize)
		: m_Codec(codec), m_nRowGroupSize(nRowGroupSize), m_nRowCount(0), m_sCreatedBy(PARQUETWRITER_DEFAULTCREATEDBY)
	{
		if ((codec != eParquetCompressionCodec::Uncompressed) && (codec != eParquetCompressionCodec::GZIP))
			throw std::runtime_error("unsupported parquet compression codec");

		if ((nRowGroupSize == 0) || (nRowGroupSize > PARQUETWRITER_MAXROWGROUPSIZE))
			throw std::runtime_error("invalid parquet row group size");
	}

	CParquetWriter::~CParquetWriter()
	{
	}

	uint32_t CParquetWriter::getValueSize(eParquetPhysicalType physicalType)
	{
		switch (physicalType) {
		case eParquetPhysicalType::Int32: return 4;
		case eParquetPhysicalType::Int64: return 8;
		case eParquetPhysicalType::Float: return 4;
		case eParquetPhysicalType::Double: return 8;
		default:
			throw std::runtime_error("invalid parquet physical type");
		}
	}

	CParquetWriter::sParquetColumn& CParquetWriter::addColumn(const std::string& sName, eParquetPhysicalType physicalType, uint64_t nRowCount)
	{
		if (sName.empty())
			throw std::runtime_error("empty parquet column name");

		for (auto& column : m_Columns)
			if (column.m_sName == sName)
				throw std::runtime_error("duplicate parquet column name: " + sName);

		if (m_Columns.empty())
			m_nRowCount = nRowCount;
		else if (m_nRowCount != nRowCount)
			throw std::runtime_error("parquet column row count mismatch: " + sName);

		m_Columns.push_back(sParquetColumn());
		auto& column = m_Columns.back();
		column.m_sName = sName;
		column.m_PhysicalType = physicalType;
		column.m_PlainData.resize((size_t)(nRowCount * getValueSize(physicalType)));

		return column;
	}

	void CParquetWriter::addInt32Column(const std::string& sName, const std::vector<int32_t>& values)
	{
		auto& column = addColumn(sName, eParquetPhysicalType::Int32, values.size());
		uint8_t* pTarget = column.m_PlainData.data();
		for (size_t nIndex = 0; nIndex < values.size(); nIndex++)
			encodeUint32LE(pTarget + nIndex * 4, (uint32_t)values[nIndex]);
	}

	void CParquetWriter::addInt64Column(const std::string& sName, const std::vector<int64_t>& values)
	{
		auto& column = addColumn(sName, eParquetPhysicalType::Int64, values.size());
		uint8_t* pTarget = column.m_PlainData.data();
		for (size_t nIndex = 0; nIndex < values.size(); nIndex++)
			encodeUint64LE(pTarget + nIndex * 8, (uint64_t)values[nIndex]);
	}

	void CParquetWriter::addFloatColumn(const std::string& sName, const std::vector<float>& values)
	{
		auto& column = addColumn(sName, eParquetPhysicalType::Float, values.size());
		uint8_t* pTarget = column.m_PlainData.data();
		for (size_t nIndex = 0; nIndex < values.size(); nIndex++) {
			uint32_t nBits;
			memcpy(&nBits, &values[nIndex], sizeof(nBits));
			encodeUint32LE(pTarget + nIndex * 4, nBits);
		}
	}

	void CParquetWriter::addDoubleColumn(const std::string& sName, const std::vector<double>& values)
	{
		auto& column = addColumn(sName, eParquetPhysicalType::Double, values.size());
		uint8_t* pTarget = column.m_PlainData.data();
		for (size_t nIndex = 0; nIndex < values.size(); nIndex++) {
			uint64_t nBits;
			memcpy(&nBits, &values[nIndex], sizeof(nBits));
			encodeUint64LE(pTarget + nIndex * 8, nBits);
		}
	}

	void CParquetWriter::addKeyValueMetaData(const std::string& sKey, const std::string& sValue)
	{
		if (sKey.empty())
			throw std::runtime_error("empty parquet metadata key");

		m_KeyValueMetaData.push_back(std::make_pair(sKey, sValue));
	}

	void CParquetWriter::setCreatedBy(const std::string& sCreatedBy)
	{
		m_sCreatedBy = sCreatedBy;
	}

	uint64_t CParquetWriter::getRowCount() const
	{
		return m_nRowCount;
	}

	uint32_t CParquetWriter::getColumnCount() const
	{
		return (uint32_t)m_Columns.size();
	}

	void CParquetWriter::writeToBuffer(std::vector<uint8_t>& buffer)
	{
		if (m_Columns.empty())
			throw std::runtime_error("parquet file has no columns");

		buffer.clear();
		appendMagic(buffer);

		std::vector<sParquetRowGroupInfo> rowGroups;
		std::vector<uint8_t> compressedData;
		std::vector<uint8_t> pageHeader;

		for (uint64_t nStartRow = 0; nStartRow < m_nRowCount; nStartRow += m_nRowGroupSize) {
			sParquetRowGroupInfo rowGroup;
			rowGroup.m_nRowCount = std::min(m_nRowGroupSize, m_nRowCount - nStartRow);
			rowGroup.m_nFileOffset = buffer.size();
			rowGroup.m_nTotalByteSize = 0;
			rowGroup.m_nTotalCompressedSize = 0;

			for (auto& column : m_Columns) {
				uint32_t nValueSize = getValueSize(column.m_PhysicalType);
				const uint8_t* pPageData = column.m_PlainData.data() + (size_t)(nStartRow * nValueSize);
				uint64_t nPageSize = rowGroup.m_nRowCount * nValueSize;
				if (nPageSize > (uint64_t)std::numeric_limits<int32_t>::max())
					throw std::runtime_error("parquet page too large: " + column.m_sName);

				const uint8_t* pStoredData = pPageData;
				uint64_t nStoredSize = nPageSize;
				if (m_Codec == eParquetCompressionCodec::GZIP) {
					compressGZIP(pPageData, (size_t)nPageSize, compressedData);
					pStoredData = compressedData.data();
					nStoredSize = compressedData.size();
				}
				if (nStoredSize > (uint64_t)std::numeric_limits<int32_t>::max())
					throw std::runtime_error("parquet page too large: " + column.m_sName);

				// Required columns have no repetition or definition levels, so the page is the bare values.
				pageHeader.clear();
				CThriftCompactWriter headerWriter(pageHeader);
				headerWriter.beginStruct();
				headerWriter.writeFieldI32(1, PARQUET_PAGETYPE_DATAPAGE);
				headerWriter.writeFieldI32(2, (int32_t)nPageSize);
				headerWriter.writeFieldI32(3, (int32_t)nStoredSize);
				headerWriter.beginFieldStruct(5);
				headerWriter.writeFieldI32(1, (int32_t)rowGroup.m_nRowCount);
				headerWriter.writeFieldI32(2, PARQUET_ENCODING_PLAIN);
				headerWriter.writeFieldI32(3, PARQUET_ENCODING_RLE);
				headerWriter.writeFieldI32(4, PARQUET_ENCODING_RLE);
				headerWriter.endStruct();
				headerWriter.endStruct();

				sParquetChunkInfo chunk;
				chunk.m_nDataPageOffset = buffer.size();
				chunk.m_nValueCount = rowGroup.m_nRowCount;
				chunk.m_nUncompressedSize = pageHeader.size() + nPageSize;
				chunk.m_nCompressedSize = pageHeader.size() + nStoredSize;
				chunk.m_bHasStatistics = computeStatistics(column.m_PhysicalType, pPageData, rowGroup.m_nRowCount, chunk.m_MinValue, chunk.m_MaxValue);

				buffer.insert(buffer.end(), pageHeader.begin(), pageHeader.end());
				buffer.insert(buffer.end(), pStoredData, pStoredData + nStoredSize);

				rowGroup.m_nTotalByteSize += chunk.m_nUncompressedSize;
				rowGroup.m_nTotalCompressedSize += chunk.m_nCompressedSize;
				rowGroup.m_Chunks.push_back(std::move(chunk));
			}

			rowGroups.push_back(std::move(rowGroup));
		}

		std::vector<uint8_t> metaData;
		CThriftCompactWriter writer(metaData);
		writer.beginStruct();

		writer.writeFieldI32(1, PARQUETWRITER_FORMATVERSION);

		writer.writeFieldListHeader(2, THRIFTCOMPACT_TYPE_STRUCT, m_Columns.size() + 1);
		writer.beginStruct();
		writer.writeFieldString(4, "schema");
		writer.writeFieldI32(5, (int32_t)m_Columns.size());
		writer.endStruct();
		for (auto& column : m_Columns) {
			writer.beginStruct();
			writer.writeFieldI32(1, (int32_t)column.m_PhysicalType);
			writer.writeFieldI32(3, PARQUET_REPETITION_REQUIRED);
			writer.writeFieldString(4, column.m_sName);
			writer.endStruct();
		}

		writer.writeFieldI64(3, (int64_t)m_nRowCount);

		writer.writeFieldListHeader(4, THRIFTCOMPACT_TYPE_STRUCT, rowGroups.size());
		for (auto& rowGroup : rowGroups) {
			writer.beginStruct();

			writer.writeFieldListHeader(1, THRIFTCOMPACT_TYPE_STRUCT, rowGroup.m_Chunks.size());
			for (size_t nColumnIndex = 0; nColumnIndex < rowGroup.m_Chunks.size(); nColumnIndex++) {
				auto& chunk = rowGroup.m_Chunks[nColumnIndex];
				auto& column = m_Columns[nColumnIndex];

				writer.beginStruct();
				writer.writeFieldI64(2, (int64_t)chunk.m_nDataPageOffset);
				writer.beginFieldStruct(3);
				writer.writeFieldI32(1, (int32_t)column.m_PhysicalType);
				writer.writeFieldListHeader(2, THRIFTCOMPACT_TYPE_I32, 1);
				writer.writeI32(PARQUET_ENCODING_PLAIN);
				writer.writeFieldListHeader(3, THRIFTCOMPACT_TYPE_BINARY, 1);
				writer.writeString(column.m_sName);
				writer.writeFieldI32(4, (int32_t)m_Codec);
				writer.writeFieldI64(5, (int64_t)chunk.m_nValueCount);
				writer.writeFieldI64(6, (int64_t)chunk.m_nUncompressedSize);
				writer.writeFieldI64(7, (int64_t)chunk.m_nCompressedSize);
				writer.writeFieldI64(9, (int64_t)chunk.m_nDataPageOffset);
				if (chunk.m_bHasStatistics) {
					writer.beginFieldStruct(12);
					writer.writeFieldI64(3, 0);
					writer.writeFieldBinary(5, chunk.m_MaxValue);
					writer.writeFieldBinary(6, chunk.m_MinValue);
					writer.endStruct();
				}
				writer.endStruct();
				writer.endStruct();
			}

			writer.writeFieldI64(2, (int64_t)rowGroup.m_nTotalByteSize);
			writer.writeFieldI64(3, (int64_t)rowGroup.m_nRowCount);
			writer.writeFieldI64(5, (int64_t)rowGroup.m_nFileOffset);
			writer.writeFieldI64(6, (int64_t)rowGroup.m_nTotalCompressedSize);
			writer.endStruct();
		}

		if (!m_KeyValueMetaData.empty()) {
			writer.writeFieldListHeader(5, THRIFTCOMPACT_TYPE_STRUCT, m_KeyValueMetaData.size());
			for (auto& keyValue : m_KeyValueMetaData) {
				writer.beginStruct();
				writer.writeFieldString(1, keyValue.first);
				writer.writeFieldString(2, keyValue.second);
				writer.endStruct();
			}
		}

		writer.writeFieldString(6, m_sCreatedBy);

		// TypeDefinedOrder for every column, so readers trust the min/max statistics.
		writer.writeFieldListHeader(7, THRIFTCOMPACT_TYPE_STRUCT, m_Columns.size());
		for (size_t nColumnIndex = 0; nColumnIndex < m_Columns.size(); nColumnIndex++) {
			writer.beginStruct();
			writer.beginFieldStruct(1);
			writer.endStruct();
			writer.endStruct();
		}

		writer.endStruct();

		if (metaData.size() > (size_t)std::numeric_limits<uint32_t>::max())
			throw std::runtime_error("parquet metadata too large");

		buffer.insert(buffer.end(), metaData.begin(), metaData.end());
		uint8_t lengthBytes[4];
		encodeUint32LE(lengthBytes, (uint32_t)metaData.size());
		buffer.insert(buffer.end(), lengthBytes, lengthBytes + 4);
		appendMagic(buffer);
	}

}
