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


Abstract: A debug image of a build plate window, into which finished scanner executions are drawn.

*/

#include "libmcdriver_scanneremulator_imageinstance.hpp"
#include "libmcdriver_scanneremulator_interfaceexception.hpp"

#include <cmath>
#include <algorithm>
#include <limits>

#define SCANNEREMULATOR_MAXIMAGENAMELENGTH 64
#define SCANNEREMULATOR_MAXIMAGEAXISPIXELS 16384
#define SCANNEREMULATOR_MAXIMAGEPIXELS 16777216
#define SCANNEREMULATOR_MAXSTAMPRADIUSINPIXELS 256
#define SCANNEREMULATOR_MINMICROSTEPINTERVAL 0.1
#define SCANNEREMULATOR_MAXMICROSTEPINTERVAL 1.0E6
#define SCANNEREMULATOR_DEFAULTMICROSTEPINTERVAL 10.0
#define SCANNEREMULATOR_DEFAULTBACKGROUNDCOLOR 0x000000
#define SCANNEREMULATOR_MAXCOLOR 0xFFFFFF
#define SCANNEREMULATOR_MAXHITCOUNT 65535
#define SCANNEREMULATOR_MAXFIXEDPOINTCHANNEL (255 * 256)

#define SCANNEREMULATOR_PASSFLAG_LASERON 1
#define SCANNEREMULATOR_PASSFLAG_LASEROFF 2

namespace LibMCDriver_ScannerEmulator {
namespace Impl {

	static uint16_t blendChannel(uint16_t nAverage, uint32_t nValue, int32_t nCount)
	{
		int32_t nTarget = (int32_t)(nValue & 0xFF) * 256;
		int32_t nDelta = nTarget - (int32_t)nAverage;
		int32_t nStep;
		if (nDelta >= 0)
			nStep = (nDelta + nCount / 2) / nCount;
		else
			nStep = -((-nDelta + nCount / 2) / nCount);

		int32_t nResult = (int32_t)nAverage + nStep;
		if (nResult < 0)
			nResult = 0;
		if (nResult > SCANNEREMULATOR_MAXFIXEDPOINTCHANNEL)
			nResult = SCANNEREMULATOR_MAXFIXEDPOINTCHANNEL;
		return (uint16_t)nResult;
	}

	static uint32_t channelToByte(uint16_t nFixedPoint)
	{
		uint32_t nValue = ((uint32_t)nFixedPoint + 128) >> 8;
		if (nValue > 255)
			nValue = 255;
		return nValue;
	}

	CScannerImageInstance::CScannerImageInstance(const std::string& sName, const sScannerImageWindow& window, uint32_t nPixelCountX, uint32_t nPixelCountY)
		: m_sName(sName),
		m_Window(window),
		m_nPixelCountX(nPixelCountX),
		m_nPixelCountY(nPixelCountY),
		m_dPixelSizeX(1.0),
		m_dPixelSizeY(1.0),
		m_nBackgroundColor(SCANNEREMULATOR_DEFAULTBACKGROUNDCOLOR),
		m_dMicrostepInterval(SCANNEREMULATOR_DEFAULTMICROSTEPINTERVAL),
		m_bRecording(true),
		m_bDetached(false),
		m_nPassMinX(0),
		m_nPassMinY(0),
		m_nPassMaxX(-1),
		m_nPassMaxY(-1),
		m_nLastStampX(0),
		m_nLastStampY(0),
		m_nLastStampFlag(0)
	{
		validateName(sName);

		bool bWindowIsValid = std::isfinite(window.m_dX1) && std::isfinite(window.m_dY1) && std::isfinite(window.m_dX2) && std::isfinite(window.m_dY2)
			&& (window.m_dX1 < window.m_dX2) && (window.m_dY1 < window.m_dY2);
		if (!bWindowIsValid)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDIMAGEWINDOW, "invalid window of scanner image " + sName);

		if ((nPixelCountX < 1) || (nPixelCountX > SCANNEREMULATOR_MAXIMAGEAXISPIXELS) || (nPixelCountY < 1) || (nPixelCountY > SCANNEREMULATOR_MAXIMAGEAXISPIXELS))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDIMAGESIZE, "invalid size of scanner image " + sName + ": " + std::to_string(nPixelCountX) + "x" + std::to_string(nPixelCountY));

		uint64_t nTotalPixels = (uint64_t)nPixelCountX * (uint64_t)nPixelCountY;
		if (nTotalPixels > SCANNEREMULATOR_MAXIMAGEPIXELS)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDIMAGESIZE, "scanner image " + sName + " has too many pixels: " + std::to_string(nPixelCountX) + "x" + std::to_string(nPixelCountY));

		m_dPixelSizeX = (window.m_dX2 - window.m_dX1) / (double)nPixelCountX;
		m_dPixelSizeY = (window.m_dY2 - window.m_dY1) / (double)nPixelCountY;
		if (!(m_dPixelSizeX > 0.0) || !(m_dPixelSizeY > 0.0))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDIMAGEWINDOW, "invalid window of scanner image " + sName);

		m_Pixels.resize((size_t)nTotalPixels, sScannerImagePixel{ 0, 0, 0, 0 });
		m_PassFlags.resize((size_t)nTotalPixels, 0);
	}

	void CScannerImageInstance::validateName(const std::string& sName)
	{
		if (sName.empty() || (sName.length() > SCANNEREMULATOR_MAXIMAGENAMELENGTH))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSCANNERIMAGENAME, "invalid scanner image name: " + sName);

		for (char cChar : sName) {
			bool bIsValid = ((cChar >= 'a') && (cChar <= 'z')) || ((cChar >= 'A') && (cChar <= 'Z')) || ((cChar >= '0') && (cChar <= '9')) || (cChar == '_');
			if (!bIsValid)
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSCANNERIMAGENAME, "invalid scanner image name: " + sName);
		}
	}

	void CScannerImageInstance::validateColor(uint32_t nColor)
	{
		if (nColor > SCANNEREMULATOR_MAXCOLOR)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCOLOR, "invalid color: " + std::to_string(nColor));
	}

	std::string CScannerImageInstance::getName()
	{
		return m_sName;
	}

	sScannerImageWindow CScannerImageInstance::getWindow()
	{
		return m_Window;
	}

	uint32_t CScannerImageInstance::getPixelCountX()
	{
		return m_nPixelCountX;
	}

	uint32_t CScannerImageInstance::getPixelCountY()
	{
		return m_nPixelCountY;
	}

	void CScannerImageInstance::setBackgroundColor(uint32_t nColor)
	{
		validateColor(nColor);
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_nBackgroundColor = nColor;
	}

	uint32_t CScannerImageInstance::getBackgroundColor()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_nBackgroundColor;
	}

	void CScannerImageInstance::setMicrostepInterval(double dIntervalInMicroseconds)
	{
		if (!std::isfinite(dIntervalInMicroseconds) || (dIntervalInMicroseconds < SCANNEREMULATOR_MINMICROSTEPINTERVAL) || (dIntervalInMicroseconds > SCANNEREMULATOR_MAXMICROSTEPINTERVAL))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDMICROSTEPINTERVAL, "invalid microstep interval: " + std::to_string(dIntervalInMicroseconds));

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_dMicrostepInterval = dIntervalInMicroseconds;
	}

	double CScannerImageInstance::getMicrostepInterval()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_dMicrostepInterval;
	}

	void CScannerImageInstance::assignScanner(const std::string& sScannerName, double dSpotRadiusInMM, uint32_t nLaserOnColor, uint32_t nLaserOffColor, bool bDrawLaserOff)
	{
		if (!std::isfinite(dSpotRadiusInMM) || (dSpotRadiusInMM < 0.0))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSPOTRADIUS, "invalid spot radius: " + std::to_string(dSpotRadiusInMM));
		validateColor(nLaserOnColor);
		validateColor(nLaserOffColor);

		double dRadiusX = std::ceil(dSpotRadiusInMM / m_dPixelSizeX);
		double dRadiusY = std::ceil(dSpotRadiusInMM / m_dPixelSizeY);
		if ((dRadiusX > SCANNEREMULATOR_MAXSTAMPRADIUSINPIXELS) || (dRadiusY > SCANNEREMULATOR_MAXSTAMPRADIUSINPIXELS))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDSPOTRADIUS, "spot radius of " + std::to_string(dSpotRadiusInMM) + "mm exceeds " + std::to_string(SCANNEREMULATOR_MAXSTAMPRADIUSINPIXELS) + " pixels in scanner image " + m_sName);

		sScannerImageAssignment assignment;
		assignment.m_dSpotRadiusInMM = dSpotRadiusInMM;
		assignment.m_nLaserOnColor = nLaserOnColor;
		assignment.m_nLaserOffColor = nLaserOffColor;
		assignment.m_bDrawLaserOff = bDrawLaserOff;
		assignment.m_nStampRadiusX = (int32_t)dRadiusX;
		assignment.m_nStampRadiusY = (int32_t)dRadiusY;

		double dRadiusSquared = dSpotRadiusInMM * dSpotRadiusInMM;
		assignment.m_StampOffsets.push_back(std::make_pair(0, 0));
		for (int32_t nDeltaY = -assignment.m_nStampRadiusY; nDeltaY <= assignment.m_nStampRadiusY; nDeltaY++) {
			for (int32_t nDeltaX = -assignment.m_nStampRadiusX; nDeltaX <= assignment.m_nStampRadiusX; nDeltaX++) {
				if ((nDeltaX == 0) && (nDeltaY == 0))
					continue;
				double dDistanceX = nDeltaX * m_dPixelSizeX;
				double dDistanceY = nDeltaY * m_dPixelSizeY;
				if ((dDistanceX * dDistanceX + dDistanceY * dDistanceY) <= dRadiusSquared)
					assignment.m_StampOffsets.push_back(std::make_pair(nDeltaX, nDeltaY));
			}
		}

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		if (m_bDetached)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERIMAGENOTFOUND, "scanner image has been removed: " + m_sName);
		m_Assignments[sScannerName] = assignment;
	}

	void CScannerImageInstance::unassignScanner(const std::string& sScannerName)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_Assignments.erase(sScannerName);
	}

	bool CScannerImageInstance::isScannerAssigned(const std::string& sScannerName)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_Assignments.find(sScannerName) != m_Assignments.end();
	}

	void CScannerImageInstance::setRecording(bool bRecording)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_bRecording = bRecording;
	}

	bool CScannerImageInstance::isRecording()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_bRecording;
	}

	void CScannerImageInstance::clear()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		std::fill(m_Pixels.begin(), m_Pixels.end(), sScannerImagePixel{ 0, 0, 0, 0 });
	}

	void CScannerImageInstance::detach()
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		m_bDetached = true;
		m_Assignments.clear();
	}

	uint32_t CScannerImageInstance::getPixelColorLocked(size_t nPixelIndex)
	{
		auto& pixel = m_Pixels[nPixelIndex];
		if (pixel.m_nCount == 0)
			return m_nBackgroundColor;

		return (channelToByte(pixel.m_nRed) << 16) | (channelToByte(pixel.m_nGreen) << 8) | channelToByte(pixel.m_nBlue);
	}

	uint32_t CScannerImageInstance::getPixelColor(uint32_t nPixelX, uint32_t nPixelY)
	{
		if ((nPixelX >= m_nPixelCountX) || (nPixelY >= m_nPixelCountY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPIXELCOORDINATE, "invalid pixel coordinate: " + std::to_string(nPixelX) + "/" + std::to_string(nPixelY));

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return getPixelColorLocked((size_t)nPixelY * m_nPixelCountX + nPixelX);
	}

	uint32_t CScannerImageInstance::getPixelHitCount(uint32_t nPixelX, uint32_t nPixelY)
	{
		if ((nPixelX >= m_nPixelCountX) || (nPixelY >= m_nPixelCountY))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPIXELCOORDINATE, "invalid pixel coordinate: " + std::to_string(nPixelX) + "/" + std::to_string(nPixelY));

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		return m_Pixels[(size_t)nPixelY * m_nPixelCountX + nPixelX].m_nCount;
	}

	void CScannerImageInstance::getRGBRows(uint32_t nStartRow, uint32_t nRowCount, std::vector<uint8_t>& buffer)
	{
		if ((nStartRow >= m_nPixelCountY) || (nRowCount > (m_nPixelCountY - nStartRow)))
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPIXELCOORDINATE, "invalid row range: " + std::to_string(nStartRow) + "/" + std::to_string(nRowCount));

		buffer.resize((size_t)nRowCount * m_nPixelCountX * 3);

		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		size_t nBufferIndex = 0;
		size_t nPixelIndex = (size_t)nStartRow * m_nPixelCountX;
		size_t nPixelEnd = nPixelIndex + (size_t)nRowCount * m_nPixelCountX;
		for (; nPixelIndex < nPixelEnd; nPixelIndex++) {
			uint32_t nColor = getPixelColorLocked(nPixelIndex);
			buffer[nBufferIndex++] = (uint8_t)((nColor >> 16) & 0xFF);
			buffer[nBufferIndex++] = (uint8_t)((nColor >> 8) & 0xFF);
			buffer[nBufferIndex++] = (uint8_t)(nColor & 0xFF);
		}
	}

	void CScannerImageInstance::resetPassLocked()
	{
		m_nPassMinX = std::numeric_limits<int64_t>::max();
		m_nPassMinY = std::numeric_limits<int64_t>::max();
		m_nPassMaxX = std::numeric_limits<int64_t>::min();
		m_nPassMaxY = std::numeric_limits<int64_t>::min();
		m_nLastStampFlag = 0;
	}

	void CScannerImageInstance::stampLocked(const sScannerImageAssignment& assignment, double dXInMM, double dYInMM, bool bLaserOn)
	{
		double dPixelX = std::floor((dXInMM - m_Window.m_dX1) / m_dPixelSizeX);
		double dPixelY = std::floor((m_Window.m_dY2 - dYInMM) / m_dPixelSizeY);
		if (!std::isfinite(dPixelX) || !std::isfinite(dPixelY))
			return;

		double dMargin = (double)SCANNEREMULATOR_MAXSTAMPRADIUSINPIXELS + 1.0;
		if ((dPixelX < -dMargin) || (dPixelY < -dMargin) || (dPixelX > (double)m_nPixelCountX + dMargin) || (dPixelY > (double)m_nPixelCountY + dMargin))
			return;

		int64_t nCenterX = (int64_t)dPixelX;
		int64_t nCenterY = (int64_t)dPixelY;
		uint8_t nFlag = bLaserOn ? SCANNEREMULATOR_PASSFLAG_LASERON : SCANNEREMULATOR_PASSFLAG_LASEROFF;

		if ((nFlag == m_nLastStampFlag) && (nCenterX == m_nLastStampX) && (nCenterY == m_nLastStampY))
			return;
		m_nLastStampFlag = nFlag;
		m_nLastStampX = nCenterX;
		m_nLastStampY = nCenterY;

		int64_t nPixelCountX = m_nPixelCountX;
		int64_t nPixelCountY = m_nPixelCountY;
		int64_t nMinX = std::max<int64_t>(nCenterX - assignment.m_nStampRadiusX, 0);
		int64_t nMinY = std::max<int64_t>(nCenterY - assignment.m_nStampRadiusY, 0);
		int64_t nMaxX = std::min<int64_t>(nCenterX + assignment.m_nStampRadiusX, nPixelCountX - 1);
		int64_t nMaxY = std::min<int64_t>(nCenterY + assignment.m_nStampRadiusY, nPixelCountY - 1);
		if ((nMinX > nMaxX) || (nMinY > nMaxY))
			return;

		for (auto& offset : assignment.m_StampOffsets) {
			int64_t nX = nCenterX + offset.first;
			int64_t nY = nCenterY + offset.second;
			if ((nX < 0) || (nY < 0) || (nX >= nPixelCountX) || (nY >= nPixelCountY))
				continue;
			m_PassFlags[(size_t)(nY * nPixelCountX + nX)] |= nFlag;
		}

		m_nPassMinX = std::min(m_nPassMinX, nMinX);
		m_nPassMinY = std::min(m_nPassMinY, nMinY);
		m_nPassMaxX = std::max(m_nPassMaxX, nMaxX);
		m_nPassMaxY = std::max(m_nPassMaxY, nMaxY);
	}

	void CScannerImageInstance::mergePassLocked(const sScannerImageAssignment& assignment)
	{
		if ((m_nPassMinX > m_nPassMaxX) || (m_nPassMinY > m_nPassMaxY))
			return;

		int64_t nPixelCountX = m_nPixelCountX;
		for (int64_t nY = m_nPassMinY; nY <= m_nPassMaxY; nY++) {
			for (int64_t nX = m_nPassMinX; nX <= m_nPassMaxX; nX++) {
				size_t nIndex = (size_t)(nY * nPixelCountX + nX);
				uint8_t nFlags = m_PassFlags[nIndex];
				if (nFlags == 0)
					continue;
				m_PassFlags[nIndex] = 0;

				// Laser on wins over laser off within one execution.
				uint32_t nColor = ((nFlags & SCANNEREMULATOR_PASSFLAG_LASERON) != 0) ? assignment.m_nLaserOnColor : assignment.m_nLaserOffColor;

				auto& pixel = m_Pixels[nIndex];
				if (pixel.m_nCount < SCANNEREMULATOR_MAXHITCOUNT)
					pixel.m_nCount++;
				int32_t nCount = pixel.m_nCount;
				pixel.m_nRed = blendChannel(pixel.m_nRed, nColor >> 16, nCount);
				pixel.m_nGreen = blendChannel(pixel.m_nGreen, nColor >> 8, nCount);
				pixel.m_nBlue = blendChannel(pixel.m_nBlue, nColor, nCount);
			}
		}
	}

	void CScannerImageInstance::drawExecution(const std::string& sScannerName, const CEmulatorTimeline& timeline, double dEndTime, double dUnitsInMM)
	{
		std::lock_guard<std::mutex> lockGuard(m_Mutex);
		if (m_bDetached || !m_bRecording)
			return;

		auto iAssignmentIter = m_Assignments.find(sScannerName);
		if (iAssignmentIter == m_Assignments.end())
			return;
		auto& assignment = iAssignmentIter->second;

		if (!std::isfinite(dEndTime) || (dEndTime < 0.0))
			return;

		auto& segments = timeline.getSegments();
		auto& laserIntervals = timeline.getLaserIntervals();
		size_t nSegmentCount = segments.size();
		size_t nIntervalCount = laserIntervals.size();
		double dInterval = m_dMicrostepInterval;

		// nNextSegment: first segment that starts after dTime. nInterval: first laser interval that ends after dTime.
		size_t nNextSegment = 0;
		size_t nInterval = 0;
		double dTime = 0.0;

		resetPassLocked();

		while (true) {
			while ((nNextSegment < nSegmentCount) && (segments[nNextSegment].m_dStartTime <= dTime))
				nNextSegment++;
			while ((nInterval < nIntervalCount) && (laserIntervals[nInterval].m_dOffTime <= dTime))
				nInterval++;

			bool bLaserOn = (dTime < dEndTime) && (nInterval < nIntervalCount) && (laserIntervals[nInterval].m_dOnTime <= dTime);

			double dX = 0.0;
			double dY = 0.0;
			bool bMoving = false;
			double dSegmentEndTime = dEndTime;
			if (nNextSegment == 0) {
				timeline.getPositionAt(dTime, dX, dY);
			}
			else {
				auto& segment = segments[nNextSegment - 1];
				double dElapsed = dTime - segment.m_dStartTime;
				if ((segment.m_dDuration <= 0.0) || (dElapsed >= segment.m_dDuration)) {
					dX = segment.m_dEndX;
					dY = segment.m_dEndY;
				}
				else {
					double dFactor = dElapsed / segment.m_dDuration;
					dX = segment.m_dStartX + (segment.m_dEndX - segment.m_dStartX) * dFactor;
					dY = segment.m_dStartY + (segment.m_dEndY - segment.m_dStartY) * dFactor;
					bMoving = (segment.m_dStartX != segment.m_dEndX) || (segment.m_dStartY != segment.m_dEndY);
					dSegmentEndTime = segment.m_dStartTime + segment.m_dDuration;
				}
			}

			if (bLaserOn || assignment.m_bDrawLaserOff)
				stampLocked(assignment, dX * dUnitsInMM, dY * dUnitsInMM, bLaserOn);

			if (dTime >= dEndTime)
				break;

			double dNextEvent = std::min(dEndTime, dSegmentEndTime);
			if (nNextSegment < nSegmentCount)
				dNextEvent = std::min(dNextEvent, segments[nNextSegment].m_dStartTime);
			if (nInterval < nIntervalCount) {
				auto& interval = laserIntervals[nInterval];
				dNextEvent = std::min(dNextEvent, (interval.m_dOnTime > dTime) ? interval.m_dOnTime : interval.m_dOffTime);
			}

			// The head does not move until the next event, so stationary phases only cost one sample.
			double dNextTime = dNextEvent;
			if (bMoving) {
				double dNextGridTime = (std::floor(dTime / dInterval) + 1.0) * dInterval;
				if (dNextGridTime < dNextTime)
					dNextTime = dNextGridTime;
			}
			if (!(dNextTime > dTime))
				dNextTime = dTime + dInterval;
			if (dNextTime > dEndTime)
				dNextTime = dEndTime;

			dTime = dNextTime;
		}

		mergePassLocked(assignment);
	}

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator
