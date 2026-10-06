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


Abstract: This is the class declaration of CDataCaptureOptions

*/


#ifndef __LIBMCENV_DATACAPTUREOPTIONS
#define __LIBMCENV_DATACAPTUREOPTIONS

#include "libmcenv_interfaces.hpp"

// Parent classes
#include "libmcenv_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "libmcenv_datacaptureinstance.hpp"

namespace LibMCEnv {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CDataCaptureOptions
**************************************************************************************************************************/

class CDataCaptureOptions : public virtual IDataCaptureOptions, public virtual CBase {
private:

	sDataCaptureSettings m_Settings;

public:

	CDataCaptureOptions();

	virtual ~CDataCaptureOptions();

	// Returns a copy of the validated settings.
	sDataCaptureSettings getSettings() const;

	void SetChunkRowCount(const LibMCEnv_uint32 nChunkRowCount) override;

	LibMCEnv_uint32 GetChunkRowCount() override;

	void SetSegmentSizeInBytes(const LibMCEnv_uint64 nSegmentSizeInBytes) override;

	LibMCEnv_uint64 GetSegmentSizeInBytes() override;

	void SetMaxQueuedChunks(const LibMCEnv_uint32 nMaxQueuedChunks) override;

	LibMCEnv_uint32 GetMaxQueuedChunks() override;

	void SetBackpressureMode(const LibMCEnv::eDataCaptureBackpressureMode eBackpressureMode) override;

	LibMCEnv::eDataCaptureBackpressureMode GetBackpressureMode() override;

	void SetStorageLimitInBytes(const LibMCEnv_uint64 nStorageLimitInBytes) override;

	LibMCEnv_uint64 GetStorageLimitInBytes() override;

	void SetCompression(const LibMCEnv::eDataCaptureCompression eCompression) override;

	LibMCEnv::eDataCaptureCompression GetCompression() override;

	void SetColumnEncoding(const std::string & sColumnIdentifier, const LibMCEnv::eDataCaptureEncoding eEncoding) override;

	void SetDoubleQuantization(const std::string & sColumnIdentifier, const LibMCEnv_double dStep) override;

};

} // namespace Impl
} // namespace LibMCEnv

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCENV_DATACAPTUREOPTIONS
