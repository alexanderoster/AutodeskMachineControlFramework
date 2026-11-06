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


Abstract: This is the class declaration of COperationSequenceRevision

*/


#ifndef __LIBMCDATA_OPERATIONSEQUENCEREVISION
#define __LIBMCDATA_OPERATIONSEQUENCEREVISION

#include "libmcdata_interfaces.hpp"

// Parent classes
#include "libmcdata_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include <memory>


namespace LibMCData {
namespace Impl {

struct sOperationSequenceRevisionData {
	std::string m_sUUID;
	std::string m_sStateMachine;
	std::string m_sSequenceName;
	uint32_t m_nRevision = 0;
	uint32_t m_nOrigin = 0;
	std::string m_sContent;
	std::string m_sSHA256;
	std::string m_sUserUUID;
	std::string m_sTimestamp;
};

/*************************************************************************************************************************
 Class declaration of COperationSequenceRevision 
**************************************************************************************************************************/

class COperationSequenceRevision : public virtual IOperationSequenceRevision, public virtual CBase {

protected:

	sOperationSequenceRevisionData m_Data;

public:

	COperationSequenceRevision(const sOperationSequenceRevisionData& data);

	virtual ~COperationSequenceRevision();

	std::string GetUUID() override;

	std::string GetStateMachine() override;

	std::string GetSequenceName() override;

	LibMCData_uint32 GetRevision() override;

	LibMCData_uint32 GetOrigin() override;

	std::string GetContent() override;

	std::string GetSHA256() override;

	std::string GetUserUUID() override;

	std::string GetTimestamp() override;

	const sOperationSequenceRevisionData& getData() const;

};

} // namespace Impl
} // namespace LibMCData

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDATA_OPERATIONSEQUENCEREVISION
