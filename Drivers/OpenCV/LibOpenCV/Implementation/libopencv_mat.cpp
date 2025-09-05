/*++

Copyright (C) 2024 Autodesk Inc.

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

#include "libopencv_mat.hpp"
#include "libopencv_interfaceexception.hpp"

// Include custom headers here.


using namespace LibOpenCV::Impl;

/*************************************************************************************************************************
 Class definition of CMat 
**************************************************************************************************************************/

CMat::CMat(uint32_t nCols, uint32_t nRows, const LibOpenCV::eImageReadFormat eReadFormat)
{
	m_Mat = cv::Mat(nRows, nCols, CV_8UC3);

    if (m_Mat.empty())
        throw ELibOpenCVInterfaceException(LIBOPENCV_ERROR_COULDNOTCREATEEMPTYIMAGE, std::to_string (nCols) + "x" + std::to_string(nRows) + " pixels");
}

CMat::CMat(const std::string& sFileName, const LibOpenCV::eImageReadFormat eReadFormat)
{
	int flags = 0;
    switch (eReadFormat) {
	    case LibOpenCV::eImageReadFormat::GrayScale: flags = cv::IMREAD_GRAYSCALE; break;
        case LibOpenCV::eImageReadFormat::RGB: flags = cv::IMREAD_COLOR_RGB; break;
        case LibOpenCV::eImageReadFormat::BGR: flags = cv::IMREAD_COLOR_BGR; break;
        default:
            throw ELibOpenCVInterfaceException(LIBOPENCV_ERROR_INVALIDREADFORMAT);
    }

    // TODO: Fix Unicode conversion with filename
	m_Mat = cv::imread(sFileName, flags);
    if (m_Mat.empty ())
        throw ELibOpenCVInterfaceException(LIBOPENCV_ERROR_COULDNOTREADIMAGEFILE, sFileName);
}

CMat::~CMat()
{

}


bool CMat::Empty()
{
    return m_Mat.empty();
}

LibOpenCV_uint32 CMat::Cols()
{
	return m_Mat.cols;
}

LibOpenCV_uint32 CMat::Rows()
{
    return m_Mat.rows;
}

void CMat::WriteToFile(const std::string& sFileName, IImageSaveParameters* pSaveParameters)
{
    // TODO: Fix Unicode conversion with filename

    bool bSuccess = cv::imwrite(sFileName, m_Mat);
    if (!bSuccess)
		throw ELibOpenCVInterfaceException(LIBOPENCV_ERROR_COULDNOTWRITEIMAGEFILE, sFileName);
}
