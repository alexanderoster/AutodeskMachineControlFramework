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


Abstract: This is the class declaration of CJSONObject

*/


#ifndef __LIBMCENV_JSONOBJECT
#define __LIBMCENV_JSONOBJECT

#include "libmcenv_interfaces.hpp"

// Parent classes
#include "libmcenv_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

#include "RapidJSON/rapidjson.h"
#include "RapidJSON/document.h"


namespace LibMCEnv {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CJSONObject 
**************************************************************************************************************************/

class CJSONObject : public virtual IJSONObject, public virtual CBase {
private:
	std::shared_ptr<rapidjson::Document> m_pDocument;
	rapidjson::GenericValue<rapidjson::UTF8<>>* m_pInstance;

public:

	CJSONObject();

	CJSONObject(std::shared_ptr<rapidjson::Document> pDocument, rapidjson::Value * pReferencedObject);

	CJSONObject(const std::string & sJSONString);

	CJSONObject(const uint8_t * pData, uint64_t nDataSize);

	virtual ~CJSONObject();

	bool HasMember(const std::string & sName) override;

	LibMCEnv_uint64 GetMemberCount() override;

	std::string GetMemberName(const LibMCEnv_uint64 nIndex) override;

	LibMCEnv::eJSONObjectType GetMemberType(const std::string & sName) override;

	std::string GetValue(const std::string & sName) override;

	std::string GetUUIDValue(const std::string& sName) override;

	LibMCEnv_int64 GetIntegerValue(const std::string & sName) override;

	LibMCEnv_double GetDoubleValue(const std::string & sName) override;

	bool GetBoolValue(const std::string & sName) override;

	IJSONObject * GetObjectValue(const std::string & sName) override;

	IJSONArray * GetArrayValue(const std::string & sName) override;

	void RemoveMember(const std::string & sName) override;

	void AddValue(const std::string & sName, const std::string & sValue) override;

	void AddIntegerValue(const std::string& sName, const LibMCEnv_int64 nValue) override;

	void AddDoubleValue(const std::string& sName, const LibMCEnv_double dValue) override;

	void AddBoolValue(const std::string& sName, const bool bValue) override;

	IJSONObject * AddObjectValue(const std::string & sName) override;

	IJSONArray * AddArrayValue(const std::string & sName) override;

	std::string SerializeToString() override;
};

} // namespace Impl
} // namespace LibMCEnv

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCENV_JSONOBJECT
