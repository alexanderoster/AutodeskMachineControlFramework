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


Abstract: A debug image of a build plate window, into which finished scanner executions are drawn. All methods are thread safe.
This file does not depend on LibMCEnv, so that the engine can be reused by other drivers.

*/


#ifndef __LIBMCDRIVER_SCANNEREMULATOR_IMAGEINSTANCE
#define __LIBMCDRIVER_SCANNEREMULATOR_IMAGEINSTANCE

#include "libmcdriver_scanneremulator_timeline.hpp"

#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

namespace LibMCDriver_ScannerEmulator {
namespace Impl {

	struct sScannerImageWindow {
		double m_dX1;
		double m_dY1;
		double m_dX2;
		double m_dY2;
	};

	struct sScannerImageAssignment {
		double m_dSpotRadiusInMM;
		uint32_t m_nLaserOnColor;
		uint32_t m_nLaserOffColor;
		bool m_bDrawLaserOff;
		int32_t m_nStampRadiusX;
		int32_t m_nStampRadiusY;
		std::vector<std::pair<int32_t, int32_t>> m_StampOffsets;
	};

	// Color channels are running averages in 8.8 fixed point.
	struct sScannerImagePixel {
		uint16_t m_nRed;
		uint16_t m_nGreen;
		uint16_t m_nBlue;
		uint16_t m_nCount;
	};

	class CScannerImageInstance {
	private:
		std::mutex m_Mutex;

		std::string m_sName;
		sScannerImageWindow m_Window;
		uint32_t m_nPixelCountX;
		uint32_t m_nPixelCountY;
		double m_dPixelSizeX;
		double m_dPixelSizeY;

		uint32_t m_nBackgroundColor;
		double m_dMicrostepInterval;
		bool m_bRecording;
		bool m_bDetached;

		std::map<std::string, sScannerImageAssignment> m_Assignments;

		std::vector<sScannerImagePixel> m_Pixels;

		// Pixels touched by the execution that is currently drawn. Bit 1: laser on, bit 2: laser off.
		std::vector<uint8_t> m_PassFlags;
		int64_t m_nPassMinX;
		int64_t m_nPassMinY;
		int64_t m_nPassMaxX;
		int64_t m_nPassMaxY;

		int64_t m_nLastStampX;
		int64_t m_nLastStampY;
		uint8_t m_nLastStampFlag;

		void resetPassLocked();
		void stampLocked(const sScannerImageAssignment& assignment, double dXInMM, double dYInMM, bool bLaserOn);
		void mergePassLocked(const sScannerImageAssignment& assignment);
		uint32_t getPixelColorLocked(size_t nPixelIndex);

	public:

		CScannerImageInstance(const std::string& sName, const sScannerImageWindow& window, uint32_t nPixelCountX, uint32_t nPixelCountY);

		static void validateName(const std::string& sName);
		static void validateColor(uint32_t nColor);

		std::string getName();
		sScannerImageWindow getWindow();
		uint32_t getPixelCountX();
		uint32_t getPixelCountY();

		void setBackgroundColor(uint32_t nColor);
		uint32_t getBackgroundColor();

		void setMicrostepInterval(double dIntervalInMicroseconds);
		double getMicrostepInterval();

		void assignScanner(const std::string& sScannerName, double dSpotRadiusInMM, uint32_t nLaserOnColor, uint32_t nLaserOffColor, bool bDrawLaserOff);
		void unassignScanner(const std::string& sScannerName);
		bool isScannerAssigned(const std::string& sScannerName);

		void setRecording(bool bRecording);
		bool isRecording();

		void clear();

		// Stops all drawing for good. Called when the image is removed from the driver.
		void detach();

		uint32_t getPixelColor(uint32_t nPixelX, uint32_t nPixelY);
		uint32_t getPixelHitCount(uint32_t nPixelX, uint32_t nPixelY);

		// Writes rows [nStartRow, nStartRow + nRowCount) as RGB24.
		void getRGBRows(uint32_t nStartRow, uint32_t nRowCount, std::vector<uint8_t>& buffer);

		// Timeline positions are in field coordinates in scanner units.
		void drawExecution(const std::string& sScannerName, const CEmulatorTimeline& timeline, double dEndTime, double dUnitsInMM);
	};

	typedef std::shared_ptr<CScannerImageInstance> PScannerImageInstance;

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator

#endif // __LIBMCDRIVER_SCANNEREMULATOR_IMAGEINSTANCE
