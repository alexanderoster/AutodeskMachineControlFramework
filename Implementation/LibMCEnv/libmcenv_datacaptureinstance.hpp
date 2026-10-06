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


Abstract: Internal engine of a streaming data capture. It is shared by the
CDataCapture and CDataCaptureChunk interface classes and is not part of the
external API.

*/


#ifndef __LIBMCENV_DATACAPTUREINSTANCE
#define __LIBMCENV_DATACAPTUREINSTANCE

#include "libmcenv_interfaces.hpp"

#include "libmcdata_dynamic.hpp"
#include "common_chrono.hpp"
#include "amc_datatableformat.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace LibMCEnv {
namespace Impl {


struct sDataCaptureSettings {
	uint32_t m_nChunkRowCount;
	uint64_t m_nSegmentSizeInBytes;
	uint32_t m_nMaxQueuedChunks;
	LibMCEnv::eDataCaptureBackpressureMode m_BackpressureMode;
	uint64_t m_nStorageLimitInBytes;

	// Page codec of the capture as DATATABLE_V2_CODEC value.
	uint32_t m_nCodec;

	// Explicit encodings by column identifier as DATATABLE_V2_ENCODING value. Columns that are not listed use the default of their type.
	std::map<std::string, uint32_t> m_ColumnEncodings;

	// Quantisation steps by column identifier. Columns that are not listed are not quantised.
	std::map<std::string, double> m_ColumnQuantization;

	// Initialises the settings with the defaults.
	sDataCaptureSettings();
};


/*************************************************************************************************************************
 CDataCaptureChunkBuffer holds the rows of one chunk: one fixed-size value array per column.
 Buffers are allocated once when the schema is frozen and are recycled through the pool of the capture.
**************************************************************************************************************************/

class CDataCaptureChunkBuffer {
private:

	uint32_t m_nCapacity;
	std::vector<AMC::eDataTableV2DataType> m_DataTypes;
	std::vector<uint32_t> m_EntrySizes;
	std::vector<std::vector<uint8_t>> m_ColumnData;
	std::vector<uint32_t> m_ColumnRowCounts;

	bool m_bRowOpen;
	uint32_t m_nOpenRowIndex;

	void checkColumnIndex(uint32_t nColumnIndex) const;

public:

	CDataCaptureChunkBuffer(const std::vector<AMC::sDataTableV2Column>& columns, uint32_t nCapacity);

	void reset();

	uint32_t getCapacity() const;

	uint32_t getColumnCount() const;

	// Row count of the longest column.
	uint32_t getRowCount() const;

	bool isFull() const;

	// Replaces the rows of a column. pValues holds nValueCount plain values of the expected type.
	void setColumnValues(uint32_t nColumnIndex, AMC::eDataTableV2DataType expectedType, const uint8_t* pValues, uint64_t nValueCount);

	void beginRow();

	void setRowDoubleValue(uint32_t nColumnIndex, double dValue);

	void setRowIntegerValue(uint32_t nColumnIndex, int64_t nValue);

	void endRow();

	// Verifies that no row is open and all columns have the same row count. Returns that row count.
	uint32_t validateForCommit() const;

	const uint8_t* getColumnData(uint32_t nColumnIndex) const;

};

typedef std::shared_ptr<CDataCaptureChunkBuffer> PDataCaptureChunkBuffer;


class CDataCaptureSegment;
typedef std::shared_ptr<CDataCaptureSegment> PDataCaptureSegment;


/*************************************************************************************************************************
 CDataCaptureInstance is the capture session.

 Threading model:
   - One producer. It borrows a chunk buffer from the pool, fills it and commits it. The producer never
     touches the disk while recording.
   - One writer thread. It takes committed chunks from a bounded queue, serialises them into the open
     segment and rolls over into a new segment when the segment size is reached.
   - One finalizer thread. It registers closed segments in the manifest, finishes their storage streams
     (which hashes the file) and enforces the retention limit. Finalizing is kept off the writer thread,
     so that a segment rollover does not stall the queue.

 Memory is bounded by the pool: (MaxQueuedChunks + 1) buffers exist for the life of the capture.
**************************************************************************************************************************/

class CDataCaptureInstance {
private:

	enum class eStopRequest {
		None,
		Finish,
		Abort
	};

	LibMCData::PDataModel m_pDataModel;
	LibMCData::PDataCapture m_pManifest;
	LibMCData::PStorage m_pRetentionStorage;
	AMCCommon::PChrono m_pGlobalChrono;

	std::string m_sUUID;
	std::string m_sIdentifier;
	std::string m_sName;
	std::string m_sUserUUID;
	sDataCaptureSettings m_Settings;

	// Schema. Changed by the producer while configuring, immutable once recording has started.
	std::vector<AMC::sDataTableV2Column> m_Columns;

	// State shared between producer and writer thread, guarded by m_Mutex.
	std::mutex m_Mutex;
	std::condition_variable m_WriterCondition;
	std::condition_variable m_ProducerCondition;
	std::deque<PDataCaptureChunkBuffer> m_Queue;
	std::vector<PDataCaptureChunkBuffer> m_FreeBuffers;
	PDataCaptureChunkBuffer m_pOutstandingChunk;
	bool m_bWriterBusy;
	eStopRequest m_StopRequest;
	LibMCEnv::eDataCaptureStatus m_Status;
	bool m_bFailed;
	std::string m_sFailureMessage;
	uint64_t m_nCommittedRows;
	uint64_t m_nDroppedRows;

	std::atomic<uint64_t> m_nStorageSize;
	std::atomic<uint32_t> m_nSegmentCount;

	// Writer thread state.
	PDataCaptureSegment m_pOpenSegment;
	uint64_t m_nWrittenRows;
	std::vector<uint8_t> m_WriteBuffer;
	std::unique_ptr<AMC::CDataTableV2ChunkEncoder> m_pEncoder;
	AMC::sDataTableV2EncodedChunk m_EncodedChunk;

	// Hand-over from writer to finalizer, guarded by m_FinalizerMutex.
	std::mutex m_FinalizerMutex;
	std::condition_variable m_FinalizerCondition;
	std::deque<PDataCaptureSegment> m_FinalizerQueue;
	bool m_bWriterDone;

	// Segments that were closed but must not be finalized anymore. Finalizer thread, or the caller of abort after the threads have ended.
	std::vector<PDataCaptureSegment> m_LeftoverSegments;

	std::thread m_WriterThread;
	std::thread m_FinalizerThread;

	void ensureRecording();
	void throwFailure();
	void recordFailure(const std::string& sMessage);
	bool stopRequestIsAbortOrFailed();

	void resolveColumnEncodings();
	void startRecording();
	void requestStopAndJoin(eStopRequest stopRequest);

	void writerThreadMain();
	void runWriter();
	void openSegment();
	void closeSegment();
	void writeChunk(CDataCaptureChunkBuffer* pChunk);

	void finalizerThreadMain();
	void runFinalizer();
	void finalizeSegment(PDataCaptureSegment pSegment);
	void cleanupAfterAbort();

public:

	CDataCaptureInstance(LibMCData::PDataModel pDataModel, LibMCData::PDataCapture pManifest, const sDataCaptureSettings& settings, AMCCommon::PChrono pGlobalChrono, const std::string& sUserUUID);

	// Finishes a capture that is still recording. A capture that has been abandoned without
	// Finish or Abort keeps all data that has been committed.
	virtual ~CDataCaptureInstance();

	CDataCaptureInstance(const CDataCaptureInstance&) = delete;
	CDataCaptureInstance& operator=(const CDataCaptureInstance&) = delete;

	std::string getUUID() const;

	std::string getIdentifier() const;

	std::string getName() const;

	LibMCEnv::eDataCaptureStatus getStatus();

	void addColumn(const std::string& sIdentifier, const std::string& sDescription, const LibMCEnv::eDataTableColumnType eColumnType);

	uint32_t getColumnCount();

	// Returns a copy of the schema.
	std::vector<AMC::sDataTableV2Column> getColumns();

	PDataCaptureChunkBuffer beginChunk();

	void commitChunk(PDataCaptureChunkBuffer pChunk);

	void flush();

	uint64_t getCapturedRowCount();

	uint64_t getStorageSizeInBytes();

	uint32_t getSegmentCount();

	uint32_t getQueuedChunkCount();

	uint64_t getDroppedRowCount();

	std::string finish();

	void abort();

};

typedef std::shared_ptr<CDataCaptureInstance> PDataCaptureInstance;

} // namespace Impl
} // namespace LibMCEnv

#endif // __LIBMCENV_DATACAPTUREINSTANCE
