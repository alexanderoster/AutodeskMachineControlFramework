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


Abstract: This is a stub class definition of CMat

*/

#include "libmcdriver_opencv_mat.hpp"
#include "libmcdriver_opencv_interfaceexception.hpp"
#include "libmcdriver_opencv_imagebuffer.hpp"

// Include custom headers here.


using namespace LibMCDriver_OpenCV::Impl;


std::string COpenCVUtils::convertUTF8FileNameToOSName(const std::string& sFileName)
{
#ifdef _WIN32

    // Convert filename to UTF16-string
    int nLength = (int)sFileName.length();
    if (nLength == 0)
        throw ELibMCDriver_OpenCVInterfaceException(LIBMCDRIVER_OPENCV_ERROR_EMPTYFILENAME);

    int nBufferSize = nLength * 4 + 4;
    std::vector<wchar_t> wsWideFileName(nBufferSize);
    int nResultLength = MultiByteToWideChar(CP_UTF8, 0, sFileName.c_str(), nLength, &wsWideFileName[0], nBufferSize);
    if ((nResultLength == 0) || (nResultLength + 1 >= nBufferSize))
        throw ELibMCDriver_OpenCVInterfaceException(LIBMCDRIVER_OPENCV_ERROR_COULDNOTCONVERTFILENAME, sFileName);

    std::vector<wchar_t> wsWideShortFileName(nBufferSize);

	DWORD nShortFileNameLength = GetShortPathNameW(wsWideFileName.data(), wsWideShortFileName.data(), nBufferSize);
    if ((nShortFileNameLength == 0) || (((int64_t)nShortFileNameLength + 1) >= (int64_t)nBufferSize))
        throw ELibMCDriver_OpenCVInterfaceException(LIBMCDRIVER_OPENCV_ERROR_COULDNOTGETSHORTPATHNAME, sFileName);

    // Add terminating character
    wsWideShortFileName.at(nShortFileNameLength) = 0;

    std::vector<char> sShortFileName(nBufferSize);
    nResultLength = WideCharToMultiByte(CP_UTF8, 0, wsWideShortFileName.data(), nShortFileNameLength, sShortFileName.data(), nBufferSize, nullptr, nullptr);
    if ((nResultLength == 0) || (nResultLength + 1 >= nBufferSize))
        throw ELibMCDriver_OpenCVInterfaceException(LIBMCDRIVER_OPENCV_ERROR_COULDNOTCONVERTSHORTFILENAME, sFileName);

    // Add terminating character
    sShortFileName.at(nResultLength) = 0;

    // Return string
    return std::string (sShortFileName.data ());

#else
	return sFileName;
#endif
}

/*************************************************************************************************************************
 Class definition of CMat 
**************************************************************************************************************************/
CMat::CMat(LibOpenCV::PMat pMat, LibMCEnv::PWorkingDirectory pWorkingDirectory)
    : m_pMat (pMat), m_pWorkingDirectory (pWorkingDirectory)
{
    if (pMat.get () == nullptr)
        throw ELibMCDriver_OpenCVInterfaceException(LIBMCDRIVER_OPENCV_ERROR_INVALIDPARAM);
    if (pWorkingDirectory.get() == nullptr)
        throw ELibMCDriver_OpenCVInterfaceException(LIBMCDRIVER_OPENCV_ERROR_INVALIDPARAM);

}

CMat::~CMat()
{

}

bool CMat::Empty()
{
    return m_pMat->Empty();
}

LibMCDriver_OpenCV_uint32 CMat::Cols()
{
    return m_pMat->Cols();
}

LibMCDriver_OpenCV_uint32 CMat::Rows()
{
    return m_pMat->Rows();
}

IImageBuffer* CMat::EncodeImage(const LibMCDriver_OpenCV::eImageWriteFormat eWriteFormat, IImageSaveParameters* pSaveParameters)
{
    std::string sExtension;
    switch (eWriteFormat) {
	    case LibMCDriver_OpenCV::eImageWriteFormat::PNG:
		    sExtension = "png";
		    break;
        case LibMCDriver_OpenCV::eImageWriteFormat::JPEG:
            sExtension = "jpg";
            break;
        default:
            throw ELibMCDriver_OpenCVInterfaceException(LIBMCDRIVER_OPENCV_ERROR_UNKNOWNIMAGEWRITEFORMAT, std::to_string ((int64_t)eWriteFormat));
    }

    auto pWorkingFile = m_pWorkingDirectory->AddManagedTempFile(sExtension);

	std::string sAbsoluteFileNameUTF8 = pWorkingFile->GetAbsoluteFileName();

    std::string sFileNameToWrite = COpenCVUtils::convertUTF8FileNameToOSName ( sAbsoluteFileNameUTF8 );

    m_pMat->WriteToFile(sFileNameToWrite, nullptr);

    if (pWorkingFile->FileExists()) {
		return new CImageBuffer (pWorkingFile, eWriteFormat);

    }
    else {

        throw ELibMCDriver_OpenCVInterfaceException(LIBMCDRIVER_OPENCV_ERROR_COULDNOTWRITEIMAGETODISK, sAbsoluteFileNameUTF8);
    }


}

void CMat::EncodeImageToStream(const LibMCDriver_OpenCV::eImageWriteFormat eWriteFormat, IImageSaveParameters* pSaveParameters, LibMCEnv::PTempStreamWriter pStream)
{
    std::unique_ptr<IImageBuffer> pImageBuffer (EncodeImage (eWriteFormat, pSaveParameters));
    pImageBuffer->StoreToStream (pStream);
}
