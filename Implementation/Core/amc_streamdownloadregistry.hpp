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


#ifndef __AMC_STREAMDOWNLOADREGISTRY
#define __AMC_STREAMDOWNLOADREGISTRY

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>

// A download is never slower than this many bytes per chunk write before it counts as idle; the
// server limits the number of downloads that may hold an HTTP worker thread at the same time.
#define AMC_STREAMDOWNLOADREGISTRY_DEFAULTMAXDOWNLOADS 4

namespace AMC {

	struct sStreamDownloadProgress {
		uint32_t m_nActiveConnections;
		// Length of the contiguous start of the stream that has been written to a socket.
		uint64_t m_nDeliveredBytes;
		uint64_t m_nTotalBytes;
		// The last byte was sent and the connection that sent it ended successfully.
		bool m_bCompleted;
		bool m_bCancelled;
		// Time since the last chunk was sent, or since the download was prepared.
		uint64_t m_nMillisecondsSinceActivity;

		sStreamDownloadProgress();
	};

	// Tracks the delivery of storage streams to HTTP clients, keyed by stream UUID. The API download
	// handler reports sent chunks and connection ends; state machines read the progress and cancel.
	// A connection is registered with its first chunk, so HEAD requests and failures before the first
	// byte never touch the counters. All methods are thread safe.
	class CStreamDownloadRegistry {
	private:

		struct sEntry {
			uint64_t m_nSize;
			uint32_t m_nActiveConnections;
			// Sent byte ranges as start -> end (exclusive), merged and non-overlapping.
			std::map<uint64_t, uint64_t> m_SentRanges;
			std::chrono::steady_clock::time_point m_LastActivity;
			bool m_bCancelled;
			bool m_bCompleted;

			sEntry();
			void addRange(uint64_t nOffset, uint64_t nLength);
			uint64_t contiguousBytes() const;
		};

		std::mutex m_Mutex;
		std::map<std::string, sEntry> m_Entries;
		uint32_t m_nActiveDownloads;
		uint32_t m_nMaxDownloads;

		sEntry& findOrCreateNoLock(const std::string& sStreamUUID, uint64_t nSize);

	public:

		CStreamDownloadRegistry(uint32_t nMaxDownloads = AMC_STREAMDOWNLOADREGISTRY_DEFAULTMAXDOWNLOADS);
		virtual ~CStreamDownloadRegistry();

		// Starts a new delivery of a stream: clears sent ranges, completion and cancellation and resets
		// the activity timer. Running connections of an earlier delivery keep counting until they end.
		void prepareDownload(const std::string& sStreamUUID, uint64_t nSize);

		// Reserves one of the limited download slots. Returns false if all are in use.
		bool reserveSlot();
		void releaseSlot();
		uint32_t getActiveDownloadCount();
		uint32_t getMaxDownloads();

		// Called with the first chunk of a connection.
		void connectionStarted(const std::string& sStreamUUID, uint64_t nSize);

		// Records a chunk as written to the socket. Returns false if the download was cancelled;
		// the caller then stops sending.
		bool chunkSent(const std::string& sStreamUUID, uint64_t nOffset, uint64_t nLength);

		// Called once per registered connection when it ends. bSuccess means that all requested bytes
		// were written.
		void connectionEnded(const std::string& sStreamUUID, bool bSuccess);

		bool isCancelled(const std::string& sStreamUUID);

		bool hasActiveConnections(const std::string& sStreamUUID);

		sStreamDownloadProgress getProgress(const std::string& sStreamUUID);

		// Marks the stream as cancelled, so that every running connection stops with its next chunk.
		// Returns false if the stream had already been delivered completely.
		bool cancel(const std::string& sStreamUUID);

	};

	typedef std::shared_ptr<CStreamDownloadRegistry> PStreamDownloadRegistry;

}

#endif //__AMC_STREAMDOWNLOADREGISTRY
