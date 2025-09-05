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


Abstract: This is a stub class definition of CDriver_OpenCV

*/

#include "libmcdriver_opencv_driver_opencv.hpp"
#include "libmcdriver_opencv_interfaceexception.hpp"
#include "libmcdriver_opencv_mat.hpp"

// Include custom headers here.
#define __STRINGIZE(x) #x
#define __STRINGIZE_VALUE_OF(x) __STRINGIZE(x)

using namespace LibMCDriver_OpenCV::Impl;


class COpenCV_DLLDirectoryCache {
private:
#ifdef _WIN32
    std::wstring m_sCachedDLLDirectoryW;
#endif // _WIN32

public:
    COpenCV_DLLDirectoryCache();
    virtual ~COpenCV_DLLDirectoryCache();

};

typedef std::shared_ptr<COpenCV_DLLDirectoryCache> POpenCV_DLLDirectoryCache;


COpenCV_DLLDirectoryCache::COpenCV_DLLDirectoryCache()
{
#ifdef _WIN32
    std::vector<wchar_t> buffer;
    buffer.resize(MAX_PATH + 1);
    GetDllDirectoryW(MAX_PATH, buffer.data());

    buffer.at(MAX_PATH) = 0;
    m_sCachedDLLDirectoryW = std::wstring(buffer.data());
#endif // _WIN32
}

COpenCV_DLLDirectoryCache::~COpenCV_DLLDirectoryCache()
{
#ifdef _WIN32
    if (!m_sCachedDLLDirectoryW.empty()) {
        SetDllDirectoryW(m_sCachedDLLDirectoryW.c_str());
    }
#endif // _WIN32
}

/*************************************************************************************************************************
 Class definition of CDriver_OpenCV 
**************************************************************************************************************************/

CDriver_OpenCV::CDriver_OpenCV(LibMCEnv::PDriverEnvironment pDriverEnvironment)
    : m_pDriverEnvironment (pDriverEnvironment)
{
    if (pDriverEnvironment.get() == nullptr)
        throw ELibMCDriver_OpenCVInterfaceException(LIBMCDRIVER_OPENCV_ERROR_INVALIDPARAM);

   

}

CDriver_OpenCV::~CDriver_OpenCV()
{
    releaseOpenCV();
}

void CDriver_OpenCV::initOpenCV()
{
    if (m_pOpenCVContext.get() == nullptr) {
        
        try {


            m_pWorkingDirectory = m_pDriverEnvironment->CreateWorkingDirectory();
            m_pLibOpenCVDLL = m_pWorkingDirectory->StoreDriverData("libopencv_x64.dll", "libopencv_x64");
            m_pOpenCVWorldDLL = m_pWorkingDirectory->StoreDriverData("opencv_world4110.dll", "opencv_world4110");
            m_pOpenCVMSMFDLL = m_pWorkingDirectory->StoreDriverData("opencv_videoio_msmf4110_64.dll", "opencv_videoio_msmf4110_64");
            m_pOpenCVFFMPEGDLL = m_pWorkingDirectory->StoreDriverData("opencv_videoio_ffmpeg4110_64.dll", "opencv_videoio_ffmpeg4110_64");

            auto pDLLDirectoryCache = std::make_shared<COpenCV_DLLDirectoryCache>();

#ifdef _WIN32
            std::string sDLLDirectoryUTF8 = m_pWorkingDirectory->GetAbsoluteFilePath();

            int nPathLength = (int)sDLLDirectoryUTF8.length();
            int nPathBufferSize = nPathLength * 2 + 2;
            std::vector<wchar_t> wsDLLPath(nPathBufferSize);
            int nPathResult = MultiByteToWideChar(CP_UTF8, 0, sDLLDirectoryUTF8.c_str(), nPathLength, &wsDLLPath[0], nPathBufferSize);
            if (nPathResult == 0)
                throw ELibMCDriver_OpenCVInterfaceException(LIBMCDRIVER_OPENCV_ERROR_COULDNOTLOADLIBRARY);
            SetDllDirectoryW(wsDLLPath.data());
#endif

            m_pOpenCVWrapper = LibOpenCV::CWrapper::loadLibrary(m_pLibOpenCVDLL->GetAbsoluteFileName());
            m_pOpenCVContext = m_pOpenCVWrapper->CreateContext();

        }
        catch (...) {
             
            releaseOpenCV();
            throw;
        }
    } 
}

void CDriver_OpenCV::releaseOpenCV()
{
    m_pOpenCVContext = nullptr;
    m_pOpenCVWrapper = nullptr;

    m_pOpenCVFFMPEGDLL = nullptr;
    m_pLibOpenCVDLL = nullptr;
    m_pOpenCVMSMFDLL = nullptr;
    m_pOpenCVFFMPEGDLL = nullptr;

    m_pWorkingDirectory = nullptr;

}


void CDriver_OpenCV::Configure(const std::string& sConfigurationString)
{
}

std::string CDriver_OpenCV::GetName()
{
    return "opencv";
}

std::string CDriver_OpenCV::GetType()
{
    return "opencv";
}

void CDriver_OpenCV::GetVersion(LibMCDriver_OpenCV_uint32& nMajor, LibMCDriver_OpenCV_uint32& nMinor, LibMCDriver_OpenCV_uint32& nMicro, std::string& sBuild)
{
    nMajor = LIBMCDRIVER_OPENCV_VERSION_MAJOR;
    nMinor = LIBMCDRIVER_OPENCV_VERSION_MINOR;
    nMicro = LIBMCDRIVER_OPENCV_VERSION_MICRO;
    sBuild = __STRINGIZE_VALUE_OF(__GITHASH);
}

void CDriver_OpenCV::QueryParameters()
{
}

void CDriver_OpenCV::QueryParametersEx(LibMCEnv::PDriverStatusUpdateSession pDriverUpdateInstance)
{
}

IMat * CDriver_OpenCV::LoadImageFromBuffer(const LibMCDriver_OpenCV_uint64 nBufferBufferSize, const LibMCDriver_OpenCV_uint8 * pBufferBuffer, const LibMCDriver_OpenCV::eImageReadFormat eReadFormat)
{
    initOpenCV();

    auto pImageFile = m_pWorkingDirectory->StoreCustomDataInTempFile("dat", LibMCEnv::CInputVector<uint8_t>(pBufferBuffer, nBufferBufferSize));
    std::string sFileNameToRead = COpenCVUtils::convertUTF8FileNameToOSName(pImageFile->GetAbsoluteFileName());

    auto pImageInstance = m_pOpenCVContext->LoadImageFromFile(sFileNameToRead, (LibOpenCV::eImageReadFormat)eReadFormat);
    pImageFile = nullptr;

    return new CMat(pImageInstance, m_pWorkingDirectory);

}

IMat * CDriver_OpenCV::LoadImageFromResource(const std::string & sResourceIdentifier, const LibMCDriver_OpenCV::eImageReadFormat eReadFormat)
{
    initOpenCV();

    auto pImageFile = m_pWorkingDirectory->StoreMachineResourceDataInTempFile("dat", sResourceIdentifier);
    std::string sFileNameToRead = COpenCVUtils::convertUTF8FileNameToOSName(pImageFile->GetAbsoluteFileName());

    auto pImageInstance = m_pOpenCVContext->LoadImageFromFile(sFileNameToRead, (LibOpenCV::eImageReadFormat)eReadFormat);
    pImageFile = nullptr;

    return new CMat(pImageInstance, m_pWorkingDirectory);
}

IMat* CDriver_OpenCV::CreateEmptyImage(const LibMCDriver_OpenCV_uint32 nColumnCount, const LibMCDriver_OpenCV_uint32 nRowCount, const LibMCDriver_OpenCV::eImageReadFormat eReadFormat)
{    
    initOpenCV();

    auto pImageInstance = m_pOpenCVContext->CreateEmptyImage (nColumnCount, nRowCount, (LibOpenCV::eImageReadFormat) eReadFormat);

    return new CMat(pImageInstance, m_pWorkingDirectory);
}


