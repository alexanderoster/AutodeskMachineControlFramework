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

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL AUTODESK INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/


#ifndef __LIBMCDRIVER_SCANLAB_MEASUREMENTTAGMAPINSTANCE
#define __LIBMCDRIVER_SCANLAB_MEASUREMENTTAGMAPINSTANCE

#include "libmcdriver_scanlab_interfaces.hpp"

namespace LibMCDriver_ScanLab {
namespace Impl {


	class CRTCMeasurementTagMapInstance
	{
	private:
		std::vector<sOIEMeasurementTagData> m_MeasurementTags;

	public:

		CRTCMeasurementTagMapInstance();

		virtual ~CRTCMeasurementTagMapInstance();

		uint32_t addTag(const sOIEMeasurementTagData & tagData);

		uint32_t getSize();

		bool findTag (uint32_t nMeasurementTag, sOIEMeasurementTagData& tagData);

		bool findPartID (uint32_t nMeasurementTag, uint32_t& nPartID);

		bool findProfileID (uint32_t nMeasurementTag, uint32_t & nProfileID);

		bool findSegmentID(uint32_t nMeasurementTag, uint32_t& nSegmentID);

		bool findVectorID(uint32_t nMeasurementTag, uint32_t& nVectorID);

	};

	typedef std::shared_ptr<CRTCMeasurementTagMapInstance> PRTCMeasurementTagMapInstance;


} // namespace Impl
} // namespace LibMCDriver_ScanLab

#endif // __LIBMCDRIVER_SCANLAB_MEASUREMENTTAGMAPINSTANCE
