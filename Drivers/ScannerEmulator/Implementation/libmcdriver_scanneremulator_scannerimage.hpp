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


Abstract: This is the class declaration of CScannerImage

*/


#ifndef __LIBMCDRIVER_SCANNEREMULATOR_SCANNERIMAGE
#define __LIBMCDRIVER_SCANNEREMULATOR_SCANNERIMAGE

#include "libmcdriver_scanneremulator_interfaces.hpp"
#include "libmcdriver_scanneremulator_base.hpp"
#include "libmcdriver_scanneremulator_imageinstance.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif


namespace LibMCDriver_ScannerEmulator {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CScannerImage 
**************************************************************************************************************************/

class CScannerImage : public virtual IScannerImage, public virtual CBase {
private:

	PScannerImageInstance m_pInstance;

public:

	CScannerImage(PScannerImageInstance pInstance);

	virtual ~CScannerImage();

	std::string GetName() override;

	void GetWindow(LibMCDriver_ScannerEmulator_double & dX1, LibMCDriver_ScannerEmulator_double & dY1, LibMCDriver_ScannerEmulator_double & dX2, LibMCDriver_ScannerEmulator_double & dY2) override;

	void GetPixelCount(LibMCDriver_ScannerEmulator_uint32 & nPixelCountX, LibMCDriver_ScannerEmulator_uint32 & nPixelCountY) override;

	void SetBackgroundColor(const LibMCDriver_ScannerEmulator_uint32 nColor) override;

	LibMCDriver_ScannerEmulator_uint32 GetBackgroundColor() override;

	void SetMicrostepInterval(const LibMCDriver_ScannerEmulator_double dIntervalInMicroseconds) override;

	LibMCDriver_ScannerEmulator_double GetMicrostepInterval() override;

	void AssignScanner(IScanner* pScannerInstance, const LibMCDriver_ScannerEmulator_double dSpotRadiusInMM, const LibMCDriver_ScannerEmulator_uint32 nLaserOnColor, const LibMCDriver_ScannerEmulator_uint32 nLaserOffColor, const bool bDrawLaserOff) override;

	void UnassignScanner(const std::string & sScannerName) override;

	bool IsScannerAssigned(const std::string & sScannerName) override;

	void SetRecording(const bool bRecording) override;

	bool IsRecording() override;

	void Clear() override;

	LibMCDriver_ScannerEmulator_uint32 GetPixelColor(const LibMCDriver_ScannerEmulator_uint32 nPixelX, const LibMCDriver_ScannerEmulator_uint32 nPixelY) override;

	LibMCDriver_ScannerEmulator_uint32 GetPixelHitCount(const LibMCDriver_ScannerEmulator_uint32 nPixelX, const LibMCDriver_ScannerEmulator_uint32 nPixelY) override;

	void WriteToImageData(LibMCEnv::PImageData pImageData) override;

};

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDRIVER_SCANNEREMULATOR_SCANNERIMAGE
