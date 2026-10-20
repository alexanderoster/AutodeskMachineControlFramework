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

#ifndef __COMMON_HTTPRANGE
#define __COMMON_HTTPRANGE

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace AMCCommon {

	// Helpers for HTTP range requests on streamed responses. The HTTP library validates byte ranges
	// only for bodies it holds in memory; for a content provider it passes the parsed ranges through
	// unchecked, so the server validates and normalizes them before it sets the provider.
	class CHTTPRange {
	public:

		// A range as parsed by the HTTP library: first and last byte positions, -1 for an absent value.
		typedef std::pair<int64_t, int64_t> Range;

		// Returns true if the ranges describe exactly one satisfiable range of a body with nContentLength
		// bytes, and clamps a last position beyond the end to the last byte. Returns false for an empty
		// body, for several ranges, for a start at or beyond the end, for a last position before the start
		// and for a suffix range of zero length. A full range (-1, -1) is satisfiable.
		static bool normalizeRanges(std::vector<Range>& ranges, uint64_t nContentLength);

		// Returns true if the If-Range validator matches the entity tag (without quotes) or the
		// Last-Modified date of the response. An empty validator matches.
		static bool ifRangeMatches(const std::string& sIfRange, const std::string& sETag, const std::string& sLastModified);

		// Formats a time stamp as HTTP date (RFC 7231 IMF-fixdate), e.g. "Sun, 06 Nov 1994 08:49:37 GMT".
		static std::string formatHTTPDate(uint64_t nMicrosecondsSince1970);

	};

}

#endif //__COMMON_HTTPRANGE
