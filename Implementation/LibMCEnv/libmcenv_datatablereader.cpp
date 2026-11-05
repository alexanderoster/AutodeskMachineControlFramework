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


Abstract: This is the class definition of CDataTableReader

*/

#include "libmcenv_datatablereader.hpp"
#include "libmcenv_interfaceexception.hpp"

// Include custom headers here.
#include "libmcenv_datatable.hpp"
#include "common_utils.hpp"

#include <memory>
#include <vector>

using namespace LibMCEnv::Impl;

namespace {

	// Runs a call into the reader and reports its format errors as LibMCEnv errors.
	template <typename F>
	auto translateFormatErrors(F&& fn) -> decltype(fn())
	{
		try {
			return fn();
		}
		catch (const AMC::EDataTableFormatException& formatException) {
			throw ELibMCEnvInterfaceException(CDataTable::formatErrorToResult(formatException.getError()), formatException.what());
		}
	}

	AMC::eDataCaptureDownsampleMode convertDownsampleMode(const LibMCEnv::eDataTableDownsampleMode eMode)
	{
		switch (eMode) {
		case LibMCEnv::eDataTableDownsampleMode::Stride: return AMC::eDataCaptureDownsampleMode::Stride;
		case LibMCEnv::eDataTableDownsampleMode::MinMax: return AMC::eDataCaptureDownsampleMode::MinMax;
		case LibMCEnv::eDataTableDownsampleMode::Average: return AMC::eDataCaptureDownsampleMode::Average;
		default:
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDOWNSAMPLEPARAMETERS, "invalid downsample mode");
		}
	}

}

/*************************************************************************************************************************
 Class definition of CDataTableReader
**************************************************************************************************************************/

CDataTableReader* CDataTableReader::makeFromManifest(LibMCData::PDataModel pDataModel, LibMCData::PDataCapture pManifest, AMC::PToolpathHandler pToolpathHandler)
{
	auto pReader = translateFormatErrors([&]() {
		return std::make_shared<AMC::CDataCaptureReader>(pDataModel, pManifest);
	});

	return new CDataTableReader(pReader, pToolpathHandler);
}

CDataTableReader::CDataTableReader(AMC::PDataCaptureReader pReader, AMC::PToolpathHandler pToolpathHandler)
	: m_pReader(pReader), m_pToolpathHandler(pToolpathHandler)
{
	if (pReader.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
	if (pToolpathHandler.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

CDataTableReader::~CDataTableReader()
{

}

uint64_t CDataTableReader::getPageReadCount()
{
	return m_pReader->getPageReadCount();
}

LibMCEnv_uint64 CDataTableReader::GetRowCount()
{
	return m_pReader->getRowCount();
}

LibMCEnv_uint64 CDataTableReader::GetFirstAvailableRow()
{
	return m_pReader->getFirstAvailableRow();
}

LibMCEnv_uint32 CDataTableReader::GetColumnCount()
{
	return m_pReader->getColumnCount();
}

bool CDataTableReader::HasColumn(const std::string & sIdentifier)
{
	uint32_t nColumnIndex = 0;
	return m_pReader->findColumn(sIdentifier, nColumnIndex);
}

void CDataTableReader::GetColumnInformation(const LibMCEnv_uint32 nColumnIndex, std::string & sIdentifier, std::string & sDescription, LibMCEnv::eDataTableColumnType & eColumnType)
{
	if (nColumnIndex >= m_pReader->getColumnCount())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNINDEX);

	auto& column = m_pReader->getColumn(nColumnIndex);
	sIdentifier = column.m_sIdentifier;
	sDescription = column.m_sDescription;
	eColumnType = (LibMCEnv::eDataTableColumnType)column.m_DataType;
}

LibMCEnv_double CDataTableReader::GetColumnQuantization(const std::string & sIdentifier)
{
	return translateFormatErrors([&]() {
		return m_pReader->getColumnQuantization(m_pReader->getColumnIndex(sIdentifier));
	});
}

LibMCEnv_uint32 CDataTableReader::GetSegmentCount()
{
	return m_pReader->getSegmentCount();
}

void CDataTableReader::GetSegmentInformation(const LibMCEnv_uint32 nSegmentIndex, LibMCEnv_uint64 & nFirstRow, LibMCEnv_uint64 & nRowCount, LibMCEnv_uint64 & nSizeInBytes, bool & bIsAvailable)
{
	if (nSegmentIndex >= m_pReader->getSegmentCount())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTURESEGMENTINDEX, "invalid data capture segment index: " + std::to_string(nSegmentIndex));

	auto& segment = m_pReader->getSegment(nSegmentIndex);
	nFirstRow = segment.m_nFirstRow;
	nRowCount = segment.m_nRowCount;
	nSizeInBytes = segment.m_nSizeInBytes;
	bIsAvailable = !segment.m_bIsPurged;
}

LibMCEnv_uint64 CDataTableReader::GetChunkCount()
{
	return m_pReader->getChunkCount();
}

void CDataTableReader::GetChunkInformation(const LibMCEnv_uint64 nChunkIndex, LibMCEnv_uint32 & nSegmentIndex, LibMCEnv_uint64 & nFirstRow, LibMCEnv_uint64 & nRowCount, bool & bIsAvailable)
{
	if (nChunkIndex >= m_pReader->getChunkCount())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATATABLECHUNKINDEX, "invalid datatable chunk index: " + std::to_string(nChunkIndex));

	auto chunkInfo = translateFormatErrors([&]() {
		return m_pReader->getChunkInfo(nChunkIndex);
	});

	nSegmentIndex = chunkInfo.m_nSegmentIndex;
	nFirstRow = chunkInfo.m_nFirstRow;
	nRowCount = chunkInfo.m_nRowCount;
	bIsAvailable = chunkInfo.m_bIsAvailable;
}

bool CDataTableReader::GetChunkStatistics(const LibMCEnv_uint64 nChunkIndex, const std::string & sIdentifier, LibMCEnv_double & dMinValue, LibMCEnv_double & dMaxValue)
{
	if (nChunkIndex >= m_pReader->getChunkCount())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATATABLECHUNKINDEX, "invalid datatable chunk index: " + std::to_string(nChunkIndex));

	return translateFormatErrors([&]() {
		return m_pReader->getChunkStatistics(nChunkIndex, m_pReader->getColumnIndex(sIdentifier), dMinValue, dMaxValue);
	});
}

void CDataTableReader::ReadDoubleColumnRange(const std::string & sIdentifier, const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_double * pValuesBuffer)
{
	translateFormatErrors([&]() {
		uint32_t nColumnIndex = m_pReader->getColumnIndex(sIdentifier);
		m_pReader->validateRowRange(nFromRow, nRowCount);

		if (pValuesNeededCount != nullptr)
			*pValuesNeededCount = nRowCount;

		if (pValuesBuffer != nullptr) {
			if (nValuesBufferSize < nRowCount)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

			m_pReader->readColumnAsDouble(nColumnIndex, nFromRow, nRowCount, pValuesBuffer);
		}
	});
}

void CDataTableReader::ReadFloatColumnRange(const std::string & sIdentifier, const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_single * pValuesBuffer)
{
	translateFormatErrors([&]() {
		uint32_t nColumnIndex = m_pReader->getColumnIndex(sIdentifier);
		m_pReader->validateRowRange(nFromRow, nRowCount);

		if (pValuesNeededCount != nullptr)
			*pValuesNeededCount = nRowCount;

		if (pValuesBuffer != nullptr) {
			if (nValuesBufferSize < nRowCount)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

			m_pReader->readColumnAsFloat(nColumnIndex, nFromRow, nRowCount, pValuesBuffer);
		}
	});
}

void CDataTableReader::ReadInt64ColumnRange(const std::string & sIdentifier, const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_int64 * pValuesBuffer)
{
	translateFormatErrors([&]() {
		uint32_t nColumnIndex = m_pReader->getColumnIndex(sIdentifier);
		m_pReader->validateRowRange(nFromRow, nRowCount);

		if (pValuesNeededCount != nullptr)
			*pValuesNeededCount = nRowCount;

		if (pValuesBuffer != nullptr) {
			if (nValuesBufferSize < nRowCount)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

			m_pReader->readColumnAsInt64(nColumnIndex, nFromRow, nRowCount, pValuesBuffer);
		}
	});
}

void CDataTableReader::ReadBoolColumnRange(const std::string & sIdentifier, const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_uint8 * pValuesBuffer)
{
	translateFormatErrors([&]() {
		uint32_t nColumnIndex = m_pReader->getColumnIndex(sIdentifier);
		m_pReader->validateRowRange(nFromRow, nRowCount);

		if (m_pReader->getColumn(nColumnIndex).m_DataType != AMC::eDataTableV2DataType::Bool)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATATABLECOLUMNTYPEMISMATCH, "data capture column is not a bool column: " + sIdentifier);

		if (pValuesNeededCount != nullptr)
			*pValuesNeededCount = nRowCount;

		if (pValuesBuffer != nullptr) {
			if (nValuesBufferSize < nRowCount)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

			m_pReader->readColumnPlain(nColumnIndex, nFromRow, nRowCount, pValuesBuffer, nValuesBufferSize);
		}
	});
}

void CDataTableReader::ReadColumnDownsampled(const std::string & sIdentifier, const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, const LibMCEnv_uint32 nTargetPoints, const LibMCEnv::eDataTableDownsampleMode eMode, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_double * pValuesBuffer)
{
	if (nTargetPoints == 0)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDOWNSAMPLEPARAMETERS, "downsample target point count is zero");

	auto mode = convertDownsampleMode(eMode);

	translateFormatErrors([&]() {
		uint32_t nColumnIndex = m_pReader->getColumnIndex(sIdentifier);
		m_pReader->validateRowRange(nFromRow, nRowCount);

		uint64_t nValueCount = AMC::CDataCaptureReader::getDownsampleValueCount(nRowCount, nTargetPoints, mode);
		if (pValuesNeededCount != nullptr)
			*pValuesNeededCount = nValueCount;

		if (pValuesBuffer != nullptr) {
			if (nValuesBufferSize < nValueCount)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

			m_pReader->readColumnDownsampled(nColumnIndex, nFromRow, nRowCount, nTargetPoints, mode, pValuesBuffer);
		}
	});
}

void CDataTableReader::FindChunksInValueRange(const std::string & sIdentifier, const LibMCEnv_double dMinValue, const LibMCEnv_double dMaxValue, LibMCEnv_uint64 nChunkIndicesBufferSize, LibMCEnv_uint64* pChunkIndicesNeededCount, LibMCEnv_uint64 * pChunkIndicesBuffer)
{
	translateFormatErrors([&]() {
		auto chunkIndices = m_pReader->findChunksInValueRange(m_pReader->getColumnIndex(sIdentifier), dMinValue, dMaxValue);

		if (pChunkIndicesNeededCount != nullptr)
			*pChunkIndicesNeededCount = chunkIndices.size();

		if (pChunkIndicesBuffer != nullptr) {
			if (nChunkIndicesBufferSize < chunkIndices.size())
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

			for (size_t nIndex = 0; nIndex < chunkIndices.size(); nIndex++)
				pChunkIndicesBuffer[nIndex] = chunkIndices[nIndex];
		}
	});
}

IDataTable * CDataTableReader::ExtractDataTable(const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, const std::string & sColumnIdentifiers)
{
	return translateFormatErrors([&]() {
		m_pReader->validateRowRange(nFromRow, nRowCount);

		// An empty list selects all columns.
		std::vector<uint32_t> columnIndices;
		if (AMCCommon::CUtils::trimString(sColumnIdentifiers).empty()) {
			for (uint32_t nColumnIndex = 0; nColumnIndex < m_pReader->getColumnCount(); nColumnIndex++)
				columnIndices.push_back(nColumnIndex);
		}
		else {
			size_t nPosition = 0;
			while (nPosition <= sColumnIdentifiers.length()) {
				size_t nSeparator = sColumnIdentifiers.find(',', nPosition);
				if (nSeparator == std::string::npos)
					nSeparator = sColumnIdentifiers.length();

				std::string sIdentifier = AMCCommon::CUtils::trimString(sColumnIdentifiers.substr(nPosition, nSeparator - nPosition));
				columnIndices.push_back(m_pReader->getColumnIndex(sIdentifier));

				nPosition = nSeparator + 1;
			}
		}

		auto pDataTable = std::make_unique<CDataTable>(m_pToolpathHandler);

		for (uint32_t nColumnIndex : columnIndices) {
			auto& column = m_pReader->getColumn(nColumnIndex);
			pDataTable->AddColumn(column.m_sIdentifier, column.m_sDescription, (LibMCEnv::eDataTableColumnType)column.m_DataType);

			pDataTable->setColumnPlainData(column.m_sIdentifier, nRowCount, [&](uint8_t* pPlainValues, uint64_t nSizeInBytes) {
				m_pReader->readColumnPlain(nColumnIndex, nFromRow, nRowCount, pPlainValues, nSizeInBytes);
			});
		}

		return (IDataTable*)pDataTable.release();
	});
}
