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

#include "amc_datatableformat.hpp"
#include "amc_datatableencoding.hpp"
#include "amc_constants.hpp"

#include <cmath>
#include <cstring>

namespace AMC {

	namespace {

		template <typename T>
		void appendStruct(std::vector<uint8_t>& buffer, const T& value)
		{
			const uint8_t* pData = (const uint8_t*)&value;
			buffer.insert(buffer.end(), pData, pData + sizeof(T));
		}

		void appendBytes(std::vector<uint8_t>& buffer, const uint8_t* pData, size_t nSize)
		{
			if (nSize > 0)
				buffer.insert(buffer.end(), pData, pData + nSize);
		}

		template <typename T>
		T loadValue(const uint8_t* pData, size_t nIndex)
		{
			T value;
			memcpy(&value, pData + nIndex * sizeof(T), sizeof(T));
			return value;
		}

		template <typename T>
		bool computeFloatingPointStatistics(const uint8_t* pData, uint32_t nRowCount, uint64_t& nMinValue, uint64_t& nMaxValue)
		{
			bool bValid = false;
			double dMin = 0.0;
			double dMax = 0.0;

			for (uint32_t nIndex = 0; nIndex < nRowCount; nIndex++) {
				double dValue = (double)loadValue<T>(pData, nIndex);
				if (std::isnan(dValue))
					continue;

				if (!bValid) {
					dMin = dValue;
					dMax = dValue;
					bValid = true;
				}
				else {
					if (dValue < dMin)
						dMin = dValue;
					if (dValue > dMax)
						dMax = dValue;
				}
			}

			if (bValid) {
				memcpy(&nMinValue, &dMin, sizeof(double));
				memcpy(&nMaxValue, &dMax, sizeof(double));
			}

			return bValid;
		}

		template <typename T>
		bool computeSignedStatistics(const uint8_t* pData, uint32_t nRowCount, uint64_t& nMinValue, uint64_t& nMaxValue)
		{
			if (nRowCount == 0)
				return false;

			int64_t nMin = (int64_t)loadValue<T>(pData, 0);
			int64_t nMax = nMin;
			for (uint32_t nIndex = 1; nIndex < nRowCount; nIndex++) {
				int64_t nValue = (int64_t)loadValue<T>(pData, nIndex);
				if (nValue < nMin)
					nMin = nValue;
				if (nValue > nMax)
					nMax = nValue;
			}

			nMinValue = (uint64_t)nMin;
			nMaxValue = (uint64_t)nMax;
			return true;
		}

		template <typename T>
		bool computeUnsignedStatistics(const uint8_t* pData, uint32_t nRowCount, uint64_t& nMinValue, uint64_t& nMaxValue)
		{
			if (nRowCount == 0)
				return false;

			uint64_t nMin = (uint64_t)loadValue<T>(pData, 0);
			uint64_t nMax = nMin;
			for (uint32_t nIndex = 1; nIndex < nRowCount; nIndex++) {
				uint64_t nValue = (uint64_t)loadValue<T>(pData, nIndex);
				if (nValue < nMin)
					nMin = nValue;
				if (nValue > nMax)
					nMax = nValue;
			}

			nMinValue = nMin;
			nMaxValue = nMax;
			return true;
		}

	}


	EDataTableFormatException::EDataTableFormatException(eDataTableFormatError error, const std::string& sMessage)
		: std::runtime_error(sMessage), m_Error(error)
	{
	}

	eDataTableFormatError EDataTableFormatException::getError() const
	{
		return m_Error;
	}


	uint32_t CDataTableV2::getEntrySize(eDataTableV2DataType dataType)
	{
		switch (dataType) {
		case eDataTableV2DataType::Double: return 8;
		case eDataTableV2DataType::Int32: return 4;
		case eDataTableV2DataType::Uint32: return 4;
		case eDataTableV2DataType::Int64: return 8;
		case eDataTableV2DataType::Uint64: return 8;
		case eDataTableV2DataType::Float: return 4;
		case eDataTableV2DataType::Bool: return 1;
		case eDataTableV2DataType::Timestamp: return 8;
		default:
			throw EDataTableFormatException(eDataTableFormatError::InvalidDataType, "invalid datatable column data type: " + std::to_string((uint32_t)dataType));
		}
	}

	bool CDataTableV2::dataTypeIsValid(uint32_t nDataType)
	{
		return (nDataType >= (uint32_t)eDataTableV2DataType::Double) && (nDataType <= (uint32_t)eDataTableV2DataType::Timestamp);
	}

	bool CDataTableV2::computeStatistics(eDataTableV2DataType dataType, const uint8_t* pData, uint32_t nRowCount, uint64_t& nMinValue, uint64_t& nMaxValue)
	{
		nMinValue = 0;
		nMaxValue = 0;

		if ((pData == nullptr) || (nRowCount == 0))
			return false;

		switch (dataType) {
		case eDataTableV2DataType::Double: return computeFloatingPointStatistics<double>(pData, nRowCount, nMinValue, nMaxValue);
		case eDataTableV2DataType::Float: return computeFloatingPointStatistics<float>(pData, nRowCount, nMinValue, nMaxValue);
		case eDataTableV2DataType::Int32: return computeSignedStatistics<int32_t>(pData, nRowCount, nMinValue, nMaxValue);
		case eDataTableV2DataType::Int64: return computeSignedStatistics<int64_t>(pData, nRowCount, nMinValue, nMaxValue);
		case eDataTableV2DataType::Uint32: return computeUnsignedStatistics<uint32_t>(pData, nRowCount, nMinValue, nMaxValue);
		case eDataTableV2DataType::Uint64: return computeUnsignedStatistics<uint64_t>(pData, nRowCount, nMinValue, nMaxValue);
		case eDataTableV2DataType::Timestamp: return computeUnsignedStatistics<uint64_t>(pData, nRowCount, nMinValue, nMaxValue);
		case eDataTableV2DataType::Bool: return computeUnsignedStatistics<uint8_t>(pData, nRowCount, nMinValue, nMaxValue);
		default:
			throw EDataTableFormatException(eDataTableFormatError::InvalidDataType, "invalid datatable column data type: " + std::to_string((uint32_t)dataType));
		}
	}

	double CDataTableV2::statisticsValueToDouble(eDataTableV2DataType dataType, uint64_t nValue)
	{
		switch (dataType) {
		case eDataTableV2DataType::Double:
		case eDataTableV2DataType::Float: {
			double dValue = 0.0;
			memcpy(&dValue, &nValue, sizeof(dValue));
			return dValue;
		}
		case eDataTableV2DataType::Int32:
		case eDataTableV2DataType::Int64:
			return (double)(int64_t)nValue;
		default:
			return (double)nValue;
		}
	}

	uint32_t CDataTableV2::getDefaultEncoding(eDataTableV2DataType dataType)
	{
		switch (dataType) {
		case eDataTableV2DataType::Int32:
		case eDataTableV2DataType::Uint32:
		case eDataTableV2DataType::Int64:
		case eDataTableV2DataType::Uint64:
		case eDataTableV2DataType::Timestamp:
			return DATATABLE_V2_ENCODING_DELTA;
		case eDataTableV2DataType::Bool:
			return DATATABLE_V2_ENCODING_BITPACK;
		default:
			return DATATABLE_V2_ENCODING_PLAIN;
		}
	}

	bool CDataTableV2::encodingIsValidForColumn(eDataTableV2DataType dataType, uint32_t nEncoding, double dQuantizationStep)
	{
		switch (nEncoding) {
		case DATATABLE_V2_ENCODING_PLAIN:
			return true;

		case DATATABLE_V2_ENCODING_DELTA:
			switch (dataType) {
			case eDataTableV2DataType::Int32:
			case eDataTableV2DataType::Uint32:
			case eDataTableV2DataType::Int64:
			case eDataTableV2DataType::Uint64:
			case eDataTableV2DataType::Timestamp:
				return true;
			case eDataTableV2DataType::Double:
				return (dQuantizationStep > 0.0);
			default:
				return false;
			}

		case DATATABLE_V2_ENCODING_BITPACK:
			return (dataType == eDataTableV2DataType::Bool);

		default:
			return false;
		}
	}


	/*************************************************************************************************************************
	 CDataTableV2ChunkEncoder
	**************************************************************************************************************************/

	CDataTableV2ChunkEncoder::CDataTableV2ChunkEncoder(const std::vector<sDataTableV2Column>& columns, uint32_t nCodec)
		: m_Columns(columns), m_nCodec(nCodec)
	{
		if (!CDataTableEncoding::codecIsValid(nCodec))
			throw EDataTableFormatException(eDataTableFormatError::UnsupportedCodec, "unsupported datatable page codec: " + std::to_string(nCodec));

		for (auto& column : m_Columns) {
			m_EntrySizes.push_back(CDataTableV2::getEntrySize(column.m_DataType));

			if (!(column.m_dQuantizationStep >= 0.0) || !std::isfinite(column.m_dQuantizationStep))
				throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "invalid quantisation step of datatable column " + column.m_sIdentifier);
			if (!CDataTableV2::encodingIsValidForColumn(column.m_DataType, column.m_nEncoding, column.m_dQuantizationStep))
				throw EDataTableFormatException(eDataTableFormatError::UnsupportedEncoding, "encoding " + std::to_string(column.m_nEncoding) + " is not valid for datatable column " + column.m_sIdentifier);
		}
	}

	// Converts doubles to integer multiples of the step. Returns false if a value can not be represented,
	// in which case the page is stored as plain doubles. m_DoubleScratch receives the values as they will be read back.
	bool CDataTableV2ChunkEncoder::quantize(const uint8_t* pData, uint32_t nRowCount, double dStep)
	{
		m_IntegerScratch.resize(nRowCount);
		m_DoubleScratch.resize(nRowCount);

		for (uint32_t nIndex = 0; nIndex < nRowCount; nIndex++) {
			double dValue = loadValue<double>(pData, nIndex);
			if (!std::isfinite(dValue))
				return false;

			double dScaled = dValue / dStep;
			if (!(std::fabs(dScaled) < 4.0e18))
				return false;

			int64_t nQuantized = std::llround(dScaled);
			m_IntegerScratch[nIndex] = nQuantized;
			m_DoubleScratch[nIndex] = (double)nQuantized * dStep;
		}

		return true;
	}

	void CDataTableV2ChunkEncoder::widenIntegers(eDataTableV2DataType dataType, const uint8_t* pData, uint32_t nRowCount)
	{
		m_IntegerScratch.resize(nRowCount);

		switch (dataType) {
		case eDataTableV2DataType::Int32:
			for (uint32_t nIndex = 0; nIndex < nRowCount; nIndex++)
				m_IntegerScratch[nIndex] = (int64_t)loadValue<int32_t>(pData, nIndex);
			break;
		case eDataTableV2DataType::Uint32:
			for (uint32_t nIndex = 0; nIndex < nRowCount; nIndex++)
				m_IntegerScratch[nIndex] = (int64_t)loadValue<uint32_t>(pData, nIndex);
			break;
		default:
			// 64 bit integers and timestamps are delta encoded on their bit pattern.
			memcpy(m_IntegerScratch.data(), pData, (size_t)nRowCount * sizeof(int64_t));
			break;
		}
	}

	void CDataTableV2ChunkEncoder::encodeChunk(const std::vector<const uint8_t*>& columnData, uint32_t nRowCount, sDataTableV2EncodedChunk& chunk)
	{
		if (m_Columns.empty())
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable segment without columns can not hold chunks");
		if (columnData.size() != m_Columns.size())
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable chunk column count mismatch");
		if (nRowCount == 0)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable chunk is empty");

		chunk.m_nRowCount = nRowCount;
		chunk.m_Bytes.clear();
		chunk.m_Entries.clear();

		for (size_t nColumnIndex = 0; nColumnIndex < m_Columns.size(); nColumnIndex++) {
			auto& column = m_Columns[nColumnIndex];

			const uint8_t* pPlainData = columnData[nColumnIndex];
			if (pPlainData == nullptr)
				throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable chunk column data is null");

			uint64_t nPlainSize = (uint64_t)nRowCount * m_EntrySizes[nColumnIndex];
			if (nPlainSize > UINT32_MAX)
				throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable page is too large");

			// Encode. A page stays plain if its encoding does not make it smaller.
			const uint8_t* pPayload = pPlainData;
			size_t nPayloadSize = (size_t)nPlainSize;
			uint32_t nEncoding = DATATABLE_V2_ENCODING_PLAIN;
			const uint8_t* pStatisticsData = pPlainData;

			if (column.m_nEncoding == DATATABLE_V2_ENCODING_DELTA) {
				bool bQuantized = (column.m_DataType == eDataTableV2DataType::Double);
				bool bEncodable = true;

				if (bQuantized)
					bEncodable = quantize(pPlainData, nRowCount, column.m_dQuantizationStep);
				else
					widenIntegers(column.m_DataType, pPlainData, nRowCount);

				if (bEncodable) {
					m_EncodedScratch.clear();
					CDataTableEncoding::encodeDelta(m_IntegerScratch.data(), nRowCount, m_EncodedScratch);

					if (m_EncodedScratch.size() < nPlainSize) {
						pPayload = m_EncodedScratch.data();
						nPayloadSize = m_EncodedScratch.size();
						nEncoding = DATATABLE_V2_ENCODING_DELTA;

						// The statistics describe the values as they are read back.
						if (bQuantized)
							pStatisticsData = (const uint8_t*)m_DoubleScratch.data();
					}
				}
			}
			else if (column.m_nEncoding == DATATABLE_V2_ENCODING_BITPACK) {
				m_EncodedScratch.clear();
				CDataTableEncoding::encodeBitPack(pPlainData, nRowCount, m_EncodedScratch);

				if (m_EncodedScratch.size() < nPlainSize) {
					pPayload = m_EncodedScratch.data();
					nPayloadSize = m_EncodedScratch.size();
					nEncoding = DATATABLE_V2_ENCODING_BITPACK;
				}
			}

			// Compress. A page stays uncompressed if the codec does not make it smaller.
			const uint8_t* pStoredData = pPayload;
			size_t nStoredSize = nPayloadSize;
			uint32_t nCodec = DATATABLE_V2_CODEC_NONE;

			if (CDataTableEncoding::compress(m_nCodec, pPayload, nPayloadSize, m_CompressedScratch)) {
				pStoredData = m_CompressedScratch.data();
				nStoredSize = m_CompressedScratch.size();
				nCodec = m_nCodec;
			}

			sDataTableV2PageHeader pageHeader;
			memset(&pageHeader, 0, sizeof(pageHeader));
			pageHeader.m_nMagic = DATATABLE_V2_PAGEMAGIC;
			pageHeader.m_nColumnIndex = (uint32_t)nColumnIndex;
			pageHeader.m_nRowCount = nRowCount;
			pageHeader.m_nDataType = (uint32_t)column.m_DataType;
			pageHeader.m_nCodec = nCodec;
			pageHeader.m_nEncoding = nEncoding;
			pageHeader.m_nCompressedSize = (uint32_t)nStoredSize;
			pageHeader.m_nUncompressedSize = (uint32_t)nPayloadSize;
			appendStruct(chunk.m_Bytes, pageHeader);

			sDataTableV2PageEntry entry;
			memset(&entry, 0, sizeof(entry));
			entry.m_nFileOffset = chunk.m_Bytes.size();
			entry.m_nCompressedSize = (uint32_t)nStoredSize;
			entry.m_nUncompressedSize = (uint32_t)nPayloadSize;
			entry.m_nRowCount = nRowCount;
			entry.m_nCodec = nCodec;
			entry.m_nEncoding = nEncoding;
			if (CDataTableV2::computeStatistics(column.m_DataType, pStatisticsData, nRowCount, entry.m_nMinValue, entry.m_nMaxValue))
				entry.m_nFlags |= DATATABLE_V2_PAGEFLAG_STATISTICSVALID;

			appendBytes(chunk.m_Bytes, pStoredData, nStoredSize);
			chunk.m_Entries.push_back(entry);
		}
	}


	/*************************************************************************************************************************
	 CDataTableV2SegmentBuilder
	**************************************************************************************************************************/

	CDataTableV2SegmentBuilder::CDataTableV2SegmentBuilder(const std::vector<sDataTableV2Column>& columns, uint32_t nSegmentIndex, uint64_t nFirstRowIndex, uint32_t nChunkRowCapacity)
		: m_Columns(columns),
		m_nSegmentIndex(nSegmentIndex),
		m_nFirstRowIndex(nFirstRowIndex),
		m_nChunkRowCapacity(nChunkRowCapacity),
		m_nCurrentSize(0),
		m_nRowCount(0),
		m_nChunkCount(0),
		m_bHeaderWritten(false),
		m_bFooterWritten(false),
		m_UncompressedEncoder(columns, DATATABLE_V2_CODEC_NONE)
	{
		if (m_Columns.size() >= DATATABLE_MAXCOLUMNCOUNT)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable segment has too many columns");

		for (auto& column : m_Columns) {
			if ((column.m_sIdentifier.length() > DATATABLE_V2_MAXSTRINGLENGTH) || (column.m_sDescription.length() > DATATABLE_V2_MAXSTRINGLENGTH))
				throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable column identifier or description is too long");

			m_EntrySizes.push_back(CDataTableV2::getEntrySize(column.m_DataType));
		}
	}

	void CDataTableV2SegmentBuilder::appendHeader(std::vector<uint8_t>& buffer)
	{
		if (m_bHeaderWritten)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable segment header has been written already");

		sDataTableV2SegmentHeader header;
		memset(&header, 0, sizeof(header));
		header.m_nSignature = DATATABLE_HEADERSIGNATURE_V2;
		header.m_nVersion = DATATABLE_FORMATVERSION_2;
		header.m_nHeaderSize = sizeof(header);
		header.m_nSegmentIndex = m_nSegmentIndex;
		header.m_nColumnCount = (uint32_t)m_Columns.size();
		header.m_nFirstRowIndex = m_nFirstRowIndex;
		header.m_nChunkRowCapacity = m_nChunkRowCapacity;

		appendStruct(buffer, header);

		m_nCurrentSize += sizeof(header);
		m_bHeaderWritten = true;
	}

	void CDataTableV2SegmentBuilder::appendChunk(const std::vector<const uint8_t*>& columnData, uint32_t nRowCount, std::vector<uint8_t>& buffer)
	{
		if (!m_bHeaderWritten || m_bFooterWritten)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable segment is not open for chunks");

		m_UncompressedEncoder.encodeChunk(columnData, nRowCount, m_UncompressedChunk);
		appendEncodedChunk(m_UncompressedChunk, buffer);
	}

	void CDataTableV2SegmentBuilder::appendEncodedChunk(const sDataTableV2EncodedChunk& chunk, std::vector<uint8_t>& buffer)
	{
		if (!m_bHeaderWritten || m_bFooterWritten)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable segment is not open for chunks");
		if (chunk.m_Entries.size() != m_Columns.size())
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable chunk column count mismatch");
		if (m_Columns.empty() || (chunk.m_nRowCount == 0))
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable chunk is empty");
		if (m_nChunkCount >= DATATABLE_V2_MAXCHUNKCOUNT)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable segment has too many chunks");

		// The page offsets of the chunk become absolute offsets of the segment.
		for (auto entry : chunk.m_Entries) {
			entry.m_nFileOffset += m_nCurrentSize;
			m_Directory.push_back(entry);
		}

		appendBytes(buffer, chunk.m_Bytes.data(), chunk.m_Bytes.size());
		m_nCurrentSize += chunk.m_Bytes.size();

		m_nRowCount += chunk.m_nRowCount;
		m_nChunkCount++;
	}

	uint64_t CDataTableV2SegmentBuilder::getSchemaSizeInBytes() const
	{
		uint64_t nSchemaSize = 0;
		for (auto& column : m_Columns)
			nSchemaSize += sizeof(sDataTableV2SchemaEntry) + column.m_sIdentifier.length() + column.m_sDescription.length();

		return nSchemaSize;
	}

	void CDataTableV2SegmentBuilder::appendFooter(std::vector<uint8_t>& buffer)
	{
		if (!m_bHeaderWritten || m_bFooterWritten)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable segment footer can not be written");

		size_t nFooterBegin = buffer.size();

		sDataTableV2FooterHeader footerHeader;
		memset(&footerHeader, 0, sizeof(footerHeader));
		footerHeader.m_nMagic = DATATABLE_V2_FOOTERMAGIC;
		footerHeader.m_nColumnCount = (uint32_t)m_Columns.size();
		footerHeader.m_nChunkCount = m_nChunkCount;
		footerHeader.m_nSchemaSize = (uint32_t)getSchemaSizeInBytes();
		footerHeader.m_nRowCount = m_nRowCount;
		footerHeader.m_nFirstRowIndex = m_nFirstRowIndex;
		appendStruct(buffer, footerHeader);

		for (auto& column : m_Columns) {
			sDataTableV2SchemaEntry schemaEntry;
			memset(&schemaEntry, 0, sizeof(schemaEntry));
			schemaEntry.m_nDataType = (uint32_t)column.m_DataType;
			schemaEntry.m_nDefaultEncoding = column.m_nEncoding;
			schemaEntry.m_nIdentifierLength = (uint32_t)column.m_sIdentifier.length();
			schemaEntry.m_nDescriptionLength = (uint32_t)column.m_sDescription.length();
			schemaEntry.m_dQuantizationScale = column.m_dQuantizationStep;
			appendStruct(buffer, schemaEntry);
			appendBytes(buffer, (const uint8_t*)column.m_sIdentifier.data(), column.m_sIdentifier.length());
			appendBytes(buffer, (const uint8_t*)column.m_sDescription.data(), column.m_sDescription.length());
		}

		for (auto& entry : m_Directory)
			appendStruct(buffer, entry);

		sDataTableV2Trailer trailer;
		memset(&trailer, 0, sizeof(trailer));
		trailer.m_nFooterLength = buffer.size() - nFooterBegin;
		trailer.m_nMagic = DATATABLE_V2_TRAILERMAGIC;
		appendStruct(buffer, trailer);

		m_nCurrentSize += buffer.size() - nFooterBegin;
		m_bFooterWritten = true;
	}

	uint64_t CDataTableV2SegmentBuilder::getChunkSizeInBytes(uint32_t nRowCount) const
	{
		uint64_t nChunkSize = 0;
		for (auto nEntrySize : m_EntrySizes)
			nChunkSize += sizeof(sDataTableV2PageHeader) + (uint64_t)nRowCount * nEntrySize;

		return nChunkSize;
	}

	uint64_t CDataTableV2SegmentBuilder::getFooterSizeInBytes(uint32_t nChunkCount) const
	{
		return sizeof(sDataTableV2FooterHeader) + getSchemaSizeInBytes() + (uint64_t)nChunkCount * m_Columns.size() * sizeof(sDataTableV2PageEntry) + sizeof(sDataTableV2Trailer);
	}

	uint64_t CDataTableV2SegmentBuilder::getCurrentSize() const
	{
		return m_nCurrentSize;
	}

	uint64_t CDataTableV2SegmentBuilder::getRowCount() const
	{
		return m_nRowCount;
	}

	uint32_t CDataTableV2SegmentBuilder::getChunkCount() const
	{
		return m_nChunkCount;
	}

	uint32_t CDataTableV2SegmentBuilder::getSegmentIndex() const
	{
		return m_nSegmentIndex;
	}

	uint64_t CDataTableV2SegmentBuilder::getFirstRowIndex() const
	{
		return m_nFirstRowIndex;
	}


	/*************************************************************************************************************************
	 CDataTableV2SegmentReader
	**************************************************************************************************************************/

	CDataTableV2SegmentReader::CDataTableV2SegmentReader(ReadCallback readCallback, uint64_t nStreamSize)
		: m_ReadCallback(readCallback), m_nStreamSize(nStreamSize), m_nFooterStart(0), m_nPageReadCount(0)
	{
		if (!m_ReadCallback)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable segment read callback is empty");

		memset(&m_Header, 0, sizeof(m_Header));
		memset(&m_FooterHeader, 0, sizeof(m_FooterHeader));

		// The first two header fields have the same position in all format versions.
		if (m_nStreamSize < 2 * sizeof(uint32_t))
			throw EDataTableFormatException(eDataTableFormatError::Truncated, "datatable stream is smaller than its signature");

		m_ReadCallback(0, 2 * sizeof(uint32_t), (uint8_t*)&m_Header);
		if (m_Header.m_nSignature != DATATABLE_HEADERSIGNATURE_V2)
			throw EDataTableFormatException(eDataTableFormatError::InvalidSignature, "invalid datatable segment signature");
		if (m_Header.m_nVersion != DATATABLE_FORMATVERSION_2)
			throw EDataTableFormatException(eDataTableFormatError::UnsupportedVersion, "unsupported datatable format version: " + std::to_string(m_Header.m_nVersion));

		if (m_nStreamSize < sizeof(m_Header))
			throw EDataTableFormatException(eDataTableFormatError::Truncated, "datatable segment is smaller than its header");

		m_ReadCallback(0, sizeof(m_Header), (uint8_t*)&m_Header);
		if (m_Header.m_nHeaderSize != sizeof(m_Header))
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "invalid datatable segment header size");
		if (m_Header.m_nColumnCount >= DATATABLE_MAXCOLUMNCOUNT)
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable segment has too many columns");

		// A segment that was interrupted before its footer was written has no trailer.
		if (m_nStreamSize < sizeof(m_Header) + sizeof(sDataTableV2FooterHeader) + sizeof(sDataTableV2Trailer))
			throw EDataTableFormatException(eDataTableFormatError::Truncated, "datatable segment has no footer");

		sDataTableV2Trailer trailer;
		m_ReadCallback(m_nStreamSize - sizeof(trailer), sizeof(trailer), (uint8_t*)&trailer);
		if (trailer.m_nMagic != DATATABLE_V2_TRAILERMAGIC)
			throw EDataTableFormatException(eDataTableFormatError::Truncated, "datatable segment has no footer");

		uint64_t nMaxFooterLength = m_nStreamSize - sizeof(m_Header) - sizeof(trailer);
		if ((trailer.m_nFooterLength < sizeof(sDataTableV2FooterHeader)) || (trailer.m_nFooterLength > nMaxFooterLength))
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "invalid datatable segment footer length");

		m_nFooterStart = m_nStreamSize - sizeof(trailer) - trailer.m_nFooterLength;

		std::vector<uint8_t> footer((size_t)trailer.m_nFooterLength);
		m_ReadCallback(m_nFooterStart, trailer.m_nFooterLength, footer.data());

		parseFooter(footer);
		validateDirectory();
	}

	void CDataTableV2SegmentReader::parseFooter(const std::vector<uint8_t>& footer)
	{
		memcpy(&m_FooterHeader, footer.data(), sizeof(m_FooterHeader));
		if (m_FooterHeader.m_nMagic != DATATABLE_V2_FOOTERMAGIC)
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "invalid datatable segment footer magic");
		if (m_FooterHeader.m_nColumnCount != m_Header.m_nColumnCount)
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable segment column count mismatch");
		if (m_FooterHeader.m_nFirstRowIndex != m_Header.m_nFirstRowIndex)
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable segment first row mismatch");
		if (m_FooterHeader.m_nChunkCount > DATATABLE_V2_MAXCHUNKCOUNT)
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable segment has too many chunks");

		uint64_t nColumnCount = m_FooterHeader.m_nColumnCount;
		uint64_t nDirectorySize = (uint64_t)m_FooterHeader.m_nChunkCount * nColumnCount * sizeof(sDataTableV2PageEntry);
		uint64_t nExpectedSize = sizeof(m_FooterHeader) + (uint64_t)m_FooterHeader.m_nSchemaSize + nDirectorySize;
		if (nExpectedSize != footer.size())
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable segment footer size mismatch");

		// Column schema
		size_t nPosition = sizeof(m_FooterHeader);
		size_t nSchemaEnd = nPosition + m_FooterHeader.m_nSchemaSize;

		for (uint64_t nColumnIndex = 0; nColumnIndex < nColumnCount; nColumnIndex++) {
			if (nPosition + sizeof(sDataTableV2SchemaEntry) > nSchemaEnd)
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable segment schema exceeds the footer");

			sDataTableV2SchemaEntry schemaEntry;
			memcpy(&schemaEntry, footer.data() + nPosition, sizeof(schemaEntry));
			nPosition += sizeof(schemaEntry);

			if (!CDataTableV2::dataTypeIsValid(schemaEntry.m_nDataType))
				throw EDataTableFormatException(eDataTableFormatError::InvalidDataType, "invalid datatable column data type: " + std::to_string(schemaEntry.m_nDataType));
			if ((schemaEntry.m_nIdentifierLength > DATATABLE_V2_MAXSTRINGLENGTH) || (schemaEntry.m_nDescriptionLength > DATATABLE_V2_MAXSTRINGLENGTH))
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable column identifier or description is too long");
			if (nPosition + schemaEntry.m_nIdentifierLength + schemaEntry.m_nDescriptionLength > nSchemaEnd)
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable segment schema exceeds the footer");

			if (!(schemaEntry.m_dQuantizationScale >= 0.0) || !std::isfinite(schemaEntry.m_dQuantizationScale))
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "invalid datatable column quantisation step");
			if ((schemaEntry.m_nDefaultEncoding != DATATABLE_V2_ENCODING_PLAIN) && (schemaEntry.m_nDefaultEncoding != DATATABLE_V2_ENCODING_DELTA) && (schemaEntry.m_nDefaultEncoding != DATATABLE_V2_ENCODING_BITPACK))
				throw EDataTableFormatException(eDataTableFormatError::UnsupportedEncoding, "unsupported datatable column encoding: " + std::to_string(schemaEntry.m_nDefaultEncoding));

			sDataTableV2Column column;
			column.m_DataType = (eDataTableV2DataType)schemaEntry.m_nDataType;
			column.m_nEncoding = schemaEntry.m_nDefaultEncoding;
			column.m_dQuantizationStep = schemaEntry.m_dQuantizationScale;
			column.m_sIdentifier.assign((const char*)footer.data() + nPosition, schemaEntry.m_nIdentifierLength);
			nPosition += schemaEntry.m_nIdentifierLength;
			column.m_sDescription.assign((const char*)footer.data() + nPosition, schemaEntry.m_nDescriptionLength);
			nPosition += schemaEntry.m_nDescriptionLength;

			m_EntrySizes.push_back(CDataTableV2::getEntrySize(column.m_DataType));
			m_Columns.push_back(column);
		}

		if (nPosition != nSchemaEnd)
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable segment schema size mismatch");

		// Page directory
		size_t nEntryCount = (size_t)m_FooterHeader.m_nChunkCount * (size_t)nColumnCount;
		m_Directory.resize(nEntryCount);
		if (nEntryCount > 0)
			memcpy(m_Directory.data(), footer.data() + nSchemaEnd, nEntryCount * sizeof(sDataTableV2PageEntry));
	}

	void CDataTableV2SegmentReader::validateDirectory()
	{
		uint32_t nColumnCount = (uint32_t)m_Columns.size();
		uint64_t nTotalRowCount = 0;

		for (uint32_t nChunkIndex = 0; nChunkIndex < m_FooterHeader.m_nChunkCount; nChunkIndex++) {
			uint32_t nChunkRowCount = 0;

			for (uint32_t nColumnIndex = 0; nColumnIndex < nColumnCount; nColumnIndex++) {
				auto& entry = m_Directory[(size_t)nChunkIndex * nColumnCount + nColumnIndex];

				if (!CDataTableEncoding::codecIsValid(entry.m_nCodec))
					throw EDataTableFormatException(eDataTableFormatError::UnsupportedCodec, "unsupported datatable page codec: " + std::to_string(entry.m_nCodec));

				auto& column = m_Columns[nColumnIndex];
				uint64_t nPlainSize = (uint64_t)entry.m_nRowCount * m_EntrySizes[nColumnIndex];

				switch (entry.m_nEncoding) {
				case DATATABLE_V2_ENCODING_PLAIN:
					if (entry.m_nUncompressedSize != nPlainSize)
						throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable page size mismatch");
					break;

				case DATATABLE_V2_ENCODING_DELTA:
					if (!CDataTableV2::encodingIsValidForColumn(column.m_DataType, DATATABLE_V2_ENCODING_DELTA, column.m_dQuantizationStep))
						throw EDataTableFormatException(eDataTableFormatError::Corrupt, "delta encoded page in a datatable column that can not be delta encoded");
					if (entry.m_nUncompressedSize == 0)
						throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable page size mismatch");
					break;

				case DATATABLE_V2_ENCODING_BITPACK:
					if (column.m_DataType != eDataTableV2DataType::Bool)
						throw EDataTableFormatException(eDataTableFormatError::Corrupt, "bit packed page in a datatable column that is not bool");
					if (entry.m_nUncompressedSize != CDataTableEncoding::getBitPackSize(entry.m_nRowCount))
						throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable page size mismatch");
					break;

				default:
					throw EDataTableFormatException(eDataTableFormatError::UnsupportedEncoding, "unsupported datatable page encoding: " + std::to_string(entry.m_nEncoding));
				}

				if ((entry.m_nCodec == DATATABLE_V2_CODEC_NONE) && (entry.m_nCompressedSize != entry.m_nUncompressedSize))
					throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable page size mismatch");
				if ((entry.m_nRowCount > 0) && (entry.m_nCompressedSize == 0))
					throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable page size mismatch");

				if ((entry.m_nFileOffset < sizeof(m_Header) + sizeof(sDataTableV2PageHeader)) || (entry.m_nFileOffset > m_nFooterStart) || (entry.m_nCompressedSize > m_nFooterStart - entry.m_nFileOffset))
					throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable page lies outside of the segment data");

				if (nColumnIndex == 0)
					nChunkRowCount = entry.m_nRowCount;
				else if (entry.m_nRowCount != nChunkRowCount)
					throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable chunk columns have different row counts");
			}

			nTotalRowCount += nChunkRowCount;
		}

		if (nTotalRowCount != m_FooterHeader.m_nRowCount)
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "datatable segment row count mismatch");
	}

	uint32_t CDataTableV2SegmentReader::getSegmentIndex() const
	{
		return m_Header.m_nSegmentIndex;
	}

	uint64_t CDataTableV2SegmentReader::getFirstRowIndex() const
	{
		return m_Header.m_nFirstRowIndex;
	}

	uint64_t CDataTableV2SegmentReader::getRowCount() const
	{
		return m_FooterHeader.m_nRowCount;
	}

	uint32_t CDataTableV2SegmentReader::getChunkCount() const
	{
		return m_FooterHeader.m_nChunkCount;
	}

	uint32_t CDataTableV2SegmentReader::getColumnCount() const
	{
		return (uint32_t)m_Columns.size();
	}

	const sDataTableV2Column& CDataTableV2SegmentReader::getColumn(uint32_t nColumnIndex) const
	{
		if (nColumnIndex >= m_Columns.size())
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "invalid datatable column index");

		return m_Columns[nColumnIndex];
	}

	uint32_t CDataTableV2SegmentReader::getChunkRowCount(uint32_t nChunkIndex) const
	{
		if (m_Columns.empty())
			return 0;

		return getPageEntry(nChunkIndex, 0).m_nRowCount;
	}

	const sDataTableV2PageEntry& CDataTableV2SegmentReader::getPageEntry(uint32_t nChunkIndex, uint32_t nColumnIndex) const
	{
		if ((nChunkIndex >= m_FooterHeader.m_nChunkCount) || (nColumnIndex >= m_Columns.size()))
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "invalid datatable page index");

		return m_Directory[(size_t)nChunkIndex * m_Columns.size() + nColumnIndex];
	}

	void CDataTableV2SegmentReader::readPage(uint32_t nChunkIndex, uint32_t nColumnIndex, uint8_t* pTarget, uint64_t nTargetSize)
	{
		auto& entry = getPageEntry(nChunkIndex, nColumnIndex);
		auto& column = m_Columns[nColumnIndex];

		size_t nRowCount = entry.m_nRowCount;
		size_t nPlainSize = nRowCount * m_EntrySizes[nColumnIndex];
		if (nPlainSize == 0)
			return;

		if ((pTarget == nullptr) || (nTargetSize < nPlainSize))
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable page target buffer is too small");

		m_nPageReadCount++;

		// The common uncompressed plain page is read straight into the target.
		if ((entry.m_nCodec == DATATABLE_V2_CODEC_NONE) && (entry.m_nEncoding == DATATABLE_V2_ENCODING_PLAIN)) {
			m_ReadCallback(entry.m_nFileOffset, entry.m_nCompressedSize, pTarget);
			return;
		}

		m_ReadScratch.resize(entry.m_nCompressedSize);
		m_ReadCallback(entry.m_nFileOffset, entry.m_nCompressedSize, m_ReadScratch.data());

		// A compressed plain page is decompressed straight into the target.
		if (entry.m_nEncoding == DATATABLE_V2_ENCODING_PLAIN) {
			CDataTableEncoding::decompress(entry.m_nCodec, m_ReadScratch.data(), m_ReadScratch.size(), pTarget, nPlainSize);
			return;
		}

		const uint8_t* pEncodedData = m_ReadScratch.data();
		size_t nEncodedSize = m_ReadScratch.size();
		if (entry.m_nCodec != DATATABLE_V2_CODEC_NONE) {
			m_DecompressScratch.resize(entry.m_nUncompressedSize);
			CDataTableEncoding::decompress(entry.m_nCodec, m_ReadScratch.data(), m_ReadScratch.size(), m_DecompressScratch.data(), m_DecompressScratch.size());
			pEncodedData = m_DecompressScratch.data();
			nEncodedSize = m_DecompressScratch.size();
		}

		if (entry.m_nEncoding == DATATABLE_V2_ENCODING_BITPACK) {
			CDataTableEncoding::decodeBitPack(pEncodedData, nEncodedSize, pTarget, nRowCount);
			return;
		}

		// DELTA: 64 bit integers, narrowed or dequantised according to the column type.
		m_IntegerScratch.resize(nRowCount);
		CDataTableEncoding::decodeDelta(pEncodedData, nEncodedSize, m_IntegerScratch.data(), nRowCount);

		switch (column.m_DataType) {
		case eDataTableV2DataType::Int32:
		case eDataTableV2DataType::Uint32:
			for (size_t nIndex = 0; nIndex < nRowCount; nIndex++) {
				uint32_t nValue = (uint32_t)m_IntegerScratch[nIndex];
				memcpy(pTarget + nIndex * sizeof(uint32_t), &nValue, sizeof(uint32_t));
			}
			break;

		case eDataTableV2DataType::Double:
			for (size_t nIndex = 0; nIndex < nRowCount; nIndex++) {
				double dValue = (double)m_IntegerScratch[nIndex] * column.m_dQuantizationStep;
				memcpy(pTarget + nIndex * sizeof(double), &dValue, sizeof(double));
			}
			break;

		default:
			memcpy(pTarget, m_IntegerScratch.data(), nRowCount * sizeof(int64_t));
			break;
		}
	}

	uint64_t CDataTableV2SegmentReader::getPageReadCount() const
	{
		return m_nPageReadCount;
	}

}
