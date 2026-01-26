/*++

Copyright (C) 2025 Autodesk Inc.

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

#ifndef __AMCTEST_UNITTEST_TELEMETRY
#define __AMCTEST_UNITTEST_TELEMETRY

#include "amc_unittests.hpp"
#include "amc_telemetry.hpp"
#include "amc_unittests_libmcdata.hpp"
#include "common_utils.hpp"
#include "common_chrono.hpp"
#include "libmcdata_dynamic.hpp"

#include <cstdint>
#include <string>

namespace AMCUnitTest {

	class CUnitTestGroup_Telemetry : public CUnitTestGroup {
	public:
		std::string getTestGroupName() override {
			return "Telemetry";
		}

		void registerTests() override {
			registerTest("ChannelBasics", "Create channel and validate properties", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testChannelBasics, this));
			registerTest("ChannelFind", "Find channel by identifier/UUID", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testChannelFind, this));
			registerTest("DuplicateChannelThrows", "Duplicate channel identifier fails", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testDuplicateChannelThrows, this));
			registerTest("InvalidIdentifierThrows", "Invalid channel identifier fails", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testInvalidIdentifierThrows, this));
			registerTest("InstantMarkerBasics", "Instant marker creation updates stats", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testInstantMarkerBasics, this));
			registerTest("IntervalMarkerFinish", "Interval marker finish updates duration", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testIntervalMarkerFinish, this));
			registerTest("IntervalMarkerFinishTwiceThrows", "Finishing marker twice fails", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testIntervalMarkerFinishTwiceThrows, this));
			registerTest("MarkerDurationBeforeFinishThrows", "Duration on unfinished marker fails", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testMarkerDurationBeforeFinishThrows, this));
			registerTest("ScopeAutoFinish", "Telemetry scope finishes marker on destruction", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testScopeAutoFinish, this));
			registerTest("TypeMappingRoundTrip", "Telemetry channel type mapping round-trip", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testTypeMappingRoundTrip, this));
			registerTest("TypeMappingInvalidThrows", "Telemetry channel type mapping rejects invalid strings", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testTypeMappingInvalidThrows, this));
			registerTest("ChunkArchivePath", "Chunk read-only and archive path works", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Telemetry::testChunkArchivePath, this));
		}

		void initializeTests() override {
		}

	private:

		typedef struct _sTelemetryFixture {
			LibMCData::PWrapper m_pDataWrapper;
			LibMCData::PDataModel m_pDataModel;
			LibMCData::PTelemetrySession m_pTelemetrySession;
			AMCCommon::PChrono m_pChrono;
			AMC::PTelemetryWriter m_pWriter;
			AMC::PTelemetryHandler m_pHandler;
		} sTelemetryFixture;

		sTelemetryFixture createFixture(const std::string& sSuffix)
		{
			sTelemetryFixture fixture;

			std::string sRootPath = "Output/temp";
			if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(sRootPath))
				AMCCommon::CUtils::createDirectoryOnDisk(sRootPath);

			std::string sBasePath = sRootPath + "/telemetry_" + sSuffix;
			if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(sBasePath))
				AMCCommon::CUtils::createDirectoryOnDisk(sBasePath);

			std::string sDatabaseFile = sBasePath + "/telemetry.db";

			fixture.m_pDataWrapper = AMCUnitTest::loadLibMCDataInProcess();
			fixture.m_pDataModel = fixture.m_pDataWrapper->CreateDataModelInstance();
			fixture.m_pDataModel->InitialiseDatabase(sBasePath, LibMCData::eDataBaseType::SqLite, sDatabaseFile);
			fixture.m_pTelemetrySession = fixture.m_pDataModel->CreateTelemetrySession();

			fixture.m_pChrono = std::make_shared<AMCCommon::CChrono>();
			fixture.m_pWriter = std::make_shared<AMC::CTelemetryWriter>(fixture.m_pTelemetrySession, fixture.m_pChrono);
			fixture.m_pHandler = std::make_shared<AMC::CTelemetryHandler>(fixture.m_pWriter);

			return fixture;
		}

		AMC::PTelemetryChannel createChannel(sTelemetryFixture& fixture, const std::string& sIdentifier)
		{
			return fixture.m_pHandler->registerChannel(sIdentifier, "channel description", LibMCData::eTelemetryChannelType::CustomMarker);
		}

		void testChannelBasics()
		{
			auto fixture = createFixture("basics");
			auto pChannel = createChannel(fixture, "channel1");

			assertFalse(pChannel->getChannelUUID().empty());
			assertTrue(pChannel->getChannelIndex() == 1);
			assertTrue(pChannel->getChannelIdentifier() == "channel1");
			assertTrue(pChannel->getChannelDescription() == "channel description");
			assertTrue(pChannel->getChannelType() == LibMCData::eTelemetryChannelType::CustomMarker);
		}

		void testChannelFind()
		{
			auto fixture = createFixture("find");
			auto pChannel = createChannel(fixture, "channel2");

			auto pFoundByIdentifier = fixture.m_pHandler->findChannelByIdentifier("channel2", true);
			assertTrue(pFoundByIdentifier.get() == pChannel.get());

			auto pFoundByUUID = fixture.m_pHandler->findChannelByUUID(pChannel->getChannelUUID());
			assertTrue(pFoundByUUID.get() == pChannel.get());

			auto pMissing = fixture.m_pHandler->findChannelByIdentifier("missing", false);
			assertTrue(pMissing.get() == nullptr);
		}

		void testDuplicateChannelThrows()
		{
			auto fixture = createFixture("duplicate");
			createChannel(fixture, "channel3");

			bool thrown = false;
			try {
				createChannel(fixture, "channel3");
			}
			catch (...) {
				thrown = true;
			}
			assertTrue(thrown, "Expected duplicate channel identifier to throw");
		}

		void testInvalidIdentifierThrows()
		{
			auto fixture = createFixture("invalid");

			bool thrown = false;
			try {
				createChannel(fixture, "invalid id");
			}
			catch (...) {
				thrown = true;
			}
			assertTrue(thrown, "Expected invalid channel identifier to throw");
		}

		void testInstantMarkerBasics()
		{
			auto fixture = createFixture("instant");
			auto pChannel = createChannel(fixture, "channel4");

			uint64_t markerID = pChannel->createInstantMarker(123);
			assertTrue(markerID > 0);

			uint64_t totalMarkers = 0;
			uint64_t maxDuration = 0;
			pChannel->getStatistics(totalMarkers, maxDuration);
			assertTrue(totalMarkers == 1);
			assertTrue(maxDuration == 0);
		}

		void testIntervalMarkerFinish()
		{
			auto fixture = createFixture("interval");
			auto pChannel = createChannel(fixture, "channel5");

			auto pMarker = pChannel->startIntervalMarker(17);
			AMCCommon::CChrono::sleepMicroseconds(500);
			pMarker->finishMarker();

			assertTrue(pMarker->isFinished());
			assertTrue(pMarker->getDuration() > 0);
			assertTrue(pChannel->getMaxDurationInMicroseconds() == pMarker->getDuration());
		}

		void testIntervalMarkerFinishTwiceThrows()
		{
			auto fixture = createFixture("finish_twice");
			auto pChannel = createChannel(fixture, "channel6");

			auto pMarker = pChannel->startIntervalMarker(22);
			pMarker->finishMarker();

			bool thrown = false;
			try {
				pMarker->finishMarker();
			}
			catch (...) {
				thrown = true;
			}
			assertTrue(thrown, "Expected second finish to throw");
		}

		void testMarkerDurationBeforeFinishThrows()
		{
			auto fixture = createFixture("duration_unfinished");
			auto pChannel = createChannel(fixture, "channel7");

			auto pMarker = pChannel->startIntervalMarker(0);
			bool thrown = false;
			try {
				pMarker->getDuration();
			}
			catch (...) {
				thrown = true;
			}
			assertTrue(thrown, "Expected duration query before finish to throw");
		}

		void testScopeAutoFinish()
		{
			auto fixture = createFixture("scope");
			auto pChannel = createChannel(fixture, "channel8");

			uint64_t markerID = 0;
			{
				auto scope = pChannel->startIntervalScope(99);
				markerID = scope.getMarkerID();
				assertTrue(markerID > 0);
			}

			assertTrue(pChannel->getTotalMarkersCreated() == 1);
			assertTrue(pChannel->getMaxDurationInMicroseconds() > 0);
		}

		void testTypeMappingRoundTrip()
		{
			assertTrue(AMC::CTelemetryChannel::mapChannelTypeStringToDataChannelType(
				AMC::CTelemetryChannel::mapDataChannelTypeToTypeString(LibMCData::eTelemetryChannelType::CustomMarker))
				== LibMCData::eTelemetryChannelType::CustomMarker);

			assertTrue(AMC::CTelemetryChannel::mapChannelTypeStringToDataChannelType(
				AMC::CTelemetryChannel::mapDataChannelTypeToTypeString(LibMCData::eTelemetryChannelType::RemoteQuery))
				== LibMCData::eTelemetryChannelType::RemoteQuery);
		}

		void testTypeMappingInvalidThrows()
		{
			bool thrown = false;
			try {
				AMC::CTelemetryChannel::mapChannelTypeStringToDataChannelType("invalid");
			}
			catch (...) {
				thrown = true;
			}
			assertTrue(thrown, "Expected invalid type string to throw");
		}

		void testChunkArchivePath()
		{
			auto fixture = createFixture("archive");
			auto pChannel = createChannel(fixture, "channel9");
			pChannel->createInstantMarker(1);

			const uint64_t nChunkInterval = 60000000ULL;
			auto pChunk1 = fixture.m_pWriter->getOrCreateChunkByTimestamp(1);
			auto pChunk3 = fixture.m_pWriter->getOrCreateChunkByTimestamp(nChunkInterval * 2);
			static_cast<void>(pChunk3);
			assertTrue(pChunk1->isReadOnly());

			fixture.m_pHandler->archiveOldChunksToDB();
		}
	};

}

#endif // __AMCTEST_UNITTEST_TELEMETRY
