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


Abstract: This is the class definition of CDataCapture

*/

#include "libmcenv_datacapture.hpp"
#include "libmcenv_interfaceexception.hpp"

// Include custom headers here.
#include "libmcenv_datacapturechunk.hpp"
#include "libmcenv_datatable.hpp"

using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CDataCapture
**************************************************************************************************************************/

CDataCapture::CDataCapture(PDataCaptureInstance pInstance)
	: m_pInstance(pInstance)
{
	if (pInstance.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
}

CDataCapture::~CDataCapture()
{

}

std::string CDataCapture::GetUUID()
{
	return m_pInstance->getUUID();
}

std::string CDataCapture::GetIdentifier()
{
	return m_pInstance->getIdentifier();
}

std::string CDataCapture::GetName()
{
	return m_pInstance->getName();
}

LibMCEnv::eDataCaptureStatus CDataCapture::GetStatus()
{
	return m_pInstance->getStatus();
}

void CDataCapture::AddColumn(const std::string & sIdentifier, const std::string & sDescription, const LibMCEnv::eDataTableColumnType eColumnType)
{
	m_pInstance->addColumn(sIdentifier, sDescription, eColumnType);
}

LibMCEnv_uint32 CDataCapture::GetColumnCount()
{
	return m_pInstance->getColumnCount();
}

IDataCaptureChunk * CDataCapture::BeginChunk()
{
	auto pBuffer = m_pInstance->beginChunk();
	return new CDataCaptureChunk(m_pInstance, pBuffer);
}

void CDataCapture::CommitChunk(IDataCaptureChunk* pChunkInstance)
{
	auto pChunk = dynamic_cast<CDataCaptureChunk*> (pChunkInstance);
	if (pChunk == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECHUNKINVALID);

	if (!pChunk->belongsTo(m_pInstance.get()))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECHUNKINVALID, "data capture chunk belongs to another capture");

	// The chunk object stays valid if the commit is rejected, so that the producer can complete it.
	m_pInstance->commitChunk(pChunk->getBuffer());
	pChunk->invalidate();
}

void CDataCapture::Flush()
{
	m_pInstance->flush();
}

LibMCEnv_uint64 CDataCapture::GetCapturedRowCount()
{
	return m_pInstance->getCapturedRowCount();
}

LibMCEnv_uint64 CDataCapture::GetStorageSizeInBytes()
{
	return m_pInstance->getStorageSizeInBytes();
}

LibMCEnv_uint32 CDataCapture::GetSegmentCount()
{
	return m_pInstance->getSegmentCount();
}

LibMCEnv_uint32 CDataCapture::GetQueuedChunkCount()
{
	return m_pInstance->getQueuedChunkCount();
}

LibMCEnv_uint64 CDataCapture::GetDroppedRowCount()
{
	return m_pInstance->getDroppedRowCount();
}

std::string CDataCapture::Finish()
{
	return m_pInstance->finish();
}

void CDataCapture::Abort()
{
	m_pInstance->abort();
}

void CDataCapture::AddColumnsFromDataTable(IDataTable* pDataTable)
{
	auto pTable = dynamic_cast<CDataTable*> (pDataTable);
	if (pTable == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCAST);

	uint32_t nTableColumnCount = pTable->GetColumnCount();

	// Check for clashes first, so that the schema is not left half extended.
	auto existingColumns = m_pInstance->getColumns();
	for (uint32_t nIndex = 0; nIndex < nTableColumnCount; nIndex++) {
		std::string sIdentifier = pTable->getColumnByIndex(nIndex)->getIdentifier();
		for (auto& existingColumn : existingColumns) {
			if (existingColumn.m_sIdentifier == sIdentifier)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DUPLICATECOLUMNIDENTIFIER, "duplicate column identifier: " + sIdentifier);
		}
	}

	for (uint32_t nIndex = 0; nIndex < nTableColumnCount; nIndex++) {
		auto pColumn = pTable->getColumnByIndex(nIndex);
		m_pInstance->addColumn(pColumn->getIdentifier(), pColumn->getDescription(), pColumn->getColumnType());
	}
}

LibMCEnv_uint64 CDataCapture::AppendDataTable(IDataTable* pDataTable)
{
	auto pTable = dynamic_cast<CDataTable*> (pDataTable);
	if (pTable == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCAST);

	auto columns = m_pInstance->getColumns();
	if (columns.empty())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND, "data capture has no columns");

	// Resolve every capture column to a table column before anything is written.
	std::vector<const uint8_t*> columnData(columns.size(), nullptr);
	std::vector<size_t> entrySizes(columns.size(), 0);
	uint64_t nRowCount = 0;

	for (size_t nColumnIndex = 0; nColumnIndex < columns.size(); nColumnIndex++) {
		auto& column = columns[nColumnIndex];

		auto pTableColumn = pTable->findColumnByIdentifier(column.m_sIdentifier);
		if (pTableColumn == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND, "data table has no column " + column.m_sIdentifier);
		if ((uint32_t)pTableColumn->getColumnType() != (uint32_t)column.m_DataType)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECOLUMNTYPEMISMATCH, "data table column has a different type: " + column.m_sIdentifier);

		uint64_t nColumnRowCount = pTableColumn->getRowCount();
		if (nColumnIndex == 0)
			nRowCount = nColumnRowCount;
		else if (nColumnRowCount != nRowCount)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATACAPTURECHUNKINCOMPLETE, "data table columns have different row counts: " + column.m_sIdentifier);

		columnData[nColumnIndex] = pTableColumn->getRawData();
		entrySizes[nColumnIndex] = pTableColumn->getEntrySizeInBytes();
	}

	uint64_t nRowIndex = 0;
	while (nRowIndex < nRowCount) {
		auto pBuffer = m_pInstance->beginChunk();

		uint64_t nChunkRows = nRowCount - nRowIndex;
		if (nChunkRows > pBuffer->getCapacity())
			nChunkRows = pBuffer->getCapacity();

		for (size_t nColumnIndex = 0; nColumnIndex < columns.size(); nColumnIndex++)
			pBuffer->setColumnValues((uint32_t)nColumnIndex, columns[nColumnIndex].m_DataType, columnData[nColumnIndex] + (size_t)nRowIndex * entrySizes[nColumnIndex], nChunkRows);

		m_pInstance->commitChunk(pBuffer);
		nRowIndex += nChunkRows;
	}

	return nRowCount;
}
