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


Abstract: This is a stub class definition of COperationSequenceStore

*/

#include "libmcdata_operationsequencestore.hpp"
#include "libmcdata_operationsequencerevisioniterator.hpp"
#include "libmcdata_interfaceexception.hpp"

#include "common_utils.hpp"
#include "common_chrono.hpp"

#define OPERATIONSEQUENCESTORE_MAXNAMELENGTH 256

using namespace LibMCData::Impl;

/*************************************************************************************************************************
 Class definition of COperationSequenceStore 
**************************************************************************************************************************/

COperationSequenceStore::COperationSequenceStore(AMCData::PSQLHandler pSQLHandler)
	: m_pSQLHandler(pSQLHandler)
{
	if (pSQLHandler.get() == nullptr)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);
}

COperationSequenceStore::~COperationSequenceStore()
{
}

std::string COperationSequenceStore::normalizeStateMachine(const std::string& sStateMachine)
{
	std::string sNormalized = AMCCommon::CUtils::trimString(sStateMachine);
	if (sNormalized.empty() || (sNormalized.length() > OPERATIONSEQUENCESTORE_MAXNAMELENGTH))
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDOPERATIONSEQUENCESTATEMACHINE, "invalid state machine name: " + sStateMachine);
	return sNormalized;
}

std::string COperationSequenceStore::normalizeSequenceName(const std::string& sSequenceName)
{
	std::string sNormalized = AMCCommon::CUtils::trimString(sSequenceName);
	if (sNormalized.empty() || (sNormalized.length() > OPERATIONSEQUENCESTORE_MAXNAMELENGTH))
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDOPERATIONSEQUENCENAME, "invalid operation sequence name: " + sSequenceName);
	return sNormalized;
}

std::string COperationSequenceStore::selectColumns()
{
	return "SELECT uuid, statemachine, sequencename, revision, origin, content, sha256, useruuid, timestamp FROM operationsequences";
}

sOperationSequenceRevisionData COperationSequenceStore::readRow(AMCData::PSQLStatement pStatement)
{
	sOperationSequenceRevisionData data;
	data.m_sUUID = pStatement->getColumnUUID(1);
	data.m_sStateMachine = pStatement->getColumnString(2);
	data.m_sSequenceName = pStatement->getColumnString(3);
	data.m_nRevision = (uint32_t)pStatement->getColumnInt64(4);
	data.m_nOrigin = (uint32_t)pStatement->getColumnInt64(5);
	data.m_sContent = pStatement->getColumnString(6);
	data.m_sSHA256 = pStatement->getColumnString(7);
	data.m_sUserUUID = pStatement->getColumnUUID(8);
	data.m_sTimestamp = pStatement->getColumnString(9);
	return data;
}

IOperationSequenceRevision* COperationSequenceStore::StoreRevision(const std::string & sUUID, const std::string & sStateMachine, const std::string & sSequenceName, const LibMCData_uint32 nRevision, const LibMCData_uint32 nOrigin, const std::string & sContent, const std::string & sUserUUID, const LibMCData_uint64 nAbsoluteTimeStamp)
{
	if (nRevision == 0)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDOPERATIONSEQUENCEREVISION);

	sOperationSequenceRevisionData data;
	data.m_sUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);
	data.m_sStateMachine = normalizeStateMachine(sStateMachine);
	data.m_sSequenceName = normalizeSequenceName(sSequenceName);
	data.m_nRevision = nRevision;
	data.m_nOrigin = nOrigin;
	data.m_sContent = sContent;
	data.m_sSHA256 = AMCCommon::CUtils::calculateSHA256FromString(sContent);
	data.m_sUserUUID = sUserUUID.empty() ? AMCCommon::CUtils::createEmptyUUID() : AMCCommon::CUtils::normalizeUUIDString(sUserUUID);
	data.m_sTimestamp = AMCCommon::CChrono::convertToISO8601TimeUTC(nAbsoluteTimeStamp);

	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto pTransaction = m_pSQLHandler->beginTransaction();

	auto pCheckStatement = pTransaction->prepareStatement("SELECT uuid FROM operationsequences WHERE (statemachine=? AND sequencename=? AND revision=?) OR uuid=?");
	pCheckStatement->setString(1, data.m_sStateMachine);
	pCheckStatement->setString(2, data.m_sSequenceName);
	pCheckStatement->setInt64(3, data.m_nRevision);
	pCheckStatement->setString(4, data.m_sUUID);
	bool bExists = pCheckStatement->nextRow();
	pCheckStatement = nullptr;

	if (bExists)
		throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_OPERATIONSEQUENCEREVISIONEXISTS, data.m_sStateMachine + "." + data.m_sSequenceName + " revision " + std::to_string(data.m_nRevision));

	auto pInsertStatement = pTransaction->prepareStatement("INSERT INTO operationsequences (uuid, statemachine, sequencename, revision, origin, content, sha256, useruuid, timestamp) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
	pInsertStatement->setString(1, data.m_sUUID);
	pInsertStatement->setString(2, data.m_sStateMachine);
	pInsertStatement->setString(3, data.m_sSequenceName);
	pInsertStatement->setInt64(4, data.m_nRevision);
	pInsertStatement->setInt64(5, data.m_nOrigin);
	pInsertStatement->setString(6, data.m_sContent);
	pInsertStatement->setString(7, data.m_sSHA256);
	pInsertStatement->setString(8, data.m_sUserUUID);
	pInsertStatement->setString(9, data.m_sTimestamp);
	pInsertStatement->execute();
	pInsertStatement = nullptr;

	pTransaction->commit();

	return new COperationSequenceRevision(data);
}

LibMCData_uint32 COperationSequenceStore::GetLatestRevisionNumber(const std::string & sStateMachine, const std::string & sSequenceName)
{
	std::string sNormalizedStateMachine = normalizeStateMachine(sStateMachine);
	std::string sNormalizedSequenceName = normalizeSequenceName(sSequenceName);

	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto pStatement = m_pSQLHandler->prepareStatement("SELECT MAX(revision) FROM operationsequences WHERE statemachine=? AND sequencename=?");
	pStatement->setString(1, sNormalizedStateMachine);
	pStatement->setString(2, sNormalizedSequenceName);
	if (!pStatement->nextRow())
		return 0;
	if (pStatement->columnIsNull(1))
		return 0;

	return (uint32_t)pStatement->getColumnInt64(1);
}

IOperationSequenceRevisionIterator* COperationSequenceStore::ListLatestRevisions(const std::string & sStateMachine)
{
	std::string sNormalizedStateMachine = normalizeStateMachine(sStateMachine);

	std::unique_ptr<COperationSequenceRevisionIterator> pIterator(new COperationSequenceRevisionIterator());

	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	std::string sQuery = selectColumns() + " o WHERE o.statemachine=? AND o.revision=(SELECT MAX(i.revision) FROM operationsequences i WHERE i.statemachine=o.statemachine AND i.sequencename=o.sequencename) ORDER BY o.sequencename";
	auto pStatement = m_pSQLHandler->prepareStatement(sQuery);
	pStatement->setString(1, sNormalizedStateMachine);
	while (pStatement->nextRow())
		pIterator->addRevision(readRow(pStatement));

	return pIterator.release();
}

IOperationSequenceRevisionIterator* COperationSequenceStore::ListRevisions(const std::string & sStateMachine, const std::string & sSequenceName)
{
	std::string sNormalizedStateMachine = normalizeStateMachine(sStateMachine);
	std::string sNormalizedSequenceName = normalizeSequenceName(sSequenceName);

	std::unique_ptr<COperationSequenceRevisionIterator> pIterator(new COperationSequenceRevisionIterator());

	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto pStatement = m_pSQLHandler->prepareStatement(selectColumns() + " WHERE statemachine=? AND sequencename=? ORDER BY revision DESC");
	pStatement->setString(1, sNormalizedStateMachine);
	pStatement->setString(2, sNormalizedSequenceName);
	while (pStatement->nextRow())
		pIterator->addRevision(readRow(pStatement));

	return pIterator.release();
}

IOperationSequenceRevision* COperationSequenceStore::FindRevision(const std::string & sStateMachine, const std::string & sSequenceName, const LibMCData_uint32 nRevision)
{
	std::string sNormalizedStateMachine = normalizeStateMachine(sStateMachine);
	std::string sNormalizedSequenceName = normalizeSequenceName(sSequenceName);

	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto pStatement = m_pSQLHandler->prepareStatement(selectColumns() + " WHERE statemachine=? AND sequencename=? AND revision=?");
	pStatement->setString(1, sNormalizedStateMachine);
	pStatement->setString(2, sNormalizedSequenceName);
	pStatement->setInt64(3, nRevision);
	if (!pStatement->nextRow())
		return nullptr;

	return new COperationSequenceRevision(readRow(pStatement));
}

IOperationSequenceRevision* COperationSequenceStore::FindRevisionByUUID(const std::string & sUUID)
{
	std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sUUID);

	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto pStatement = m_pSQLHandler->prepareStatement(selectColumns() + " WHERE uuid=?");
	pStatement->setString(1, sNormalizedUUID);
	if (!pStatement->nextRow())
		return nullptr;

	return new COperationSequenceRevision(readRow(pStatement));
}
