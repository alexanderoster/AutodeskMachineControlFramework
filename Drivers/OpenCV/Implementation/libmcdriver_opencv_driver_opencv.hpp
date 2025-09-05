/*++

Copyright (C) 2025 Autodesk Inc.

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


Abstract: This is the class declaration of CDriver_OpenCV

*/


#ifndef __LIBMCDRIVER_OPENCV_DRIVER_OPENCV
#define __LIBMCDRIVER_OPENCV_DRIVER_OPENCV

#include "libmcdriver_opencv_interfaces.hpp"
#include "libopencv_dynamic.hpp"

// Parent classes
#include "libmcdriver_opencv_driver.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.

namespace LibMCDriver_OpenCV {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CDriver_OpenCV 
**************************************************************************************************************************/

class CDriver_OpenCV : public virtual IDriver_OpenCV, public virtual CDriver {
private:

    LibMCEnv::PDriverEnvironment m_pDriverEnvironment;
    LibMCEnv::PWorkingDirectory m_pWorkingDirectory;

    LibMCEnv::PWorkingFile m_pLibOpenCVDLL;
    LibMCEnv::PWorkingFile m_pOpenCVWorldDLL;
    LibMCEnv::PWorkingFile m_pOpenCVMSMFDLL;
    LibMCEnv::PWorkingFile m_pOpenCVFFMPEGDLL;

    LibOpenCV::PWrapper m_pOpenCVWrapper;
    LibOpenCV::POpenCVContext m_pOpenCVContext;

    // Initializes OpenCV if it is not initialized already
    void initOpenCV();

    // Releases all OpenCV DLLs and resources
    void releaseOpenCV();

public:

    CDriver_OpenCV(LibMCEnv::PDriverEnvironment pDriverEnvironment);

    virtual ~CDriver_OpenCV();

    void Configure(const std::string & sConfigurationString) override;

	std::string GetName() override;

	std::string GetType() override;

	void GetVersion(LibMCDriver_OpenCV_uint32 & nMajor, LibMCDriver_OpenCV_uint32 & nMinor, LibMCDriver_OpenCV_uint32 & nMicro, std::string & sBuild) override;

	void QueryParameters() override;

	void QueryParametersEx(LibMCEnv::PDriverStatusUpdateSession pDriverUpdateInstance) override;


	IMat * LoadImageFromBuffer(const LibMCDriver_OpenCV_uint64 nBufferBufferSize, const LibMCDriver_OpenCV_uint8 * pBufferBuffer, const LibMCDriver_OpenCV::eImageReadFormat eReadFormat) override;

	IMat * LoadImageFromResource(const std::string & sResourceIdentifier, const LibMCDriver_OpenCV::eImageReadFormat eReadFormat) override;

	IMat* CreateEmptyImage(const LibMCDriver_OpenCV_uint32 nColumnCount, const LibMCDriver_OpenCV_uint32 nRowCount, const LibMCDriver_OpenCV::eImageReadFormat eReadFormat) override;
};

} // namespace Impl
} // namespace LibMCDriver_OpenCV

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDRIVER_OPENCV_DRIVER_OPENCV
