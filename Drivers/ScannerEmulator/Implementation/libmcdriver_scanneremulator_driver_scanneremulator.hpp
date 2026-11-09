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


Abstract: This is the class declaration of CDriver_ScannerEmulator

*/


#ifndef __LIBMCDRIVER_SCANNEREMULATOR_DRIVER_SCANNEREMULATOR
#define __LIBMCDRIVER_SCANNEREMULATOR_DRIVER_SCANNEREMULATOR

#include "libmcdriver_scanneremulator_interfaces.hpp"
#include "libmcdriver_scanneremulator_driver.hpp"
#include "libmcdriver_scanneremulator_scannerinstance.hpp"

#include <map>
#include <vector>
#include <mutex>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif


namespace LibMCDriver_ScannerEmulator {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CDriver_ScannerEmulator 
**************************************************************************************************************************/

class CDriver_ScannerEmulator : public virtual IDriver_ScannerEmulator, public virtual CDriver {
private:

	std::string m_sName;
	LibMCEnv::PDriverEnvironment m_pDriverEnvironment;

	std::mutex m_ScannerMutex;
	std::map<std::string, PScannerInstance> m_Scanners;
	std::vector<PScannerInstance> m_ConfiguredScanners;
	std::map<std::string, PScannerImageInstance> m_ScannerImages;

	PScannerInstance findScannerInstance(const std::string& sName, bool bMustExist);

	PScannerInstance parseScannerConfiguration(LibMCEnv::CXMLDocumentNode* pScannerNode);

	void registerScannerParameters(const std::string& sScannerName);

public:

	CDriver_ScannerEmulator(const std::string& sName, LibMCEnv::PDriverEnvironment pDriverEnvironment);

	virtual ~CDriver_ScannerEmulator();

	void Configure(const std::string& sConfigurationString) override;

	std::string GetName() override;

	std::string GetType() override;

	void GetVersion(LibMCDriver_ScannerEmulator_uint32& nMajor, LibMCDriver_ScannerEmulator_uint32& nMinor, LibMCDriver_ScannerEmulator_uint32& nMicro, std::string& sBuild) override;

	void QueryParameters() override;

	void QueryParametersEx(LibMCEnv::PDriverStatusUpdateSession pDriverUpdateInstance) override;

	IScanner * CreateScanner(const std::string & sName, const LibMCDriver_ScannerEmulator_double dX1, const LibMCDriver_ScannerEmulator_double dY1, const LibMCDriver_ScannerEmulator_double dX2, const LibMCDriver_ScannerEmulator_double dY2, const LibMCDriver_ScannerEmulator_double dUnitsInMM) override;

	bool HasScanner(const std::string & sName) override;

	IScanner * FindScanner(const std::string & sName) override;

	void RemoveScanner(const std::string & sName) override;

	LibMCDriver_ScannerEmulator_uint32 GetScannerCount() override;

	IScanner * GetScanner(const LibMCDriver_ScannerEmulator_uint32 nIndex) override;

	void DrawLayerMultiLaser(const std::string & sStreamUUID, const LibMCDriver_ScannerEmulator_uint32 nLayerIndex, const bool bFailIfNonAssignedDataExists) override;

	IScannerImage * CreateScannerImage(const std::string & sName, const LibMCDriver_ScannerEmulator_double dX1, const LibMCDriver_ScannerEmulator_double dY1, const LibMCDriver_ScannerEmulator_double dX2, const LibMCDriver_ScannerEmulator_double dY2, const LibMCDriver_ScannerEmulator_uint32 nPixelCountX, const LibMCDriver_ScannerEmulator_uint32 nPixelCountY) override;

	bool HasScannerImage(const std::string & sName) override;

	IScannerImage * FindScannerImage(const std::string & sName) override;

	void RemoveScannerImage(const std::string & sName) override;

	LibMCDriver_ScannerEmulator_uint32 GetScannerImageCount() override;

};

} // namespace Impl
} // namespace LibMCDriver_ScannerEmulator

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDRIVER_SCANNEREMULATOR_DRIVER_SCANNEREMULATOR
