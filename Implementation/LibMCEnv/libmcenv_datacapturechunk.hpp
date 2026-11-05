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


Abstract: This is the class declaration of CDataCaptureChunk

*/


#ifndef __LIBMCENV_DATACAPTURECHUNK
#define __LIBMCENV_DATACAPTURECHUNK

#include "libmcenv_interfaces.hpp"

// Parent classes
#include "libmcenv_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "libmcenv_datacaptureinstance.hpp"

namespace LibMCEnv {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CDataCaptureChunk
**************************************************************************************************************************/

class CDataCaptureChunk : public virtual IDataCaptureChunk, public virtual CBase {
private:

	// Keeps the capture alive as long as the producer holds the chunk.
	PDataCaptureInstance m_pInstance;

	// Released when the chunk has been committed.
	PDataCaptureChunkBuffer m_pBuffer;

	CDataCaptureChunkBuffer* getValidBuffer();

public:

	CDataCaptureChunk(PDataCaptureInstance pInstance, PDataCaptureChunkBuffer pBuffer);

	virtual ~CDataCaptureChunk();

	// Returns if the chunk has been borrowed from the given capture.
	bool belongsTo(CDataCaptureInstance* pInstance);

	// Returns the buffer of a chunk that has not been committed yet.
	PDataCaptureChunkBuffer getBuffer();

	// Marks the chunk as committed. All further calls fail.
	void invalidate();

	LibMCEnv_uint32 GetCapacity() override;

	LibMCEnv_uint32 GetRowCount() override;

	bool IsFull() override;

	void SetDoubleColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_double * pValuesBuffer) override;

	void SetInt32ColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_int32 * pValuesBuffer) override;

	void SetUint32ColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint32 * pValuesBuffer) override;

	void SetInt64ColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_int64 * pValuesBuffer) override;

	void SetUint64ColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint64 * pValuesBuffer) override;

	void SetFloatColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_single * pValuesBuffer) override;

	void SetBoolColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint8 * pValuesBuffer) override;

	void SetTimestampColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint64 * pValuesBuffer) override;

	void BeginRow() override;

	void SetRowDoubleValue(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_double dValue) override;

	void SetRowIntegerValue(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_int64 nValue) override;

	void EndRow() override;

};

} // namespace Impl
} // namespace LibMCEnv

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCENV_DATACAPTURECHUNK
