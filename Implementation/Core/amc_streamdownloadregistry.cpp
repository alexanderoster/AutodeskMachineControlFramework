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

*/

#include "amc_streamdownloadregistry.hpp"
#include "libmc_exceptiontypes.hpp"
#include "common_utils.hpp"

using namespace AMC;

sStreamDownloadProgress::sStreamDownloadProgress()
	: m_nActiveConnections(0), m_nDeliveredBytes(0), m_nTotalBytes(0), m_bCompleted(false), m_bCancelled(false), m_nMillisecondsSinceActivity(0)
{
}

CStreamDownloadRegistry::sEntry::sEntry()
	: m_nSize(0), m_nActiveConnections(0), m_LastActivity(std::chrono::steady_clock::now()), m_bCancelled(false), m_bCompleted(false)
{
}

void CStreamDownloadRegistry::sEntry::addRange(uint64_t nOffset, uint64_t nLength)
{
	if (nLength == 0)
		return;

	uint64_t nStart = nOffset;
	uint64_t nEnd = nOffset + nLength;

	// Merge with every range that touches or overlaps [nStart, nEnd).
	auto iter = m_SentRanges.lower_bound(nStart);
	if (iter != m_SentRanges.begin()) {
		auto iPrev = std::prev(iter);
		if (iPrev->second >= nStart)
			iter = iPrev;
	}

	while ((iter != m_SentRanges.end()) && (iter->first <= nEnd)) {
		if (iter->first < nStart)
			nStart = iter->first;
		if (iter->second > nEnd)
			nEnd = iter->second;
		iter = m_SentRanges.erase(iter);
	}

	m_SentRanges.insert(std::make_pair(nStart, nEnd));
}

uint64_t CStreamDownloadRegistry::sEntry::contiguousBytes() const
{
	auto iter = m_SentRanges.find(0);
	if (iter == m_SentRanges.end())
		return 0;
	return iter->second;
}

CStreamDownloadRegistry::CStreamDownloadRegistry(uint32_t nMaxDownloads)
	: m_nActiveDownloads(0), m_nMaxDownloads(nMaxDownloads)
{
	if (m_nMaxDownloads == 0)
		m_nMaxDownloads = 1;
}

CStreamDownloadRegistry::~CStreamDownloadRegistry()
{
}

CStreamDownloadRegistry::sEntry& CStreamDownloadRegistry::findOrCreateNoLock(const std::string& sStreamUUID, uint64_t nSize)
{
	auto iter = m_Entries.find(sStreamUUID);
	if (iter == m_Entries.end()) {
		sEntry entry;
		entry.m_nSize = nSize;
		iter = m_Entries.insert(std::make_pair(sStreamUUID, entry)).first;
	}
	else if (nSize > 0) {
		iter->second.m_nSize = nSize;
	}

	return iter->second;
}

void CStreamDownloadRegistry::prepareDownload(const std::string& sStreamUUID, uint64_t nSize)
{
	std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto& entry = findOrCreateNoLock(sNormalizedUUID, nSize);
	entry.m_SentRanges.clear();
	entry.m_bCancelled = false;
	entry.m_bCompleted = false;
	entry.m_LastActivity = std::chrono::steady_clock::now();
}

bool CStreamDownloadRegistry::reserveSlot()
{
	std::lock_guard<std::mutex> lockGuard(m_Mutex);
	if (m_nActiveDownloads >= m_nMaxDownloads)
		return false;

	m_nActiveDownloads++;
	return true;
}

void CStreamDownloadRegistry::releaseSlot()
{
	std::lock_guard<std::mutex> lockGuard(m_Mutex);
	if (m_nActiveDownloads > 0)
		m_nActiveDownloads--;
}

uint32_t CStreamDownloadRegistry::getActiveDownloadCount()
{
	std::lock_guard<std::mutex> lockGuard(m_Mutex);
	return m_nActiveDownloads;
}

uint32_t CStreamDownloadRegistry::getMaxDownloads()
{
	std::lock_guard<std::mutex> lockGuard(m_Mutex);
	return m_nMaxDownloads;
}

void CStreamDownloadRegistry::connectionStarted(const std::string& sStreamUUID, uint64_t nSize)
{
	std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto& entry = findOrCreateNoLock(sNormalizedUUID, nSize);
	entry.m_nActiveConnections++;
	entry.m_LastActivity = std::chrono::steady_clock::now();
}

bool CStreamDownloadRegistry::chunkSent(const std::string& sStreamUUID, uint64_t nOffset, uint64_t nLength)
{
	std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto& entry = findOrCreateNoLock(sNormalizedUUID, 0);
	if (entry.m_bCancelled)
		return false;

	entry.addRange(nOffset, nLength);
	entry.m_LastActivity = std::chrono::steady_clock::now();
	return true;
}

void CStreamDownloadRegistry::connectionEnded(const std::string& sStreamUUID, bool bSuccess)
{
	std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto iter = m_Entries.find(sNormalizedUUID);
	if (iter == m_Entries.end())
		return;

	auto& entry = iter->second;
	if (entry.m_nActiveConnections > 0)
		entry.m_nActiveConnections--;

	entry.m_LastActivity = std::chrono::steady_clock::now();

	if (bSuccess && (!entry.m_bCancelled) && (entry.m_nSize > 0) && (entry.contiguousBytes() >= entry.m_nSize))
		entry.m_bCompleted = true;
}

bool CStreamDownloadRegistry::isCancelled(const std::string& sStreamUUID)
{
	std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto iter = m_Entries.find(sNormalizedUUID);
	if (iter == m_Entries.end())
		return false;

	return iter->second.m_bCancelled;
}

bool CStreamDownloadRegistry::hasActiveConnections(const std::string& sStreamUUID)
{
	std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto iter = m_Entries.find(sNormalizedUUID);
	if (iter == m_Entries.end())
		return false;

	return (iter->second.m_nActiveConnections > 0);
}

sStreamDownloadProgress CStreamDownloadRegistry::getProgress(const std::string& sStreamUUID)
{
	std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	sStreamDownloadProgress progress;

	auto iter = m_Entries.find(sNormalizedUUID);
	if (iter == m_Entries.end())
		return progress;

	auto& entry = iter->second;
	progress.m_nActiveConnections = entry.m_nActiveConnections;
	progress.m_nDeliveredBytes = entry.contiguousBytes();
	progress.m_nTotalBytes = entry.m_nSize;
	progress.m_bCompleted = entry.m_bCompleted;
	progress.m_bCancelled = entry.m_bCancelled;

	auto now = std::chrono::steady_clock::now();
	if (now > entry.m_LastActivity)
		progress.m_nMillisecondsSinceActivity = (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - entry.m_LastActivity).count();

	return progress;
}

bool CStreamDownloadRegistry::cancel(const std::string& sStreamUUID)
{
	std::string sNormalizedUUID = AMCCommon::CUtils::normalizeUUIDString(sStreamUUID);
	std::lock_guard<std::mutex> lockGuard(m_Mutex);

	auto& entry = findOrCreateNoLock(sNormalizedUUID, 0);
	if (entry.m_bCompleted)
		return false;

	entry.m_bCancelled = true;
	entry.m_LastActivity = std::chrono::steady_clock::now();
	return true;
}
