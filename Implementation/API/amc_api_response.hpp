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


#ifndef __AMC_API_RESPONSE
#define __AMC_API_RESPONSE

#include <cstdint>
#include "header_protection.hpp"

#include "amc_api_types.hpp"
#include "libmcdata_dynamic.hpp"
#include "common_chrono.hpp"

#include <vector>
#include <memory>

namespace AMC {

	class CStreamDownloadRegistry;
	typedef std::shared_ptr<CStreamDownloadRegistry> PStreamDownloadRegistry;

	class CAPIResponse {
	protected:

		std::vector<uint8_t> m_StreamData;

		std::string m_sContentType;

		uint32_t m_nHTTPCode;

		// If not empty, return a content disposition
		std::string m_sContentDispositionName;
			
	public:

		CAPIResponse (uint32_t nHTTPCode, const std::string & sContentType);

		virtual ~CAPIResponse();
		
		size_t getStreamSize () const;
		
		const uint8_t * getStreamData () const;
		
		std::string getContentType () const;

		uint32_t getHTTPCode() const;

		std::string getContentDispositionName () const;

		void setContentDispositionName(const std::string & sContentDispositionName);


	};


	class CAPIStringResponse : public CAPIResponse {
	private:
			
	public:

		CAPIStringResponse(uint32_t nHTTPCode, const std::string & sContentType, const std::string & sStringValue);

	};


	class CAPIFixedFloatBufferResponse : public CAPIResponse {
	private:

		size_t m_nWriteIndex;

	public:

		CAPIFixedFloatBufferResponse(const std::string& sContentType);

		void resizeTo (size_t nFloatCount);

		void resetWriting ();

		void addFloat (float fValue);

	};


	class CAPIFixedBufferResponse : public CAPIResponse {
	private:

	public:

		CAPIFixedBufferResponse(const std::string& sContentType);

		std::vector<uint8_t>& getBuffer();

	};


	// A response whose body is a storage stream on disk. The HTTP server sends it in chunks through
	// the request handler instead of loading it into memory, so files larger than the address space
	// and larger than one socket write can be downloaded. Every response holds its own stream object
	// and thus its own file handle. Delivery is reported to the download registry.
	class CAPIStorageStreamResponse : public CAPIResponse {
	private:

		LibMCData::PStorage m_pStorage;
		LibMCData::PStorageStream m_pStream;
		PStreamDownloadRegistry m_pRegistry;
		AMCCommon::PChrono m_pGlobalChrono;

		std::string m_sStreamUUID;
		uint64_t m_nSize;
		std::string m_sETag;
		std::string m_sLastModified;
		std::string m_sDownloadUUID;

		bool m_bHeadOnly;
		bool m_bSlotReserved;
		bool m_bRegistered;
		bool m_bFinished;
		uint64_t m_nSentBytes;

	public:

		CAPIStorageStreamResponse(LibMCData::PStorage pStorage, LibMCData::PStorageStream pStream, PStreamDownloadRegistry pRegistry, AMCCommon::PChrono pGlobalChrono, bool bHeadOnly);

		// Releases the download slot and ends a registered connection that was never finished.
		virtual ~CAPIStorageStreamResponse();

		// Takes one of the limited download slots. Returns false if none is free.
		bool reserveSlot();

		// Access log entry of this download, written when the download ends.
		void setDownloadUUID(const std::string& sDownloadUUID);

		std::string getStreamUUID() const;
		uint64_t getSize() const;
		// Strong entity tag without quotes (the SHA-256 of the stream).
		std::string getETag() const;
		// Creation time of the stream as HTTP date.
		std::string getLastModified() const;

		// Reads a chunk and records it as sent. The first chunk registers the connection. Fails if the
		// range is outside the stream or the download was cancelled.
		void readChunk(uint64_t nOffset, uint64_t nLength, std::vector<uint8_t>& buffer);

		// Ends the connection in the registry and the access log. Safe to call more than once.
		void finish(bool bSuccess);

	};

	typedef std::shared_ptr<CAPIResponse> PAPIResponse;

	
}


#endif //__AMC_API_RESPONSE

