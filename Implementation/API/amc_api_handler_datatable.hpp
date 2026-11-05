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

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS' AND
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


#ifndef __AMC_API_HANDLER_DATATABLE
#define __AMC_API_HANDLER_DATATABLE

#include "amc_api_handler.hpp"
#include "amc_api_response.hpp"
#include "amc_systemstate.hpp"

#include <vector>
#include <string>

namespace AMC {

	/*************************************************************************************************************************
	 Serves streaming data captures to the frontend.

	   GET api/datatable/execution/{executionuuid}      captures of a build execution
	   GET api/datatable/{captureuuid}/info             schema, row counts, segments and value ranges
	   GET api/datatable/{captureuuid}                  data, selected by query parameters:
	         format=json|csv|bin|bin32                   default json; bin is float64, bin32 is float32
	         columns=a,b                                 default all columns
	         fromrow=N  rowcount=N                       default everything that is still stored
	         downsample=none|stride|minmax|average       default none
	         targetpoints=N                              default 1000; count is accepted as an alias
	   GET api/datatable/{captureuuid}/segment/{index}  download ticket for one segment stream

	 Responses are buffered as a whole, so the data endpoint enforces a size cap and rejects larger requests
	 with an error that names the parameters to reduce it. A full capture is exported segment by segment
	 through download tickets. The handler requires an authorized session, like the build handler.
	**************************************************************************************************************************/

	class CAPIHandler_DataTable : public CAPIHandler {
	private:
		PSystemState m_pSystemState;

		PAPIResponse handleExecutionListRequest(const std::string& sExecutionUUID);
		PAPIResponse handleInfoRequest(const std::string& sCaptureUUID);
		PAPIResponse handleDataRequest(const std::string& sCaptureUUID, CAPIFormFields& pFormFields);
		PAPIResponse handleSegmentTicketRequest(const std::string& sCaptureUUID, const std::string& sSegmentIndex, PAPIAuth pAuth);

	public:

		CAPIHandler_DataTable(PSystemState pSystemState);

		virtual ~CAPIHandler_DataTable();

		virtual std::string getBaseURI() override;

		virtual PAPIResponse handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields& pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth) override;
	};

}

#endif //__AMC_API_HANDLER_DATATABLE
