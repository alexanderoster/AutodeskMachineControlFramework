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


Abstract: This is the class declaration of CRTCPowerRamp

*/


#ifndef __LIBMCDRIVER_SCANLAB_RTCPOWERRAMP
#define __LIBMCDRIVER_SCANLAB_RTCPOWERRAMP

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>

// Header only and without driver dependencies, so that amc_unittest can test it.

// Maximum length of a sub-vector of a power ramp in mm.
#define RTCPOWERRAMP_DEFAULT_STEPLENGTH 0.1

// Upper bound of sub-vectors per vector. Longer vectors get longer sub-vectors.
#define RTCPOWERRAMP_MAX_STEPSPERVECTOR 1024

// Support points closer than this to their predecessor or to the vector end are ignored.
#define RTCPOWERRAMP_MIN_PARAMETERDISTANCE 1.0e-9

namespace LibMCDriver_ScanLab {
namespace Impl {

typedef struct _sRTCPowerRampSupportPoint {
	double m_dParameter;
	double m_dValue;
} sRTCPowerRampSupportPoint;

typedef struct _sRTCPowerRampStep {
	double m_dStartParameter;
	double m_dEndParameter;
	double m_dValue;
} sRTCPowerRampStep;


/*************************************************************************************************************************
 Class declaration of CRTCPowerRamp
**************************************************************************************************************************/

// A power ramp is a piecewise-linear value curve along a vector, parametrized from 0 (start) to 1 (end).
// The knots are the start value, the support points in increasing parameter order and the end value.
//
// The RTC card can only switch the power between marks, so a ramp is drawn as a sequence of sub-vectors with
// constant power. Each sub-vector runs with the curve value at its midpoint, which is the mean value of the
// linear piece it covers. Support points are always sub-vector boundaries, so kinks are kept exactly.
class CRTCPowerRamp
{
private:

	static void buildKnots(double dStartValue, double dEndValue, const sRTCPowerRampSupportPoint* pSupportPoints, size_t nSupportPointCount, std::vector<sRTCPowerRampSupportPoint>& knots)
	{
		knots.clear();
		knots.reserve(nSupportPointCount + 2);
		knots.push_back({ 0.0, dStartValue });

		if (pSupportPoints != nullptr) {
			for (size_t nIndex = 0; nIndex < nSupportPointCount; nIndex++) {
				double dParameter = pSupportPoints[nIndex].m_dParameter;
				if (dParameter < knots.back().m_dParameter + RTCPOWERRAMP_MIN_PARAMETERDISTANCE)
					continue;
				if (dParameter > 1.0 - RTCPOWERRAMP_MIN_PARAMETERDISTANCE)
					continue;

				knots.push_back({ dParameter, pSupportPoints[nIndex].m_dValue });
			}
		}

		knots.push_back({ 1.0, dEndValue });
	}

public:

	// Returns the curve value at a parameter between 0 and 1.
	static double evaluate(double dStartValue, double dEndValue, const sRTCPowerRampSupportPoint* pSupportPoints, size_t nSupportPointCount, double dParameter)
	{
		std::vector<sRTCPowerRampSupportPoint> knots;
		buildKnots(dStartValue, dEndValue, pSupportPoints, nSupportPointCount, knots);

		if (dParameter <= 0.0)
			return dStartValue;
		if (dParameter >= 1.0)
			return dEndValue;

		for (size_t nIndex = 1; nIndex < knots.size(); nIndex++) {
			auto& knot0 = knots[nIndex - 1];
			auto& knot1 = knots[nIndex];
			if (dParameter <= knot1.m_dParameter) {
				double dRatio = (dParameter - knot0.m_dParameter) / (knot1.m_dParameter - knot0.m_dParameter);
				return knot0.m_dValue + dRatio * (knot1.m_dValue - knot0.m_dValue);
			}
		}

		return dEndValue;
	}

	// Splits a vector of the given length into constant value sub-vectors. Linear pieces whose values differ
	// by at most dValueTolerance are treated as constant. Adjacent sub-vectors whose values differ by at most
	// dValueTolerance from the first one are merged. A step length of 0 or less gives one sub-vector per piece.
	static void computeSteps(double dVectorLength, double dStartValue, double dEndValue, const sRTCPowerRampSupportPoint* pSupportPoints, size_t nSupportPointCount, double dMaxStepLength, double dValueTolerance, std::vector<sRTCPowerRampStep>& steps)
	{
		steps.clear();

		std::vector<sRTCPowerRampSupportPoint> knots;
		buildKnots(dStartValue, dEndValue, pSupportPoints, nSupportPointCount, knots);

		if (dVectorLength < 0.0)
			dVectorLength = 0.0;
		if (dValueTolerance < 0.0)
			dValueTolerance = 0.0;

		double dEffectiveStepLength = 0.0;
		if (dMaxStepLength > 0.0) {
			double dMinimumStepLength = dVectorLength / (double)RTCPOWERRAMP_MAX_STEPSPERVECTOR;
			dEffectiveStepLength = (dMaxStepLength > dMinimumStepLength) ? dMaxStepLength : dMinimumStepLength;
		}

		for (size_t nIndex = 1; nIndex < knots.size(); nIndex++) {
			auto& knot0 = knots[nIndex - 1];
			auto& knot1 = knots[nIndex];

			double dParameterSpan = knot1.m_dParameter - knot0.m_dParameter;
			double dValueSpan = knot1.m_dValue - knot0.m_dValue;

			uint32_t nStepCount = 1;
			if ((std::abs(dValueSpan) > dValueTolerance) && (dEffectiveStepLength > 0.0)) {
				double dPieceLength = dParameterSpan * dVectorLength;
				double dStepCount = std::ceil(dPieceLength / dEffectiveStepLength);
				if (dStepCount > 1.0)
					nStepCount = (dStepCount < (double)RTCPOWERRAMP_MAX_STEPSPERVECTOR) ? (uint32_t)dStepCount : RTCPOWERRAMP_MAX_STEPSPERVECTOR;
			}

			for (uint32_t nStepIndex = 0; nStepIndex < nStepCount; nStepIndex++) {
				double dStartParameter = knot0.m_dParameter + dParameterSpan * ((double)nStepIndex / (double)nStepCount);
				double dEndParameter = (nStepIndex + 1 == nStepCount) ? knot1.m_dParameter : (knot0.m_dParameter + dParameterSpan * ((double)(nStepIndex + 1) / (double)nStepCount));
				double dValue = knot0.m_dValue + dValueSpan * (((double)nStepIndex + 0.5) / (double)nStepCount);

				if (!steps.empty() && (std::abs(steps.back().m_dValue - dValue) <= dValueTolerance)) {
					steps.back().m_dEndParameter = dEndParameter;
				}
				else {
					steps.push_back({ dStartParameter, dEndParameter, dValue });
				}
			}
		}
	}

};


/*************************************************************************************************************************
 Class declaration of CRTCPowerRampList
**************************************************************************************************************************/

// Ramp steps of all vectors of a segment, stored in one buffer.
class CRTCPowerRampList
{
private:

	std::vector<sRTCPowerRampStep> m_Steps;
	std::vector<size_t> m_ElementOffsets;

public:

	CRTCPowerRampList()
	{
		m_ElementOffsets.push_back(0);
	}

	void clear()
	{
		m_Steps.clear();
		m_ElementOffsets.clear();
		m_ElementOffsets.push_back(0);
	}

	void addElement(const std::vector<sRTCPowerRampStep>& steps)
	{
		m_Steps.insert(m_Steps.end(), steps.begin(), steps.end());
		m_ElementOffsets.push_back(m_Steps.size());
	}

	size_t getElementCount() const
	{
		return m_ElementOffsets.size() - 1;
	}

	size_t getTotalStepCount() const
	{
		return m_Steps.size();
	}

	// Returns nullptr if the element has no steps or does not exist.
	const sRTCPowerRampStep* getElementSteps(size_t nElementIndex, size_t& nStepCount) const
	{
		nStepCount = 0;
		if (nElementIndex + 1 >= m_ElementOffsets.size())
			return nullptr;

		size_t nStart = m_ElementOffsets[nElementIndex];
		nStepCount = m_ElementOffsets[nElementIndex + 1] - nStart;
		if (nStepCount == 0)
			return nullptr;

		return &m_Steps[nStart];
	}

	sRTCPowerRampStep* getStepsBuffer()
	{
		return m_Steps.empty() ? nullptr : m_Steps.data();
	}

};

} // namespace Impl
} // namespace LibMCDriver_ScanLab

#endif // __LIBMCDRIVER_SCANLAB_RTCPOWERRAMP
