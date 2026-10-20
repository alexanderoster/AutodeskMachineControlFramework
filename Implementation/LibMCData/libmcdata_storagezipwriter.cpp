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


Abstract: This is a stub class definition of CStorageZIPWriter

*/

#include "libmcdata_storagezipwriter.hpp"
#include "common_utils.hpp"
#include "libmcdata_interfaceexception.hpp"

// Include custom headers here.
#include "amcdata_storagestate.hpp"

using namespace LibMCData::Impl;

/*************************************************************************************************************************
 Class definition of CStorageZIPWriter 
**************************************************************************************************************************/


CStorageZIPWriter::CStorageZIPWriter(AMCData::PStorageWriter_ZIPStream pStorageWriter, AMCData::PSQLHandler pSQLHandler, AMCData::PStorageState pStorageState)
	: m_pStorageWriter (pStorageWriter), m_pSQLHandler (pSQLHandler), m_pStorageState (pStorageState)
{
	if (pStorageWriter.get () == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
	if (pStorageState.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

}

CStorageZIPWriter::~CStorageZIPWriter()
{
	if ((m_pStorageWriter.get() != nullptr) && (!m_pStorageWriter->isFinalized()))
		m_pStorageState->deletePartialWriter(m_pStorageWriter->getUUID());

	m_pStorageWriter = nullptr;
}

AMCData::CStorageWriter_ZIPStream* CStorageZIPWriter::getWriter()
{
	if (m_pStorageWriter.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_ZIPWRITINGALREADYFINISHED, "ZIP stream has been discarded");

	return m_pStorageWriter.get();
}

void CStorageZIPWriter::Discard()
{
	if (m_pStorageWriter.get() == nullptr)
		return;

	if (m_pStorageWriter->isFinalized())
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_ZIPSTREAMALREADYFINISHED, "ZIP stream has already been finished: " + m_pStorageWriter->getUUID());

	std::string sUUID = m_pStorageWriter->getUUID();
	m_pStorageState->deletePartialWriter(sUUID);

	// The last reference closes the file; an unfinished writer deletes it.
	m_pStorageWriter = nullptr;
	AMCCommon::CUtils::deleteFileFromDisk(m_pStorageState->getStreamPath(sUUID), false);

	auto pTransaction = m_pSQLHandler->beginTransaction();

	auto pLinkStatement = pTransaction->prepareStatement("DELETE FROM journal_tempstreams WHERE streamuuid=?");
	pLinkStatement->setString(1, sUUID);
	pLinkStatement->execute();
	pLinkStatement = nullptr;

	auto pStreamStatement = pTransaction->prepareStatement("DELETE FROM storage_streams WHERE uuid=? AND status=?");
	pStreamStatement->setString(1, sUUID);
	pStreamStatement->setString(2, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssNew));
	pStreamStatement->execute();
	pStreamStatement = nullptr;

	pTransaction->commit();
}

LibMCData_uint32 CStorageZIPWriter::StartNewEntry(const std::string & sFileName, const LibMCData_uint64 nAbsoluteTimeStamp)
{
	return getWriter()->startNewEntry(sFileName, nAbsoluteTimeStamp);
}

void CStorageZIPWriter::FinishCurrentEntry()
{
	getWriter()->finishCurrentEntry();
}

LibMCData_uint32 CStorageZIPWriter::GetOpenEntryID()
{
	return getWriter()->getOpenEntryID();
}

void CStorageZIPWriter::WriteData(const LibMCData_uint32 nEntryID, const LibMCData_uint64 nDataBufferSize, const LibMCData_uint8 * pDataBuffer)
{
	getWriter()->writeToCurrentEntry(nEntryID, pDataBuffer, nDataBufferSize);
}

LibMCData_uint64 CStorageZIPWriter::GetEntrySize(const LibMCData_uint32 nEntryID)
{
	return getWriter()->getEntrySize(nEntryID);
}

void CStorageZIPWriter::Finish()
{
	std::string sCalculatedSHA256;
	std::string sCalculatedBlockSHA256;
	
	getWriter()->finalize (sCalculatedSHA256, sCalculatedBlockSHA256);

	uint64_t nSize = getWriter()->getZIPStreamSize();

	std::string sUpdateQuery = "UPDATE storage_streams SET size=?, status=?, sha2=?, sha256_block64k=? WHERE uuid=? AND status=?";
	auto pUpdateStatement = m_pSQLHandler->prepareStatement(sUpdateQuery);
	pUpdateStatement->setString(1, std::to_string(nSize));
	pUpdateStatement->setString(2, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssValidated));
	pUpdateStatement->setString(3, sCalculatedSHA256);
	pUpdateStatement->setString(4, sCalculatedBlockSHA256);
	pUpdateStatement->setString(5, getWriter()->getUUID ());
	pUpdateStatement->setString(6, AMCData::CStorageState::storageStreamStatusToString(AMCData::eStorageStreamStatus::sssNew));
	pUpdateStatement->execute();
	pUpdateStatement = nullptr;

	// The finished writer is no longer a partial writer; keeping it would hold the file reference forever.
	m_pStorageState->deletePartialWriter(m_pStorageWriter->getUUID());
}

bool CStorageZIPWriter::IsFinished()
{
	return getWriter()->isFinalized ();
}

LibMCData_uint64 CStorageZIPWriter::GetZIPStreamSize()
{
	return getWriter()->getZIPStreamSize();
}