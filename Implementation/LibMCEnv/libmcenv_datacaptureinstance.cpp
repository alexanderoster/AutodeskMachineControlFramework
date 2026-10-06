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


Abstract: Internal engine of a streaming data capture.

*/

#include "libmcenv_datacaptureinstance.hpp"
#include "libmcenv_interfaceexception.hpp"
#include "libmcenv_tempstreamwriter.hpp"

#include "amc_constants.hpp"
#include "common_utils.hpp"

#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>

using namespace LibMCEnv::Impl;


sDataCaptureSettings::sDataCaptureSettings()
	: m_nChunkRowCount(DATACAPTURE_DEFAULTCHUNKROWCOUNT),
	m_nSegmentSizeInBytes(DATACAPTURE_DEFAULTSEGMENTSIZE),
	m_nMaxQueuedChunks(DATACAPTURE_DEFAULTMAXQUEUEDCHUNKS),
	m_BackpressureMode(LibMCEnv::eDataCaptureBackpressureMode::Block),
	m_nStorageLimitInBytes(0),
	m_nCodec(DATATABLE_V2_CODEC_LZ4)
{
}


/*************************************************************************************************************************
 Class definition of CDataCaptureChunkBuffer
**************************************************************************************************************************/

CDataCaptureChunkBuffer::CDataCaptureChunkBuffer(const std::vector<AMC::sDataTableV2Column>& columns, uint32_t nCapacity)
	: m_nCapacity(nCapacity), m_bRowOpen(false), m_nOpenRowIndex(0)
{
	if (nCapacity == 0)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	for (auto& column : columns) {
		uint32_t nEntrySize = AMC::CDataTableV2::getEntrySize(column.m_DataType);
		m_DataTypes.push_back(column.m_DataType);
		m_EntrySizes.push_back(nEntrySize);
		m_ColumnData.push_back(std::vector<uint8_t>((size_t)nCapacity * nEntrySize));
		m_ColumnRowCounts.push_back(0);
	}
}

void CDataCaptureChunkBuffer::checkColumnIndex(uint32_t nColumnIndex) const
{
	if (nColumnIndex >= m_ColumnData.size())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNINDEX, "invalid data capture column index: " + std::to_string(nColumnIndex));
}

void CDataCaptureChunkBuffer::reset()
{
	for (auto& nRowCount : m_ColumnRowCounts)
		nRowCount = 0;

	m_bRowOpen = false;
	m_nOpenRowIndex = 0;
}

uint32_t CDataCaptureChunkBuffer::getCapacity() const
{
	return m_nCapacity;
}

uint32_t CDataCaptureChunkBuffer::getColumnCount() const
{
	return (uint32_t)m_ColumnData.size();
}

uint32_t CDataCaptureChunkBuffer::getRowCount() const
{
	uint32_t nMaxRowCount = 0;
	for (auto nRowCount : m_ColumnRowCounts) {
		if (nRowCount > nMaxRowCount)
			nMaxRowCount = nRowCount;
	}

	return nMaxRowCount;
}

bool CDataCaptureChunkBuffer::isFull() const
{
	return getRowCount() >= m_nCapacity;
}

void CDataCaptureChunkBuffer::setColumnValues(uint32_t nColumnIndex, AMC::eDataTableV2DataType expectedType, const uint8_t* pValues, uint64_t nValueCount)
{
	checkColumnIndex(nColumnIndex);

	if (m_bRowOpen)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREROWALREADYOPEN);
	if (m_DataTypes[nColumnIndex] != expectedType)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECOLUMNTYPEMISMATCH, "data capture column has a different type: " + std::to_string(nColumnIndex));
	if (nValueCount > m_nCapacity)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECHUNKOVERFLOW, "data capture chunk capacity exceeded: " + std::to_string(nValueCount) + " > " + std::to_string(m_nCapacity));

	if (nValueCount > 0) {
		if (pValues == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& columnData = m_ColumnData[nColumnIndex];
		size_t nByteCount = (size_t)nValueCount * m_EntrySizes[nColumnIndex];
		memcpy(columnData.data(), pValues, nByteCount);

		// Bool values cross the ABI as bytes; any nonzero byte is true and is stored as 1.
		if (expectedType == AMC::eDataTableV2DataType::Bool) {
			for (size_t nIndex = 0; nIndex < nByteCount; nIndex++)
				columnData[nIndex] = (columnData[nIndex] != 0) ? 1 : 0;
		}
	}

	m_ColumnRowCounts[nColumnIndex] = (uint32_t)nValueCount;
}

void CDataCaptureChunkBuffer::beginRow()
{
	if (m_bRowOpen)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREROWALREADYOPEN);
	if (m_ColumnData.empty())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM, "data capture has no columns");

	uint32_t nRowIndex = validateForCommit();
	if (nRowIndex >= m_nCapacity)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECHUNKOVERFLOW, "data capture chunk is full");

	// Columns that are not set in this row are zero.
	for (size_t nColumnIndex = 0; nColumnIndex < m_ColumnData.size(); nColumnIndex++) {
		uint32_t nEntrySize = m_EntrySizes[nColumnIndex];
		memset(m_ColumnData[nColumnIndex].data() + (size_t)nRowIndex * nEntrySize, 0, nEntrySize);
	}

	m_nOpenRowIndex = nRowIndex;
	m_bRowOpen = true;
}

void CDataCaptureChunkBuffer::setRowDoubleValue(uint32_t nColumnIndex, double dValue)
{
	checkColumnIndex(nColumnIndex);

	if (!m_bRowOpen)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREROWNOTOPEN);

	uint8_t* pTarget = m_ColumnData[nColumnIndex].data() + (size_t)m_nOpenRowIndex * m_EntrySizes[nColumnIndex];

	switch (m_DataTypes[nColumnIndex]) {
	case AMC::eDataTableV2DataType::Double:
		memcpy(pTarget, &dValue, sizeof(double));
		break;
	case AMC::eDataTableV2DataType::Float: {
		float fValue = (float)dValue;
		memcpy(pTarget, &fValue, sizeof(float));
		break;
	}
	default:
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECOLUMNTYPEMISMATCH, "data capture column is not a floating point column: " + std::to_string(nColumnIndex));
	}
}

void CDataCaptureChunkBuffer::setRowIntegerValue(uint32_t nColumnIndex, int64_t nValue)
{
	checkColumnIndex(nColumnIndex);

	if (!m_bRowOpen)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREROWNOTOPEN);

	uint8_t* pTarget = m_ColumnData[nColumnIndex].data() + (size_t)m_nOpenRowIndex * m_EntrySizes[nColumnIndex];

	switch (m_DataTypes[nColumnIndex]) {
	case AMC::eDataTableV2DataType::Int32: {
		if ((nValue < (int64_t)(std::numeric_limits<int32_t>::min)()) || (nValue > (int64_t)(std::numeric_limits<int32_t>::max)()))
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREVALUEOUTOFRANGE, "value is out of range for an int32 column: " + std::to_string(nValue));
		int32_t nTypedValue = (int32_t)nValue;
		memcpy(pTarget, &nTypedValue, sizeof(nTypedValue));
		break;
	}
	case AMC::eDataTableV2DataType::Uint32: {
		if ((nValue < 0) || (nValue > (int64_t)(std::numeric_limits<uint32_t>::max)()))
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREVALUEOUTOFRANGE, "value is out of range for an uint32 column: " + std::to_string(nValue));
		uint32_t nTypedValue = (uint32_t)nValue;
		memcpy(pTarget, &nTypedValue, sizeof(nTypedValue));
		break;
	}
	case AMC::eDataTableV2DataType::Int64:
		memcpy(pTarget, &nValue, sizeof(nValue));
		break;
	case AMC::eDataTableV2DataType::Uint64:
	case AMC::eDataTableV2DataType::Timestamp: {
		if (nValue < 0)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREVALUEOUTOFRANGE, "value is out of range for an unsigned column: " + std::to_string(nValue));
		uint64_t nTypedValue = (uint64_t)nValue;
		memcpy(pTarget, &nTypedValue, sizeof(nTypedValue));
		break;
	}
	case AMC::eDataTableV2DataType::Bool:
		*pTarget = (nValue != 0) ? 1 : 0;
		break;
	default:
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECOLUMNTYPEMISMATCH, "data capture column is not an integer column: " + std::to_string(nColumnIndex));
	}
}

void CDataCaptureChunkBuffer::endRow()
{
	if (!m_bRowOpen)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREROWNOTOPEN);

	for (auto& nRowCount : m_ColumnRowCounts)
		nRowCount = m_nOpenRowIndex + 1;

	m_bRowOpen = false;
}

uint32_t CDataCaptureChunkBuffer::validateForCommit() const
{
	if (m_bRowOpen)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREROWALREADYOPEN, "data capture chunk has an open row");

	if (m_ColumnRowCounts.empty())
		return 0;

	uint32_t nRowCount = m_ColumnRowCounts[0];
	for (size_t nColumnIndex = 1; nColumnIndex < m_ColumnRowCounts.size(); nColumnIndex++) {
		if (m_ColumnRowCounts[nColumnIndex] != nRowCount)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECHUNKINCOMPLETE, "data capture chunk column " + std::to_string(nColumnIndex) + " has " + std::to_string(m_ColumnRowCounts[nColumnIndex]) + " rows, expected " + std::to_string(nRowCount));
	}

	return nRowCount;
}

const uint8_t* CDataCaptureChunkBuffer::getColumnData(uint32_t nColumnIndex) const
{
	checkColumnIndex(nColumnIndex);
	return m_ColumnData[nColumnIndex].data();
}


/*************************************************************************************************************************
 Class CDataCaptureSegment: one segment stream of the capture.
 Segments are written through CTempStreamWriter, which stores them as random write streams.
 Only WriteData is used and the writer never seeks, so the segment is strictly append-only.
**************************************************************************************************************************/

namespace LibMCEnv {
namespace Impl {

class CDataCaptureSegment {
private:

	std::unique_ptr<CTempStreamWriter> m_pStream;
	AMC::CDataTableV2SegmentBuilder m_Builder;
	uint64_t m_nStartTimeStamp;
	uint64_t m_nEndTimeStamp;
	bool m_bStreamFinished;
	bool m_bDiscarded;

	uint64_t writeBuffer(std::vector<uint8_t>& buffer)
	{
		if (!buffer.empty())
			m_pStream->WriteData(buffer.size(), buffer.data());

		return buffer.size();
	}

public:

	CDataCaptureSegment(LibMCData::PDataModel pDataModel, const std::string& sStreamName, const std::string& sUserUUID, AMCCommon::PChrono pGlobalChrono, const std::vector<AMC::sDataTableV2Column>& columns, uint32_t nSegmentIndex, uint64_t nFirstRowIndex, uint32_t nChunkRowCapacity)
		: m_Builder(columns, nSegmentIndex, nFirstRowIndex, nChunkRowCapacity),
		m_nStartTimeStamp(pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970()),
		m_nEndTimeStamp(0),
		m_bStreamFinished(false),
		m_bDiscarded(false)
	{
		m_pStream = std::make_unique<CTempStreamWriter>(pDataModel, sStreamName, DATACAPTURE_SEGMENTMIMETYPE, sUserUUID, pGlobalChrono);
	}

	// Writes the segment header. Returns the number of bytes written.
	uint64_t open(std::vector<uint8_t>& buffer)
	{
		buffer.clear();
		m_Builder.appendHeader(buffer);
		return writeBuffer(buffer);
	}

	// Writes one encoded chunk. Returns the number of bytes written.
	uint64_t writeChunk(const AMC::sDataTableV2EncodedChunk& encodedChunk, std::vector<uint8_t>& buffer)
	{
		buffer.clear();
		m_Builder.appendEncodedChunk(encodedChunk, buffer);
		return writeBuffer(buffer);
	}

	// Writes footer and trailer. Returns the number of bytes written.
	uint64_t close(std::vector<uint8_t>& buffer, uint64_t nEndTimeStamp)
	{
		buffer.clear();
		m_Builder.appendFooter(buffer);
		m_nEndTimeStamp = nEndTimeStamp;
		return writeBuffer(buffer);
	}

	const AMC::CDataTableV2SegmentBuilder& getBuilder() const
	{
		return m_Builder;
	}

	std::string getStreamUUID()
	{
		return m_pStream->GetUUID();
	}

	uint64_t getStartTimeStamp() const
	{
		return m_nStartTimeStamp;
	}

	uint64_t getEndTimeStamp() const
	{
		return m_nEndTimeStamp;
	}

	// Finishes the storage stream. The storage layer hashes the complete file here.
	void finishStream()
	{
		m_pStream->Finish();
		m_bStreamFinished = true;
	}

	// Deletes the data of a segment whose stream has not been finished.
	void discard()
	{
		if (!m_bStreamFinished && !m_bDiscarded)
			m_pStream->Discard();

		m_bDiscarded = true;
	}

};

} // namespace Impl
} // namespace LibMCEnv


/*************************************************************************************************************************
 Class definition of CDataCaptureInstance
**************************************************************************************************************************/

CDataCaptureInstance::CDataCaptureInstance(LibMCData::PDataModel pDataModel, LibMCData::PDataCapture pManifest, const sDataCaptureSettings& settings, AMCCommon::PChrono pGlobalChrono, const std::string& sUserUUID)
	: m_pDataModel(pDataModel),
	m_pManifest(pManifest),
	m_pGlobalChrono(pGlobalChrono),
	m_sUserUUID(sUserUUID),
	m_Settings(settings),
	m_bWriterBusy(false),
	m_StopRequest(eStopRequest::None),
	m_Status(LibMCEnv::eDataCaptureStatus::Configuring),
	m_bFailed(false),
	m_nCommittedRows(0),
	m_nDroppedRows(0),
	m_nStorageSize(0),
	m_nSegmentCount(0),
	m_nWrittenRows(0),
	m_bWriterDone(false)
{
	if (pDataModel.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
	if (pManifest.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
	if (pGlobalChrono.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	m_sUUID = pManifest->GetUUID();
	m_sIdentifier = pManifest->GetIdentifier();
	m_sName = pManifest->GetName();

	m_pRetentionStorage = pDataModel->CreateStorage();
}

CDataCaptureInstance::~CDataCaptureInstance()
{
	requestStopAndJoin(eStopRequest::Finish);
}

std::string CDataCaptureInstance::getUUID() const
{
	return m_sUUID;
}

std::string CDataCaptureInstance::getIdentifier() const
{
	return m_sIdentifier;
}

std::string CDataCaptureInstance::getName() const
{
	return m_sName;
}

LibMCEnv::eDataCaptureStatus CDataCaptureInstance::getStatus()
{
	std::lock_guard<std::mutex> lock(m_Mutex);

	if (m_bFailed && (m_Status == LibMCEnv::eDataCaptureStatus::Recording))
		return LibMCEnv::eDataCaptureStatus::Failed;

	return m_Status;
}

void CDataCaptureInstance::throwFailure()
{
	throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREWRITERFAILED, "data capture writer failed: " + m_sFailureMessage);
}

void CDataCaptureInstance::ensureRecording()
{
	if (m_bFailed)
		throwFailure();

	if ((m_Status != LibMCEnv::eDataCaptureStatus::Recording) || (m_StopRequest != eStopRequest::None))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURENOTRECORDING);
}

void CDataCaptureInstance::recordFailure(const std::string& sMessage)
{
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		if (!m_bFailed) {
			m_bFailed = true;
			m_sFailureMessage = sMessage;
		}
	}

	// Wake everybody: the producer must not wait for a writer that has stopped, and vice versa.
	m_WriterCondition.notify_all();
	m_ProducerCondition.notify_all();
}

bool CDataCaptureInstance::stopRequestIsAbortOrFailed()
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_bFailed || (m_StopRequest == eStopRequest::Abort);
}

void CDataCaptureInstance::addColumn(const std::string& sIdentifier, const std::string& sDescription, const LibMCEnv::eDataTableColumnType eColumnType)
{
	if (sIdentifier.empty())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNIDENTIFIEREMPTY);
	if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString(sIdentifier))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNIDENTIFIER, "invalid column identifier: " + sIdentifier);
	if (!AMC::CDataTableV2::dataTypeIsValid((uint32_t)eColumnType))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNDATATYPE);

	std::lock_guard<std::mutex> lock(m_Mutex);

	if (m_Status != LibMCEnv::eDataCaptureStatus::Configuring)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURENOTCONFIGURABLE);

	for (auto& column : m_Columns) {
		if (column.m_sIdentifier == sIdentifier)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DUPLICATECOLUMNIDENTIFIER, "duplicate column identifier: " + sIdentifier);
	}

	if (m_Columns.size() >= DATATABLE_MAXCOLUMNCOUNT)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATATABLEHASTOOMANYCOLUMS);

	AMC::sDataTableV2Column column;
	column.m_sIdentifier = sIdentifier;
	column.m_sDescription = sDescription;
	column.m_DataType = (AMC::eDataTableV2DataType)eColumnType;
	m_Columns.push_back(column);
}

uint32_t CDataCaptureInstance::getColumnCount()
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	return (uint32_t)m_Columns.size();
}

std::vector<AMC::sDataTableV2Column> CDataCaptureInstance::getColumns()
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_Columns;
}

// Decides how each column is encoded: the default of its type, unless the options say otherwise.
// Fails before anything is written if an option names a column that does not exist or does not fit.
void CDataCaptureInstance::resolveColumnEncodings()
{
	for (auto& column : m_Columns) {
		column.m_nEncoding = AMC::CDataTableV2::getDefaultEncoding(column.m_DataType);
		column.m_dQuantizationStep = 0.0;

		auto iQuantization = m_Settings.m_ColumnQuantization.find(column.m_sIdentifier);
		if (iQuantization != m_Settings.m_ColumnQuantization.end()) {
			if (column.m_DataType != AMC::eDataTableV2DataType::Double)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "quantisation requires a double column: " + column.m_sIdentifier);

			column.m_dQuantizationStep = iQuantization->second;
			column.m_nEncoding = DATATABLE_V2_ENCODING_DELTA;
		}

		auto iEncoding = m_Settings.m_ColumnEncodings.find(column.m_sIdentifier);
		if (iEncoding != m_Settings.m_ColumnEncodings.end()) {
			if ((column.m_dQuantizationStep > 0.0) && (iEncoding->second != DATATABLE_V2_ENCODING_DELTA))
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "a quantised column is delta encoded: " + column.m_sIdentifier);
			if (!AMC::CDataTableV2::encodingIsValidForColumn(column.m_DataType, iEncoding->second, column.m_dQuantizationStep))
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "encoding does not fit the type of column " + column.m_sIdentifier);

			column.m_nEncoding = iEncoding->second;
		}
	}

	auto columnExists = [this](const std::string& sIdentifier) {
		for (auto& column : m_Columns) {
			if (column.m_sIdentifier == sIdentifier)
				return true;
		}
		return false;
	};

	for (auto& encodingOption : m_Settings.m_ColumnEncodings) {
		if (!columnExists(encodingOption.first))
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND, "encoding option names an unknown column: " + encodingOption.first);
	}
	for (auto& quantizationOption : m_Settings.m_ColumnQuantization) {
		if (!columnExists(quantizationOption.first))
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND, "quantisation option names an unknown column: " + quantizationOption.first);
	}
}

// Freezes the schema, allocates the chunk pool and starts the threads. m_Mutex MUST be held by the caller.
void CDataCaptureInstance::startRecording()
{
	resolveColumnEncodings();
	m_pEncoder = std::make_unique<AMC::CDataTableV2ChunkEncoder>(m_Columns, m_Settings.m_nCodec);

	for (size_t nColumnIndex = 0; nColumnIndex < m_Columns.size(); nColumnIndex++) {
		auto& column = m_Columns[nColumnIndex];
		m_pManifest->AddColumn((uint32_t)nColumnIndex, column.m_sIdentifier, column.m_sDescription, (uint32_t)column.m_DataType);
	}

	// The queue holds at most MaxQueuedChunks buffers and the writer thread holds one more. The producer
	// only gets a buffer while the queue is not full, so one buffer on top of the queue depth is enough.
	size_t nPoolDepth = (size_t)m_Settings.m_nMaxQueuedChunks + 1;
	for (size_t nIndex = 0; nIndex < nPoolDepth; nIndex++)
		m_FreeBuffers.push_back(std::make_shared<CDataCaptureChunkBuffer>(m_Columns, m_Settings.m_nChunkRowCount));

	m_Status = LibMCEnv::eDataCaptureStatus::Recording;

	m_WriterThread = std::thread(&CDataCaptureInstance::writerThreadMain, this);
	m_FinalizerThread = std::thread(&CDataCaptureInstance::finalizerThreadMain, this);
}

PDataCaptureChunkBuffer CDataCaptureInstance::beginChunk()
{
	std::unique_lock<std::mutex> lock(m_Mutex);

	if (m_Status == LibMCEnv::eDataCaptureStatus::Configuring)
		startRecording();

	ensureRecording();

	if (m_pOutstandingChunk.get() != nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECHUNKALREADYOPEN);

	// Backpressure: the producer only gets a chunk while the queue is not full.
	while (m_Queue.size() >= m_Settings.m_nMaxQueuedChunks) {

		switch (m_Settings.m_BackpressureMode) {
		case LibMCEnv::eDataCaptureBackpressureMode::Fail:
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREQUEUEFULL);

		case LibMCEnv::eDataCaptureBackpressureMode::DropOldest: {
			auto pDroppedChunk = m_Queue.front();
			m_Queue.pop_front();
			m_nDroppedRows += pDroppedChunk->getRowCount();
			m_FreeBuffers.push_back(pDroppedChunk);
			break;
		}

		default:
			m_ProducerCondition.wait(lock);
			break;
		}

		ensureRecording();
	}

	// Follows from the pool depth: a queue that is not full always leaves a buffer for the producer.
	if (m_FreeBuffers.empty())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTUREQUEUEFULL, "data capture chunk pool is exhausted");

	auto pChunk = m_FreeBuffers.back();
	m_FreeBuffers.pop_back();
	pChunk->reset();

	m_pOutstandingChunk = pChunk;
	return pChunk;
}

void CDataCaptureInstance::commitChunk(PDataCaptureChunkBuffer pChunk)
{
	{
		std::lock_guard<std::mutex> lock(m_Mutex);

		ensureRecording();

		if ((pChunk.get() == nullptr) || (pChunk != m_pOutstandingChunk))
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECHUNKINVALID);

		// The chunk stays with the producer if it is not consistent, so that it can be completed.
		uint32_t nRowCount = pChunk->validateForCommit();

		m_pOutstandingChunk = nullptr;

		if (nRowCount == 0) {
			m_FreeBuffers.push_back(pChunk);
			return;
		}

		m_Queue.push_back(pChunk);
		m_nCommittedRows += nRowCount;
	}

	m_WriterCondition.notify_one();
}

void CDataCaptureInstance::flush()
{
	std::unique_lock<std::mutex> lock(m_Mutex);

	if (m_Status == LibMCEnv::eDataCaptureStatus::Configuring)
		return;

	m_ProducerCondition.wait(lock, [this]() {
		return m_bFailed || (m_Status != LibMCEnv::eDataCaptureStatus::Recording) || (m_Queue.empty() && !m_bWriterBusy);
	});

	if (m_bFailed)
		throwFailure();
}

uint64_t CDataCaptureInstance::getCapturedRowCount()
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_nCommittedRows - m_nDroppedRows;
}

uint64_t CDataCaptureInstance::getStorageSizeInBytes()
{
	return m_nStorageSize.load();
}

uint32_t CDataCaptureInstance::getSegmentCount()
{
	return m_nSegmentCount.load();
}

uint32_t CDataCaptureInstance::getQueuedChunkCount()
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	return (uint32_t)m_Queue.size();
}

uint64_t CDataCaptureInstance::getDroppedRowCount()
{
	std::lock_guard<std::mutex> lock(m_Mutex);
	return m_nDroppedRows;
}

void CDataCaptureInstance::requestStopAndJoin(eStopRequest stopRequest)
{
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		if ((m_StopRequest == eStopRequest::None) || (stopRequest == eStopRequest::Abort))
			m_StopRequest = stopRequest;
	}

	m_WriterCondition.notify_all();
	m_ProducerCondition.notify_all();

	if (m_WriterThread.joinable())
		m_WriterThread.join();
	if (m_FinalizerThread.joinable())
		m_FinalizerThread.join();
}

std::string CDataCaptureInstance::finish()
{
	{
		std::lock_guard<std::mutex> lock(m_Mutex);

		if (m_Status == LibMCEnv::eDataCaptureStatus::Finished)
			return m_sUUID;

		if (m_bFailed)
			throwFailure();

		if ((m_Status != LibMCEnv::eDataCaptureStatus::Configuring) && (m_Status != LibMCEnv::eDataCaptureStatus::Recording))
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURENOTRECORDING);

		if (m_pOutstandingChunk.get() != nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECHUNKOUTSTANDING);

		// A capture without any chunk still stores its schema in one empty segment.
		if (m_Status == LibMCEnv::eDataCaptureStatus::Configuring)
			startRecording();
	}

	requestStopAndJoin(eStopRequest::Finish);

	std::lock_guard<std::mutex> lock(m_Mutex);
	if (m_bFailed)
		throwFailure();

	m_Status = LibMCEnv::eDataCaptureStatus::Finished;
	return m_sUUID;
}

void CDataCaptureInstance::abort()
{
	{
		std::lock_guard<std::mutex> lock(m_Mutex);

		if (m_Status == LibMCEnv::eDataCaptureStatus::Aborted)
			return;

		if (m_Status == LibMCEnv::eDataCaptureStatus::Finished)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURENOTRECORDING);

		if (m_Status == LibMCEnv::eDataCaptureStatus::Configuring) {
			// Nothing has been written yet and no thread is running.
			m_pManifest->Abort(m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970());
			m_Status = LibMCEnv::eDataCaptureStatus::Aborted;
			return;
		}
	}

	requestStopAndJoin(eStopRequest::Abort);

	bool bFailed = false;
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		bFailed = m_bFailed;
	}

	// A thread that has failed did not clean up. Both threads have ended, so it is done here.
	if (bFailed)
		cleanupAfterAbort();

	std::lock_guard<std::mutex> lock(m_Mutex);
	m_pOutstandingChunk = nullptr;
	m_Status = LibMCEnv::eDataCaptureStatus::Aborted;
}


/*************************************************************************************************************************
 Writer thread
**************************************************************************************************************************/

void CDataCaptureInstance::writerThreadMain()
{
	try {
		runWriter();
	}
	catch (const std::exception& writerException) {
		recordFailure(writerException.what());
	}

	// Tell the finalizer that no further segment will arrive.
	{
		std::lock_guard<std::mutex> lock(m_FinalizerMutex);
		m_bWriterDone = true;
	}
	m_FinalizerCondition.notify_all();
}

void CDataCaptureInstance::runWriter()
{
	// The first segment is opened immediately. Nothing is accumulated waiting for the end of the capture.
	openSegment();

	for (;;) {
		PDataCaptureChunkBuffer pChunk;

		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			m_WriterCondition.wait(lock, [this]() {
				return !m_Queue.empty() || (m_StopRequest != eStopRequest::None) || m_bFailed;
			});

			// On abort or failure the open segment is left to the cleanup.
			if (m_bFailed || (m_StopRequest == eStopRequest::Abort))
				return;

			// Finish has been requested and the queue has drained.
			if (m_Queue.empty())
				break;

			pChunk = m_Queue.front();
			m_Queue.pop_front();
			m_bWriterBusy = true;
		}

		// The queue has room again.
		m_ProducerCondition.notify_all();

		// Returns the buffer to the pool when the chunk has been written or writing has failed.
		struct CBufferReturn {
			CDataCaptureInstance* m_pInstance;
			PDataCaptureChunkBuffer m_pChunk;
			~CBufferReturn()
			{
				{
					std::lock_guard<std::mutex> lock(m_pInstance->m_Mutex);
					m_pInstance->m_FreeBuffers.push_back(m_pChunk);
					m_pInstance->m_bWriterBusy = false;
				}
				m_pInstance->m_ProducerCondition.notify_all();
			}
		} bufferReturn{ this, pChunk };

		writeChunk(pChunk.get());
	}

	closeSegment();
}

void CDataCaptureInstance::openSegment()
{
	uint32_t nSegmentIndex = m_nSegmentCount.load();

	std::stringstream sStreamName;
	sStreamName << m_sIdentifier << "_segment" << std::setw(4) << std::setfill('0') << nSegmentIndex;

	auto pSegment = std::make_shared<CDataCaptureSegment>(m_pDataModel, sStreamName.str(), m_sUserUUID, m_pGlobalChrono, m_Columns, nSegmentIndex, m_nWrittenRows, m_Settings.m_nChunkRowCount);

	m_pOpenSegment = pSegment;
	m_nSegmentCount++;

	m_nStorageSize += pSegment->open(m_WriteBuffer);
}

void CDataCaptureInstance::closeSegment()
{
	m_nStorageSize += m_pOpenSegment->close(m_WriteBuffer, m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970());
	m_nWrittenRows += m_pOpenSegment->getBuilder().getRowCount();

	{
		std::lock_guard<std::mutex> lock(m_FinalizerMutex);
		m_FinalizerQueue.push_back(m_pOpenSegment);
	}
	m_pOpenSegment = nullptr;

	m_FinalizerCondition.notify_all();
}

void CDataCaptureInstance::writeChunk(CDataCaptureChunkBuffer* pChunk)
{
	// Encode and compress the chunk first, so that its size on disk is known.
	uint32_t nColumnCount = pChunk->getColumnCount();
	std::vector<const uint8_t*> columnData(nColumnCount);
	for (uint32_t nColumnIndex = 0; nColumnIndex < nColumnCount; nColumnIndex++)
		columnData[nColumnIndex] = pChunk->getColumnData(nColumnIndex);

	m_pEncoder->encodeChunk(columnData, pChunk->getRowCount(), m_EncodedChunk);

	// Roll over before a chunk that would exceed the segment size. A chunk is never split
	// and a segment always holds at least one chunk.
	if (m_Settings.m_nSegmentSizeInBytes > 0) {
		auto& builder = m_pOpenSegment->getBuilder();
		if (builder.getChunkCount() > 0) {
			uint64_t nProjectedSize = builder.getCurrentSize() + m_EncodedChunk.m_Bytes.size() + builder.getFooterSizeInBytes(builder.getChunkCount() + 1);
			if (nProjectedSize > m_Settings.m_nSegmentSizeInBytes) {
				closeSegment();
				openSegment();
			}
		}
	}

	m_nStorageSize += m_pOpenSegment->writeChunk(m_EncodedChunk, m_WriteBuffer);
}


/*************************************************************************************************************************
 Finalizer thread
**************************************************************************************************************************/

void CDataCaptureInstance::finalizerThreadMain()
{
	try {
		runFinalizer();
	}
	catch (const std::exception& finalizerException) {
		recordFailure(finalizerException.what());
	}
}

void CDataCaptureInstance::runFinalizer()
{
	for (;;) {
		PDataCaptureSegment pSegment;

		{
			std::unique_lock<std::mutex> lock(m_FinalizerMutex);
			m_FinalizerCondition.wait(lock, [this]() {
				return !m_FinalizerQueue.empty() || m_bWriterDone;
			});

			// The writer has ended and everything it closed has been handled.
			if (m_FinalizerQueue.empty())
				break;

			pSegment = m_FinalizerQueue.front();
			m_FinalizerQueue.pop_front();
		}

		if (stopRequestIsAbortOrFailed())
			m_LeftoverSegments.push_back(pSegment);
		else
			finalizeSegment(pSegment);
	}

	bool bFailed = false;
	bool bAbort = false;
	{
		std::lock_guard<std::mutex> lock(m_Mutex);
		bFailed = m_bFailed;
		bAbort = (m_StopRequest == eStopRequest::Abort);
	}

	// After a failure the capture stays as it is. Abort cleans it up.
	if (bFailed)
		return;

	if (bAbort)
		cleanupAfterAbort();
	else
		m_pManifest->Finish(m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970());
}

void CDataCaptureInstance::finalizeSegment(PDataCaptureSegment pSegment)
{
	auto& builder = pSegment->getBuilder();

	// The segment is registered before its stream is finished: a segment that is known to the manifest
	// can always be purged, so a failure in between does not leave an orphaned file behind.
	m_pManifest->AddSegment(builder.getSegmentIndex(), pSegment->getStreamUUID(), builder.getFirstRowIndex(), builder.getRowCount(), builder.getChunkCount(), builder.getCurrentSize(), pSegment->getStartTimeStamp(), pSegment->getEndTimeStamp());

	pSegment->finishStream();

	if (m_Settings.m_nStorageLimitInBytes > 0)
		m_pRetentionStorage->EnforceDataCaptureStorageLimit(m_Settings.m_nStorageLimitInBytes);
}

// Discards everything that has not been finalized and deletes the registered segments.
// Runs on the finalizer thread, or on the thread that calls abort after both threads have ended.
void CDataCaptureInstance::cleanupAfterAbort()
{
	{
		std::lock_guard<std::mutex> lock(m_FinalizerMutex);
		for (auto& pSegment : m_FinalizerQueue)
			m_LeftoverSegments.push_back(pSegment);
		m_FinalizerQueue.clear();
	}

	if (m_pOpenSegment.get() != nullptr) {
		m_LeftoverSegments.push_back(m_pOpenSegment);
		m_pOpenSegment = nullptr;
	}

	for (auto& pSegment : m_LeftoverSegments)
		pSegment->discard();
	m_LeftoverSegments.clear();

	if (m_pManifest->GetStatus() != LibMCData::eDataCaptureStatus::Aborted)
		m_pManifest->Abort(m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970());
}
