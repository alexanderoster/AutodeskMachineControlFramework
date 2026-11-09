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


Abstract: This is the class definition of CScannerImage

*/

#include "libmcdriver_scanneremulator_scannerimage.hpp"
#include "libmcdriver_scanneremulator_scanner.hpp"
#include "libmcdriver_scanneremulator_interfaceexception.hpp"

#include <vector>

#define SCANNEREMULATOR_IMAGEROWBAND 256

using namespace LibMCDriver_ScannerEmulator;
using namespace LibMCDriver_ScannerEmulator::Impl;

/*************************************************************************************************************************
 Class definition of CScannerImage 
**************************************************************************************************************************/

CScannerImage::CScannerImage(PScannerImageInstance pInstance)
	: m_pInstance(pInstance)
{
	if (pInstance.get() == nullptr)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);
}

CScannerImage::~CScannerImage()
{
}

std::string CScannerImage::GetName()
{
	return m_pInstance->getName();
}

void CScannerImage::GetWindow(LibMCDriver_ScannerEmulator_double & dX1, LibMCDriver_ScannerEmulator_double & dY1, LibMCDriver_ScannerEmulator_double & dX2, LibMCDriver_ScannerEmulator_double & dY2)
{
	auto window = m_pInstance->getWindow();
	dX1 = window.m_dX1;
	dY1 = window.m_dY1;
	dX2 = window.m_dX2;
	dY2 = window.m_dY2;
}

void CScannerImage::GetPixelCount(LibMCDriver_ScannerEmulator_uint32 & nPixelCountX, LibMCDriver_ScannerEmulator_uint32 & nPixelCountY)
{
	nPixelCountX = m_pInstance->getPixelCountX();
	nPixelCountY = m_pInstance->getPixelCountY();
}

void CScannerImage::SetBackgroundColor(const LibMCDriver_ScannerEmulator_uint32 nColor)
{
	m_pInstance->setBackgroundColor(nColor);
}

LibMCDriver_ScannerEmulator_uint32 CScannerImage::GetBackgroundColor()
{
	return m_pInstance->getBackgroundColor();
}

void CScannerImage::SetMicrostepInterval(const LibMCDriver_ScannerEmulator_double dIntervalInMicroseconds)
{
	m_pInstance->setMicrostepInterval(dIntervalInMicroseconds);
}

LibMCDriver_ScannerEmulator_double CScannerImage::GetMicrostepInterval()
{
	return m_pInstance->getMicrostepInterval();
}

void CScannerImage::AssignScanner(IScanner* pScannerInstance, const LibMCDriver_ScannerEmulator_double dSpotRadiusInMM, const LibMCDriver_ScannerEmulator_uint32 nLaserOnColor, const LibMCDriver_ScannerEmulator_uint32 nLaserOffColor, const bool bDrawLaserOff)
{
	if (pScannerInstance == nullptr)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSCANNERINSTANCE);

	auto pScanner = dynamic_cast<CScanner*>(pScannerInstance);
	if (pScanner == nullptr)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSCANNERINSTANCE);

	auto pScannerInstanceData = pScanner->getInstance();
	m_pInstance->assignScanner(pScannerInstanceData->getName(), dSpotRadiusInMM, nLaserOnColor, nLaserOffColor, bDrawLaserOff);
	pScannerInstanceData->attachImage(m_pInstance);
}

void CScannerImage::UnassignScanner(const std::string & sScannerName)
{
	m_pInstance->unassignScanner(sScannerName);
}

bool CScannerImage::IsScannerAssigned(const std::string & sScannerName)
{
	return m_pInstance->isScannerAssigned(sScannerName);
}

void CScannerImage::SetRecording(const bool bRecording)
{
	m_pInstance->setRecording(bRecording);
}

bool CScannerImage::IsRecording()
{
	return m_pInstance->isRecording();
}

void CScannerImage::Clear()
{
	m_pInstance->clear();
}

LibMCDriver_ScannerEmulator_uint32 CScannerImage::GetPixelColor(const LibMCDriver_ScannerEmulator_uint32 nPixelX, const LibMCDriver_ScannerEmulator_uint32 nPixelY)
{
	return m_pInstance->getPixelColor(nPixelX, nPixelY);
}

LibMCDriver_ScannerEmulator_uint32 CScannerImage::GetPixelHitCount(const LibMCDriver_ScannerEmulator_uint32 nPixelX, const LibMCDriver_ScannerEmulator_uint32 nPixelY)
{
	return m_pInstance->getPixelHitCount(nPixelX, nPixelY);
}

void CScannerImage::WriteToImageData(LibMCEnv::PImageData pImageData)
{
	if (pImageData.get() == nullptr)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);

	uint32_t nPixelCountX = m_pInstance->getPixelCountX();
	uint32_t nPixelCountY = m_pInstance->getPixelCountY();

	uint32_t nImageSizeX = 0;
	uint32_t nImageSizeY = 0;
	pImageData->GetSizeInPixels(nImageSizeX, nImageSizeY);
	if ((nImageSizeX != nPixelCountX) || (nImageSizeY != nPixelCountY))
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_IMAGESIZEMISMATCH, "image has " + std::to_string(nImageSizeX) + "x" + std::to_string(nImageSizeY) + " pixels, scanner image " + m_pInstance->getName() + " has " + std::to_string(nPixelCountX) + "x" + std::to_string(nPixelCountY));

	std::vector<uint8_t> buffer;
	for (uint32_t nStartRow = 0; nStartRow < nPixelCountY; nStartRow += SCANNEREMULATOR_IMAGEROWBAND) {
		uint32_t nRowCount = nPixelCountY - nStartRow;
		if (nRowCount > SCANNEREMULATOR_IMAGEROWBAND)
			nRowCount = SCANNEREMULATOR_IMAGEROWBAND;

		m_pInstance->getRGBRows(nStartRow, nRowCount, buffer);
		pImageData->SetPixels(0, nStartRow, nPixelCountX, nRowCount, LibMCEnv::eImagePixelFormat::RGB24bit, buffer);
	}
}
