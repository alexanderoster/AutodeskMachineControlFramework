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


Abstract: This is the class definition of CDataCaptureOptions

*/

#include "libmcenv_datacaptureoptions.hpp"
#include "libmcenv_interfaceexception.hpp"

// Include custom headers here.
#include "amc_constants.hpp"
#include "amc_datatableformat.hpp"
#include <cmath>

using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CDataCaptureOptions
**************************************************************************************************************************/

CDataCaptureOptions::CDataCaptureOptions()
{

}

CDataCaptureOptions::~CDataCaptureOptions()
{

}

sDataCaptureSettings CDataCaptureOptions::getSettings() const
{
	return m_Settings;
}

void CDataCaptureOptions::SetChunkRowCount(const LibMCEnv_uint32 nChunkRowCount)
{
	if ((nChunkRowCount < DATACAPTURE_MINCHUNKROWCOUNT) || (nChunkRowCount > DATACAPTURE_MAXCHUNKROWCOUNT))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "invalid data capture chunk row count: " + std::to_string(nChunkRowCount));

	m_Settings.m_nChunkRowCount = nChunkRowCount;
}

LibMCEnv_uint32 CDataCaptureOptions::GetChunkRowCount()
{
	return m_Settings.m_nChunkRowCount;
}

void CDataCaptureOptions::SetSegmentSizeInBytes(const LibMCEnv_uint64 nSegmentSizeInBytes)
{
	// 0 disables the rollover.
	if ((nSegmentSizeInBytes != 0) && (nSegmentSizeInBytes < DATACAPTURE_MINSEGMENTSIZE))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "invalid data capture segment size: " + std::to_string(nSegmentSizeInBytes));

	m_Settings.m_nSegmentSizeInBytes = nSegmentSizeInBytes;
}

LibMCEnv_uint64 CDataCaptureOptions::GetSegmentSizeInBytes()
{
	return m_Settings.m_nSegmentSizeInBytes;
}

void CDataCaptureOptions::SetMaxQueuedChunks(const LibMCEnv_uint32 nMaxQueuedChunks)
{
	if ((nMaxQueuedChunks < DATACAPTURE_MINMAXQUEUEDCHUNKS) || (nMaxQueuedChunks > DATACAPTURE_MAXMAXQUEUEDCHUNKS))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "invalid data capture queue depth: " + std::to_string(nMaxQueuedChunks));

	m_Settings.m_nMaxQueuedChunks = nMaxQueuedChunks;
}

LibMCEnv_uint32 CDataCaptureOptions::GetMaxQueuedChunks()
{
	return m_Settings.m_nMaxQueuedChunks;
}

void CDataCaptureOptions::SetBackpressureMode(const LibMCEnv::eDataCaptureBackpressureMode eBackpressureMode)
{
	switch (eBackpressureMode) {
	case LibMCEnv::eDataCaptureBackpressureMode::Block:
	case LibMCEnv::eDataCaptureBackpressureMode::DropOldest:
	case LibMCEnv::eDataCaptureBackpressureMode::Fail:
		m_Settings.m_BackpressureMode = eBackpressureMode;
		break;
	default:
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "invalid data capture backpressure mode");
	}
}

LibMCEnv::eDataCaptureBackpressureMode CDataCaptureOptions::GetBackpressureMode()
{
	return m_Settings.m_BackpressureMode;
}

void CDataCaptureOptions::SetStorageLimitInBytes(const LibMCEnv_uint64 nStorageLimitInBytes)
{
	m_Settings.m_nStorageLimitInBytes = nStorageLimitInBytes;
}

LibMCEnv_uint64 CDataCaptureOptions::GetStorageLimitInBytes()
{
	return m_Settings.m_nStorageLimitInBytes;
}

void CDataCaptureOptions::SetCompression(const LibMCEnv::eDataCaptureCompression eCompression)
{
	switch (eCompression) {
	case LibMCEnv::eDataCaptureCompression::None:
		m_Settings.m_nCodec = DATATABLE_V2_CODEC_NONE;
		break;
	case LibMCEnv::eDataCaptureCompression::LZ4:
		m_Settings.m_nCodec = DATATABLE_V2_CODEC_LZ4;
		break;
	case LibMCEnv::eDataCaptureCompression::Deflate:
		m_Settings.m_nCodec = DATATABLE_V2_CODEC_DEFLATE;
		break;
	default:
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "invalid data capture compression");
	}
}

LibMCEnv::eDataCaptureCompression CDataCaptureOptions::GetCompression()
{
	switch (m_Settings.m_nCodec) {
	case DATATABLE_V2_CODEC_LZ4: return LibMCEnv::eDataCaptureCompression::LZ4;
	case DATATABLE_V2_CODEC_DEFLATE: return LibMCEnv::eDataCaptureCompression::Deflate;
	default: return LibMCEnv::eDataCaptureCompression::None;
	}
}

void CDataCaptureOptions::SetColumnEncoding(const std::string & sColumnIdentifier, const LibMCEnv::eDataCaptureEncoding eEncoding)
{
	if (sColumnIdentifier.empty())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "column identifier of an encoding option is empty");

	switch (eEncoding) {
	case LibMCEnv::eDataCaptureEncoding::Automatic:
		m_Settings.m_ColumnEncodings.erase(sColumnIdentifier);
		break;
	case LibMCEnv::eDataCaptureEncoding::Plain:
		m_Settings.m_ColumnEncodings[sColumnIdentifier] = DATATABLE_V2_ENCODING_PLAIN;
		break;
	case LibMCEnv::eDataCaptureEncoding::Delta:
		m_Settings.m_ColumnEncodings[sColumnIdentifier] = DATATABLE_V2_ENCODING_DELTA;
		break;
	case LibMCEnv::eDataCaptureEncoding::BitPack:
		m_Settings.m_ColumnEncodings[sColumnIdentifier] = DATATABLE_V2_ENCODING_BITPACK;
		break;
	default:
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "invalid data capture encoding");
	}
}

void CDataCaptureOptions::SetDoubleQuantization(const std::string & sColumnIdentifier, const LibMCEnv_double dStep)
{
	if (sColumnIdentifier.empty())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "column identifier of a quantisation option is empty");
	if (!std::isfinite(dStep) || (dStep < 0.0))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATACAPTUREOPTION, "invalid quantisation step: " + std::to_string(dStep));

	if (dStep == 0.0)
		m_Settings.m_ColumnQuantization.erase(sColumnIdentifier);
	else
		m_Settings.m_ColumnQuantization[sColumnIdentifier] = dStep;
}
