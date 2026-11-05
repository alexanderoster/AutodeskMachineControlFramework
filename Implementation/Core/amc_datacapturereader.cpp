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

#include "amc_datacapturereader.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace AMC {

	namespace {

		template <typename T>
		T loadPlainValue(const uint8_t* pData, uint64_t nIndex)
		{
			T value;
			memcpy(&value, pData + nIndex * sizeof(T), sizeof(T));
			return value;
		}

		template <typename T>
		void convertRunToDouble(const uint8_t* pData, uint64_t nCount, double* pTarget)
		{
			for (uint64_t nIndex = 0; nIndex < nCount; nIndex++)
				pTarget[nIndex] = (double)loadPlainValue<T>(pData, nIndex);
		}

		void convertPlainToDouble(eDataTableV2DataType dataType, const uint8_t* pData, uint64_t nCount, double* pTarget)
		{
			switch (dataType) {
			case eDataTableV2DataType::Double: memcpy(pTarget, pData, (size_t)nCount * sizeof(double)); break;
			case eDataTableV2DataType::Float: convertRunToDouble<float>(pData, nCount, pTarget); break;
			case eDataTableV2DataType::Int32: convertRunToDouble<int32_t>(pData, nCount, pTarget); break;
			case eDataTableV2DataType::Uint32: convertRunToDouble<uint32_t>(pData, nCount, pTarget); break;
			case eDataTableV2DataType::Int64: convertRunToDouble<int64_t>(pData, nCount, pTarget); break;
			case eDataTableV2DataType::Uint64: convertRunToDouble<uint64_t>(pData, nCount, pTarget); break;
			case eDataTableV2DataType::Timestamp: convertRunToDouble<uint64_t>(pData, nCount, pTarget); break;
			case eDataTableV2DataType::Bool: convertRunToDouble<uint8_t>(pData, nCount, pTarget); break;
			default:
				throw EDataTableFormatException(eDataTableFormatError::InvalidDataType, "invalid datatable column data type");
			}
		}

	}


	/*************************************************************************************************************************
	 One segment stream that is open for reading. The storage stream MUST outlive the segment reader,
	 because the read callbacks belong to the stream instance.
	**************************************************************************************************************************/

	class CDataCaptureOpenSegment {
	public:
		LibMCData::PStorageStream m_pStream;
		std::unique_ptr<CDataTableV2SegmentReader> m_pReader;

		// Row index of the first row of each chunk relative to the segment, followed by the segment row count.
		std::vector<uint64_t> m_ChunkFirstRows;

		CDataCaptureOpenSegment(LibMCData::PStorageStream pStream)
			: m_pStream(pStream)
		{
			void* pReadCallback = nullptr;
			void* pSeekCallback = nullptr;
			void* pUserData = nullptr;
			m_pStream->GetCallbacks(pReadCallback, pSeekCallback, pUserData);

			if ((pReadCallback == nullptr) || (pSeekCallback == nullptr) || (pUserData == nullptr))
				throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "datatable segment stream has no read access");

			auto readCallback = [pReadCallback, pSeekCallback, pUserData](uint64_t nOffset, uint64_t nSize, uint8_t* pBuffer) {
				if (nSize == 0)
					return;

				uint32_t nSeekErrorCode = 0;
				((LibMCData::StreamSeekCallback)pSeekCallback)(nOffset, pUserData, &nSeekErrorCode);
				if (nSeekErrorCode != 0)
					throw EDataTableFormatException(eDataTableFormatError::Corrupt, "could not seek in datatable segment stream, error " + std::to_string(nSeekErrorCode));

				uint32_t nReadErrorCode = 0;
				((LibMCData::StreamReadCallback)pReadCallback)((void*)pBuffer, nSize, pUserData, &nReadErrorCode);
				if (nReadErrorCode != 0)
					throw EDataTableFormatException(eDataTableFormatError::Corrupt, "could not read from datatable segment stream, error " + std::to_string(nReadErrorCode));
			};

			m_pReader = std::make_unique<CDataTableV2SegmentReader>(readCallback, m_pStream->GetSize());

			uint32_t nChunkCount = m_pReader->getChunkCount();
			uint64_t nRow = 0;
			m_ChunkFirstRows.reserve((size_t)nChunkCount + 1);
			for (uint32_t nChunkIndex = 0; nChunkIndex < nChunkCount; nChunkIndex++) {
				m_ChunkFirstRows.push_back(nRow);
				nRow += m_pReader->getChunkRowCount(nChunkIndex);
			}
			m_ChunkFirstRows.push_back(nRow);
		}
	};


	CDataCaptureReader::CDataCaptureReader(LibMCData::PDataModel pDataModel, LibMCData::PDataCapture pManifest)
		: m_pDataModel(pDataModel), m_nRowCount(0), m_nChunkCount(0), m_nPageReadCount(0),
		m_bPageCached(false), m_nCachedSegmentIndex(0), m_nCachedChunkIndex(0), m_nCachedColumnIndex(0)
	{
		if (pDataModel.get() == nullptr)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "data capture reader has no data model");
		if (pManifest.get() == nullptr)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "data capture reader has no manifest");

		m_pStorage = m_pDataModel->CreateStorage();

		m_sUUID = pManifest->GetUUID();
		m_sIdentifier = pManifest->GetIdentifier();
		m_sName = pManifest->GetName();

		uint32_t nColumnCount = pManifest->GetColumnCount();
		for (uint32_t nColumnIndex = 0; nColumnIndex < nColumnCount; nColumnIndex++) {
			sDataTableV2Column column;
			uint32_t nColumnType = 0;
			pManifest->GetColumnInformation(nColumnIndex, column.m_sIdentifier, column.m_sDescription, nColumnType);

			if (!CDataTableV2::dataTypeIsValid(nColumnType))
				throw EDataTableFormatException(eDataTableFormatError::InvalidDataType, "invalid data capture column type: " + std::to_string(nColumnType));

			column.m_DataType = (eDataTableV2DataType)nColumnType;
			m_EntrySizes.push_back(CDataTableV2::getEntrySize(column.m_DataType));
			m_Columns.push_back(column);
		}

		uint32_t nSegmentCount = pManifest->GetSegmentCount();
		for (uint32_t nSegmentIndex = 0; nSegmentIndex < nSegmentCount; nSegmentIndex++) {
			sDataCaptureSegmentInfo segment;
			pManifest->GetSegmentInformation(nSegmentIndex, segment.m_sStreamUUID, segment.m_nFirstRow, segment.m_nRowCount, segment.m_nSizeInBytes, segment.m_bIsPurged);
			segment.m_nChunkCount = pManifest->GetSegmentChunkCount(nSegmentIndex);
			segment.m_nFirstChunkIndex = m_nChunkCount;

			if (segment.m_nFirstRow != m_nRowCount)
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "data capture segments are not contiguous");

			m_nRowCount += segment.m_nRowCount;
			m_nChunkCount += segment.m_nChunkCount;
			m_Segments.push_back(segment);
		}
	}

	CDataCaptureReader::~CDataCaptureReader()
	{
	}

	std::string CDataCaptureReader::getUUID() const
	{
		return m_sUUID;
	}

	std::string CDataCaptureReader::getIdentifier() const
	{
		return m_sIdentifier;
	}

	std::string CDataCaptureReader::getName() const
	{
		return m_sName;
	}

	uint64_t CDataCaptureReader::getRowCount() const
	{
		return m_nRowCount;
	}

	uint64_t CDataCaptureReader::getFirstAvailableRow() const
	{
		for (auto& segment : m_Segments) {
			if (!segment.m_bIsPurged)
				return segment.m_nFirstRow;
		}

		return m_nRowCount;
	}

	uint32_t CDataCaptureReader::getColumnCount() const
	{
		return (uint32_t)m_Columns.size();
	}

	void CDataCaptureReader::checkColumnIndex(uint32_t nColumnIndex) const
	{
		if (nColumnIndex >= m_Columns.size())
			throw EDataTableFormatException(eDataTableFormatError::ColumnNotFound, "invalid data capture column index: " + std::to_string(nColumnIndex));
	}

	void CDataCaptureReader::checkRowRange(uint64_t nFromRow, uint64_t nRowCount) const
	{
		if ((nFromRow > m_nRowCount) || (nRowCount > m_nRowCount - nFromRow))
			throw EDataTableFormatException(eDataTableFormatError::InvalidRange, "row range " + std::to_string(nFromRow) + " + " + std::to_string(nRowCount) + " exceeds the " + std::to_string(m_nRowCount) + " rows of the data capture");
	}

	void CDataCaptureReader::validateRowRange(uint64_t nFromRow, uint64_t nRowCount) const
	{
		checkRowRange(nFromRow, nRowCount);
	}

	const sDataTableV2Column& CDataCaptureReader::getColumn(uint32_t nColumnIndex) const
	{
		checkColumnIndex(nColumnIndex);
		return m_Columns[nColumnIndex];
	}

	bool CDataCaptureReader::findColumn(const std::string& sIdentifier, uint32_t& nColumnIndex) const
	{
		for (size_t nIndex = 0; nIndex < m_Columns.size(); nIndex++) {
			if (m_Columns[nIndex].m_sIdentifier == sIdentifier) {
				nColumnIndex = (uint32_t)nIndex;
				return true;
			}
		}

		return false;
	}

	uint32_t CDataCaptureReader::getColumnIndex(const std::string& sIdentifier) const
	{
		uint32_t nColumnIndex = 0;
		if (!findColumn(sIdentifier, nColumnIndex))
			throw EDataTableFormatException(eDataTableFormatError::ColumnNotFound, "data capture column not found: " + sIdentifier);

		return nColumnIndex;
	}

	double CDataCaptureReader::getColumnQuantization(uint32_t nColumnIndex)
	{
		checkColumnIndex(nColumnIndex);

		// The quantisation step is part of the segment schema.
		for (size_t nSegmentIndex = 0; nSegmentIndex < m_Segments.size(); nSegmentIndex++) {
			if (!m_Segments[nSegmentIndex].m_bIsPurged)
				return openSegment((uint32_t)nSegmentIndex)->m_pReader->getColumn(nColumnIndex).m_dQuantizationStep;
		}

		return 0.0;
	}

	uint32_t CDataCaptureReader::getSegmentCount() const
	{
		return (uint32_t)m_Segments.size();
	}

	const sDataCaptureSegmentInfo& CDataCaptureReader::getSegment(uint32_t nSegmentIndex) const
	{
		if (nSegmentIndex >= m_Segments.size())
			throw EDataTableFormatException(eDataTableFormatError::InvalidRange, "invalid data capture segment index: " + std::to_string(nSegmentIndex));

		return m_Segments[nSegmentIndex];
	}

	uint64_t CDataCaptureReader::getChunkCount() const
	{
		return m_nChunkCount;
	}

	uint32_t CDataCaptureReader::findSegmentOfRow(uint64_t nRow) const
	{
		// The first segment whose first row is beyond the row, minus one.
		auto iIter = std::upper_bound(m_Segments.begin(), m_Segments.end(), nRow, [](uint64_t nValue, const sDataCaptureSegmentInfo& segment) {
			return nValue < segment.m_nFirstRow;
		});

		if (iIter == m_Segments.begin())
			throw EDataTableFormatException(eDataTableFormatError::InvalidRange, "row is not part of the data capture: " + std::to_string(nRow));

		return (uint32_t)(iIter - m_Segments.begin()) - 1;
	}

	CDataCaptureOpenSegment* CDataCaptureReader::openSegment(uint32_t nSegmentIndex)
	{
		auto& segment = getSegment(nSegmentIndex);

		auto iIter = m_OpenSegments.find(nSegmentIndex);
		if (iIter != m_OpenSegments.end()) {
			m_OpenSegmentOrder.remove(nSegmentIndex);
			m_OpenSegmentOrder.push_back(nSegmentIndex);
			return iIter->second.get();
		}

		if (segment.m_bIsPurged)
			throw EDataTableFormatException(eDataTableFormatError::SegmentPurged, "data capture segment " + std::to_string(nSegmentIndex) + " has been deleted by retention");

		auto pOpenSegment = std::make_shared<CDataCaptureOpenSegment>(m_pStorage->RetrieveStream(segment.m_sStreamUUID));

		// The segment must be the one the manifest describes.
		auto pReader = pOpenSegment->m_pReader.get();
		bool bMatches = (pReader->getRowCount() == segment.m_nRowCount) && (pReader->getChunkCount() == segment.m_nChunkCount) && (pReader->getFirstRowIndex() == segment.m_nFirstRow) && (pReader->getColumnCount() == m_Columns.size());
		for (uint32_t nColumnIndex = 0; bMatches && (nColumnIndex < m_Columns.size()); nColumnIndex++)
			bMatches = (pReader->getColumn(nColumnIndex).m_DataType == m_Columns[nColumnIndex].m_DataType);
		if (!bMatches)
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "data capture segment " + std::to_string(nSegmentIndex) + " does not match the manifest");

		// Least recently used segments are closed first.
		while (m_OpenSegments.size() >= DATACAPTUREREADER_MAXOPENSEGMENTS) {
			m_OpenSegments.erase(m_OpenSegmentOrder.front());
			m_OpenSegmentOrder.pop_front();
		}

		m_OpenSegments.insert(std::make_pair(nSegmentIndex, pOpenSegment));
		m_OpenSegmentOrder.push_back(nSegmentIndex);

		return pOpenSegment.get();
	}

	sDataCaptureChunkInfo CDataCaptureReader::getChunkInfo(uint64_t nChunkIndex)
	{
		if (nChunkIndex >= m_nChunkCount)
			throw EDataTableFormatException(eDataTableFormatError::InvalidRange, "invalid data capture chunk index: " + std::to_string(nChunkIndex));

		auto iIter = std::upper_bound(m_Segments.begin(), m_Segments.end(), nChunkIndex, [](uint64_t nValue, const sDataCaptureSegmentInfo& segment) {
			return nValue < segment.m_nFirstChunkIndex;
		});

		// Segments without chunks share their first chunk index with the next segment, so step back to the one that holds the chunk.
		uint32_t nSegmentIndex = (uint32_t)(iIter - m_Segments.begin()) - 1;
		auto& segment = m_Segments[nSegmentIndex];

		sDataCaptureChunkInfo chunkInfo;
		chunkInfo.m_nSegmentIndex = nSegmentIndex;
		chunkInfo.m_nFirstRow = segment.m_nFirstRow;
		chunkInfo.m_nRowCount = 0;
		chunkInfo.m_bIsAvailable = !segment.m_bIsPurged;

		if (chunkInfo.m_bIsAvailable) {
			auto pOpenSegment = openSegment(nSegmentIndex);
			size_t nChunkInSegment = (size_t)(nChunkIndex - segment.m_nFirstChunkIndex);
			chunkInfo.m_nFirstRow = segment.m_nFirstRow + pOpenSegment->m_ChunkFirstRows[nChunkInSegment];
			chunkInfo.m_nRowCount = pOpenSegment->m_ChunkFirstRows[nChunkInSegment + 1] - pOpenSegment->m_ChunkFirstRows[nChunkInSegment];
		}

		return chunkInfo;
	}

	bool CDataCaptureReader::getChunkStatistics(uint64_t nChunkIndex, uint32_t nColumnIndex, double& dMinValue, double& dMaxValue)
	{
		checkColumnIndex(nColumnIndex);

		dMinValue = 0.0;
		dMaxValue = 0.0;

		auto chunkInfo = getChunkInfo(nChunkIndex);
		if (!chunkInfo.m_bIsAvailable)
			return false;

		auto& segment = m_Segments[chunkInfo.m_nSegmentIndex];
		auto pOpenSegment = openSegment(chunkInfo.m_nSegmentIndex);
		auto& entry = pOpenSegment->m_pReader->getPageEntry((uint32_t)(nChunkIndex - segment.m_nFirstChunkIndex), nColumnIndex);

		if ((entry.m_nFlags & DATATABLE_V2_PAGEFLAG_STATISTICSVALID) == 0)
			return false;

		dMinValue = CDataTableV2::statisticsValueToDouble(m_Columns[nColumnIndex].m_DataType, entry.m_nMinValue);
		dMaxValue = CDataTableV2::statisticsValueToDouble(m_Columns[nColumnIndex].m_DataType, entry.m_nMaxValue);
		return true;
	}

	// Returns the plain values of a page. The last page stays in the scratch buffer, so that
	// consecutive requests for the same page cost one read.
	const uint8_t* CDataCaptureReader::loadPage(CDataCaptureOpenSegment* pOpenSegment, uint32_t nSegmentIndex, uint32_t nChunkIndex, uint32_t nColumnIndex, uint64_t nChunkRowCount)
	{
		if (m_bPageCached && (m_nCachedSegmentIndex == nSegmentIndex) && (m_nCachedChunkIndex == nChunkIndex) && (m_nCachedColumnIndex == nColumnIndex))
			return m_PageScratch.data();

		m_bPageCached = false;
		m_PageScratch.resize((size_t)(nChunkRowCount * m_EntrySizes[nColumnIndex]));
		pOpenSegment->m_pReader->readPage(nChunkIndex, nColumnIndex, m_PageScratch.data(), m_PageScratch.size());
		m_nPageReadCount++;

		m_nCachedSegmentIndex = nSegmentIndex;
		m_nCachedChunkIndex = nChunkIndex;
		m_nCachedColumnIndex = nColumnIndex;
		m_bPageCached = true;

		return m_PageScratch.data();
	}

	void CDataCaptureReader::forEachPageInRange(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, const std::function<void(const uint8_t* pPlainValues, uint64_t nRangeOffset, uint64_t nCount)>& callback)
	{
		checkColumnIndex(nColumnIndex);
		checkRowRange(nFromRow, nRowCount);

		if (nRowCount == 0)
			return;

		uint32_t nEntrySize = m_EntrySizes[nColumnIndex];
		uint64_t nRow = nFromRow;
		uint64_t nEndRow = nFromRow + nRowCount;
		uint32_t nSegmentIndex = findSegmentOfRow(nRow);

		while (nRow < nEndRow) {
			auto& segment = m_Segments[nSegmentIndex];
			uint64_t nSegmentEndRow = segment.m_nFirstRow + segment.m_nRowCount;

			// Skips segments without rows.
			if (nRow >= nSegmentEndRow) {
				nSegmentIndex++;
				continue;
			}

			if (segment.m_bIsPurged)
				throw EDataTableFormatException(eDataTableFormatError::SegmentPurged, "rows " + std::to_string(segment.m_nFirstRow) + " to " + std::to_string(nSegmentEndRow - 1) + " of the data capture have been deleted by retention");

			auto pOpenSegment = openSegment(nSegmentIndex);
			auto& chunkFirstRows = pOpenSegment->m_ChunkFirstRows;

			// The chunk that holds the row: the first chunk that starts beyond it, minus one.
			uint64_t nRowInSegment = nRow - segment.m_nFirstRow;
			size_t nChunkIndex = (size_t)(std::upper_bound(chunkFirstRows.begin(), chunkFirstRows.end(), nRowInSegment) - chunkFirstRows.begin()) - 1;

			while ((nRow < nEndRow) && (nRow < nSegmentEndRow)) {
				uint64_t nChunkRowCount = chunkFirstRows[nChunkIndex + 1] - chunkFirstRows[nChunkIndex];
				uint64_t nOffsetInChunk = nRow - segment.m_nFirstRow - chunkFirstRows[nChunkIndex];
				uint64_t nCount = (std::min)(nChunkRowCount - nOffsetInChunk, nEndRow - nRow);

				const uint8_t* pPageData = loadPage(pOpenSegment, nSegmentIndex, (uint32_t)nChunkIndex, nColumnIndex, nChunkRowCount);

				callback(pPageData + nOffsetInChunk * nEntrySize, nRow - nFromRow, nCount);

				nRow += nCount;
				nChunkIndex++;
			}

			nSegmentIndex++;
		}
	}

	double CDataCaptureReader::plainValueToDouble(uint32_t nColumnIndex, const uint8_t* pPlainValues, uint64_t nIndex) const
	{
		double dValue = 0.0;
		convertPlainToDouble(m_Columns[nColumnIndex].m_DataType, pPlainValues + nIndex * m_EntrySizes[nColumnIndex], 1, &dValue);
		return dValue;
	}

	void CDataCaptureReader::readColumnPlain(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, uint8_t* pTarget, uint64_t nTargetSize)
	{
		checkColumnIndex(nColumnIndex);
		checkRowRange(nFromRow, nRowCount);

		if (nRowCount == 0)
			return;

		uint32_t nEntrySize = m_EntrySizes[nColumnIndex];
		if ((pTarget == nullptr) || (nTargetSize / nEntrySize < nRowCount))
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "data capture read buffer is too small");

		forEachPageInRange(nColumnIndex, nFromRow, nRowCount, [pTarget, nEntrySize](const uint8_t* pPlainValues, uint64_t nRangeOffset, uint64_t nCount) {
			memcpy(pTarget + nRangeOffset * nEntrySize, pPlainValues, (size_t)(nCount * nEntrySize));
		});
	}

	void CDataCaptureReader::readColumnAsDouble(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, double* pTarget)
	{
		checkColumnIndex(nColumnIndex);
		checkRowRange(nFromRow, nRowCount);

		if (nRowCount == 0)
			return;
		if (pTarget == nullptr)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "data capture read buffer is null");

		auto dataType = m_Columns[nColumnIndex].m_DataType;
		forEachPageInRange(nColumnIndex, nFromRow, nRowCount, [pTarget, dataType](const uint8_t* pPlainValues, uint64_t nRangeOffset, uint64_t nCount) {
			convertPlainToDouble(dataType, pPlainValues, nCount, pTarget + nRangeOffset);
		});
	}

	void CDataCaptureReader::readColumnAsFloat(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, float* pTarget)
	{
		checkColumnIndex(nColumnIndex);
		checkRowRange(nFromRow, nRowCount);

		auto dataType = m_Columns[nColumnIndex].m_DataType;
		if ((dataType != eDataTableV2DataType::Float) && (dataType != eDataTableV2DataType::Double))
			throw EDataTableFormatException(eDataTableFormatError::TypeMismatch, "data capture column " + m_Columns[nColumnIndex].m_sIdentifier + " is not a floating point column");

		if (nRowCount == 0)
			return;
		if (pTarget == nullptr)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "data capture read buffer is null");

		forEachPageInRange(nColumnIndex, nFromRow, nRowCount, [pTarget, dataType](const uint8_t* pPlainValues, uint64_t nRangeOffset, uint64_t nCount) {
			if (dataType == eDataTableV2DataType::Float) {
				memcpy(pTarget + nRangeOffset, pPlainValues, (size_t)nCount * sizeof(float));
			}
			else {
				for (uint64_t nIndex = 0; nIndex < nCount; nIndex++)
					pTarget[nRangeOffset + nIndex] = (float)loadPlainValue<double>(pPlainValues, nIndex);
			}
		});
	}

	void CDataCaptureReader::readColumnAsInt64(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, int64_t* pTarget)
	{
		checkColumnIndex(nColumnIndex);
		checkRowRange(nFromRow, nRowCount);

		auto dataType = m_Columns[nColumnIndex].m_DataType;
		if ((dataType == eDataTableV2DataType::Float) || (dataType == eDataTableV2DataType::Double))
			throw EDataTableFormatException(eDataTableFormatError::TypeMismatch, "data capture column " + m_Columns[nColumnIndex].m_sIdentifier + " is not an integer column");

		if (nRowCount == 0)
			return;
		if (pTarget == nullptr)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "data capture read buffer is null");

		std::string sIdentifier = m_Columns[nColumnIndex].m_sIdentifier;
		forEachPageInRange(nColumnIndex, nFromRow, nRowCount, [pTarget, dataType, &sIdentifier](const uint8_t* pPlainValues, uint64_t nRangeOffset, uint64_t nCount) {
			int64_t* pRunTarget = pTarget + nRangeOffset;

			switch (dataType) {
			case eDataTableV2DataType::Int32:
				for (uint64_t nIndex = 0; nIndex < nCount; nIndex++)
					pRunTarget[nIndex] = (int64_t)loadPlainValue<int32_t>(pPlainValues, nIndex);
				break;
			case eDataTableV2DataType::Uint32:
				for (uint64_t nIndex = 0; nIndex < nCount; nIndex++)
					pRunTarget[nIndex] = (int64_t)loadPlainValue<uint32_t>(pPlainValues, nIndex);
				break;
			case eDataTableV2DataType::Int64:
				memcpy(pRunTarget, pPlainValues, (size_t)nCount * sizeof(int64_t));
				break;
			case eDataTableV2DataType::Bool:
				for (uint64_t nIndex = 0; nIndex < nCount; nIndex++)
					pRunTarget[nIndex] = (int64_t)loadPlainValue<uint8_t>(pPlainValues, nIndex);
				break;
			default:
				// Uint64 and Timestamp: values beyond the signed range can not be returned.
				for (uint64_t nIndex = 0; nIndex < nCount; nIndex++) {
					uint64_t nValue = loadPlainValue<uint64_t>(pPlainValues, nIndex);
					if (nValue > (uint64_t)(std::numeric_limits<int64_t>::max)())
						throw EDataTableFormatException(eDataTableFormatError::TypeMismatch, "data capture column " + sIdentifier + " holds a value beyond the signed 64 bit range");
					pRunTarget[nIndex] = (int64_t)nValue;
				}
				break;
			}
		});
	}

	uint64_t CDataCaptureReader::getDownsampleBucketCount(uint64_t nRowCount, uint32_t nTargetPoints)
	{
		if (nTargetPoints == 0)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "downsample target point count is zero");

		return (std::min)(nRowCount, (uint64_t)nTargetPoints);
	}

	uint64_t CDataCaptureReader::getDownsampleValueCount(uint64_t nRowCount, uint32_t nTargetPoints, eDataCaptureDownsampleMode mode)
	{
		uint64_t nBucketCount = getDownsampleBucketCount(nRowCount, nTargetPoints);

		switch (mode) {
		case eDataCaptureDownsampleMode::Stride:
		case eDataCaptureDownsampleMode::Average:
			return nBucketCount;
		case eDataCaptureDownsampleMode::MinMax:
			return 2 * nBucketCount;
		default:
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "invalid downsample mode");
		}
	}

	void CDataCaptureReader::readColumnDownsampled(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, uint32_t nTargetPoints, eDataCaptureDownsampleMode mode, double* pTarget)
	{
		checkColumnIndex(nColumnIndex);
		checkRowRange(nFromRow, nRowCount);

		// Validates the target point count and the mode.
		getDownsampleValueCount(nRowCount, nTargetPoints, mode);

		uint64_t nBucketCount = getDownsampleBucketCount(nRowCount, nTargetPoints);
		if (nBucketCount == 0)
			return;
		if (pTarget == nullptr)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "data capture read buffer is null");

		// Bucket b covers the rows [bucketStart(b), bucketStart(b + 1)) relative to the range.
		// Every bucket holds at least one row, because there are not more buckets than rows.
		uint64_t nQuotient = nRowCount / nBucketCount;
		uint64_t nRemainder = nRowCount % nBucketCount;
		auto bucketStart = [nQuotient, nRemainder, nBucketCount](uint64_t nBucketIndex) {
			return nBucketIndex * nQuotient + (nBucketIndex * nRemainder) / nBucketCount;
		};

		auto dataType = m_Columns[nColumnIndex].m_DataType;
		uint32_t nEntrySize = m_EntrySizes[nColumnIndex];
		double dNaN = std::numeric_limits<double>::quiet_NaN();

		if (mode == eDataCaptureDownsampleMode::Stride) {
			// Only the pages that hold the first row of a bucket are read. Consecutive buckets
			// that start in the same page share one read through the page cache.
			for (uint64_t nBucketIndex = 0; nBucketIndex < nBucketCount; nBucketIndex++)
				readColumnAsDouble(nColumnIndex, nFromRow + bucketStart(nBucketIndex), 1, pTarget + nBucketIndex);
			return;
		}

		std::vector<double> bucketMin((size_t)nBucketCount, dNaN);
		std::vector<double> bucketMax((size_t)nBucketCount, dNaN);
		std::vector<double> bucketSum((size_t)nBucketCount, 0.0);
		std::vector<uint64_t> bucketValueCount((size_t)nBucketCount, 0);

		auto accumulate = [&](uint64_t nBucketIndex, double dMinValue, double dMaxValue) {
			if (std::isnan(bucketMin[(size_t)nBucketIndex]) || (dMinValue < bucketMin[(size_t)nBucketIndex]))
				bucketMin[(size_t)nBucketIndex] = dMinValue;
			if (std::isnan(bucketMax[(size_t)nBucketIndex]) || (dMaxValue > bucketMax[(size_t)nBucketIndex]))
				bucketMax[(size_t)nBucketIndex] = dMaxValue;
		};

		// Walks the chunks of the range. In MinMax mode a chunk that lies completely inside one bucket
		// contributes its page statistics and is not read at all.
		uint64_t nRow = nFromRow;
		uint64_t nEndRow = nFromRow + nRowCount;
		uint32_t nSegmentIndex = findSegmentOfRow(nRow);
		uint64_t nBucketIndex = 0;
		uint64_t nBucketEnd = bucketStart(1);
		std::vector<double> runValues;

		while (nRow < nEndRow) {
			auto& segment = m_Segments[nSegmentIndex];
			uint64_t nSegmentEndRow = segment.m_nFirstRow + segment.m_nRowCount;

			if (nRow >= nSegmentEndRow) {
				nSegmentIndex++;
				continue;
			}

			if (segment.m_bIsPurged)
				throw EDataTableFormatException(eDataTableFormatError::SegmentPurged, "rows " + std::to_string(segment.m_nFirstRow) + " to " + std::to_string(nSegmentEndRow - 1) + " of the data capture have been deleted by retention");

			auto pOpenSegment = openSegment(nSegmentIndex);
			auto& chunkFirstRows = pOpenSegment->m_ChunkFirstRows;

			uint64_t nRowInSegment = nRow - segment.m_nFirstRow;
			size_t nChunkIndex = (size_t)(std::upper_bound(chunkFirstRows.begin(), chunkFirstRows.end(), nRowInSegment) - chunkFirstRows.begin()) - 1;

			while ((nRow < nEndRow) && (nRow < nSegmentEndRow)) {
				uint64_t nChunkRowCount = chunkFirstRows[nChunkIndex + 1] - chunkFirstRows[nChunkIndex];
				uint64_t nOffsetInChunk = nRow - segment.m_nFirstRow - chunkFirstRows[nChunkIndex];
				uint64_t nCount = (std::min)(nChunkRowCount - nOffsetInChunk, nEndRow - nRow);

				uint64_t nRangeOffset = nRow - nFromRow;
				while (nRangeOffset >= nBucketEnd) {
					nBucketIndex++;
					nBucketEnd = bucketStart(nBucketIndex + 1);
				}

				bool bWholeChunkInBucket = (nOffsetInChunk == 0) && (nCount == nChunkRowCount) && (nRangeOffset + nCount <= nBucketEnd);

				if ((mode == eDataCaptureDownsampleMode::MinMax) && bWholeChunkInBucket) {
					auto& entry = pOpenSegment->m_pReader->getPageEntry((uint32_t)nChunkIndex, nColumnIndex);

					// A page without statistics holds no comparable value and contributes nothing.
					if ((entry.m_nFlags & DATATABLE_V2_PAGEFLAG_STATISTICSVALID) != 0)
						accumulate(nBucketIndex, CDataTableV2::statisticsValueToDouble(dataType, entry.m_nMinValue), CDataTableV2::statisticsValueToDouble(dataType, entry.m_nMaxValue));
				}
				else {
					const uint8_t* pPageData = loadPage(pOpenSegment, nSegmentIndex, (uint32_t)nChunkIndex, nColumnIndex, nChunkRowCount);

					runValues.resize((size_t)nCount);
					convertPlainToDouble(dataType, pPageData + nOffsetInChunk * nEntrySize, nCount, runValues.data());

					for (uint64_t nIndex = 0; nIndex < nCount; nIndex++) {
						while (nRangeOffset + nIndex >= nBucketEnd) {
							nBucketIndex++;
							nBucketEnd = bucketStart(nBucketIndex + 1);
						}

						double dValue = runValues[(size_t)nIndex];
						if (std::isnan(dValue))
							continue;

						accumulate(nBucketIndex, dValue, dValue);
						bucketSum[(size_t)nBucketIndex] += dValue;
						bucketValueCount[(size_t)nBucketIndex]++;
					}
				}

				nRow += nCount;
				nChunkIndex++;
			}

			nSegmentIndex++;
		}

		if (mode == eDataCaptureDownsampleMode::MinMax) {
			for (uint64_t nIndex = 0; nIndex < nBucketCount; nIndex++) {
				pTarget[2 * nIndex] = bucketMin[(size_t)nIndex];
				pTarget[2 * nIndex + 1] = bucketMax[(size_t)nIndex];
			}
		}
		else {
			for (uint64_t nIndex = 0; nIndex < nBucketCount; nIndex++)
				pTarget[nIndex] = (bucketValueCount[(size_t)nIndex] > 0) ? (bucketSum[(size_t)nIndex] / (double)bucketValueCount[(size_t)nIndex]) : dNaN;
		}
	}

	std::vector<uint64_t> CDataCaptureReader::findChunksInValueRange(uint32_t nColumnIndex, double dMinValue, double dMaxValue)
	{
		checkColumnIndex(nColumnIndex);

		std::vector<uint64_t> chunkIndices;
		if (std::isnan(dMinValue) || std::isnan(dMaxValue) || (dMinValue > dMaxValue))
			return chunkIndices;

		auto dataType = m_Columns[nColumnIndex].m_DataType;

		for (size_t nSegmentIndex = 0; nSegmentIndex < m_Segments.size(); nSegmentIndex++) {
			auto& segment = m_Segments[nSegmentIndex];
			if (segment.m_bIsPurged || (segment.m_nChunkCount == 0))
				continue;

			auto pOpenSegment = openSegment((uint32_t)nSegmentIndex);
			for (uint32_t nChunkInSegment = 0; nChunkInSegment < segment.m_nChunkCount; nChunkInSegment++) {
				auto& entry = pOpenSegment->m_pReader->getPageEntry(nChunkInSegment, nColumnIndex);

				// A page without statistics holds no comparable value and can not match.
				if ((entry.m_nFlags & DATATABLE_V2_PAGEFLAG_STATISTICSVALID) == 0)
					continue;

				double dPageMin = CDataTableV2::statisticsValueToDouble(dataType, entry.m_nMinValue);
				double dPageMax = CDataTableV2::statisticsValueToDouble(dataType, entry.m_nMaxValue);
				if ((dPageMax >= dMinValue) && (dPageMin <= dMaxValue))
					chunkIndices.push_back(segment.m_nFirstChunkIndex + nChunkInSegment);
			}
		}

		return chunkIndices;
	}

	uint64_t CDataCaptureReader::getPageReadCount() const
	{
		return m_nPageReadCount;
	}

}
