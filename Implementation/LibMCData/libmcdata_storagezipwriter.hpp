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


Abstract: This is the class declaration of CStorageZIPWriter

*/


#ifndef __LIBMCDATA_STORAGEZIPWRITER
#define __LIBMCDATA_STORAGEZIPWRITER

#include "libmcdata_interfaces.hpp"

// Parent classes
#include "libmcdata_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "amcdata_storagewriter.hpp"
#include "amcdata_sqlhandler.hpp"

namespace LibMCData {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CStorageZIPWriter 
**************************************************************************************************************************/

class CStorageZIPWriter : public virtual IStorageZIPWriter, public virtual CBase {
private:

	AMCData::PStorageWriter_ZIPStream m_pStorageWriter;
	AMCData::PSQLHandler m_pSQLHandler;

public:

	CStorageZIPWriter(AMCData::PStorageWriter_ZIPStream pStorageWriter, AMCData::PSQLHandler pSQLHandler);

	virtual ~CStorageZIPWriter();

	LibMCData_uint32 StartNewEntry(const std::string & sFileName, const LibMCData_uint64 nAbsoluteTimeStamp) override;

	void FinishCurrentEntry() override;

	LibMCData_uint32 GetOpenEntryID() override;

	void WriteData(const LibMCData_uint32 nEntryID, const LibMCData_uint64 nDataBufferSize, const LibMCData_uint8 * pDataBuffer) override;

	uint64_t GetEntrySize(const LibMCData_uint32 nEntryID) override;

	void Finish() override;

	bool IsFinished() override;

	LibMCData_uint64 GetZIPStreamSize() override;

};

} // namespace Impl
} // namespace LibMCData

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDATA_STORAGEZIPWRITER