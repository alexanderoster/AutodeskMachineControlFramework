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


Abstract: This is a stub class definition of COIEMeasurementTagMap

*/

#include "libmcdriver_scanlab_oiemeasurementtagmap.hpp"
#include "libmcdriver_scanlab_interfaceexception.hpp"

// Include custom headers here.


using namespace LibMCDriver_ScanLab::Impl;

/*************************************************************************************************************************
 Class definition of COIEMeasurementTagMap 
**************************************************************************************************************************/

COIEMeasurementTagMap::COIEMeasurementTagMap(PRTCMeasurementTagMapInstance pInstance)
	: m_pInstance (pInstance)
{
	if (pInstance.get() == nullptr)
		throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDPARAM);
}

COIEMeasurementTagMap::~COIEMeasurementTagMap()
{

}


LibMCDriver_ScanLab_uint32 COIEMeasurementTagMap::GetOIEMaxMeasurementTag()
{
	return m_pInstance->getSize();;
}

void COIEMeasurementTagMap::MapOIEMeasurementTag(const LibMCDriver_ScanLab_uint32 nMeasurementTag, LibMCDriver_ScanLab_uint32 & nPartID, LibMCDriver_ScanLab_uint32 & nProfileID, LibMCDriver_ScanLab_uint32 & nSegmentID, LibMCDriver_ScanLab_uint32 & nVectorID)
{
	sOIEMeasurementTagData tagData;
	if (m_pInstance->findTag(nMeasurementTag, tagData)) {
		nPartID = tagData.m_PartID;
		nSegmentID = tagData.m_SegmentID;
		nProfileID = tagData.m_ProfileID;
		nVectorID = tagData.m_VectorID;

	}
	else
		throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDOIEMEASUREMENTTAG, "Invalid OIE Measurement Tag" + std::to_string(nMeasurementTag));
}

LibMCDriver_ScanLab::sOIEMeasurementTagData COIEMeasurementTagMap::MapOIEMeasurementTagData(const LibMCDriver_ScanLab_uint32 nMeasurementTag)
{
	sOIEMeasurementTagData tagData;
	if (m_pInstance->findTag(nMeasurementTag, tagData)) {
		return tagData;
	}
	else
		throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDOIEMEASUREMENTTAG, "Invalid OIE Measurement Tag" + std::to_string(nMeasurementTag));

}

void COIEMeasurementTagMap::MapOIEMeasurementTags(const LibMCDriver_ScanLab_uint64 nMeasurementTagBufferSize, const LibMCDriver_ScanLab_uint32 * pMeasurementTagBuffer, LibMCDriver_ScanLab_uint64 nDataBufferSize, LibMCDriver_ScanLab_uint64* pDataNeededCount, LibMCDriver_ScanLab::sOIEMeasurementTagData * pDataBuffer)
{
	if (pDataNeededCount != nullptr)
		*pDataNeededCount = nMeasurementTagBufferSize;

	if (nMeasurementTagBufferSize > 0) {

		if (pMeasurementTagBuffer == nullptr)
			throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDPARAM);

		if (pDataBuffer != nullptr) {
			if (nDataBufferSize < nMeasurementTagBufferSize)
				throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_BUFFERTOOSMALL);


			sOIEMeasurementTagData tagData;
			tagData.m_PartID = 0;
			tagData.m_ProfileID = 0;
			tagData.m_SegmentID = 0;
			tagData.m_VectorID = 0;

			uint32_t nCurrentMeasurementTag = 0xFFFFFFFF;

			const uint32_t* pTagPtr = pMeasurementTagBuffer;
			sOIEMeasurementTagData* pDataPtr = pDataBuffer;

			for (uint64_t nIndex = 0; nIndex < nMeasurementTagBufferSize; nIndex++) {
				uint32_t nMeasurementTag = *pTagPtr;
				pTagPtr++;

				if (nMeasurementTag != nCurrentMeasurementTag) {

					if (nMeasurementTag != 0) {

						if (!m_pInstance->findTag(nMeasurementTag, tagData))
							throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDOIEMEASUREMENTTAG, "Invalid OIE Measurement Tag " + std::to_string(nMeasurementTag));
					}
					else {
						tagData.m_PartID = 0;
						tagData.m_ProfileID = 0;
						tagData.m_SegmentID = 0;
						tagData.m_VectorID = 0;

					}

					nCurrentMeasurementTag = nMeasurementTag;
				}

				*pDataPtr = tagData;
				pDataPtr++;

			}
		}

	}
}




void COIEMeasurementTagMap::MapOIEMeasurementPartIDs(const LibMCDriver_ScanLab_uint64 nMeasurementTagBufferSize, const LibMCDriver_ScanLab_uint32 * pMeasurementTagBuffer, LibMCDriver_ScanLab_uint64 nDataBufferSize, LibMCDriver_ScanLab_uint64* pDataNeededCount, LibMCDriver_ScanLab_uint32 * pDataBuffer)
{
	if (pDataNeededCount != nullptr)
		*pDataNeededCount = nMeasurementTagBufferSize;

	if (nMeasurementTagBufferSize > 0) {

		if (pMeasurementTagBuffer == nullptr)
			throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDPARAM);

		if (pDataBuffer != nullptr) {
			if (nDataBufferSize < nMeasurementTagBufferSize)
				throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_BUFFERTOOSMALL);

			
			uint32_t nPartID = 0;

			uint32_t nCurrentMeasurementTag = 0xFFFFFFFF;

			const uint32_t* pTagPtr = pMeasurementTagBuffer;
			uint32_t* pDataPtr = pDataBuffer;

			for (uint64_t nIndex = 0; nIndex < nMeasurementTagBufferSize; nIndex++) {
				uint32_t nMeasurementTag = *pTagPtr;
				pTagPtr++;

				if (nMeasurementTag != nCurrentMeasurementTag) {
					if (nMeasurementTag != 0) {
						if (!m_pInstance->findPartID(nMeasurementTag, nPartID))
							throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDOIEMEASUREMENTTAG, "Invalid OIE Measurement Tag " + std::to_string(nMeasurementTag));
					}
					else {
						nPartID = 0;
					}

					nCurrentMeasurementTag = nMeasurementTag;
				}

				*pDataPtr = nPartID;
				pDataPtr++;

			}
		}

	}
}

void COIEMeasurementTagMap::MapOIEMeasurementProfileIDs(const LibMCDriver_ScanLab_uint64 nMeasurementTagBufferSize, const LibMCDriver_ScanLab_uint32 * pMeasurementTagBuffer, LibMCDriver_ScanLab_uint64 nDataBufferSize, LibMCDriver_ScanLab_uint64* pDataNeededCount, LibMCDriver_ScanLab_uint32 * pDataBuffer)
{
	if (pDataNeededCount != nullptr)
		*pDataNeededCount = nMeasurementTagBufferSize;

	if (nMeasurementTagBufferSize > 0) {

		if (pMeasurementTagBuffer == nullptr)
			throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDPARAM);

		if (pDataBuffer != nullptr) {
			if (nDataBufferSize < nMeasurementTagBufferSize)
				throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_BUFFERTOOSMALL);


			uint32_t nProfileID = 0;

			uint32_t nCurrentMeasurementTag = 0xFFFFFFFF;

			const uint32_t* pTagPtr = pMeasurementTagBuffer;
			uint32_t* pDataPtr = pDataBuffer;

			for (uint64_t nIndex = 0; nIndex < nMeasurementTagBufferSize; nIndex++) {
				uint32_t nMeasurementTag = *pTagPtr;
				pTagPtr++;

				if (nMeasurementTag != nCurrentMeasurementTag) {
					if (nMeasurementTag != 0) {
						if (!m_pInstance->findProfileID(nMeasurementTag, nProfileID))
							throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDOIEMEASUREMENTTAG, "Invalid OIE Measurement Tag " + std::to_string(nMeasurementTag));
					}
					else {
						nProfileID = 0;
					}

					nCurrentMeasurementTag = nMeasurementTag;
				}

				*pDataPtr = nProfileID;
				pDataPtr++;

			}
		}

	}
}

void COIEMeasurementTagMap::MapOIEMeasurementSegmentIDs(const LibMCDriver_ScanLab_uint64 nMeasurementTagBufferSize, const LibMCDriver_ScanLab_uint32* pMeasurementTagBuffer, LibMCDriver_ScanLab_uint64 nDataBufferSize, LibMCDriver_ScanLab_uint64* pDataNeededCount, LibMCDriver_ScanLab_uint32* pDataBuffer)
{
	if (pDataNeededCount != nullptr)
		*pDataNeededCount = nMeasurementTagBufferSize;

	if (nMeasurementTagBufferSize > 0) {

		if (pMeasurementTagBuffer == nullptr)
			throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDPARAM);

		if (pDataBuffer != nullptr) {
			if (nDataBufferSize < nMeasurementTagBufferSize)
				throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_BUFFERTOOSMALL);


			uint32_t nSegmentID = 0;

			uint32_t nCurrentMeasurementTag = 0xFFFFFFFF;

			const uint32_t* pTagPtr = pMeasurementTagBuffer;
			uint32_t* pDataPtr = pDataBuffer;

			for (uint64_t nIndex = 0; nIndex < nMeasurementTagBufferSize; nIndex++) {
				uint32_t nMeasurementTag = *pTagPtr;
				pTagPtr++;

				if (nMeasurementTag != nCurrentMeasurementTag) {
					if (nMeasurementTag != 0) {
						if (!m_pInstance->findSegmentID(nMeasurementTag, nSegmentID))
							throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDOIEMEASUREMENTTAG, "Invalid OIE Measurement Tag " + std::to_string(nMeasurementTag));
					}
					else {
						nSegmentID = 0;
					}


					nCurrentMeasurementTag = nMeasurementTag;
				}

				*pDataPtr = nSegmentID;
				pDataPtr++;

			}
		}

	}
}




void COIEMeasurementTagMap::MapOIEMeasurementVectorIDs(const LibMCDriver_ScanLab_uint64 nMeasurementTagBufferSize, const LibMCDriver_ScanLab_uint32 * pMeasurementTagBuffer, LibMCDriver_ScanLab_uint64 nDataBufferSize, LibMCDriver_ScanLab_uint64* pDataNeededCount, LibMCDriver_ScanLab_uint32 * pDataBuffer)
{
	if (pDataNeededCount != nullptr)
		*pDataNeededCount = nMeasurementTagBufferSize;

	if (nMeasurementTagBufferSize > 0) {

		if (pMeasurementTagBuffer == nullptr)
			throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDPARAM);

		if (pDataBuffer != nullptr) {
			if (nDataBufferSize < nMeasurementTagBufferSize)
				throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_BUFFERTOOSMALL);


			uint32_t nVectorID = 0;

			uint32_t nCurrentMeasurementTag = 0xFFFFFFFF;

			const uint32_t* pTagPtr = pMeasurementTagBuffer;
			uint32_t* pDataPtr = pDataBuffer;

			for (uint64_t nIndex = 0; nIndex < nMeasurementTagBufferSize; nIndex++) {
				uint32_t nMeasurementTag = *pTagPtr;
				pTagPtr++;

				if (nMeasurementTag != nCurrentMeasurementTag) {
					if (nMeasurementTag != 0) {
						if (!m_pInstance->findVectorID(nMeasurementTag, nVectorID))
							throw ELibMCDriver_ScanLabInterfaceException(LIBMCDRIVER_SCANLAB_ERROR_INVALIDOIEMEASUREMENTTAG, "Invalid OIE Measurement Tag " + std::to_string(nMeasurementTag));
					}
					else {
						nVectorID = 0;
					}

					nCurrentMeasurementTag = nMeasurementTag;
				}

				*pDataPtr = nVectorID;
				pDataPtr++;

			}
		}

	}
}

