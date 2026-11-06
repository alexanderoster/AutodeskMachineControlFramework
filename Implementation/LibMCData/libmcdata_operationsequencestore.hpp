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


Abstract: This is the class declaration of COperationSequenceStore

*/


#ifndef __LIBMCDATA_OPERATIONSEQUENCESTORE
#define __LIBMCDATA_OPERATIONSEQUENCESTORE

#include "libmcdata_interfaces.hpp"

// Parent classes
#include "libmcdata_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

#include "amcdata_sqlhandler.hpp"

// Include custom headers here.
#include "libmcdata_operationsequencerevision.hpp"
#include <mutex>


namespace LibMCData {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of COperationSequenceStore 
**************************************************************************************************************************/

class COperationSequenceStore : public virtual IOperationSequenceStore, public virtual CBase {

protected:
	std::mutex m_Mutex;
	AMCData::PSQLHandler m_pSQLHandler;

	static std::string normalizeStateMachine(const std::string& sStateMachine);
	static std::string normalizeSequenceName(const std::string& sSequenceName);

	static std::string selectColumns();
	static sOperationSequenceRevisionData readRow(AMCData::PSQLStatement pStatement);

public:

	COperationSequenceStore(AMCData::PSQLHandler pSQLHandler);

	virtual ~COperationSequenceStore();

	IOperationSequenceRevision* StoreRevision(const std::string & sUUID, const std::string & sStateMachine, const std::string & sSequenceName, const LibMCData_uint32 nRevision, const LibMCData_uint32 nOrigin, const std::string & sContent, const std::string & sUserUUID, const LibMCData_uint64 nAbsoluteTimeStamp) override;

	LibMCData_uint32 GetLatestRevisionNumber(const std::string & sStateMachine, const std::string & sSequenceName) override;

	IOperationSequenceRevisionIterator* ListLatestRevisions(const std::string & sStateMachine) override;

	IOperationSequenceRevisionIterator* ListRevisions(const std::string & sStateMachine, const std::string & sSequenceName) override;

	IOperationSequenceRevision* FindRevision(const std::string & sStateMachine, const std::string & sSequenceName, const LibMCData_uint32 nRevision) override;

	IOperationSequenceRevision* FindRevisionByUUID(const std::string & sUUID) override;

};

} // namespace Impl
} // namespace LibMCData

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDATA_OPERATIONSEQUENCESTORE
