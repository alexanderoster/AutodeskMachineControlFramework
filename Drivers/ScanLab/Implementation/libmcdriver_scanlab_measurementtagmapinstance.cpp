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

#include "libmcdriver_scanlab_measurementtagmapinstance.hpp"
#include "libmcdriver_scanlab_interfaceexception.hpp"


using namespace LibMCDriver_ScanLab::Impl;

CRTCMeasurementTagMapInstance::CRTCMeasurementTagMapInstance()
{

}

CRTCMeasurementTagMapInstance::~CRTCMeasurementTagMapInstance()
{

}

uint32_t CRTCMeasurementTagMapInstance::addTag(const sOIEMeasurementTagData& tagData)
{
	m_MeasurementTags.push_back(tagData);
	return (uint32_t)m_MeasurementTags.size();
}

uint32_t CRTCMeasurementTagMapInstance::getSize()
{
	return (uint32_t)m_MeasurementTags.size();
}

bool CRTCMeasurementTagMapInstance::findTag(uint32_t nMeasurementTag, sOIEMeasurementTagData& tagData)
{
	if ((nMeasurementTag > 0) && (nMeasurementTag <= m_MeasurementTags.size ())) 
	{	
		tagData = m_MeasurementTags.at(nMeasurementTag - 1);
		return true;
	}
		
	tagData.m_PartID = 0;
	tagData.m_ProfileID = 0;
	tagData.m_SegmentID = 0;
	tagData.m_VectorID = 0;

	return false;
}

bool CRTCMeasurementTagMapInstance::findPartID(uint32_t nMeasurementTag, uint32_t& nPartID)
{
	if ((nMeasurementTag > 0) && (nMeasurementTag <= m_MeasurementTags.size()))
	{
		nPartID = m_MeasurementTags.at(nMeasurementTag - 1).m_PartID;
		return true;
	}

	nPartID = 0;

	return false;

}

bool CRTCMeasurementTagMapInstance::findProfileID(uint32_t nMeasurementTag, uint32_t& nProfileID)
{
	if ((nMeasurementTag > 0) && (nMeasurementTag <= m_MeasurementTags.size()))
	{
		nProfileID = m_MeasurementTags.at(nMeasurementTag - 1).m_ProfileID;
		return true;
	}

	nProfileID = 0;

	return false;

}

bool CRTCMeasurementTagMapInstance::findSegmentID(uint32_t nMeasurementTag, uint32_t& nSegmentID)
{
	if ((nMeasurementTag > 0) && (nMeasurementTag <= m_MeasurementTags.size()))
	{
		nSegmentID = m_MeasurementTags.at(nMeasurementTag - 1).m_SegmentID;
		return true;
	}

	nSegmentID = 0;

	return false;

}

bool CRTCMeasurementTagMapInstance::findVectorID(uint32_t nMeasurementTag, uint32_t& nVectorID)
{
	if ((nMeasurementTag > 0) && (nMeasurementTag <= m_MeasurementTags.size()))
	{
		nVectorID = m_MeasurementTags.at(nMeasurementTag - 1).m_VectorID;
		return true;
	}

	nVectorID = 0;

	return false;

}
