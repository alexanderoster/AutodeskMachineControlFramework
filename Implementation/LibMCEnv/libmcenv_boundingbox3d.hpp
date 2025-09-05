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


Abstract: This is the class declaration of CBoundingBox3D

*/


#ifndef __LIBMCENV_BOUNDINGBOX3D
#define __LIBMCENV_BOUNDINGBOX3D

#include "libmcenv_interfaces.hpp"

// Parent classes
#include "libmcenv_base.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include <array>

namespace LibMCEnv {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CBoundingBox3D 
**************************************************************************************************************************/

class CBoundingBox3D : public virtual IBoundingBox3D, public virtual CBase {
private:

	std::array<double, 3> m_dMinimum;
	std::array<double, 3> m_dMaximum;

	bool m_bIsEmpty;

public:

	CBoundingBox3D();

	CBoundingBox3D(bool bIsEmpty, double dMinX, double dMinY, double dMinZ, double dMaxX, double dMaxY, double dMaxZ);

	virtual ~CBoundingBox3D();

	bool IsEmpty() override;

	void Clear() override;

	void SetExtent(const LibMCEnv::sFloatPosition3D MinimumPoint, const LibMCEnv::sFloatPosition3D MaximumPoint) override;

	void GetExtents(LibMCEnv_double & dX, LibMCEnv_double & dY, LibMCEnv_double & dZ) override;

	void AddPoint(const LibMCEnv::sFloatPosition3D Point) override;

	void AddPointCoordinates(const LibMCEnv_double dX, const LibMCEnv_double dY, const LibMCEnv_double dZ);

	bool HasMinimumExtents(const LibMCEnv_double dMinimumExtents) override;

	LibMCEnv::sFloatPosition3D GetMinimum() override;

	LibMCEnv::sFloatPosition3D GetMaximum() override;

	void GetMinimumCoordinates(LibMCEnv_double & dX, LibMCEnv_double & dY, LibMCEnv_double & dZ) override;

	void GetMaximumCoordinates(LibMCEnv_double & dX, LibMCEnv_double & dY, LibMCEnv_double & dZ) override;

	IBoundingBox3D * Duplicate() override;

	void Merge(IBoundingBox3D* pAnotherInstance) override;

};

} // namespace Impl
} // namespace LibMCEnv

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCENV_BOUNDINGBOX3D
