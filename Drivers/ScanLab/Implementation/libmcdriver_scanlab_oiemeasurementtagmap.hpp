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


Abstract: This is the class declaration of COIEMeasurementTagMap

*/


#ifndef __LIBMCDRIVER_SCANLAB_OIEMEASUREMENTTAGMAP
#define __LIBMCDRIVER_SCANLAB_OIEMEASUREMENTTAGMAP

#include "libmcdriver_scanlab_interfaces.hpp"

// Parent classes
#include "libmcdriver_scanlab_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "libmcdriver_scanlab_measurementtagmapinstance.hpp"


namespace LibMCDriver_ScanLab {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of COIEMeasurementTagMap 
**************************************************************************************************************************/

class COIEMeasurementTagMap : public virtual IOIEMeasurementTagMap, public virtual CBase {
private:

	PRTCMeasurementTagMapInstance m_pInstance;

public:

	COIEMeasurementTagMap(PRTCMeasurementTagMapInstance pInstance);

	virtual ~COIEMeasurementTagMap();

	LibMCDriver_ScanLab_uint32 GetOIEMaxMeasurementTag() override;

	void MapOIEMeasurementTag(const LibMCDriver_ScanLab_uint32 nMeasurementTag, LibMCDriver_ScanLab_uint32 & nPartID, LibMCDriver_ScanLab_uint32 & nProfileID, LibMCDriver_ScanLab_uint32 & nSegmentID, LibMCDriver_ScanLab_uint32 & nVectorID) override;

	LibMCDriver_ScanLab::sOIEMeasurementTagData MapOIEMeasurementTagData(const LibMCDriver_ScanLab_uint32 nMeasurementTag) override;

	void MapOIEMeasurementTags(const LibMCDriver_ScanLab_uint64 nMeasurementTagBufferSize, const LibMCDriver_ScanLab_uint32 * pMeasurementTagBuffer, LibMCDriver_ScanLab_uint64 nDataBufferSize, LibMCDriver_ScanLab_uint64* pDataNeededCount, LibMCDriver_ScanLab::sOIEMeasurementTagData * pDataBuffer) override;

	void MapOIEMeasurementPartIDs(const LibMCDriver_ScanLab_uint64 nMeasurementTagBufferSize, const LibMCDriver_ScanLab_uint32 * pMeasurementTagBuffer, LibMCDriver_ScanLab_uint64 nDataBufferSize, LibMCDriver_ScanLab_uint64* pDataNeededCount, LibMCDriver_ScanLab_uint32 * pDataBuffer) override;

	void MapOIEMeasurementProfileIDs(const LibMCDriver_ScanLab_uint64 nMeasurementTagBufferSize, const LibMCDriver_ScanLab_uint32 * pMeasurementTagBuffer, LibMCDriver_ScanLab_uint64 nDataBufferSize, LibMCDriver_ScanLab_uint64* pDataNeededCount, LibMCDriver_ScanLab_uint32 * pDataBuffer) override;

	void MapOIEMeasurementSegmentIDs(const LibMCDriver_ScanLab_uint64 nMeasurementTagBufferSize, const LibMCDriver_ScanLab_uint32 * pMeasurementTagBuffer, LibMCDriver_ScanLab_uint64 nDataBufferSize, LibMCDriver_ScanLab_uint64* pDataNeededCount, LibMCDriver_ScanLab_uint32 * pDataBuffer) override;

	void MapOIEMeasurementVectorIDs(const LibMCDriver_ScanLab_uint64 nMeasurementTagBufferSize, const LibMCDriver_ScanLab_uint32 * pMeasurementTagBuffer, LibMCDriver_ScanLab_uint64 nDataBufferSize, LibMCDriver_ScanLab_uint64* pDataNeededCount, LibMCDriver_ScanLab_uint32 * pDataBuffer) override;

};

} // namespace Impl
} // namespace LibMCDriver_ScanLab

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDRIVER_SCANLAB_OIEMEASUREMENTTAGMAP
