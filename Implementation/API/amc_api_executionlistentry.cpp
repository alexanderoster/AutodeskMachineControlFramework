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

#include "amc_api_executionlistentry.hpp"
#include "libmc_interfaceexception.hpp"
#include "common_chrono.hpp"

#include <string>

namespace AMC {

	void writeExecutionListEntryToJSON(CJSONWriterObject& entryObject, LibMCData::PBuildJobExecution pExecution, uint64_t nGlobalTimerInMicroseconds)
	{
		if (pExecution.get() == nullptr)
			throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

		LibMCData::eBuildJobExecutionStatus status = pExecution->GetLoadedStatus();

		uint64_t nStartTimeStamp = pExecution->GetStartTimeStampInMicroseconds();
		uint64_t nEndTimeStamp = 0;
		// Failed (e.g. aborted) executions also have an end time stamp.
		if ((status == LibMCData::eBuildJobExecutionStatus::Finished) || (status == LibMCData::eBuildJobExecutionStatus::Failed))
			nEndTimeStamp = pExecution->GetLoadedEndTimeStampInMicroseconds();

		int64_t nDurationInSeconds = pExecution->ComputeLoadedElapsedTimeInMicroseconds(nGlobalTimerInMicroseconds, false) / 1000000LL;

		entryObject.addString("executionuuid", pExecution->GetExecutionUUID());
		entryObject.addString("executionname", pExecution->GetJobName());
		entryObject.addString("executiondescription", pExecution->GetLoadedDescription());
		entryObject.addString("executionstarttimestamp", AMCCommon::CChrono::convertToISO8601TimeUTC(nStartTimeStamp));
		entryObject.addString("executionendtimestamp", AMCCommon::CChrono::convertToISO8601TimeUTC(nEndTimeStamp));
		entryObject.addInteger("executionduration", nDurationInSeconds);
		entryObject.addString("executionstatus", pExecution->GetLoadedStatusString());
		entryObject.addString("executionbuildstatus", pExecution->GetJobStatusString());
		entryObject.addInteger("executionlayercount", pExecution->GetJobLayerCount());

		entryObject.addString("jobuuid", pExecution->GetJobUUID());

		// A missing job row is loaded as a Deleted job without name and thumbnail.
		if (pExecution->JobHasThumbnailStream())
			entryObject.addString("executionthumbnail", pExecution->GetJobThumbnailStreamUUID());
	}

}
