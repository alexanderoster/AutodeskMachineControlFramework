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


Abstract: This is the class definition of CDataCaptureChunk

*/

#include "libmcenv_datacapturechunk.hpp"
#include "libmcenv_interfaceexception.hpp"

// Include custom headers here.


using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CDataCaptureChunk
**************************************************************************************************************************/

CDataCaptureChunk::CDataCaptureChunk(PDataCaptureInstance pInstance, PDataCaptureChunkBuffer pBuffer)
	: m_pInstance(pInstance), m_pBuffer(pBuffer)
{
	if (pInstance.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
	if (pBuffer.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

CDataCaptureChunk::~CDataCaptureChunk()
{

}

CDataCaptureChunkBuffer* CDataCaptureChunk::getValidBuffer()
{
	if (m_pBuffer.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECHUNKINVALID);

	return m_pBuffer.get();
}

bool CDataCaptureChunk::belongsTo(CDataCaptureInstance* pInstance)
{
	return (m_pInstance.get() == pInstance);
}

PDataCaptureChunkBuffer CDataCaptureChunk::getBuffer()
{
	getValidBuffer();
	return m_pBuffer;
}

void CDataCaptureChunk::invalidate()
{
	m_pBuffer = nullptr;
}

LibMCEnv_uint32 CDataCaptureChunk::GetCapacity()
{
	return getValidBuffer()->getCapacity();
}

LibMCEnv_uint32 CDataCaptureChunk::GetRowCount()
{
	return getValidBuffer()->getRowCount();
}

bool CDataCaptureChunk::IsFull()
{
	return getValidBuffer()->isFull();
}

void CDataCaptureChunk::SetDoubleColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_double * pValuesBuffer)
{
	getValidBuffer()->setColumnValues(nColumnIndex, AMC::eDataTableV2DataType::Double, (const uint8_t*)pValuesBuffer, nValuesBufferSize);
}

void CDataCaptureChunk::SetInt32ColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_int32 * pValuesBuffer)
{
	getValidBuffer()->setColumnValues(nColumnIndex, AMC::eDataTableV2DataType::Int32, (const uint8_t*)pValuesBuffer, nValuesBufferSize);
}

void CDataCaptureChunk::SetUint32ColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint32 * pValuesBuffer)
{
	getValidBuffer()->setColumnValues(nColumnIndex, AMC::eDataTableV2DataType::Uint32, (const uint8_t*)pValuesBuffer, nValuesBufferSize);
}

void CDataCaptureChunk::SetInt64ColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_int64 * pValuesBuffer)
{
	getValidBuffer()->setColumnValues(nColumnIndex, AMC::eDataTableV2DataType::Int64, (const uint8_t*)pValuesBuffer, nValuesBufferSize);
}

void CDataCaptureChunk::SetUint64ColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint64 * pValuesBuffer)
{
	getValidBuffer()->setColumnValues(nColumnIndex, AMC::eDataTableV2DataType::Uint64, (const uint8_t*)pValuesBuffer, nValuesBufferSize);
}

void CDataCaptureChunk::SetFloatColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_single * pValuesBuffer)
{
	getValidBuffer()->setColumnValues(nColumnIndex, AMC::eDataTableV2DataType::Float, (const uint8_t*)pValuesBuffer, nValuesBufferSize);
}

void CDataCaptureChunk::SetBoolColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint8 * pValuesBuffer)
{
	getValidBuffer()->setColumnValues(nColumnIndex, AMC::eDataTableV2DataType::Bool, (const uint8_t*)pValuesBuffer, nValuesBufferSize);
}

void CDataCaptureChunk::SetTimestampColumnValues(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint64 * pValuesBuffer)
{
	getValidBuffer()->setColumnValues(nColumnIndex, AMC::eDataTableV2DataType::Timestamp, (const uint8_t*)pValuesBuffer, nValuesBufferSize);
}

void CDataCaptureChunk::BeginRow()
{
	getValidBuffer()->beginRow();
}

void CDataCaptureChunk::SetRowDoubleValue(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_double dValue)
{
	getValidBuffer()->setRowDoubleValue(nColumnIndex, dValue);
}

void CDataCaptureChunk::SetRowIntegerValue(const LibMCEnv_uint32 nColumnIndex, const LibMCEnv_int64 nValue)
{
	getValidBuffer()->setRowIntegerValue(nColumnIndex, nValue);
}

void CDataCaptureChunk::EndRow()
{
	getValidBuffer()->endRow();
}
