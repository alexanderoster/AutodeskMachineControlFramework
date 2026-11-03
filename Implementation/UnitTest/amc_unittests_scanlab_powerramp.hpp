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


#ifndef __AMCTEST_UNITTEST_SCANLAB_POWERRAMP
#define __AMCTEST_UNITTEST_SCANLAB_POWERRAMP


#include "amc_unittests.hpp"
#include "Drivers/ScanLab/Implementation/libmcdriver_scanlab_rtcpowerramp.hpp"

#include <cmath>
#include <vector>


namespace AMCUnitTest {

	class CUnitTestGroup_ScanLab_PowerRamp : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override {
			return "ScanLab_PowerRamp";
		}

		void registerTests() override {
			registerTest("Evaluate", "The ramp curve interpolates linearly between its knots", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ScanLab_PowerRamp::testEvaluate, this));
			registerTest("ConstantVector", "A constant value gives one sub-vector", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ScanLab_PowerRamp::testConstantVector, this));
			registerTest("LinearRamp", "A linear ramp is split into sub-vectors at their midpoint value", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ScanLab_PowerRamp::testLinearRamp, this));
			registerTest("SupportPoints", "Support points are sub-vector boundaries", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ScanLab_PowerRamp::testSupportPoints, this));
			registerTest("StepLimit", "Very long vectors are limited in sub-vector count", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ScanLab_PowerRamp::testStepLimit, this));
			registerTest("InvalidSupportPoints", "Support points out of order or at the ends are ignored", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ScanLab_PowerRamp::testInvalidSupportPoints, this));
			registerTest("RampList", "The ramp list stores the steps per element", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_ScanLab_PowerRamp::testRampList, this));
		}

		void initializeTests() override {
		}

	private:

		typedef LibMCDriver_ScanLab::Impl::CRTCPowerRamp CPowerRamp;
		typedef LibMCDriver_ScanLab::Impl::CRTCPowerRampList CPowerRampList;
		typedef LibMCDriver_ScanLab::Impl::sRTCPowerRampSupportPoint sSupportPoint;
		typedef LibMCDriver_ScanLab::Impl::sRTCPowerRampStep sStep;

		bool nearlyEqual(const double a, const double b, const double epsilon = 1.0e-9)
		{
			return std::abs(a - b) < epsilon;
		}

		// Steps must cover the vector from 0 to 1 without gaps.
		void assertStepsCoverVector(const std::vector<sStep>& steps)
		{
			assertTrue(!steps.empty(), "steps are not empty");
			assertTrue(nearlyEqual(steps.front().m_dStartParameter, 0.0), "steps start at 0");
			assertTrue(nearlyEqual(steps.back().m_dEndParameter, 1.0), "steps end at 1");
			for (size_t nIndex = 1; nIndex < steps.size(); nIndex++) {
				assertTrue(nearlyEqual(steps[nIndex - 1].m_dEndParameter, steps[nIndex].m_dStartParameter), "steps are contiguous");
				assertTrue(steps[nIndex].m_dEndParameter > steps[nIndex].m_dStartParameter, "steps have positive length");
			}
		}

		void testEvaluate()
		{
			assertTrue(nearlyEqual(CPowerRamp::evaluate(100.0, 200.0, nullptr, 0, 0.0), 100.0), "start");
			assertTrue(nearlyEqual(CPowerRamp::evaluate(100.0, 200.0, nullptr, 0, 1.0), 200.0), "end");
			assertTrue(nearlyEqual(CPowerRamp::evaluate(100.0, 200.0, nullptr, 0, 0.25), 125.0), "linear");

			// The example from the specification: f1=0.2, sub t=0.2 f=0.8, sub t=0.8 f=0.6, f2=1.0
			sSupportPoint supportPoints[2] = { { 0.2, 0.8 }, { 0.8, 0.6 } };
			assertTrue(nearlyEqual(CPowerRamp::evaluate(0.2, 1.0, supportPoints, 2, 0.1), 0.5), "first piece");
			assertTrue(nearlyEqual(CPowerRamp::evaluate(0.2, 1.0, supportPoints, 2, 0.2), 0.8), "first support point");
			assertTrue(nearlyEqual(CPowerRamp::evaluate(0.2, 1.0, supportPoints, 2, 0.5), 0.7), "middle piece");
			assertTrue(nearlyEqual(CPowerRamp::evaluate(0.2, 1.0, supportPoints, 2, 0.9), 0.8), "last piece");
		}

		void testConstantVector()
		{
			std::vector<sStep> steps;
			CPowerRamp::computeSteps(10.0, 150.0, 150.0, nullptr, 0, 0.1, 0.001, steps);
			assertStepsCoverVector(steps);
			assertTrue(steps.size() == 1, "one step");
			assertTrue(nearlyEqual(steps[0].m_dValue, 150.0), "constant value");

			// Support points on the constant value merge into the same step.
			sSupportPoint supportPoints[1] = { { 0.5, 150.0 } };
			CPowerRamp::computeSteps(10.0, 150.0, 150.0, supportPoints, 1, 0.1, 0.001, steps);
			assertTrue(steps.size() == 1, "one step with support point");
		}

		void testLinearRamp()
		{
			std::vector<sStep> steps;
			CPowerRamp::computeSteps(1.0, 100.0, 200.0, nullptr, 0, 0.1, 0.001, steps);
			assertStepsCoverVector(steps);
			assertTrue(steps.size() == 10, "1 mm in 0.1 mm steps");
			assertTrue(nearlyEqual(steps.front().m_dValue, 105.0), "first step at its midpoint");
			assertTrue(nearlyEqual(steps.back().m_dValue, 195.0), "last step at its midpoint");

			// The mean value of the sub-vectors equals the mean value of the ramp.
			double dWeightedSum = 0.0;
			for (auto& step : steps)
				dWeightedSum += step.m_dValue * (step.m_dEndParameter - step.m_dStartParameter);
			assertTrue(nearlyEqual(dWeightedSum, 150.0), "mean value");

			// Shorter vectors than the step length give one step at the mean value.
			CPowerRamp::computeSteps(0.05, 100.0, 200.0, nullptr, 0, 0.1, 0.001, steps);
			assertTrue(steps.size() == 1, "short vector");
			assertTrue(nearlyEqual(steps[0].m_dValue, 150.0), "short vector mean value");

			// Falling ramps work the same way.
			CPowerRamp::computeSteps(1.0, 200.0, 100.0, nullptr, 0, 0.1, 0.001, steps);
			assertTrue(steps.size() == 10, "falling ramp");
			assertTrue(nearlyEqual(steps.front().m_dValue, 195.0), "falling ramp first step");

			// Zero length vectors still get one step.
			CPowerRamp::computeSteps(0.0, 100.0, 200.0, nullptr, 0, 0.1, 0.001, steps);
			assertStepsCoverVector(steps);
			assertTrue(steps.size() == 1, "zero length vector");
		}

		void testSupportPoints()
		{
			std::vector<sStep> steps;
			sSupportPoint supportPoints[2] = { { 0.2, 300.0 }, { 0.8, 300.0 } };
			CPowerRamp::computeSteps(1.0, 100.0, 100.0, supportPoints, 2, 0.1, 0.001, steps);
			assertStepsCoverVector(steps);

			// 2 steps up to t=0.2, one constant step to t=0.8, 2 steps down to t=1.0
			assertTrue(steps.size() == 5, "step count");
			assertTrue(nearlyEqual(steps[1].m_dEndParameter, 0.2), "support point is a boundary");
			assertTrue(nearlyEqual(steps[2].m_dStartParameter, 0.2), "constant piece starts at the support point");
			assertTrue(nearlyEqual(steps[2].m_dEndParameter, 0.8), "constant piece ends at the support point");
			assertTrue(nearlyEqual(steps[2].m_dValue, 300.0), "constant piece value");
			assertTrue(nearlyEqual(steps[0].m_dValue, 150.0), "rising piece midpoint");
			assertTrue(nearlyEqual(steps[4].m_dValue, 150.0), "falling piece midpoint");
		}

		void testStepLimit()
		{
			std::vector<sStep> steps;
			CPowerRamp::computeSteps(10000.0, 0.0, 100.0, nullptr, 0, 0.1, 0.001, steps);
			assertStepsCoverVector(steps);
			assertTrue(steps.size() <= RTCPOWERRAMP_MAX_STEPSPERVECTOR, "step count is limited");

			CPowerRamp::computeSteps(1.0, 0.0, 100.0, nullptr, 0, 0.0, 0.001, steps);
			assertStepsCoverVector(steps);
			assertTrue(steps.size() == 1, "no step length gives one step");
		}

		void testInvalidSupportPoints()
		{
			std::vector<sStep> steps;
			sSupportPoint supportPoints[4] = { { 0.5, 300.0 }, { 0.4, 0.0 }, { 0.0, 0.0 }, { 1.0, 0.0 } };
			CPowerRamp::computeSteps(1.0, 100.0, 200.0, supportPoints, 4, 1.0, 0.001, steps);
			assertStepsCoverVector(steps);
			assertTrue(steps.size() == 2, "only the first support point is used");
			assertTrue(nearlyEqual(steps[0].m_dEndParameter, 0.5), "boundary at the valid support point");
			assertTrue(nearlyEqual(steps[0].m_dValue, 200.0), "rising piece midpoint");
			assertTrue(nearlyEqual(steps[1].m_dValue, 250.0), "falling piece midpoint");
			assertTrue(nearlyEqual(CPowerRamp::evaluate(100.0, 200.0, supportPoints, 4, 0.5), 300.0), "evaluate ignores them as well");
		}

		void testRampList()
		{
			CPowerRampList rampList;
			assertTrue(rampList.getElementCount() == 0, "empty list");

			std::vector<sStep> steps;
			CPowerRamp::computeSteps(1.0, 100.0, 200.0, nullptr, 0, 0.5, 0.001, steps);
			rampList.addElement(steps);
			rampList.addElement(std::vector<sStep>());
			CPowerRamp::computeSteps(1.0, 100.0, 100.0, nullptr, 0, 0.5, 0.001, steps);
			rampList.addElement(steps);

			assertTrue(rampList.getElementCount() == 3, "element count");
			assertTrue(rampList.getTotalStepCount() == 3, "total step count");

			size_t nStepCount = 0;
			auto pSteps = rampList.getElementSteps(0, nStepCount);
			assertTrue((pSteps != nullptr) && (nStepCount == 2), "first element");
			assertTrue(nearlyEqual(pSteps[1].m_dValue, 175.0), "first element values");

			pSteps = rampList.getElementSteps(1, nStepCount);
			assertTrue((pSteps == nullptr) && (nStepCount == 0), "empty element");

			pSteps = rampList.getElementSteps(2, nStepCount);
			assertTrue((pSteps != nullptr) && (nStepCount == 1), "constant element");

			pSteps = rampList.getElementSteps(3, nStepCount);
			assertTrue(pSteps == nullptr, "element out of range");

			rampList.clear();
			assertTrue(rampList.getElementCount() == 0, "cleared list");
		}

	};

}

#endif // __AMCTEST_UNITTEST_SCANLAB_POWERRAMP
