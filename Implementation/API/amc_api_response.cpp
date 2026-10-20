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

#define __AMCIMPL_API_CONSTANTS

#include "amc_api_response.hpp"
#include "amc_api_constants.hpp"
#include "libmc_interfaceexception.hpp"
#include "amc_streamdownloadregistry.hpp"
#include "common_httprange.hpp"
#include "common_utils.hpp"

#include <cstring>

using namespace AMC;


CAPIResponse::CAPIResponse(uint32_t nHTTPCode, const std::string& sContentType)
	: m_sContentType (sContentType), m_nHTTPCode (nHTTPCode)
{

}

CAPIResponse::~CAPIResponse()
{

}

size_t CAPIResponse::getStreamSize() const
{
	return m_StreamData.size();
}

const uint8_t* CAPIResponse::getStreamData() const
{
	if (m_StreamData.size() > 0)
		return m_StreamData.data();

	return nullptr;
}

std::string CAPIResponse::getContentType() const
{
	return m_sContentType;
}

uint32_t CAPIResponse::getHTTPCode() const
{
	return m_nHTTPCode;
}

std::string CAPIResponse::getContentDispositionName() const
{
	return m_sContentDispositionName;
}

void CAPIResponse::setContentDispositionName(const std::string& sContentDispositionName)
{
	m_sContentDispositionName = sContentDispositionName;
}



CAPIStringResponse::CAPIStringResponse(uint32_t nHTTPCode, const std::string& sContentType, const std::string& sStringValue)
	: CAPIResponse (nHTTPCode, sContentType)
{
	size_t len = sStringValue.length();
	if (len > 0) {
		m_StreamData.resize(len);
		const char* pChar = sStringValue.c_str();
		uint8_t* pData = m_StreamData.data();

		for (size_t idx = 0; idx < len; idx++) {
			*pData = (uint8_t)*pChar;
			pChar++; pData++;
		}

	}


}


CAPIFixedBufferResponse::CAPIFixedBufferResponse(const std::string& sContentType)
	: CAPIResponse (AMC_API_HTTP_SUCCESS, sContentType)
{
}

std::vector<uint8_t>& CAPIFixedBufferResponse::getBuffer()
{
	return m_StreamData;
}

CAPIFixedFloatBufferResponse::CAPIFixedFloatBufferResponse(const std::string& sContentType)
	: CAPIResponse(AMC_API_HTTP_SUCCESS, sContentType), m_nWriteIndex (0)
{

}

void CAPIFixedFloatBufferResponse::resizeTo(size_t nFloatCount)
{
	size_t nByteCount = sizeof(float) * nFloatCount;
	m_StreamData.resize(nByteCount);

	for (auto& it : m_StreamData)
		it = 0;
}

void CAPIFixedFloatBufferResponse::resetWriting()
{
	m_nWriteIndex = 0;
}

void CAPIFixedFloatBufferResponse::addFloat(float fValue)
{
	size_t nStartAddress = m_nWriteIndex * 4;
	if ((nStartAddress + 4) > m_StreamData.size())
		throw ELibMCInterfaceException(LIBMC_ERROR_FIXEDFLOATBUFFEROVERRUN);

	uint8_t* pTarget = &m_StreamData.at(nStartAddress);
	uint8_t* pSource = (uint8_t*)&fValue;

	for (size_t nIndex = 0; nIndex < 4; nIndex++)
		pTarget[nIndex] = pSource[nIndex];

	m_nWriteIndex++;



}


CAPIStorageStreamResponse::CAPIStorageStreamResponse(LibMCData::PStorage pStorage, LibMCData::PStorageStream pStream, PStreamDownloadRegistry pRegistry, AMCCommon::PChrono pGlobalChrono, bool bHeadOnly)
	: CAPIResponse(AMC_API_HTTP_SUCCESS, "application/octet-stream"),
	m_pStorage(pStorage), m_pStream(pStream), m_pRegistry(pRegistry), m_pGlobalChrono(pGlobalChrono),
	m_nSize(0), m_bHeadOnly(bHeadOnly), m_bSlotReserved(false), m_bRegistered(false), m_bFinished(false), m_nSentBytes(0)
{
	if (pStorage.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (pStream.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (pRegistry.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
	if (pGlobalChrono.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	m_sStreamUUID = AMCCommon::CUtils::normalizeUUIDString(pStream->GetUUID());
	m_nSize = pStream->GetSize();
	m_sContentType = pStream->GetMIMEType();
	m_sETag = pStream->GetSHA2();

	std::string sTimeStamp = pStream->GetTimeStamp();
	if (!sTimeStamp.empty())
		m_sLastModified = AMCCommon::CHTTPRange::formatHTTPDate(AMCCommon::CChrono::parseISO8601TimeUTC(sTimeStamp));
}

CAPIStorageStreamResponse::~CAPIStorageStreamResponse()
{
	if (m_bRegistered && (!m_bFinished))
		m_pRegistry->connectionEnded(m_sStreamUUID, false);

	if (m_bSlotReserved)
		m_pRegistry->releaseSlot();
}

bool CAPIStorageStreamResponse::reserveSlot()
{
	if (m_bSlotReserved)
		return true;

	m_bSlotReserved = m_pRegistry->reserveSlot();
	return m_bSlotReserved;
}

void CAPIStorageStreamResponse::setDownloadUUID(const std::string& sDownloadUUID)
{
	m_sDownloadUUID = sDownloadUUID;
}

std::string CAPIStorageStreamResponse::getStreamUUID() const
{
	return m_sStreamUUID;
}

uint64_t CAPIStorageStreamResponse::getSize() const
{
	return m_nSize;
}

std::string CAPIStorageStreamResponse::getETag() const
{
	return m_sETag;
}

std::string CAPIStorageStreamResponse::getLastModified() const
{
	return m_sLastModified;
}

void CAPIStorageStreamResponse::readChunk(uint64_t nOffset, uint64_t nLength, std::vector<uint8_t>& buffer)
{
	if (m_bHeadOnly)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDSTREAMCHUNK, "a HEAD response has no body");
	if ((nLength == 0) || (nLength > AMC_API_DOWNLOADCHUNK_MAXLENGTH))
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDSTREAMCHUNK, "invalid chunk length: " + std::to_string(nLength));
	if ((nOffset >= m_nSize) || (nLength > m_nSize - nOffset))
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDSTREAMCHUNK, "chunk outside of stream: " + std::to_string(nOffset) + "+" + std::to_string(nLength));

	if (!m_bRegistered) {
		m_pRegistry->connectionStarted(m_sStreamUUID, m_nSize);
		m_bRegistered = true;
	}

	if (m_pRegistry->isCancelled(m_sStreamUUID))
		throw ELibMCInterfaceException(LIBMC_ERROR_DOWNLOADCANCELLED, "download cancelled: " + m_sStreamUUID);

	void* pReadCallback = nullptr;
	void* pSeekCallback = nullptr;
	void* pStreamHandle = nullptr;
	m_pStream->GetCallbacks(pReadCallback, pSeekCallback, pStreamHandle);
	if ((pReadCallback == nullptr) || (pSeekCallback == nullptr) || (pStreamHandle == nullptr))
		throw ELibMCInterfaceException(LIBMC_ERROR_INTERNALERROR, "storage stream has no read callbacks");

	buffer.resize((size_t)nLength);

	uint32_t nSeekErrorCode = 0;
	((LibMCData::StreamSeekCallback)pSeekCallback)(nOffset, pStreamHandle, &nSeekErrorCode);
	if (nSeekErrorCode != 0)
		throw ELibMCInterfaceException(LIBMC_ERROR_INTERNALERROR, "seeking the storage stream failed with error " + std::to_string(nSeekErrorCode));

	uint32_t nReadErrorCode = 0;
	((LibMCData::StreamReadCallback)pReadCallback)(buffer.data(), nLength, pStreamHandle, &nReadErrorCode);
	if (nReadErrorCode != 0)
		throw ELibMCInterfaceException(LIBMC_ERROR_INTERNALERROR, "reading the storage stream failed with error " + std::to_string(nReadErrorCode));

	// Recorded before the socket write: a chunk that fails to send ends the connection without success,
	// so the completion flag can not be set by an unsent chunk.
	if (!m_pRegistry->chunkSent(m_sStreamUUID, nOffset, nLength))
		throw ELibMCInterfaceException(LIBMC_ERROR_DOWNLOADCANCELLED, "download cancelled: " + m_sStreamUUID);

	m_nSentBytes += nLength;
}

void CAPIStorageStreamResponse::finish(bool bSuccess)
{
	if (m_bFinished)
		return;
	m_bFinished = true;

	if (m_bRegistered)
		m_pRegistry->connectionEnded(m_sStreamUUID, bSuccess);

	if ((!m_bHeadOnly) && (!m_sDownloadUUID.empty()))
		m_pStorage->LogDownloadFinish(m_sDownloadUUID, m_nSentBytes, bSuccess, m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970());
}
