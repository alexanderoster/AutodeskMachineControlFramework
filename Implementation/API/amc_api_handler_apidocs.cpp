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


#include "amc_api_handler_apidocs.hpp"

#include "libmc_interfaceexception.hpp"

#include <vector>
#include <memory>
#include <string>
#include <iostream>

#include <cstdint>
#include <string>
#include <vector>

#include "Libraries/libzip/zip.h"
#include "pugixml.hpp"

#include "Libraries/RapidJSON/document.h"
#include "Libraries/RapidJSON/stringbuffer.h"
#include "Libraries/RapidJSON/writer.h"


using namespace AMC;



CAPIHandler_APIDocs::CAPIHandler_APIDocs(const std::string& sClientHash)
	: CAPIHandler (sClientHash)
{
	
}

CAPIHandler_APIDocs::~CAPIHandler_APIDocs()
{
	
}
				
std::string CAPIHandler_APIDocs::getBaseURI ()
{
	return "docs";
}
		
void CAPIHandler_APIDocs::checkAuthorizationMode(const std::string& sURI, const eAPIRequestType requestType, bool& bNeedsToBeAuthorized, bool& bCreateNewSession)
{
	bNeedsToBeAuthorized = false;
	bCreateNewSession = false;
}


PAPIResponse CAPIHandler_APIDocs::handleRequest(const std::string& sURI, const eAPIRequestType requestType, CAPIFormFields & pFormFields, const uint8_t* pBodyData, const size_t nBodyDataSize, PAPIAuth pAuth)
{
	if (sURI.length() < 4)
		return nullptr;

	std::string sSubURI = AMCCommon::CUtils::removeLeadingPathDelimiter(sURI.substr(4));

	if (requestType == eAPIRequestType::rtGet) {
		auto iIterator = m_FilesToServe.find(AMCCommon::CUtils::toLowerString (sSubURI));
		if (iIterator != m_FilesToServe.end())
			return iIterator->second;
	}

	return nullptr;
}

void CAPIHandler_APIDocs::LoadAPIDocsPackage(PResourcePackage pResourcePackage)
{
	if (pResourcePackage.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

	size_t nCount = pResourcePackage->getEntryCount();
	for (size_t nIndex = 0; nIndex < nCount; nIndex++) {
		auto pEntry = pResourcePackage->getEntry(nIndex);

		std::string sNameOriginal = pEntry->getName();
		std::string sNameLowerCase = AMCCommon::CUtils::toLowerString(sNameOriginal);

		auto apiResponse = std::make_shared<CAPIFixedBufferResponse>(pEntry->getContentType());

		if (sNameLowerCase == "amcf_openapi.json") {

			std::string sOpenAPIContent = pResourcePackage->readEntryUTF8String(sNameOriginal);
			patchAPIJSON (sOpenAPIContent, apiResponse->getBuffer());
		
		}
		else {
			pResourcePackage->readEntry(sNameOriginal, apiResponse->getBuffer());
		}

		m_FilesToServe.insert(std::make_pair(sNameLowerCase, apiResponse));

	}


}

void CAPIHandler_APIDocs::setCustomDocumentationJSON(const std::string & sCustomDocumentationJSON)
{
	m_sCustomDocumentationJSON = sCustomDocumentationJSON;
}



    // Parse helper
    static bool parseJson(const std::string& s, rapidjson::Document& doc) {
        doc.Parse(s.c_str());
        return !doc.HasParseError() && doc.IsObject();
    }

    // Ensure `root[name]` exists and is an object; return it.
    static rapidjson::Value& getOrCreateObject(rapidjson::Value& root,
        const char* name,
        rapidjson::Document::AllocatorType& alloc) {
        auto it = root.FindMember(name);
        if (it == root.MemberEnd()) {
            // create: name + empty object
            rapidjson::Value key(name, static_cast<rapidjson::SizeType>(std::strlen(name)), alloc);
            rapidjson::Value obj(rapidjson::kObjectType);
            root.AddMember(key, obj, alloc);
            it = root.FindMember(name); // re-find to get a reference
        }
        else if (!it->value.IsObject()) {
            it->value.SetObject();
        }
        return it->value;
    }

    // Deep-merge source object into destination object.
    // - When both values are objects -> recurse
    // - Otherwise -> overwrite destination with source
    static void mergeObjectMembers(rapidjson::Value& dstObj,
        const rapidjson::Value& srcObj,
        rapidjson::Document::AllocatorType& alloc) {
        for (auto m = srcObj.MemberBegin(); m != srcObj.MemberEnd(); ++m) {
            // Copy the member name into dst's allocator
            rapidjson::Value key(m->name.GetString(), m->name.GetStringLength(), alloc);

            auto it = dstObj.FindMember(m->name);
            if (it == dstObj.MemberEnd()) {
                // New member -> deep copy value
                rapidjson::Value val;
                val.CopyFrom(m->value, alloc);
                dstObj.AddMember(key, val, alloc);
            }
            else {
                // Existing key
                if (it->value.IsObject() && m->value.IsObject()) {
                    mergeObjectMembers(it->value, m->value, alloc);
                }
                else {
                    it->value.CopyFrom(m->value, alloc); // overwrite
                }
            }
        }
    }


void CAPIHandler_APIDocs::patchAPIJSON(const std::string& sOpenAPIJSON,
    std::vector<uint8_t>& apiBuffer)
{
    // Fast path: nothing to patch
    if (m_sCustomDocumentationJSON.empty()) {
        apiBuffer.assign(sOpenAPIJSON.begin(), sOpenAPIJSON.end());
        return;
    }

    rapidjson::Document base;
    rapidjson::Document custom;
    if (!parseJson(sOpenAPIJSON, base)) 
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDDEFAULTOPENAPIJSON);
    
    if (!parseJson(m_sCustomDocumentationJSON, custom)) 
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCUSTOMOPENAPIJSON);
    

    // Start merged := base
    rapidjson::Document merged(rapidjson::kObjectType);
    merged.CopyFrom(base, merged.GetAllocator());
    auto& alloc = merged.GetAllocator();

    // Merge "paths"
    if (custom.HasMember("paths") && custom["paths"].IsObject()) {
        rapidjson::Value& dstPaths = getOrCreateObject(merged, "paths", alloc);
        mergeObjectMembers(dstPaths, custom["paths"], alloc);
    }

    // Merge "components.schemas"
    if (custom.HasMember("components") && custom["components"].IsObject()) {
        const rapidjson::Value& srcComponents = custom["components"];
        if (srcComponents.HasMember("schemas") && srcComponents["schemas"].IsObject()) {
            rapidjson::Value& dstComponents = getOrCreateObject(merged, "components", alloc);
            rapidjson::Value& dstSchemas = getOrCreateObject(dstComponents, "schemas", alloc);
            mergeObjectMembers(dstSchemas, srcComponents["schemas"], alloc);
        }
    }

    // Serialize back to bytes
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);
    merged.Accept(writer);
    const char* jsonOut = sb.GetString();

    apiBuffer.assign(jsonOut, jsonOut + std::strlen(jsonOut));
}

		


