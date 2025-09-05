/*++

Copyright (C) 2025 Autodesk Inc.

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


Abstract: This is the class declaration of COpenFOAMCaseInstance

*/


#ifndef __LIBMCDRIVER_OPENFOAM_CASEINSTANCE
#define __LIBMCDRIVER_OPENFOAM_CASEINSTANCE

#include "libmcdriver_openfoam_interfaces.hpp"
#include "libmcdriver_openfoam_dictbuilder.hpp"

#include <map>
#include <vector>
#include <array>

namespace LibMCDriver_OpenFOAM {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of COpenFOAMCase 
**************************************************************************************************************************/

enum class eOpenFoamSurfaceType : uint32_t {
	ofstInvalid = 0,
	ofstInletPatch = 1,
	ofstOutletPatch = 2,
	ofstWall = 3
};


class COpenFOAMCaseSurfaceInstance {
private:
	std::string m_sIdentifier;
	std::string m_s3MFUUID;
	std::string m_sGroupName;
	eOpenFoamSurfaceType m_SurfaceType;
	std::array<double, 3> m_dFlowVelocity;

public:	

	COpenFOAMCaseSurfaceInstance(LibMCEnv::PXMLDocumentNode pXMLNode);
	
	virtual ~COpenFOAMCaseSurfaceInstance();

	std::string getIdentifier ();

	std::string getSTLFileName();

	std::string get3MFUUID ();

	std::string getGroupName ();

	eOpenFoamSurfaceType surfaceType();


};

typedef std::shared_ptr<COpenFOAMCaseSurfaceInstance> POpenFOAMCaseSurfaceInstance;

class COpenFOAMCaseDefinition {
private:

	double m_dGridSizeInMM;
	std::string m_sBuildItemUUID;

	double m_dTurbulentKE;
	double m_dTurbulentOmega;
	double m_dPressure;

	std::map<std::string, POpenFOAMCaseSurfaceInstance> m_SurfaceMap;
	std::vector<POpenFOAMCaseSurfaceInstance> m_Surfaces;

public:

	COpenFOAMCaseDefinition(LibMCEnv::PXMLDocumentNode pXMLNode);

	virtual ~COpenFOAMCaseDefinition();

	double getGridSizeInMM ();

	std::string getBuildItemUUID ();

	std::vector<POpenFOAMCaseSurfaceInstance> & getSurfaces();

	double getTurbulentKE ();
	double getTurbulentOmega ();
	double getPressure ();


};

typedef std::shared_ptr<COpenFOAMCaseDefinition> POpenFOAMCaseDefinition;


class COpenFOAMCaseInstance {
private:

	
	eCaseStatus m_Status;
	std::string m_sIdentifier;
	uint32_t m_nKeyCharLength;

	POpenFOAMCaseDefinition m_pCaseDefinition;
	std::string m_sCaseDefinitionXML;

	LibMCEnv::PDriverEnvironment m_pDriverEnvironment;

	LibMCEnv::PWorkingDirectory m_pOpenCaseDirectory;
	LibMCEnv::PWorkingDirectory m_pConstantDirectory;
	LibMCEnv::PWorkingDirectory m_pSystemDirectory;	
	LibMCEnv::PWorkingDirectory m_pTriSurfaceDirectory;
	LibMCEnv::PWorkingDirectory m_pInitialDirectory;

	LibMCEnv::PWorkingFile m_pBlockMeshDictFile;
	LibMCEnv::PWorkingFile m_pControlDictFile;
	LibMCEnv::PWorkingFile m_pDecomposeParDictFile;
	LibMCEnv::PWorkingFile m_pSnappyHexMeshDictFile;
	LibMCEnv::PWorkingFile m_pMeshQualityDictFile;
	LibMCEnv::PWorkingFile m_pFVSolutionFile;
	LibMCEnv::PWorkingFile m_pFVSchemesFile;
	LibMCEnv::PWorkingFile m_pTransportPropertiesFile;
	LibMCEnv::PWorkingFile m_pTurbulencePropertiesFile;

	LibMCEnv::PWorkingFile m_pInitialCondition_K;
	LibMCEnv::PWorkingFile m_pInitialCondition_Nut;
	LibMCEnv::PWorkingFile m_pInitialCondition_Omega;
	LibMCEnv::PWorkingFile m_pInitialCondition_P;
	LibMCEnv::PWorkingFile m_pInitialCondition_U;

	LibMCEnv::PWorkingFile m_pCaseFile;

	std::map<std::string, LibMCEnv::PWorkingFile> m_SurfaceASCIISTLs;


	std::string m_sBuildUUID;

	eOpenFOAMVersion m_OpenFOAMVersion;
	uint32_t m_nSTLWriteBufferSizeInKB;

	POpenFOAMDictBuilder createBlockMeshDict();
	POpenFOAMDictBuilder createControlDict();
	POpenFOAMDictBuilder createDecomposeParDict();
	POpenFOAMDictBuilder createSnappyHexMeshDict();
	POpenFOAMDictBuilder createMeshQualityDict();
	POpenFOAMDictBuilder createFVSolutionDict();
	POpenFOAMDictBuilder createFVSchemesDict();

	POpenFOAMDictBuilder createTransportPropertiesFile();
	POpenFOAMDictBuilder createTurbulencePropertiesFile();

	POpenFOAMDictBuilder createInitialCondition_K();
	POpenFOAMDictBuilder createInitialCondition_Nut();
	POpenFOAMDictBuilder createInitialCondition_Omega();
	POpenFOAMDictBuilder createInitialCondition_P();
	POpenFOAMDictBuilder createInitialCondition_U();

	void writeSurfaceAsASCIISTL(COpenFOAMCaseSurfaceInstance* pSurface, LibMCEnv::CWorkingFileWriter* pWriterInstance, double dUnitFactorPerMM);

	void computeDomainOutbox (double & dMinXInMM, double & dMinYInMM, double & dMinZInMM, double & dMaxXInMM, double & dMaxYInMM, double & dMaxZInMM);

	void computePointInDomainNotOnGrid (double& dXInMM, double& dYInMM, double& dZInMM);

public:

	COpenFOAMCaseInstance(const std::string & sIdentifier, LibMCEnv::PDriverEnvironment);

	virtual ~COpenFOAMCaseInstance();

	std::string getIdentifier();

	eCaseStatus getStatus ();

	void setBuild(LibMCEnv::PBuild pBuild);

	bool hasBuild();

	bool canRun();

	std::string getBuildUUID();

	void startComputation();

	void cancelComputation();

	void releaseCase();

	static bool checkIdentifier(const std::string& sIdentifier);

	static bool checkUUID(const std::string& sUUID);

	void createOpenFOAMInputDeck(LibMCEnv::PZIPStreamWriter pZIPStream);

};

typedef std::shared_ptr<COpenFOAMCaseInstance> POpenFOAMCaseInstance;

} // namespace Impl
} // namespace LibMCDriver_OpenFOAM

#endif // __LIBMCDRIVER_OPENFOAM_CASEINSTANCE
