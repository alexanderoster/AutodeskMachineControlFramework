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

#ifndef __AMCTEST_UNITTEST_STREAMDOWNLOAD
#define __AMCTEST_UNITTEST_STREAMDOWNLOAD

#include "amc_unittests.hpp"
#include "amc_streamdownloadregistry.hpp"
#include "common_httprange.hpp"
#include "common_utils.hpp"

#include <string>
#include <vector>

namespace AMCUnitTest {

	// Tests of the download registry (contiguous delivery, cancel, connection counting) and of the
	// HTTP range validation that the server applies before it streams a storage stream.
	class CUnitTestGroup_StreamDownload : public CUnitTestGroup {
	public:
		CUnitTestGroup_StreamDownload() = default;
		virtual ~CUnitTestGroup_StreamDownload() = default;

		std::string getTestGroupName() override {
			return "StreamDownload";
		}

		void registerTests() override {
			registerTest("RegistryContiguousDelivery", "Delivered bytes grow only without gaps and resume continues at the gap", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_StreamDownload::test_RegistryContiguousDelivery, this));
			registerTest("RegistryCompletion", "Completion needs the last byte and a successful connection end", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_StreamDownload::test_RegistryCompletion, this));
			registerTest("RegistryCancel", "Cancel stops chunks, is refused after completion and is cleared by a new delivery", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_StreamDownload::test_RegistryCancel, this));
			registerTest("RegistryConnectionCounting", "Connection ends without a start and double ends never make the count negative", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_StreamDownload::test_RegistryConnectionCounting, this));
			registerTest("RegistrySlots", "The number of concurrent downloads is limited", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_StreamDownload::test_RegistrySlots, this));
			registerTest("RangeValidation", "Single ranges are validated and clamped, everything else is unsatisfiable", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_StreamDownload::test_RangeValidation, this));
			registerTest("RangeLargeOffsets", "Ranges beyond 4 GiB keep their offsets", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_StreamDownload::test_RangeLargeOffsets, this));
			registerTest("IfRangeAndHTTPDate", "If-Range matching and HTTP date formatting", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_StreamDownload::test_IfRangeAndHTTPDate, this));
		}

		void initializeTests() override {
		}

	private:

		void test_RegistryContiguousDelivery()
		{
			AMC::CStreamDownloadRegistry registry(4);
			std::string sUUID = AMCCommon::CUtils::createUUID();
			const uint64_t nSize = 10000;

			registry.prepareDownload(sUUID, nSize);
			auto progress = registry.getProgress(sUUID);
			assertTrue(progress.m_nTotalBytes == nSize, "Total bytes should be set by prepareDownload");
			assertTrue(progress.m_nDeliveredBytes == 0, "Nothing delivered yet");
			assertTrue(progress.m_nActiveConnections == 0, "No connection yet");

			registry.connectionStarted(sUUID, nSize);
			assertTrue(registry.chunkSent(sUUID, 0, 4000), "Chunk should be accepted");
			assertTrue(registry.getProgress(sUUID).m_nDeliveredBytes == 4000, "First chunk delivered");

			// A chunk after a gap does not count until the gap is filled.
			assertTrue(registry.chunkSent(sUUID, 6000, 2000), "Chunk after gap should be accepted");
			assertTrue(registry.getProgress(sUUID).m_nDeliveredBytes == 4000, "Gap keeps the delivered bytes");

			assertTrue(registry.chunkSent(sUUID, 4000, 2000), "Gap filler should be accepted");
			assertTrue(registry.getProgress(sUUID).m_nDeliveredBytes == 8000, "Filled gap merges the ranges");

			registry.connectionEnded(sUUID, false);
			progress = registry.getProgress(sUUID);
			assertTrue(progress.m_nActiveConnections == 0, "Connection ended");
			assertFalse(progress.m_bCompleted, "Not completed after an interrupted connection");

			// Resume with a range request from the gap.
			registry.connectionStarted(sUUID, nSize);
			assertTrue(registry.chunkSent(sUUID, 8000, 2000), "Resume chunk should be accepted");
			registry.connectionEnded(sUUID, true);
			progress = registry.getProgress(sUUID);
			assertTrue(progress.m_nDeliveredBytes == nSize, "All bytes delivered after resume");
			assertTrue(progress.m_bCompleted, "Completed after resume");
		}

		void test_RegistryCompletion()
		{
			AMC::CStreamDownloadRegistry registry(4);
			std::string sUUID = AMCCommon::CUtils::createUUID();
			const uint64_t nSize = 100;

			registry.prepareDownload(sUUID, nSize);
			registry.connectionStarted(sUUID, nSize);
			registry.chunkSent(sUUID, 0, 100);
			assertFalse(registry.getProgress(sUUID).m_bCompleted, "The last byte alone does not complete the download");

			registry.connectionEnded(sUUID, false);
			assertFalse(registry.getProgress(sUUID).m_bCompleted, "A failed connection end does not complete the download");

			registry.connectionStarted(sUUID, nSize);
			registry.chunkSent(sUUID, 50, 50);
			registry.connectionEnded(sUUID, true);
			assertTrue(registry.getProgress(sUUID).m_bCompleted, "Successful end with all bytes completes the download");

			// An unknown stream reports nothing.
			auto unknown = registry.getProgress(AMCCommon::CUtils::createUUID());
			assertTrue(unknown.m_nTotalBytes == 0, "Unknown stream has no size");
			assertFalse(unknown.m_bCompleted, "Unknown stream is not completed");
		}

		void test_RegistryCancel()
		{
			AMC::CStreamDownloadRegistry registry(4);
			std::string sUUID = AMCCommon::CUtils::createUUID();
			const uint64_t nSize = 1000;

			registry.prepareDownload(sUUID, nSize);
			registry.connectionStarted(sUUID, nSize);
			assertTrue(registry.chunkSent(sUUID, 0, 100), "Chunk before cancel should be accepted");

			assertTrue(registry.cancel(sUUID), "Cancel of a running download succeeds");
			assertTrue(registry.isCancelled(sUUID), "Stream is cancelled");
			assertFalse(registry.chunkSent(sUUID, 100, 100), "Chunk after cancel is refused");
			assertTrue(registry.getProgress(sUUID).m_bCancelled, "Progress reports the cancel");

			registry.connectionEnded(sUUID, true);
			assertFalse(registry.getProgress(sUUID).m_bCompleted, "A cancelled download never completes");

			// A new delivery of the same stream starts clean.
			registry.prepareDownload(sUUID, nSize);
			assertFalse(registry.isCancelled(sUUID), "prepareDownload clears the cancel");
			assertTrue(registry.getProgress(sUUID).m_nDeliveredBytes == 0, "prepareDownload clears the delivered bytes");

			registry.connectionStarted(sUUID, nSize);
			registry.chunkSent(sUUID, 0, 1000);
			registry.connectionEnded(sUUID, true);
			assertTrue(registry.getProgress(sUUID).m_bCompleted, "Completed");
			assertFalse(registry.cancel(sUUID), "Cancel after completion is refused");
			assertFalse(registry.isCancelled(sUUID), "Completed download stays uncancelled");
		}

		void test_RegistryConnectionCounting()
		{
			AMC::CStreamDownloadRegistry registry(4);
			std::string sUUID = AMCCommon::CUtils::createUUID();

			registry.prepareDownload(sUUID, 10);
			// HEAD requests and failures before the first chunk report an end without a start.
			registry.connectionEnded(sUUID, false);
			registry.connectionEnded(sUUID, true);
			assertTrue(registry.getProgress(sUUID).m_nActiveConnections == 0, "Ends without starts do not go negative");
			assertFalse(registry.getProgress(sUUID).m_bCompleted, "An end without data does not complete");

			registry.connectionStarted(sUUID, 10);
			registry.connectionStarted(sUUID, 10);
			assertTrue(registry.getProgress(sUUID).m_nActiveConnections == 2, "Two connections");
			assertTrue(registry.hasActiveConnections(sUUID), "Has active connections");
			registry.connectionEnded(sUUID, false);
			assertTrue(registry.getProgress(sUUID).m_nActiveConnections == 1, "One connection left");
			registry.connectionEnded(sUUID, false);
			registry.connectionEnded(sUUID, false);
			assertTrue(registry.getProgress(sUUID).m_nActiveConnections == 0, "Double end does not go negative");
			assertFalse(registry.hasActiveConnections(sUUID), "No active connections");
		}

		void test_RegistrySlots()
		{
			AMC::CStreamDownloadRegistry registry(2);
			assertTrue(registry.getMaxDownloads() == 2, "Max downloads");
			assertTrue(registry.reserveSlot(), "First slot");
			assertTrue(registry.reserveSlot(), "Second slot");
			assertFalse(registry.reserveSlot(), "Third slot is refused");
			assertTrue(registry.getActiveDownloadCount() == 2, "Two active downloads");
			registry.releaseSlot();
			assertTrue(registry.reserveSlot(), "Slot available again after release");
			registry.releaseSlot();
			registry.releaseSlot();
			registry.releaseSlot();
			assertTrue(registry.getActiveDownloadCount() == 0, "Release never goes negative");
		}

		void test_RangeValidation()
		{
			typedef AMCCommon::CHTTPRange::Range Range;
			const uint64_t nSize = 1000;

			std::vector<Range> ranges;
			ranges.push_back(Range(0, 99));
			assertTrue(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "0-99 is satisfiable");
			assertTrue(ranges[0].second == 99, "0-99 keeps its end");

			ranges.assign(1, Range(500, -1));
			assertTrue(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "Open end is satisfiable");

			ranges.assign(1, Range(-1, -1));
			assertTrue(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "Full range is satisfiable");

			ranges.assign(1, Range(-1, 100));
			assertTrue(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "Suffix range is satisfiable");

			ranges.assign(1, Range(-1, 0));
			assertFalse(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "Suffix range of zero length is unsatisfiable");

			ranges.assign(1, Range(0, 999999));
			assertTrue(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "End beyond the body is satisfiable");
			assertTrue(ranges[0].second == 999, "End beyond the body is clamped to the last byte");

			ranges.assign(1, Range(1000, -1));
			assertFalse(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "Start at the end is unsatisfiable");

			ranges.assign(1, Range(5000, 5010));
			assertFalse(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "Start beyond the end is unsatisfiable");

			ranges.assign(1, Range(100, 50));
			assertFalse(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "End before start is unsatisfiable");

			ranges.clear();
			ranges.push_back(Range(0, 99));
			ranges.push_back(Range(200, 299));
			assertFalse(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "Several ranges are unsatisfiable");

			ranges.assign(1, Range(0, 0));
			assertFalse(AMCCommon::CHTTPRange::normalizeRanges(ranges, 0), "Any range of an empty body is unsatisfiable");

			ranges.clear();
			assertFalse(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "No range is not a range request");
		}

		void test_RangeLargeOffsets()
		{
			typedef AMCCommon::CHTTPRange::Range Range;
			const uint64_t nSize = 10000000000ULL;

			std::vector<Range> ranges;
			ranges.push_back(Range(5000000000LL, 5000000099LL));
			assertTrue(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "Range beyond 4 GiB is satisfiable");
			assertTrue(ranges[0].first == 5000000000LL, "Start beyond 4 GiB is kept");
			assertTrue(ranges[0].second == 5000000099LL, "End beyond 4 GiB is kept");

			ranges.assign(1, Range(9999999999LL, 20000000000LL));
			assertTrue(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "Last byte is satisfiable");
			assertTrue(ranges[0].second == 9999999999LL, "End is clamped to the last byte");

			ranges.assign(1, Range(10000000000LL, -1));
			assertFalse(AMCCommon::CHTTPRange::normalizeRanges(ranges, nSize), "Start at the size is unsatisfiable");
		}

		void test_IfRangeAndHTTPDate()
		{
			std::string sETag = "abcdef0123456789";
			std::string sDate = "Sun, 06 Nov 1994 08:49:37 GMT";
			assertTrue(AMCCommon::CHTTPRange::ifRangeMatches("", sETag, sDate), "Empty If-Range matches");
			assertTrue(AMCCommon::CHTTPRange::ifRangeMatches("\"abcdef0123456789\"", sETag, sDate), "Quoted entity tag matches");
			assertTrue(AMCCommon::CHTTPRange::ifRangeMatches(sDate, sETag, sDate), "Date matches");
			assertFalse(AMCCommon::CHTTPRange::ifRangeMatches("\"other\"", sETag, sDate), "Other entity tag does not match");
			assertFalse(AMCCommon::CHTTPRange::ifRangeMatches("Mon, 07 Nov 1994 08:49:37 GMT", sETag, sDate), "Other date does not match");

			// 784111777 seconds since 1970 is Sun, 06 Nov 1994 08:49:37 GMT (the RFC 7231 example).
			std::string sFormatted = AMCCommon::CHTTPRange::formatHTTPDate(784111777ULL * 1000000ULL);
			assertTrue(sFormatted == sDate, "HTTP date formatting: " + sFormatted);
		}

	};

}

#endif //__AMCTEST_UNITTEST_STREAMDOWNLOAD
