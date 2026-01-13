/*++

Copyright (C) 2023 Autodesk Inc.

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


#include "amc_telemetry.hpp"

#include "common_utils.hpp"
#include "libmc_exceptiontypes.hpp"

#include "libmcdata_dynamic.hpp"

namespace AMC {

	CTelemetryDataChunk::CTelemetryDataChunk(CTelemetryWriter* pWriter, uint32_t nChunkID)	
		: m_nTelemetryChunkID(nChunkID), m_nMinTimestamp(0), m_nMaxTimestamp(0), m_nMinMarkerID(0), m_nMaxMarkerID(0),
		m_pWriter(pWriter)
	{
		if (pWriter == nullptr)
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	}

	CTelemetryDataChunk::~CTelemetryDataChunk()
	{

	}

	bool CTelemetryDataChunk::isEmpty() 
	{
		std::lock_guard<std::mutex> lock(m_ChunkMutex);

		return m_Entries.empty();
	}

	void CTelemetryDataChunk::writeEntry(const sTelemetryChunkEntry& entry)
	{
		uint32_t nEntryIndex;

		{
			std::lock_guard<std::mutex> lock(m_ChunkMutex);

			m_Entries.emplace_back(entry);
			// entry index shall be one-based
			nEntryIndex = static_cast<uint32_t> (m_Entries.size());

			if (nEntryIndex == 1) {
				m_nMinMarkerID = entry.m_nMarkerID;
				m_nMaxMarkerID = entry.m_nMarkerID;
				m_nMinTimestamp = entry.m_nTimestamp;
				m_nMaxTimestamp = entry.m_nTimestamp;
			}
			else {
				if (entry.m_nMarkerID < m_nMinMarkerID)
					m_nMinMarkerID = entry.m_nMarkerID;
				if (entry.m_nMarkerID > m_nMaxMarkerID)
					m_nMaxMarkerID = entry.m_nMarkerID;
				if (entry.m_nTimestamp < m_nMinTimestamp)
					m_nMinTimestamp = entry.m_nTimestamp;
				if (entry.m_nTimestamp > m_nMaxTimestamp)
					m_nMaxTimestamp = entry.m_nTimestamp;

			}

		}

		if (entry.m_EntryType == eTelemetryChunkEntryType::IntervalStartMarker) {
			m_pWriter->registerOpenInterval (entry.m_nMarkerID, m_nTelemetryChunkID, nEntryIndex);
		}
		else if (entry.m_EntryType == eTelemetryChunkEntryType::IntervalEndMarker) {
			m_pWriter->eraseOpenInterval(entry.m_nMarkerID);
		}

	}




	CTelemetryWriter::CTelemetryWriter(LibMCData::PTelemetrySession pTelemetrySession, AMCCommon::PChrono pGlobalChrono)
		: m_nNextChunkIndex(1), m_pTelemetrySession(pTelemetrySession), m_nNextMarkerID(1), m_pGlobalChrono (pGlobalChrono)
	{
		if (pGlobalChrono.get () == nullptr)
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
		if (pTelemetrySession.get() == nullptr)
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

		createNewChunk();
	}

	CTelemetryWriter::~CTelemetryWriter()
	{

	}

	PTelemetryDataChunk CTelemetryWriter::getCurrentChunk()
	{
		std::lock_guard<std::mutex> lock(m_ChunkMutex);
		return m_pCurrentChunk;
	}

	PTelemetryDataChunk CTelemetryWriter::createNewChunk()
	{
		std::lock_guard<std::mutex> lock(m_ChunkMutex);
		auto pChunk = std::make_shared<CTelemetryDataChunk> (this, m_nNextChunkIndex);
		m_nNextChunkIndex++;
		m_Chunks.emplace_back(pChunk);
		m_pCurrentChunk = pChunk;

		return pChunk;
	}

	void CTelemetryWriter::writeEntry(const sTelemetryChunkEntry& entry)
	{
		auto pChunk = getCurrentChunk();
		pChunk->writeEntry(entry);

	}

	void CTelemetryWriter::registerOpenInterval(uint64_t nMarkerID, uint32_t nChunkID, uint32_t nChunkEntryIndex)
	{
		std::lock_guard<std::mutex> lock(m_OpenIntervalMutex);
		sTelemetryOpenIntervalMarker openIntervalMarker;
		openIntervalMarker.m_nChunkID = nChunkID;
		openIntervalMarker.m_nEntryIndex = nChunkEntryIndex;
		auto insertResult = m_OpenIntervalMarkers.emplace(nMarkerID, openIntervalMarker);
		if (insertResult.second == false)
			throw ELibMCCustomException(LIBMC_ERROR_TELEMETRYMARKERALREADYREGISTERED, std::to_string(nMarkerID));
	}

	void CTelemetryWriter::eraseOpenInterval(uint64_t nMarkerID)
	{
		std::lock_guard<std::mutex> lock(m_OpenIntervalMutex);
		m_OpenIntervalMarkers.erase(nMarkerID);

	}

	void CTelemetryWriter::createChannelInDB(const std::string& sUUID, LibMCData::eTelemetryChannelType channelType, uint32_t nChannelIndex, const std::string& sChannelIdentifier, const std::string& sChannelDescription)
	{
		std::lock_guard<std::mutex> lock(m_DataMutex);
		m_pTelemetrySession->CreateChannelInDB (sUUID, channelType, nChannelIndex, sChannelIdentifier, sChannelDescription);
	}

	uint64_t CTelemetryWriter::createMarkerID()
	{
		return m_nNextMarkerID.fetch_add(1, std::memory_order_relaxed);
	}

	uint64_t CTelemetryWriter::getCurrentTimestamp()
	{
		return m_pGlobalChrono->getElapsedMicroseconds();
	}


	CTelemetryMarker::CTelemetryMarker(WTelemetryChannel pChannel, uint64_t nMarkerID, bool bMarkInstant, uint64_t nContextData)
		: m_pChannel(pChannel), m_nMarkerID(nMarkerID), m_nStartTimestamp(0), m_nFinishTimestamp(0), m_nContextData(nContextData)
	{

		auto pLockedChannel = m_pChannel.lock();
		if (pLockedChannel.get() == nullptr)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDPARAM, "Telemetry Channel is null");

		m_nStartTimestamp = pLockedChannel->getCurrentTimestamp();
		uint32_t nChannelIndex = pLockedChannel->getChannelIndex();
		auto pWriter = pLockedChannel->getWriter();

		if (bMarkInstant) {
			m_nFinishTimestamp = m_nStartTimestamp;
			pWriter->writeEntry({ eTelemetryChunkEntryType::InstantMarker, nChannelIndex, m_nMarkerID, m_nStartTimestamp, m_nContextData });
		}
		else {
			pWriter->writeEntry({ eTelemetryChunkEntryType::IntervalStartMarker, nChannelIndex, m_nMarkerID, m_nStartTimestamp, m_nContextData });

		}

	}

	CTelemetryMarker::~CTelemetryMarker()
	{

	}

	WTelemetryChannel CTelemetryMarker::getChannel() const
	{
		return m_pChannel;
	}

	void CTelemetryMarker::finishMarker()
	{
		auto pLockedChannel = m_pChannel.lock();
		if (pLockedChannel.get () == nullptr)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDPARAM, "Telemetry Channel is null");

		const uint64_t nTimestamp = pLockedChannel->getCurrentTimestamp();
		uint32_t nChannelIndex = pLockedChannel->getChannelIndex();
		auto pWriter = pLockedChannel->getWriter();

		if (nTimestamp < m_nStartTimestamp)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDTELEMETRYMARKERFINISHTIMESTAMP,
				pLockedChannel->getChannelIdentifier() + " / " + std::to_string(m_nMarkerID));

		uint64_t expected = 0;
		if (!m_nFinishTimestamp.compare_exchange_strong(
			expected, nTimestamp,
			std::memory_order_release,
			std::memory_order_relaxed))
		{
			// expected now contains the current (non-zero) finish timestamp
			throw ELibMCCustomException(LIBMC_ERROR_TELEMETRYMARKERHASALREADYBEENFINISHED,
				pLockedChannel->getChannelIdentifier() + " / " + std::to_string(m_nMarkerID));
		}

		pWriter->writeEntry({ eTelemetryChunkEntryType::IntervalEndMarker, nChannelIndex, m_nMarkerID, nTimestamp, m_nContextData });

		pLockedChannel->unregisterMarker(m_nMarkerID);
	}



	bool CTelemetryMarker::isFinished() const
	{
		auto nFinishTimestamp = m_nFinishTimestamp.load(std::memory_order_acquire);
		return (nFinishTimestamp != 0);
	}
	 
	uint64_t CTelemetryMarker::getDuration() const
	{
		auto nFinishTimestamp = m_nFinishTimestamp.load(std::memory_order_acquire);
		if (nFinishTimestamp == 0)
			throw ELibMCCustomException(LIBMC_ERROR_UNFINISHEDTELEMETRYMARKERSHAVENODURATION, std::to_string(m_nMarkerID));

		return (nFinishTimestamp - m_nStartTimestamp);
	}

	uint64_t CTelemetryMarker::getMarkerID() const
	{
		return m_nMarkerID;
	}

	uint64_t CTelemetryMarker::getStartTimestamp() const
	{
		return m_nStartTimestamp;
	}

	uint64_t CTelemetryMarker::getFinishTimestamp() const
	{
		auto nFinishTimestamp = m_nFinishTimestamp.load(std::memory_order_acquire);
		return nFinishTimestamp;
	}

	uint64_t CTelemetryMarker::getContextData() const
	{
		return m_nContextData;
	}

	CTelemetryChannel::CTelemetryChannel(const std::string& sChannelUUID, uint32_t nChannelIndex, const std::string& sChannelIdentifier, const std::string& sChannelDescription, LibMCData::eTelemetryChannelType channelType, PTelemetryWriter pTelemetryWriter)
		: m_sChannelUUID(sChannelUUID), m_nChannelIndex(nChannelIndex), m_sChannelIdentifier(sChannelIdentifier), m_sChannelDescription(sChannelDescription), m_eChannelType(channelType), m_pTelemetryWriter (pTelemetryWriter)
	{

		if (pTelemetryWriter.get () == nullptr)
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	}

	CTelemetryChannel::~CTelemetryChannel()
	{

	}
	 
	std::string CTelemetryChannel::getChannelUUID() const
	{
		return m_sChannelUUID;
	}

	uint32_t CTelemetryChannel::getChannelIndex() const
	{
		return m_nChannelIndex;
	}

	std::string CTelemetryChannel::getChannelIdentifier() const
	{
		return m_sChannelIdentifier;
	}

	std::string CTelemetryChannel::getChannelDescription() const
	{
		return m_sChannelDescription;
	}

	LibMCData::eTelemetryChannelType CTelemetryChannel::getChannelType() const
	{
		return m_eChannelType;
	}

	PTelemetryMarker CTelemetryChannel::createMarkerEx(bool bMarkInstant, uint64_t nContextData)
	{

		uint64_t nMarkerID = m_pTelemetryWriter->createMarkerID();

		// Retrieve the shared pointer I am living in...
		auto selfPtr = shared_from_this();

		auto pMarker = std::make_shared<CTelemetryMarker>(selfPtr, nMarkerID, bMarkInstant, nContextData);

		if (!bMarkInstant)
		{
			std::lock_guard<std::mutex> lockGuard(m_MarkerMutex);
			m_Markers.emplace(nMarkerID, pMarker);
		}

		return pMarker;

	}


	void CTelemetryChannel::createInstantMarker(uint64_t nContextData)
	{
		createMarkerEx(true, nContextData);
	}

	CTelemetryScope CTelemetryChannel::startIntervalScope(uint64_t nContextData)
	{
		return CTelemetryScope (createMarkerEx(false, nContextData));
	}

	PTelemetryMarker CTelemetryChannel::startIntervalMarker(uint64_t nContextData)
	{
		return createMarkerEx(false, nContextData);
	}


	uint64_t CTelemetryChannel::getCurrentTimestamp()
	{
		uint64_t nTimestamp = m_pTelemetryWriter->getCurrentTimestamp();
		// Ensure Timestamp is never zero (should never happen anyhow)..
		if (nTimestamp == 0)
			nTimestamp = 1;

		return nTimestamp;
	}

	void CTelemetryChannel::unregisterMarker(uint64_t markerID)
	{
		std::lock_guard<std::mutex> lock(m_MarkerMutex);
		m_Markers.erase(markerID);
	}

	PTelemetryWriter CTelemetryChannel::getWriter() const
	{
		return m_pTelemetryWriter;
	}


	

	CTelemetryScope::CTelemetryScope(PTelemetryMarker pMarker)
		: m_pMarker(pMarker)
	{
		if (pMarker.get() == nullptr)
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDPARAM, "Telemetry Marker is null");
	}

	CTelemetryScope::~CTelemetryScope()
	{
		if (m_pMarker.get() != nullptr) {
			try {
				m_pMarker->finishMarker();
			}
			catch (...) {
				// ignore in destructor
			}
		}
	}


	CTelemetryHandler::CTelemetryHandler(PTelemetryWriter pWriter)
		: m_nNextChannelIndex (1),  m_pTelemetryWriter (pWriter)
	{
		if (pWriter.get () == nullptr)
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	}

	CTelemetryHandler::~CTelemetryHandler()
	{
		m_ChannelsByUUID.clear();
		m_ChannelsByIdentifier.clear();
		m_pTelemetryWriter = nullptr;
	}

	PTelemetryChannel CTelemetryHandler::registerChannel(const std::string& sChannelIdentifier, const std::string& sChannelDescription, LibMCData::eTelemetryChannelType channelType)
	{
		if (!AMCCommon::CUtils::stringIsValidAlphanumericPathString(sChannelIdentifier))
			throw ELibMCCustomException(LIBMC_ERROR_INVALIDTELEMETRYCHANNELIDENTIFIER, sChannelIdentifier);

		std::lock_guard<std::mutex> lockGuard(m_ChannelMutex);

		auto iIdentifierIter = m_ChannelsByIdentifier.find(sChannelIdentifier);
		if (iIdentifierIter != m_ChannelsByIdentifier.end())
			throw ELibMCCustomException(LIBMC_ERROR_TELEMETRYCHANNELALREADYEXISTS, sChannelIdentifier);

		std::string sUUID = AMCCommon::CUtils::createUUID();
		uint32_t nChannelIndex = m_nNextChannelIndex;
		m_nNextChannelIndex++;

		auto pChannel = std::make_shared<CTelemetryChannel>(sUUID, nChannelIndex, sChannelIdentifier, sChannelDescription, channelType, m_pTelemetryWriter);

		m_ChannelsByIdentifier.emplace(sChannelIdentifier, pChannel);
		m_ChannelsByUUID.emplace(sUUID, pChannel);

		// We want to keep the lock of the channel mutex while creating the channel in the DB, to be able to roll back in case of error.
		try 
		{
			m_pTelemetryWriter->createChannelInDB(sUUID, channelType, nChannelIndex, sChannelIdentifier, sChannelDescription);
		}
		catch (...)
		{
			// roll back changes in case of error...
			m_ChannelsByIdentifier.erase(sChannelIdentifier);
			m_ChannelsByUUID.erase(sUUID);

			throw;
		}


		return pChannel;

	}

	PTelemetryChannel CTelemetryHandler::findChannelByUUID(const std::string& sChannelUUID)
	{
		std::lock_guard<std::mutex> lockGuard(m_ChannelMutex);

		auto iUUIDIter = m_ChannelsByUUID.find(sChannelUUID);
		if (iUUIDIter == m_ChannelsByUUID.end())
			throw ELibMCCustomException(LIBMC_ERROR_TELEMETRYCHANNELNOTFOUND, sChannelUUID);
		return iUUIDIter->second;
	}

	PTelemetryChannel CTelemetryHandler::findChannelByIdentifier(const std::string& sChannelIdentifier, bool bFailIfNotExisting)
	{

		std::lock_guard<std::mutex> lockGuard(m_ChannelMutex);

		auto iIdentifierIter = m_ChannelsByIdentifier.find(sChannelIdentifier);	
		if (iIdentifierIter == m_ChannelsByIdentifier.end()) {
			if (bFailIfNotExisting)
				throw ELibMCCustomException(LIBMC_ERROR_TELEMETRYCHANNELNOTFOUND, sChannelIdentifier);
			return nullptr;
		}
		return iIdentifierIter->second;
	}

	void CTelemetryHandler::createInstantMarker(const std::string& sChannelIdentifier, uint64_t nContextData)
	{
		auto pChannel = findChannelByIdentifier(sChannelIdentifier, true);
		pChannel->createInstantMarker(nContextData);

	}

	CTelemetryScope CTelemetryHandler::startIntervalMarker(const std::string& sChannelIdentifier, uint64_t nContextData)
	{
		auto pChannel = findChannelByIdentifier(sChannelIdentifier, true);
		return pChannel->startIntervalMarker(nContextData);
	}



}




