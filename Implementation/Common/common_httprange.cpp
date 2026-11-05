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

#include "common_httprange.hpp"

#include <ctime>
#include <cstdio>

using namespace AMCCommon;

bool CHTTPRange::normalizeRanges(std::vector<Range>& ranges, uint64_t nContentLength)
{
	if (ranges.size() != 1)
		return false;

	if (nContentLength == 0)
		return false;

	int64_t nLength = (int64_t)nContentLength;
	auto& range = ranges[0];
	int64_t nFirst = range.first;
	int64_t nLast = range.second;

	if ((nFirst == -1) && (nLast == -1))
		return true;

	if (nFirst == -1) {
		// Suffix range "-N": the last N bytes. N must be positive; a suffix longer than the body is clamped
		// by the HTTP library.
		return (nLast > 0);
	}

	if (nFirst < 0)
		return false;

	if (nFirst >= nLength)
		return false;

	if (nLast == -1)
		return true;

	if (nLast < nFirst)
		return false;

	if (nLast >= nLength)
		range.second = nLength - 1;

	return true;
}

bool CHTTPRange::ifRangeMatches(const std::string& sIfRange, const std::string& sETag, const std::string& sLastModified)
{
	if (sIfRange.empty())
		return true;

	if (sIfRange == "\"" + sETag + "\"")
		return true;

	if ((!sETag.empty()) && (sIfRange == sETag))
		return true;

	if ((!sLastModified.empty()) && (sIfRange == sLastModified))
		return true;

	return false;
}

std::string CHTTPRange::formatHTTPDate(uint64_t nMicrosecondsSince1970)
{
	static const char* pDayNames[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char* pMonthNames[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	time_t nSeconds = (time_t)(nMicrosecondsSince1970 / 1000000ULL);
	struct tm timeInfo;
#ifdef _WIN32
	if (gmtime_s(&timeInfo, &nSeconds) != 0)
		return "";
#else
	if (gmtime_r(&nSeconds, &timeInfo) == nullptr)
		return "";
#endif

	if ((timeInfo.tm_wday < 0) || (timeInfo.tm_wday > 6) || (timeInfo.tm_mon < 0) || (timeInfo.tm_mon > 11))
		return "";

	char buffer[64];
	snprintf(buffer, sizeof(buffer), "%s, %02d %s %04d %02d:%02d:%02d GMT",
		pDayNames[timeInfo.tm_wday], timeInfo.tm_mday, pMonthNames[timeInfo.tm_mon], timeInfo.tm_year + 1900,
		timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

	return std::string(buffer);
}
