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

#ifndef __AMCTEST_UNITTEST_SIGNALSLOT
#define __AMCTEST_UNITTEST_SIGNALSLOT

#include "amc_unittests.hpp"

#define _STATESIGNAL_HEADERPROTECTION
#include "amc_statesignal.hpp"
#undef _STATESIGNAL_HEADERPROTECTION

#include <thread>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <vector>
#include <list>
#include <mutex>
#include "common_utils.hpp"


namespace AMCUnitTest {




class CUnitTestGroup_SignalSlot : public CUnitTestGroup {
public:
    CUnitTestGroup_SignalSlot() = default;
    virtual ~CUnitTestGroup_SignalSlot() = default;

    std::string getTestGroupName() override {
        return "SignalSlot";
    }

    void initializeTests() override {
        // Nothing to initialize
    }

    void registerTests() override {
        registerTest("CreateSignalSlotBasic", "Creates a signal slot with parameters", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_SignalSlot::test_CreateSignalSlotBasic, this));
        registerTest("AddSignalToQueue", "Adds a signal to the queue and verifies availability", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_SignalSlot::test_AddSignalToQueue, this));
        registerTest("SignalPhaseTransition", "Tests InQueue → InProcess → Handled", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_SignalSlot::test_SignalPhaseTransition, this));
        registerTest("SignalFailureTransition", "Tests InQueue → Failed", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_SignalSlot::test_SignalFailureTransition, this));
        registerTest("QueueOverflow", "Tests rejection of signal if queue is full", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_SignalSlot::test_QueueOverflow, this));
        registerTest("PeekQueue", "Tests peeking the front UUID in the queue", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_SignalSlot::test_PeekQueue, this));
        registerTest("ParameterResultAccess", "Tests getting parameter and result JSON", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_SignalSlot::test_ParameterResultAccess, this));
        registerTest("ClearQueueWorks", "Clears the queue and marks signals as cleared", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_SignalSlot::test_ClearQueueWorks, this));
        registerTest("TimeoutAndOverflowTest", "Simulates queue overflow and timeout scenarios", eUnitTestCategory::utOptionalPass, std::bind(&CUnitTestGroup_SignalSlot::test_TimeoutAndOverflowTest, this));
    }

private:

    void test_CreateSignalSlotBasic() {
        std::list<AMC::CStateSignalParameter> params;
        std::list<AMC::CStateSignalParameter> results;
        params.emplace_back("p1", "string", true);
        results.emplace_back("r1", "int", true);

        AMC::CStateSignalSlot slot("instance", "signal", params, results, 500, 10);

        assertTrue(slot.getNameInternal() == "signal");
        assertTrue(slot.getInstanceNameInternal() == "instance");
        assertTrue(slot.getDefaultReactionTimeoutInternal() == 500);
        assertTrue(slot.getTotalSignalQueueSizeInternal() == 10);
    }

    void test_AddSignalToQueue() {
        AMC::CStateSignalSlot slot("instance", "signal", {}, {}, 1000, 5);
        std::string uuid = "11111111-2222-3333-4444-555555555555";

        bool added = slot.addNewInQueueSignalInternal(uuid, "{\"param\":\"value\"}", 500);
        assertTrue(added);

        assertIntegerRange(slot.getAvailableSignalQueueEntriesInternal(), 0, 4);
        assertTrue(slot.getSignalPhaseInternal(uuid) == AMC::eAMCSignalPhase::InQueue);
    }

    void test_SignalPhaseTransition() {
        AMC::CStateSignalSlot slot("instance", "signal", {}, {}, 1000, 3);
        std::string uuid = "77777777-8888-9999-aaaa-bbbbbbbbbbbb";

        assertTrue(slot.addNewInQueueSignalInternal(uuid, "{}", 500));
        assertTrue(slot.changeSignalPhaseToInProcessInternal(uuid));
        assertTrue(slot.getSignalPhaseInternal(uuid) == AMC::eAMCSignalPhase::InProcess);

        assertTrue(slot.changeSignalPhaseToHandledInternal(uuid, "{\"result\":true}"));
        assertTrue(slot.getSignalPhaseInternal(uuid) == AMC::eAMCSignalPhase::Handled);

        assertTrue(slot.getResultDataJSONInternal(uuid) == "{\"result\":true}");
    }

    void test_SignalFailureTransition() {
        AMC::CStateSignalSlot slot("instance", "signal", {}, {}, 1000, 2);
        std::string uuid = "deadbeef-dead-beef-dead-beefdeadbeef";

        assertTrue(slot.addNewInQueueSignalInternal(uuid, "{}", 500));
        assertTrue(slot.changeSignalPhaseToInFailedInternal(uuid, "{\"ok\":false}", "error"));
        assertTrue(slot.getSignalPhaseInternal(uuid) == AMC::eAMCSignalPhase::Failed);
    }

    void test_QueueOverflow() {
        AMC::CStateSignalSlot slot("instance", "signal", {}, {}, 1000, 1);
        assertTrue(slot.addNewInQueueSignalInternal("00000001-0000-0000-0000-000000000001", "{}", 500));
        assertFalse(slot.addNewInQueueSignalInternal("00000002-0000-0000-0000-000000000002", "{}", 500)); // queue full
    }

    void test_PeekQueue() {
        AMC::CStateSignalSlot slot("instance", "signal", {}, {}, 1000, 3);
        slot.addNewInQueueSignalInternal("aaaaaaaa-0000-0000-0000-000000000001", "{\"a\":1}", 400);
        slot.addNewInQueueSignalInternal("bbbbbbbb-0000-0000-0000-000000000002", "{\"b\":2}", 400);
        assertTrue(slot.peekMessageFromQueueInternal() == AMCCommon::CUtils::normalizeUUIDString("aaaaaaaa-0000-0000-0000-000000000001"));
    }

    void test_ParameterResultAccess() {
        AMC::CStateSignalSlot slot("instance", "signal", {}, {}, 1000, 2);
        std::string uuid = "e5e57000-0000-0000-0000-000000000001";
        slot.addNewInQueueSignalInternal(uuid, "{\"param\":\"abc\"}", 1000);
        assertTrue(slot.getParameterDataJSONInternal(uuid) == "{\"param\":\"abc\"}");
        slot.changeSignalPhaseToHandledInternal(uuid, "{\"result\":123}");
        assertTrue(slot.getResultDataJSONInternal(uuid) == "{\"result\":123}");
    }

    void test_ClearQueueWorks() {
        AMC::CStateSignalSlot slot("instance", "signal", {}, {}, 1000, 2);
        slot.addNewInQueueSignalInternal("11110001-0000-0000-0000-000000000001", "{}", 500);
        slot.addNewInQueueSignalInternal("22220002-0000-0000-0000-000000000002", "{}", 500);

        std::vector<std::string> clearedUUIDs;
        slot.clearQueueInternal(clearedUUIDs);
        assertTrue(slot.getAvailableSignalQueueEntriesInternal() == 2);
    }


    void test_TimeoutAndOverflowTest() {
        const int capacity = 10;
        const int total = 15;
        AMC::CStateSignalSlot slot("overflowInstance", "overflowSignal", {}, {}, 50, capacity);

        std::vector<std::string> accepted, rejected;

        for (int i = 0; i < total; ++i) {
            std::stringstream ss;
            ss << "f10" << std::setfill('0') << std::setw(5) << i << "-0000-0000-0000-000000000000";
            std::string uuid = ss.str();

            bool ok = slot.addNewInQueueSignalInternal(uuid, "{}", 50);
            if (ok) accepted.push_back(uuid);
            else rejected.push_back(uuid);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        int timedOut = 0;
        for (const auto& uuid : accepted) {
            if (slot.getSignalPhaseInternal(uuid) == AMC::eAMCSignalPhase::InQueue) {
                timedOut++;
            }
        }

        assertTrue(accepted.size() == capacity);
        assertTrue(rejected.size() == total - capacity);
        assertTrue(timedOut > 0);
    }
};


}


#endif // __AMCTEST_UNITTEST_SIGNALSLOT