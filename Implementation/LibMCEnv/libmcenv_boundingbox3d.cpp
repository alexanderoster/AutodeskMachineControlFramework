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


Abstract: This is a stub class definition of CBoundingBox3D

*/

#include "libmcenv_boundingbox3d.hpp"
#include "libmcenv_interfaceexception.hpp"

// Include custom headers here.


using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CBoundingBox3D 
**************************************************************************************************************************/

CBoundingBox3D::CBoundingBox3D()
	: m_bIsEmpty(true), m_dMinimum({ 0.0, 0.0, 0.0 }), m_dMaximum({ 0.0, 0.0, 0.0 })
{
}

CBoundingBox3D::CBoundingBox3D(bool bIsEmpty, double dMinX, double dMinY, double dMinZ, double dMaxX, double dMaxY, double dMaxZ)
	: m_bIsEmpty(bIsEmpty)
{
	if (!m_bIsEmpty) {
		m_dMinimum = { dMinX, dMinY, dMinZ };
		m_dMaximum = { dMaxX, dMaxY, dMaxZ };
	} else {
		m_dMinimum = { 0.0, 0.0, 0.0 };
		m_dMaximum = { 0.0, 0.0, 0.0 };
	}
}


CBoundingBox3D::~CBoundingBox3D()
{

}

bool CBoundingBox3D::IsEmpty()
{
	return m_bIsEmpty;
}

void CBoundingBox3D::Clear()
{
	m_dMinimum = { 0.0, 0.0, 0.0 };
	m_dMaximum = { 0.0, 0.0, 0.0 };
	m_bIsEmpty = true;
}

void CBoundingBox3D::SetExtent(const LibMCEnv::sFloatPosition3D MinimumPoint, const LibMCEnv::sFloatPosition3D MaximumPoint)
{
	Clear();
	AddPoint(MinimumPoint);
	AddPoint(MaximumPoint);

}

void CBoundingBox3D::GetExtents(LibMCEnv_double & dX, LibMCEnv_double & dY, LibMCEnv_double & dZ)
{
	if (m_bIsEmpty)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_ATTEMPTEDTOACCESSEMPTYBOUNDINGBOX);

	dX = m_dMaximum.at(0) - m_dMinimum.at(0);
	dY = m_dMaximum.at(1) - m_dMinimum.at(1);
	dZ = m_dMaximum.at(2) - m_dMinimum.at(2);
}

void CBoundingBox3D::AddPoint(const LibMCEnv::sFloatPosition3D Point)
{
	if (m_bIsEmpty) {
		m_bIsEmpty = false;
		for (uint32_t j = 0; j < 3; j++) {
			double dCoordinate = Point.m_Coordinates[j];
			m_dMinimum.at(j) = dCoordinate;
			m_dMaximum.at(j) = dCoordinate;
		}
	}
	else {

		for (uint32_t j = 0; j < 3; j++) {
			double dCoordinate = Point.m_Coordinates[j];
			if (dCoordinate < m_dMinimum.at(j))
				m_dMinimum.at(j) = dCoordinate;
			if (dCoordinate > m_dMaximum.at(j))
				m_dMaximum.at(j) = dCoordinate;
		}

	}
}

void CBoundingBox3D::AddPointCoordinates(const LibMCEnv_double dX, const LibMCEnv_double dY, const LibMCEnv_double dZ)
{
	if (m_bIsEmpty) {
		m_bIsEmpty = false;
		m_dMinimum.at(0) = dX;
		m_dMaximum.at(0) = dX;
		m_dMinimum.at(1) = dY;
		m_dMaximum.at(1) = dY;
		m_dMinimum.at(2) = dZ;
		m_dMaximum.at(2) = dZ;
	}
	else {

		if (dX < m_dMinimum.at(0))
			m_dMinimum.at(0) = dX;
		if (dX > m_dMaximum.at(0))
			m_dMaximum.at(0) = dX;
		if (dY < m_dMinimum.at(1))
			m_dMinimum.at(1) = dY;
		if (dY > m_dMaximum.at(1))
			m_dMaximum.at(1) = dY;
		if (dZ < m_dMinimum.at(2))
			m_dMinimum.at(2) = dZ;
		if (dZ > m_dMaximum.at(2))
			m_dMaximum.at(2) = dZ;

	}
}

bool CBoundingBox3D::HasMinimumExtents(const LibMCEnv_double dMinimumExtents)
{
	if (m_bIsEmpty)
		return false;

	if (dMinimumExtents > 0.0) {

		double dX = m_dMaximum.at(0) - m_dMinimum.at(0);
		if (dX < dMinimumExtents)
			return false;

		double dY = m_dMaximum.at(1) - m_dMinimum.at(1);
		if (dY < dMinimumExtents)
			return false;

		double dZ = m_dMaximum.at(2) - m_dMinimum.at(2);
		if (dZ < dMinimumExtents)
			return false;

		return true;
	}
	else {
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_MINIMUMEXTENTSARENOTPOSITIVE);
		
	}


}

LibMCEnv::sFloatPosition3D CBoundingBox3D::GetMinimum()
{
	if (m_bIsEmpty)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_ATTEMPTEDTOACCESSEMPTYBOUNDINGBOX);

	return { m_dMinimum.at(0), m_dMinimum.at(1), m_dMinimum.at(2) };
}

LibMCEnv::sFloatPosition3D CBoundingBox3D::GetMaximum()
{
	if (m_bIsEmpty)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_ATTEMPTEDTOACCESSEMPTYBOUNDINGBOX);

	return { m_dMaximum.at(0), m_dMaximum.at(1), m_dMaximum.at(2) };
}

void CBoundingBox3D::GetMinimumCoordinates(LibMCEnv_double & dX, LibMCEnv_double & dY, LibMCEnv_double & dZ)
{
	if (m_bIsEmpty)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_ATTEMPTEDTOACCESSEMPTYBOUNDINGBOX);

	dX = m_dMinimum.at(0);
	dY = m_dMinimum.at(1);
	dZ = m_dMinimum.at(2);
}

void CBoundingBox3D::GetMaximumCoordinates(LibMCEnv_double & dX, LibMCEnv_double & dY, LibMCEnv_double & dZ)
{
	if (m_bIsEmpty)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_ATTEMPTEDTOACCESSEMPTYBOUNDINGBOX);

	dX = m_dMaximum.at(0);
	dY = m_dMaximum.at(1);
	dZ = m_dMaximum.at(2);

}

IBoundingBox3D * CBoundingBox3D::Duplicate()
{
	return new CBoundingBox3D(m_bIsEmpty, m_dMinimum.at(0), m_dMinimum.at(1), m_dMinimum.at(2), m_dMaximum.at(0), m_dMaximum.at(1), m_dMaximum.at(2));
}

void CBoundingBox3D::Merge(IBoundingBox3D* pAnotherInstance)
{
	if (pAnotherInstance != nullptr) {		
		if (!pAnotherInstance->IsEmpty ()) {
			AddPoint(pAnotherInstance->GetMinimum());
			AddPoint(pAnotherInstance->GetMaximum());
		}
	}
}

