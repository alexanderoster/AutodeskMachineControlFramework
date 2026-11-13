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


#ifndef __AMC_API_HANDLER_USERS
#define __AMC_API_HANDLER_USERS

#include "amc_api_handler.hpp"
#include "amc_api_response.hpp"
#include "amc_api_sessionhandler.hpp"
#include "amc_api_usermanagementservice.hpp"

#include "amc_systemstate.hpp"

#include <vector>
#include <string>

namespace AMC {

	class CAPIJSONRequest;

	enum class APIHandler_UsersType : uint32_t {
		utUnknown = 0,
		utMeta = 1,
		utList = 2,
		utGetUser = 3,
		utGetOwnUser = 4,
		utGetSessions = 5,
		utCreateUser = 6,
		utUpdateUser = 7,
		utResetPassword = 8,
		utDeleteUser = 9,
		utRevokeSessions = 10,
		utUpdateOwnProfile = 11,
		utPasswordChallenge = 12,
		utChangeOwnPassword = 13
	};

	class CAPIHandler_Users : public CAPIHandler {
	private:

		PSystemState m_pSystemState;
		PUserManagementService m_pService;

		APIHandler_UsersType parseRequest(const std::string& sURI, const eAPIRequestType requestType, std::string& sUserUUID);

		void writeUser(CJSONWriterObject& userObject, const CUserManagementUser& user, PUserInformation pCaller, const std::vector<CAPIUserSessionInfo>& sessions);

		void handleMetaRequest(CJSONWriter& writer, PAPIAuth pAuth);
		void handleListRequest(CJSONWriter& writer, PAPIAuth pAuth);
		void handleGetUserRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& sUserUUID);
		void handleGetSessionsRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& sUserUUID);
		void handleCreateUserRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth);
		void handleUpdateUserRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth, const std::string& sUserUUID);
		void handleResetPasswordRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth, const std::string& sUserUUID);
		void handleDeleteUserRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& sUserUUID);
		void handleRevokeSessionsRequest(CJSONWriter& writer, PAPIAuth pAuth, const std::string& sUserUUID);
		void handleUpdateOwnProfileRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth);
		void handlePasswordChallengeRequest(CJSONWriter& writer, PAPIAuth pAuth);
		void handleChangeOwnPasswordRequest(CJSONWriter& writer, const CAPIJSONRequest& request, PAPIAuth pAuth);

	public:

		CAPIHandler_Users(PSystemState pSystemState, PUserManagementService pService);

		virtual ~CAPIHandler_Users();

		virtual std::string getBaseURI() override;

		virtual PAPIResponse handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields& pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth) override;

		virtual bool expectsRawBody(const std::string& sURI, const eAPIRequestType requestType) override;

	};

}


#endif //__AMC_API_HANDLER_USERS
