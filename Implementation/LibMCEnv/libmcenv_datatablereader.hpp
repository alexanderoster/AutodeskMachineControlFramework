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


Abstract: This is the class declaration of CDataTableReader

*/


#ifndef __LIBMCENV_DATATABLEREADER
#define __LIBMCENV_DATATABLEREADER

#include "libmcenv_interfaces.hpp"

// Parent classes
#include "libmcenv_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "amc_datacapturereader.hpp"
#include "amc_toolpathhandler.hpp"
#include "libmcdata_dynamic.hpp"

namespace LibMCEnv {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CDataTableReader

 The reading logic lives in AMC::CDataCaptureReader, which the API handlers use as well.
 This class exposes it to plugins and drivers and translates its errors.
**************************************************************************************************************************/

class CDataTableReader : public virtual IDataTableReader, public virtual CBase {
private:

	AMC::PDataCaptureReader m_pReader;
	AMC::PToolpathHandler m_pToolpathHandler;

public:

	static CDataTableReader* makeFromManifest(LibMCData::PDataModel pDataModel, LibMCData::PDataCapture pManifest, AMC::PToolpathHandler pToolpathHandler);

	CDataTableReader(AMC::PDataCaptureReader pReader, AMC::PToolpathHandler pToolpathHandler);

	virtual ~CDataTableReader();

	// Number of pages that have been read from storage so far. Not part of the external API.
	uint64_t getPageReadCount();

	LibMCEnv_uint64 GetRowCount() override;

	LibMCEnv_uint64 GetFirstAvailableRow() override;

	LibMCEnv_uint32 GetColumnCount() override;

	bool HasColumn(const std::string & sIdentifier) override;

	void GetColumnInformation(const LibMCEnv_uint32 nColumnIndex, std::string & sIdentifier, std::string & sDescription, LibMCEnv::eDataTableColumnType & eColumnType) override;

	LibMCEnv_double GetColumnQuantization(const std::string & sIdentifier) override;

	LibMCEnv_uint32 GetSegmentCount() override;

	void GetSegmentInformation(const LibMCEnv_uint32 nSegmentIndex, LibMCEnv_uint64 & nFirstRow, LibMCEnv_uint64 & nRowCount, LibMCEnv_uint64 & nSizeInBytes, bool & bIsAvailable) override;

	LibMCEnv_uint64 GetChunkCount() override;

	void GetChunkInformation(const LibMCEnv_uint64 nChunkIndex, LibMCEnv_uint32 & nSegmentIndex, LibMCEnv_uint64 & nFirstRow, LibMCEnv_uint64 & nRowCount, bool & bIsAvailable) override;

	bool GetChunkStatistics(const LibMCEnv_uint64 nChunkIndex, const std::string & sIdentifier, LibMCEnv_double & dMinValue, LibMCEnv_double & dMaxValue) override;

	void ReadDoubleColumnRange(const std::string & sIdentifier, const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_double * pValuesBuffer) override;

	void ReadFloatColumnRange(const std::string & sIdentifier, const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_single * pValuesBuffer) override;

	void ReadInt64ColumnRange(const std::string & sIdentifier, const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_int64 * pValuesBuffer) override;

	void ReadBoolColumnRange(const std::string & sIdentifier, const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_uint8 * pValuesBuffer) override;

	void ReadColumnDownsampled(const std::string & sIdentifier, const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, const LibMCEnv_uint32 nTargetPoints, const LibMCEnv::eDataTableDownsampleMode eMode, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_double * pValuesBuffer) override;

	void FindChunksInValueRange(const std::string & sIdentifier, const LibMCEnv_double dMinValue, const LibMCEnv_double dMaxValue, LibMCEnv_uint64 nChunkIndicesBufferSize, LibMCEnv_uint64* pChunkIndicesNeededCount, LibMCEnv_uint64 * pChunkIndicesBuffer) override;

	IDataTable * ExtractDataTable(const LibMCEnv_uint64 nFromRow, const LibMCEnv_uint64 nRowCount, const std::string & sColumnIdentifiers) override;

};

} // namespace Impl
} // namespace LibMCEnv

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCENV_DATATABLEREADER
