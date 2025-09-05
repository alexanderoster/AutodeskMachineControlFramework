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


Abstract: This is a stub class definition of COpenFOAMCase

*/

#include "libmcdriver_openfoam_caseinstance.hpp"
#include "libmcdriver_openfoam_interfaceexception.hpp"
#include <cmath>


using namespace LibMCDriver_OpenFOAM::Impl;

#define OPENFOAM_MAXIDENTIFIER_LENGTH 128
#define OPENFOAM_GRIDSIZE_MIN 0.001
#define OPENFOAM_GRIDSIZE_MAX 1000.0

#define SCHEMA_OPENFOAMDEFINITION_1_0 "https://schemas.autodesk.com/amc/openfoamdefinition-1.0"
#define SCHEMA_OPENFOAMRELATIONSHIP_1_0 "https://schemas.autodesk.com/amc/openfoamrelationship-1.0"


COpenFOAMCaseSurfaceInstance::COpenFOAMCaseSurfaceInstance(LibMCEnv::PXMLDocumentNode pXMLNode)
    : m_SurfaceType(eOpenFoamSurfaceType::ofstInvalid), m_dFlowVelocity({ 0.0, 0.0, 0.0 })
{
    if (pXMLNode.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);
    if (pXMLNode->GetNameSpace() != SCHEMA_OPENFOAMDEFINITION_1_0)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDOPENFOAMDEFINITION, pXMLNode->GetNameSpace());

    m_sIdentifier = pXMLNode->GetAttributeValueDef("", "identifier", "");
    m_s3MFUUID = pXMLNode->GetAttributeValueDef("", "uuid", "");
    m_sGroupName = pXMLNode->GetAttributeValueDef("", "group", "");
    std::string sSurfaceType = pXMLNode->GetAttributeValueDef("", "type", "");

    if (m_sIdentifier.empty ())
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_MISSINGSURFACEIDENTIFIER);
    if (m_s3MFUUID.empty())
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_MISSINGSURFACEUUID);

    if (!COpenFOAMCaseInstance::checkIdentifier (m_sIdentifier))
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDSURFACEIDENTIFIER, m_sIdentifier);
    if (!COpenFOAMCaseInstance::checkUUID(m_s3MFUUID))
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDSURFACEUUID, m_sIdentifier);

    if (!m_sGroupName.empty()) { 
        // Groups are optional, but must match the identifier guidelines
        if (!COpenFOAMCaseInstance::checkIdentifier(m_sGroupName))
            throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDSURFACEGROUP, m_sIdentifier);
    }

    if (sSurfaceType.empty())
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_MISSINGSURFACETYPE, m_sIdentifier);

    if (sSurfaceType == "inlet") { 
        m_SurfaceType = eOpenFoamSurfaceType::ofstInletPatch;
    } else if (sSurfaceType == "outlet") {
        m_SurfaceType = eOpenFoamSurfaceType::ofstOutletPatch;
    } else if (sSurfaceType == "wall") {
        m_SurfaceType = eOpenFoamSurfaceType::ofstWall;
    }
    else {
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDSURFACETYPE, m_sIdentifier);
    }

}

COpenFOAMCaseSurfaceInstance::~COpenFOAMCaseSurfaceInstance()
{

}

std::string COpenFOAMCaseSurfaceInstance::getIdentifier()
{
    return m_sIdentifier;
}

std::string COpenFOAMCaseSurfaceInstance::getSTLFileName()
{
    return m_sIdentifier + ".stl";
}

std::string COpenFOAMCaseSurfaceInstance::get3MFUUID()
{
    return m_s3MFUUID;
}

std::string COpenFOAMCaseSurfaceInstance::getGroupName()
{
    return m_sGroupName;

}

eOpenFoamSurfaceType COpenFOAMCaseSurfaceInstance::surfaceType()
{
    return m_SurfaceType;

}



COpenFOAMCaseDefinition::COpenFOAMCaseDefinition(LibMCEnv::PXMLDocumentNode pXMLNode)
{
    if (pXMLNode.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);

    if (pXMLNode->GetNameSpace () != SCHEMA_OPENFOAMDEFINITION_1_0)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDOPENFOAMDEFINITION, pXMLNode->GetNameSpace ());

    auto pMeshingNode = pXMLNode->FindChild("", "meshing", true);
    m_dGridSizeInMM = pMeshingNode->GetAttributeDoubleValue("", "gridsize", OPENFOAM_GRIDSIZE_MIN, OPENFOAM_GRIDSIZE_MAX);
    m_sBuildItemUUID = pMeshingNode->GetAttributeUUIDValue("", "builditem");

    auto pSurfacesNode = pXMLNode->FindChild("", "surfaces", true);
    auto pSurfaceNodes = pSurfacesNode->GetChildrenByName("", "surface");
    uint64_t nNodeCount = pSurfaceNodes->GetNodeCount();
    for (uint64_t nNodeIndex = 0; nNodeIndex < nNodeCount; nNodeIndex++) {

        auto pSurfaceNode = pSurfaceNodes->GetNode(nNodeIndex);
        auto pSurfaceInstance = std::make_shared<COpenFOAMCaseSurfaceInstance>(pSurfaceNode);

        std::string sIdentifier = pSurfaceInstance->getIdentifier();

        auto iSurfaceIter = m_SurfaceMap.find(sIdentifier);
        if (iSurfaceIter != m_SurfaceMap.end ())
            throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_DUPLICATEOPENFOAMSURFACE, sIdentifier);

        m_SurfaceMap.insert(std::make_pair (sIdentifier, pSurfaceInstance));
        m_Surfaces.push_back(pSurfaceInstance);
    }

	m_dTurbulentKE = 0.24; 
    m_dTurbulentOmega = 1.78;
    m_dPressure = 0.0;


}

COpenFOAMCaseDefinition::~COpenFOAMCaseDefinition()
{

}

double COpenFOAMCaseDefinition::getGridSizeInMM()
{
    return m_dGridSizeInMM;
}

std::string COpenFOAMCaseDefinition::getBuildItemUUID()
{
    return m_sBuildItemUUID;
}

std::vector<POpenFOAMCaseSurfaceInstance>& COpenFOAMCaseDefinition::getSurfaces()
{
    return m_Surfaces;
}

double COpenFOAMCaseDefinition::getTurbulentKE()
{
    return m_dTurbulentKE;
}

double COpenFOAMCaseDefinition::getTurbulentOmega()
{
    return m_dTurbulentOmega;
}

double COpenFOAMCaseDefinition::getPressure()
{
    return m_dPressure;
}


COpenFOAMCaseInstance::COpenFOAMCaseInstance(const std::string& sIdentifier, LibMCEnv::PDriverEnvironment pDriverEnvironment)
    : m_sIdentifier (sIdentifier), m_pDriverEnvironment (pDriverEnvironment), 
    m_sBuildUUID ("00000000-0000-0000-0000-000000000000"),
    m_Status (eCaseStatus::InPreparation),
    m_nKeyCharLength (32),
    m_nSTLWriteBufferSizeInKB (1024*32)
{
    if (pDriverEnvironment.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);

    if (!checkIdentifier(sIdentifier))
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDCASEIDENTIFIER, sIdentifier);

    m_OpenFOAMVersion = eOpenFOAMVersion::ofv2406;

}

COpenFOAMCaseInstance::~COpenFOAMCaseInstance()
{

}

std::string COpenFOAMCaseInstance::getIdentifier()
{ 
    return m_sIdentifier;
}


LibMCDriver_OpenFOAM::eCaseStatus COpenFOAMCaseInstance::getStatus()
{
    return m_Status;
}

void COpenFOAMCaseInstance::setBuild(LibMCEnv::PBuild pBuild)
{
	if (pBuild.get() == nullptr)
		throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);

    pBuild->LoadToolpath();

    auto pToolpath = pBuild->CreateToolpathAccessor();

    uint32_t nPartCount = pToolpath->GetPartCount();
    for (uint32_t nPartIndex = 0; nPartIndex < nPartCount; nPartIndex++) {
        auto pPart = pToolpath->GetPart(nPartIndex);
        std::string sName = pPart->GetName();
        m_pDriverEnvironment->LogMessage("Part #" + std::to_string (nPartIndex) + ": " + sName);
    }

    if (!pToolpath->HasBinaryMetaDataSchema(SCHEMA_OPENFOAMRELATIONSHIP_1_0))
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_OPENFOAMDEFINITIONMISSINGINBUILD, pBuild->GetBuildUUID ());

    m_sCaseDefinitionXML = pToolpath->GetBinaryMetaDataAsStringBySchema(SCHEMA_OPENFOAMRELATIONSHIP_1_0);
    auto pXMLDocument = m_pDriverEnvironment->ParseXMLString(m_sCaseDefinitionXML);
    
    m_pCaseDefinition = std::make_shared<COpenFOAMCaseDefinition>(pXMLDocument->GetRootNode());

    // Ensure that toolpath part exists
    pToolpath->FindPartByUUID(m_pCaseDefinition->getBuildItemUUID ());
    
    m_sBuildUUID = pBuild->GetBuildUUID();

    

}

bool COpenFOAMCaseInstance::hasBuild()
{
    return m_sBuildUUID != "00000000-0000-0000-0000-000000000000";
}

std::string COpenFOAMCaseInstance::getBuildUUID()
{
    return m_sBuildUUID;
}

bool COpenFOAMCaseInstance::canRun()
{
    if (m_Status == eCaseStatus::InPreparation) {

        // TODO: Add More checks

        return hasBuild();
    }

    return false;
}



POpenFOAMDictBuilder COpenFOAMCaseInstance::createBlockMeshDict()
{

    if (m_pCaseDefinition.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEDEFINITIONNOTINITIALIZED);

    double dBlockSizeInMM = m_pCaseDefinition->getGridSizeInMM();
    double dBlockSizeInM = dBlockSizeInMM * 0.001;

    double dOutboxMinXinMM, dOutboxMinYinMM, dOutboxMinZinMM;
    double dOutboxMaxXinMM, dOutboxMaxYinMM, dOutboxMaxZinMM;

    computeDomainOutbox (dOutboxMinXinMM, dOutboxMinYinMM, dOutboxMinZinMM, dOutboxMaxXinMM, dOutboxMaxYinMM, dOutboxMaxZinMM);

    // Add One block border around the outbox..
    int64_t nMinXInBlocks = (int64_t)floor(dOutboxMinXinMM / dBlockSizeInMM) - 1;
    int64_t nMinYInBlocks = (int64_t)floor(dOutboxMinYinMM / dBlockSizeInMM) - 1;
    int64_t nMinZInBlocks = (int64_t)floor(dOutboxMinZinMM / dBlockSizeInMM) - 1;
    int64_t nMaxXInBlocks = (int64_t)ceil(dOutboxMaxXinMM / dBlockSizeInMM) + 1;
    int64_t nMaxYInBlocks = (int64_t)ceil(dOutboxMaxYinMM / dBlockSizeInMM) + 1;
    int64_t nMaxZInBlocks = (int64_t)ceil(dOutboxMaxZinMM / dBlockSizeInMM) + 1;

    if ((nMinXInBlocks >= nMaxXInBlocks) || (nMinYInBlocks >= nMaxYInBlocks) || (nMinZInBlocks >= nMaxZInBlocks))
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDBLOCKMESHOUTBOX, "invalid block mesh outbox: " + std::to_string (dOutboxMinXinMM) + "/" + std::to_string(dOutboxMinYinMM) + "/" + std::to_string(dOutboxMinZinMM) + " - " + std::to_string(dOutboxMaxXinMM) + "/" + std::to_string(dOutboxMaxYinMM) + "/" + std::to_string(dOutboxMaxZinMM));

    double dMinX = nMinXInBlocks * dBlockSizeInM;
    double dMinY = nMinYInBlocks * dBlockSizeInM;
    double dMinZ = nMinZInBlocks * dBlockSizeInM;;
    double dMaxX = nMaxXInBlocks * dBlockSizeInM;;
    double dMaxY = nMaxYInBlocks * dBlockSizeInM;;
    double dMaxZ = nMaxZInBlocks * dBlockSizeInM;;

    int64_t nBlocksInX = nMaxXInBlocks - nMinXInBlocks;
    int64_t nBlocksInY = nMaxYInBlocks - nMinYInBlocks;
    int64_t nBlocksInZ = nMaxZInBlocks - nMinZInBlocks;

    auto blockMeshDict = std::make_shared<COpenFOAMDictBuilder> ("blockMeshDict", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdDictionary);
    blockMeshDict->writeDouble("scale", 1.0);
    blockMeshDict->beginEnumBlock("vertices");

    blockMeshDict->writeVec3dEnum("", dMinX, dMinY, dMinZ);
    blockMeshDict->writeVec3dEnum("", dMaxX, dMinY, dMinZ);
    blockMeshDict->writeVec3dEnum("", dMaxX, dMaxY, dMinZ);
    blockMeshDict->writeVec3dEnum("", dMinX, dMaxY, dMinZ);
    blockMeshDict->writeVec3dEnum("", dMinX, dMinY, dMaxZ);
    blockMeshDict->writeVec3dEnum("", dMaxX, dMinY, dMaxZ);
    blockMeshDict->writeVec3dEnum("", dMaxX, dMaxY, dMaxZ);
    blockMeshDict->writeVec3dEnum("", dMinX, dMaxY, dMaxZ);

    blockMeshDict->endEnumBlock();

    blockMeshDict->beginEnumBlock("blocks");
    blockMeshDict->writeHexBlock(0, 1, 2, 3, 4, 5, 6, 7, (uint32_t)nBlocksInX, (uint32_t)nBlocksInY, (uint32_t)nBlocksInZ);
    blockMeshDict->endEnumBlock();

    blockMeshDict->beginEnumBlock("edges");
    blockMeshDict->endEnumBlock();

    blockMeshDict->beginEnumBlock("boundary");
    blockMeshDict->beginBlock("domain");
    blockMeshDict->writeString("type", "patch");

    blockMeshDict->beginEnumBlock("faces");
    blockMeshDict->writeVec4i("", 3, 7, 6, 2);
    blockMeshDict->writeVec4i("", 0, 4, 7, 3);
    blockMeshDict->writeVec4i("", 2, 6, 5, 1);
    blockMeshDict->writeVec4i("", 1, 5, 4, 0);
    blockMeshDict->writeVec4i("", 0, 3, 2, 1);
    blockMeshDict->writeVec4i("", 4, 5, 6, 7);
    blockMeshDict->endEnumBlock();
    blockMeshDict->endBlock();
    blockMeshDict->endEnumBlock();

    return blockMeshDict;

}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createControlDict()
{

    uint32_t nEndTime = 200;
    uint32_t nWriteInterval = nEndTime;
    uint32_t nWritePrecision = 6;
    uint32_t nTimePrecision = 6;

    auto controlDict = std::make_shared<COpenFOAMDictBuilder> ("controlDict", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdDictionary);
    controlDict->writeString("application", "simpleFoam");
    controlDict->writeString("startFrom", "startTime");
    controlDict->writeInteger("startTime", 0);
    controlDict->writeString("stopAt", "endTime");
    controlDict->writeInteger("endTime", nEndTime);
    controlDict->writeInteger("deltaT", 1);
    controlDict->writeString("writeControl", "timeStep");
    controlDict->writeInteger("writeInterval", nWriteInterval);
    controlDict->writeInteger("purgeWrite", 0);
    controlDict->writeString("writeFormat", "binary");
    controlDict->writeInteger("writePrecision", nWritePrecision);
    controlDict->writeString("writeCompression", "off");
    controlDict->writeString("timeFormat", "general");
    controlDict->writeInteger("timePrecision", nTimePrecision);
    controlDict->writeString("runTimeModifiable", "true");

    return controlDict;
}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createDecomposeParDict()
{
    auto decomposeParDict = std::make_shared<COpenFOAMDictBuilder> ("decomposeParDict", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdDictionary);
    decomposeParDict->writeInteger("numberOfSubdomains", 6);
    decomposeParDict->writeString("method", "hierarchical");

    decomposeParDict->beginBlock("coeffs");
    decomposeParDict->writeVec3i("n", 3, 2, 1);
    decomposeParDict->endBlock();

    return decomposeParDict;
}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createSnappyHexMeshDict()
{
    if (m_pCaseDefinition.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEDEFINITIONNOTINITIALIZED);

    auto snappyHexMeshDict = std::make_shared<COpenFOAMDictBuilder> ("snappyHexMeshDict", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdDictionary);

    snappyHexMeshDict->writeBool("castellatedMesh", true);
    snappyHexMeshDict->writeBool("snap", true);
    snappyHexMeshDict->writeBool("addLayers", false);

    snappyHexMeshDict->beginBlock("geometry");

    auto & surfaces = m_pCaseDefinition->getSurfaces();
    for (auto pSurface : surfaces) {

        snappyHexMeshDict->beginBlock(pSurface->getSTLFileName());
        snappyHexMeshDict->writeString("type", "triSurfaceMesh");
        snappyHexMeshDict->writeString("name", pSurface->getIdentifier());
        snappyHexMeshDict->endBlock();

    }

    snappyHexMeshDict->endBlock();

    snappyHexMeshDict->beginBlock("castellatedMeshControls");
    snappyHexMeshDict->writeInteger("maxLocalCells", 100000);
    snappyHexMeshDict->writeInteger("maxGlobalCells", 2000000);
    snappyHexMeshDict->writeInteger("minRefinementCells", 10);
    snappyHexMeshDict->writeDouble("maxLoadUnbalance", 0.1);
    snappyHexMeshDict->writeInteger("nCellsBetweenLevels", 3);

    snappyHexMeshDict->beginEnumBlock("features");
    snappyHexMeshDict->endEnumBlock();

    snappyHexMeshDict->beginBlock("refinementSurfaces");

    for (auto pSurface : surfaces) {

        snappyHexMeshDict->beginBlock(pSurface->getIdentifier());
        snappyHexMeshDict->writeString("", "level (1 2)");
        snappyHexMeshDict->beginBlock("patchInfo");

        switch (pSurface->surfaceType()) {
            case eOpenFoamSurfaceType::ofstInletPatch:
                snappyHexMeshDict->writeString("type", "patch");
                break;
            case eOpenFoamSurfaceType::ofstOutletPatch:
                snappyHexMeshDict->writeString("type", "patch");
                break;
            case eOpenFoamSurfaceType::ofstWall:
                snappyHexMeshDict->writeString("type", "wall");
                break;

            default:
                throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);

        }


        std::string sGroupName = pSurface->getGroupName();
        if (!sGroupName.empty ())
            snappyHexMeshDict->writeString("", "inGroups(" + sGroupName + ")");

        snappyHexMeshDict->endBlock();
        snappyHexMeshDict->endBlock();

    }

    snappyHexMeshDict->endBlock();

    snappyHexMeshDict->writeDouble("resolveFeatureAngle", 30.0);

    snappyHexMeshDict->beginBlock("refinementRegions");
    snappyHexMeshDict->endBlock();

    double dPointInMeshXInMM = 0.0;
    double dPointInMeshYInMM = 0.0;
    double dPointInMeshZInMM = 0.0;
    computePointInDomainNotOnGrid(dPointInMeshXInMM, dPointInMeshYInMM, dPointInMeshZInMM);

    double dPointInMeshXInM = dPointInMeshXInMM * 0.001;
    double dPointInMeshYInM = dPointInMeshYInMM * 0.001;
    double dPointInMeshZInM = dPointInMeshZInMM * 0.001;
    snappyHexMeshDict->writeVec3dValue("locationInMesh", dPointInMeshXInM, dPointInMeshYInM, dPointInMeshZInM);
    snappyHexMeshDict->writeBool("allowFreeStandingZoneFaces", true);

    snappyHexMeshDict->endBlock();

    snappyHexMeshDict->beginBlock("snapControls");
    snappyHexMeshDict->writeInteger("nSmoothPatch", 3);
    snappyHexMeshDict->writeDouble("tolerance", 2.0);
    snappyHexMeshDict->writeInteger("nSolveIter", 30);
    snappyHexMeshDict->writeInteger("nRelaxIter", 5);
    snappyHexMeshDict->writeInteger("nFeatureSnapIter", 10);
    snappyHexMeshDict->writeBool("implicitFeatureSnap", false);
    snappyHexMeshDict->writeBool("explicitFeatureSnap", true);
    snappyHexMeshDict->writeBool("multiRegionFeatureSnap", false);
    snappyHexMeshDict->endBlock();

    snappyHexMeshDict->beginBlock("addLayersControls");
    snappyHexMeshDict->endBlock();
    
    snappyHexMeshDict->beginBlock("meshQualityControls");
    snappyHexMeshDict->writeInclude("meshQualityDict");
    snappyHexMeshDict->writeInteger("nSmoothScale", 4);
    snappyHexMeshDict->writeDouble("errorReduction", 0.75);
    snappyHexMeshDict->endBlock();

    snappyHexMeshDict->beginEnumBlock("writeFlags");
    snappyHexMeshDict->writeEnumString("scalarLevels");
    snappyHexMeshDict->writeEnumString("layerSets");
    snappyHexMeshDict->writeEnumString("layerFields");
    snappyHexMeshDict->endEnumBlock();

    snappyHexMeshDict->writeDouble("mergeTolerance", 1e-6);


    return snappyHexMeshDict;
}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createMeshQualityDict()
{
    auto meshQualityDict = std::make_shared<COpenFOAMDictBuilder>("meshQualityDict", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdDictionary);
    meshQualityDict->writeIncludeEtc("caseDicts/meshQualityDict");
    meshQualityDict->writeDouble("minFaceWeight", 0.02);

    return meshQualityDict;
}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createFVSolutionDict()
{
    auto fvSolutionDict = std::make_shared<COpenFOAMDictBuilder> ("fvSolution", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdDictionary);

    fvSolutionDict->beginBlock("solvers");
    
    fvSolutionDict->beginBlock("p");
    fvSolutionDict->writeString("solver", "GAMG");
    fvSolutionDict->writeString("smoother", "GaussSeidel");
    fvSolutionDict->writeDouble("tolerance", 1e-7);
    fvSolutionDict->writeDouble("relTol", 0.01);
    fvSolutionDict->endBlock();

    fvSolutionDict->beginBlock("Phi");
    fvSolutionDict->writeString("", "$p");
    fvSolutionDict->endBlock();

    fvSolutionDict->beginBlock("U");
    fvSolutionDict->writeString("solver", "smoothSolver");
    fvSolutionDict->writeString("smoother", "GaussSeidel");
    fvSolutionDict->writeDouble("tolerance", 1e-8);
    fvSolutionDict->writeDouble("relTol", 0.1);
    fvSolutionDict->writeInteger("nSweeps", 1);
    fvSolutionDict->endBlock();

    fvSolutionDict->beginBlock("k");
    fvSolutionDict->writeString("solver", "smoothSolver");
    fvSolutionDict->writeString("smoother", "GaussSeidel");
    fvSolutionDict->writeDouble("tolerance", 1e-8);
    fvSolutionDict->writeDouble("relTol", 0.1);
    fvSolutionDict->writeInteger("nSweeps", 1);
    fvSolutionDict->endBlock();

    fvSolutionDict->beginBlock("omega");
    fvSolutionDict->writeString("solver", "smoothSolver");
    fvSolutionDict->writeString("smoother", "GaussSeidel");
    fvSolutionDict->writeDouble("tolerance", 1e-8);
    fvSolutionDict->writeDouble("relTol", 0.1);
    fvSolutionDict->writeInteger("nSweeps", 1);
    fvSolutionDict->endBlock();

    fvSolutionDict->endBlock();

    fvSolutionDict->beginBlock("SIMPLE");
    fvSolutionDict->writeInteger("nNonOrthogonalCorrectors", 0);
    fvSolutionDict->writeString("consistent", "yes");
    fvSolutionDict->endBlock();

    fvSolutionDict->beginBlock("potentialFlow");
    fvSolutionDict->writeInteger("nNonOrthogonalCorrectors", 10);
    fvSolutionDict->endBlock();


    fvSolutionDict->beginBlock("relaxationFactors");
    fvSolutionDict->beginBlock("equations");
    fvSolutionDict->writeDouble("U", 0.9);
    fvSolutionDict->writeDouble("k", 0.7);
    fvSolutionDict->writeDouble("omega", 0.7);
    fvSolutionDict->endBlock();
    fvSolutionDict->endBlock();
        
    fvSolutionDict->beginBlock("cache");
    fvSolutionDict->writeString("", "grad(U)");
    fvSolutionDict->endBlock();

    return fvSolutionDict;

}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createFVSchemesDict()
{
    auto fvSchemesDict = std::make_shared<COpenFOAMDictBuilder> ("fvSchemes", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdDictionary);

    fvSchemesDict->beginBlock("ddtSchemes");
    fvSchemesDict->writeString("default", "steadyState");
    fvSchemesDict->endBlock();

    fvSchemesDict->beginBlock("gradSchemes");
    fvSchemesDict->writeString("default", "Gauss linear");
    fvSchemesDict->writeString("grad(U)", "cellLimited Gauss linear 1");
    fvSchemesDict->endBlock();

    fvSchemesDict->beginBlock("divSchemes");
    fvSchemesDict->writeString("default", "none");
    fvSchemesDict->writeString("div(phi,U)", "bounded Gauss linearUpwindV grad(U)");
    fvSchemesDict->writeString("turbulence", "bounded Gauss upwind");
    fvSchemesDict->writeString("div(phi,k)", "$turbulence");
    fvSchemesDict->writeString("div(phi,omega)", "$turbulence");
    fvSchemesDict->writeString("div((nuEff*dev2(T(grad(U)))))", "Gauss linear");
    fvSchemesDict->endBlock();

    fvSchemesDict->beginBlock("laplacianSchemes");
    fvSchemesDict->writeString("default", "Gauss linear corrected");
    fvSchemesDict->endBlock();

    fvSchemesDict->beginBlock("interpolationSchemes");
    fvSchemesDict->writeString("default", "linear");
    fvSchemesDict->endBlock();

    fvSchemesDict->beginBlock("snGradSchemes");
    fvSchemesDict->writeString("default", "corrected");
    fvSchemesDict->endBlock();

    fvSchemesDict->beginBlock("wallDist");
    fvSchemesDict->writeString("method", "meshWave");
    fvSchemesDict->endBlock();

    return fvSchemesDict;

}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createTransportPropertiesFile()
{
    auto transportProperties = std::make_shared<COpenFOAMDictBuilder>("transportProperties", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdDictionary);

    transportProperties->writeString("transportModel", "Newtonian");
    transportProperties->writeDouble("nu", 1.5e-05);

    return transportProperties;

}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createTurbulencePropertiesFile()
{
    auto turbulenceProperties = std::make_shared<COpenFOAMDictBuilder>("turbulenceProperties", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdDictionary);

    turbulenceProperties->writeString("simulationType", "RAS");
    turbulenceProperties->beginBlock("RAS");
    turbulenceProperties->writeString("RASModel", "kOmegaSST");
    turbulenceProperties->writeString("turbulence", "on");
    turbulenceProperties->writeString("printCoeffs", "on");
    turbulenceProperties->endBlock();

    return turbulenceProperties;

}


POpenFOAMDictBuilder COpenFOAMCaseInstance::createInitialCondition_K()
{
    if (m_pCaseDefinition.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEDEFINITIONNOTINITIALIZED);

    auto initialConditionK = std::make_shared<COpenFOAMDictBuilder>("k", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdVolScalarField);

    initialConditionK->writeString("dimensions", "[0 2 -2 0 0 0 0]");
    initialConditionK->writeString("internalField", "uniform " + std::to_string (m_pCaseDefinition->getTurbulentKE ()));

	initialConditionK->beginBlock("boundaryField");
    initialConditionK->writeIncludeEtc("caseDicts/setConstraintTypes");

    auto& surfaces = m_pCaseDefinition->getSurfaces();

    for (auto pSurface : surfaces) {
        switch (pSurface->surfaceType()) {
		case eOpenFoamSurfaceType::ofstInletPatch: {
			initialConditionK->beginBlock(pSurface->getIdentifier());
			initialConditionK->writeString("type", "fixedValue");
			initialConditionK->writeString("value", "$internalField");
			initialConditionK->endBlock();
			break;
		}
		case eOpenFoamSurfaceType::ofstOutletPatch: {
			initialConditionK->beginBlock(pSurface->getIdentifier());
			initialConditionK->writeString("type", "inletOutlet");
            initialConditionK->writeString("inletValue", "$internalField");
            initialConditionK->writeString("value", "$internalField");
            initialConditionK->endBlock();
            break;
        }
		case eOpenFoamSurfaceType::ofstWall: {
			initialConditionK->beginBlock(pSurface->getIdentifier());
			initialConditionK->writeString("type", "kqRWallFunction");
            initialConditionK->writeString("value", "$internalField");
            initialConditionK->endBlock();
			break;
		}
		default:
			throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);
        }

    }

    initialConditionK->endBlock();

    return initialConditionK;

}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createInitialCondition_Nut()
{
    if (m_pCaseDefinition.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEDEFINITIONNOTINITIALIZED);

    auto initialConditionNut = std::make_shared<COpenFOAMDictBuilder>("nut", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdVolScalarField);

    initialConditionNut->writeString("dimensions", "[0 2 -1 0 0 0 0]");
    initialConditionNut->writeString("internalField", "uniform 0");

    initialConditionNut->beginBlock("boundaryField");
    initialConditionNut->writeIncludeEtc("caseDicts/setConstraintTypes");

    auto& surfaces = m_pCaseDefinition->getSurfaces();

    for (auto pSurface : surfaces) {
        switch (pSurface->surfaceType()) {
        case eOpenFoamSurfaceType::ofstInletPatch: {
            initialConditionNut->beginBlock(pSurface->getIdentifier());
            initialConditionNut->writeString("type", "calculated");
            initialConditionNut->writeString("value", "uniform 0");
            initialConditionNut->endBlock();
            break;
        }
        case eOpenFoamSurfaceType::ofstOutletPatch: {
            initialConditionNut->beginBlock(pSurface->getIdentifier());
            initialConditionNut->writeString("type", "calculated");
            initialConditionNut->writeString("value", "uniform 0");
            initialConditionNut->endBlock();
            break;
        }
        case eOpenFoamSurfaceType::ofstWall: {
            initialConditionNut->beginBlock(pSurface->getIdentifier());
            initialConditionNut->writeString("type", "nutkWallFunction");
            initialConditionNut->writeString("value", "uniform 0");
            initialConditionNut->endBlock();
            break;
        }
        default:
            throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);
        }

    }

    initialConditionNut->endBlock();

    return initialConditionNut;

}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createInitialCondition_Omega()
{
    if (m_pCaseDefinition.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEDEFINITIONNOTINITIALIZED);

    auto initialConditionOmega = std::make_shared<COpenFOAMDictBuilder>("omega", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdVolScalarField);

    initialConditionOmega->writeString("dimensions", "[0 0 -1 0 0 0 0]");
    initialConditionOmega->writeString("internalField", "uniform " + std::to_string(m_pCaseDefinition->getTurbulentOmega()));

    initialConditionOmega->beginBlock("boundaryField");
    initialConditionOmega->writeIncludeEtc("caseDicts/setConstraintTypes");

    auto& surfaces = m_pCaseDefinition->getSurfaces();

    for (auto pSurface : surfaces) {
        switch (pSurface->surfaceType()) {
        case eOpenFoamSurfaceType::ofstInletPatch: {
            initialConditionOmega->beginBlock(pSurface->getIdentifier());
            initialConditionOmega->writeString("type", "fixedValue");
            initialConditionOmega->writeString("value", "$internalField");
            initialConditionOmega->endBlock();
            break;
        }
        case eOpenFoamSurfaceType::ofstOutletPatch: {
            initialConditionOmega->beginBlock(pSurface->getIdentifier());
            initialConditionOmega->writeString("type", "inletOutlet");
            initialConditionOmega->writeString("inletValue", "$internalField");
            initialConditionOmega->writeString("value", "$internalField");
            initialConditionOmega->endBlock();
            break;
        }
        case eOpenFoamSurfaceType::ofstWall: {
            initialConditionOmega->beginBlock(pSurface->getIdentifier());
            initialConditionOmega->writeString("type", "omegaWallFunction");
            initialConditionOmega->writeString("value", "uniform " + std::to_string(m_pCaseDefinition->getTurbulentOmega()));
            initialConditionOmega->endBlock();
            break;
        }
        default:
            throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);
        }

    }

    initialConditionOmega->endBlock();

    return initialConditionOmega;


}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createInitialCondition_P()
{
    if (m_pCaseDefinition.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEDEFINITIONNOTINITIALIZED);

    auto initialConditionP = std::make_shared<COpenFOAMDictBuilder>("p", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdVolScalarField);

    initialConditionP->writeString("dimensions", "[0 2 -2 0 0 0 0]");
    initialConditionP->writeString("internalField", "uniform " + std::to_string (m_pCaseDefinition->getPressure ()));

    initialConditionP->beginBlock("boundaryField");
    initialConditionP->writeIncludeEtc("caseDicts/setConstraintTypes");

    auto& surfaces = m_pCaseDefinition->getSurfaces();

    for (auto pSurface : surfaces) {
        switch (pSurface->surfaceType()) {
        case eOpenFoamSurfaceType::ofstInletPatch: {
            initialConditionP->beginBlock(pSurface->getIdentifier());
            initialConditionP->writeString("type", "zeroGradient");
            initialConditionP->endBlock();
            break;
        }
        case eOpenFoamSurfaceType::ofstOutletPatch: {
            initialConditionP->beginBlock(pSurface->getIdentifier());
            initialConditionP->writeString("type", "fixedValue");
            initialConditionP->writeString("value", "$internalField");
            initialConditionP->endBlock();
            break;
        }
        case eOpenFoamSurfaceType::ofstWall: {
            initialConditionP->beginBlock(pSurface->getIdentifier());
            initialConditionP->writeString("type", "zeroGradient");
            initialConditionP->endBlock();
            break;
        }
        default:
            throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);
        }

    }

    initialConditionP->endBlock();

    return initialConditionP;

}

POpenFOAMDictBuilder COpenFOAMCaseInstance::createInitialCondition_U()
{
    if (m_pCaseDefinition.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEDEFINITIONNOTINITIALIZED);

    auto initialConditionU = std::make_shared<COpenFOAMDictBuilder>("U", m_OpenFOAMVersion, m_nKeyCharLength, eOpenFOAMDictType::ofdVolVectorField);

    initialConditionU->writeString("dimensions", "[0 1 -1 0 0 0 0]");
    initialConditionU->writeString("internalField", "uniform (0 0 0)");

    initialConditionU->beginBlock("boundaryField");
    initialConditionU->writeIncludeEtc("caseDicts/setConstraintTypes");

    auto& surfaces = m_pCaseDefinition->getSurfaces();

    for (auto pSurface : surfaces) {
        switch (pSurface->surfaceType()) {
        case eOpenFoamSurfaceType::ofstInletPatch: {
            initialConditionU->beginBlock(pSurface->getIdentifier());
            initialConditionU->writeString("type", "fixedValue");
            initialConditionU->writeString("value", "uniform (-1 0 0)");
            initialConditionU->endBlock();
            break;
        }
        case eOpenFoamSurfaceType::ofstOutletPatch: {
            initialConditionU->beginBlock(pSurface->getIdentifier());
            initialConditionU->writeString("type", "inletOutlet");
            initialConditionU->writeString("inletValue", "uniform (0 0 0)");
            initialConditionU->writeString("value", "$internalField");
            initialConditionU->endBlock();
            break;
        }
        case eOpenFoamSurfaceType::ofstWall: {
            initialConditionU->beginBlock(pSurface->getIdentifier());
            initialConditionU->writeString("type", "noSlip");
            initialConditionU->endBlock();
            break;
        }
        default:
            throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);
        }

    }

    initialConditionU->endBlock();

    return initialConditionU;

}



void COpenFOAMCaseInstance::startComputation()
{
    if (!canRun())
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CANNOTRUNOPENFOAMCASE);

    m_Status = eCaseStatus::Running;   

    std::string sCaseFileName = m_sIdentifier + ".foam";

    m_pOpenCaseDirectory = m_pDriverEnvironment->CreateWorkingDirectory();
	m_pConstantDirectory = m_pOpenCaseDirectory->CreateSubDirectory("constant");
    m_pSystemDirectory = m_pOpenCaseDirectory->CreateSubDirectory("system");
	m_pTriSurfaceDirectory = m_pConstantDirectory->CreateSubDirectory("triSurface");
    m_pInitialDirectory = m_pOpenCaseDirectory->CreateSubDirectory("0");

    m_pBlockMeshDictFile = m_pSystemDirectory->StoreCustomString("blockMeshDict", createBlockMeshDict ()->getAsString());
    m_pControlDictFile = m_pSystemDirectory->StoreCustomString("controlDict", createControlDict ()->getAsString());
    m_pDecomposeParDictFile = m_pSystemDirectory->StoreCustomString("decomposeParDict", createDecomposeParDict()->getAsString());
    m_pSnappyHexMeshDictFile = m_pSystemDirectory->StoreCustomString("snappyHexMeshDict", createSnappyHexMeshDict()->getAsString());
    m_pMeshQualityDictFile = m_pSystemDirectory->StoreCustomString("meshQualityDict", createMeshQualityDict()->getAsString());
    m_pFVSolutionFile = m_pSystemDirectory->StoreCustomString("fvSolution", createFVSolutionDict()->getAsString());
    m_pFVSchemesFile = m_pSystemDirectory->StoreCustomString("fvSchemes", createFVSchemesDict()->getAsString());

    m_pTransportPropertiesFile = m_pConstantDirectory->StoreCustomString("transportProperties", createTransportPropertiesFile()->getAsString());
    m_pTurbulencePropertiesFile = m_pConstantDirectory->StoreCustomString("turbulenceProperties", createTurbulencePropertiesFile()->getAsString());

    m_pInitialCondition_K = m_pInitialDirectory->StoreCustomString("k", createInitialCondition_K()->getAsString());
    m_pInitialCondition_Nut = m_pInitialDirectory->StoreCustomString("nut", createInitialCondition_Nut()->getAsString());
    m_pInitialCondition_Omega = m_pInitialDirectory->StoreCustomString("omega", createInitialCondition_Omega()->getAsString());
    m_pInitialCondition_P = m_pInitialDirectory->StoreCustomString("p", createInitialCondition_P()->getAsString());
    m_pInitialCondition_U = m_pInitialDirectory->StoreCustomString("U", createInitialCondition_U()->getAsString());

    m_pCaseFile = m_pOpenCaseDirectory->StoreCustomString(sCaseFileName, "");

    m_SurfaceASCIISTLs.clear();
    if (m_pCaseDefinition.get() != nullptr) {
        auto& surfaces = m_pCaseDefinition->getSurfaces();

        for (auto pSurface : surfaces) {

            auto pBufferedWriter = m_pTriSurfaceDirectory->AddBufferedWriter(pSurface->getSTLFileName(), m_nSTLWriteBufferSizeInKB);
            std::string sIdentifier = pSurface->getIdentifier();

            double dMMinM = 0.001;
            writeSurfaceAsASCIISTL (pSurface.get(), pBufferedWriter.get(), dMMinM);

            m_SurfaceASCIISTLs.insert(std::make_pair (sIdentifier, pBufferedWriter->Finish()));
        }
    }

    /*auto pExecutable = m_pOpenCaseDirectory->StoreDriverData("blockMesh.exe", "blockmesh");
    auto pProcess = pExecutable->ExecuteFile();
    pProcess->SetVerboseLogging(true); 
    pProcess->StartProcess("", 100000); */

    m_pOpenCaseDirectory->CleanUp();
}

void COpenFOAMCaseInstance::cancelComputation()
{

 
}

void addContentToOpenFOAMInputDeckZIP(LibMCEnv::PWorkingFile pWorkingFile, const std::string& sZIPPath, LibMCEnv::PZIPStreamWriter pZIPStream)
{
    if (pZIPStream.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);
    if (pWorkingFile.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);

    std::vector<uint8_t> contentBuffer;
    pWorkingFile->ReadContent (contentBuffer);

    auto pZIPEntry = pZIPStream->CreateZIPEntry(sZIPPath);
    if (contentBuffer.size() > 0)
        pZIPEntry->WriteData(contentBuffer);

}

void COpenFOAMCaseInstance::createOpenFOAMInputDeck(LibMCEnv::PZIPStreamWriter pZIPStream)
{
    if (pZIPStream.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);

    addContentToOpenFOAMInputDeckZIP(m_pCaseFile, "case.foam", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pBlockMeshDictFile, "system/blockMeshDict", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pControlDictFile, "system/controlDict", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pDecomposeParDictFile, "system/decomposeParDict", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pSnappyHexMeshDictFile, "system/snappyHexMeshDict", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pMeshQualityDictFile, "system/meshQualityDict", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pFVSolutionFile, "system/fvSolution", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pFVSchemesFile, "system/fvSchemes", pZIPStream);

    addContentToOpenFOAMInputDeckZIP(m_pTransportPropertiesFile, "constant/transportProperties", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pTurbulencePropertiesFile, "constant/turbulenceProperties", pZIPStream);

    addContentToOpenFOAMInputDeckZIP(m_pInitialCondition_K, "0/k", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pInitialCondition_Nut, "0/nut", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pInitialCondition_Omega, "0/omega", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pInitialCondition_P, "0/p", pZIPStream);
    addContentToOpenFOAMInputDeckZIP(m_pInitialCondition_U, "0/U", pZIPStream);

    if (m_pCaseDefinition.get() != nullptr) {
        auto& surfaces = m_pCaseDefinition->getSurfaces();

        for (auto pSurface : surfaces) {
            auto iIter = m_SurfaceASCIISTLs.find(pSurface->getIdentifier());
            if (iIter == m_SurfaceASCIISTLs.end())
                throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_SURFACESTLNOTFOUND);

            addContentToOpenFOAMInputDeckZIP(iIter->second, "constant/triSurface/" + pSurface->getSTLFileName (), pZIPStream);
        }

    }

    auto pCaseDefinitionEntry = pZIPStream->CreateZIPEntry("definition.xml");
    pCaseDefinitionEntry->WriteString(m_sCaseDefinitionXML);

}


void COpenFOAMCaseInstance::releaseCase()
{
    m_SurfaceASCIISTLs.clear();
    m_pOpenCaseDirectory = nullptr;
    m_pBlockMeshDictFile = nullptr;
    m_pControlDictFile = nullptr;
    m_pDecomposeParDictFile = nullptr;
    m_pSnappyHexMeshDictFile = nullptr;
    m_pMeshQualityDictFile = nullptr;
    m_pFVSolutionFile = nullptr;
    m_pFVSchemesFile = nullptr;
    m_pTransportPropertiesFile = nullptr;
    m_pTurbulencePropertiesFile = nullptr;
    m_pCaseFile = nullptr;

    if (m_pOpenCaseDirectory.get () != nullptr)
        m_pOpenCaseDirectory->CleanUp();
    m_pOpenCaseDirectory = nullptr;
}


bool COpenFOAMCaseInstance::checkIdentifier(const std::string& sIdentifier)
{
    if (sIdentifier.empty())
        return false;

    if (sIdentifier.length() > OPENFOAM_MAXIDENTIFIER_LENGTH)
        return false;

    for (auto ch : sIdentifier)
        if (!isalnum(ch) && ch != '_' && ch != '-')
            return false;

    return true;
}

bool COpenFOAMCaseInstance::checkUUID(const std::string& sUUID)
{
    if (sUUID.length() != 36)
        return false;

    bool bCharsAreAllZero = true;

    size_t nIndex = 0;
    for (char ch : sUUID) {
        char lowerChar = ::tolower(ch);

        if ((nIndex == 8) || (nIndex == 13) || (nIndex == 18) || (nIndex == 23)) {
            if (lowerChar != '-')
                return false;
        }
        else {
            bool bCharIsValid = (((lowerChar >= '0') && (lowerChar <= '9')) ||
                ((lowerChar >= 'a') && (lowerChar <= 'f')));
            if (!bCharIsValid)
                return false;

            if (lowerChar != '0')
                bCharsAreAllZero = false;
        }

        nIndex++;

    }

    return !bCharsAreAllZero;
}

void COpenFOAMCaseInstance::computeDomainOutbox(double& dMinXInMM, double& dMinYInMM, double& dMinZInMM, double& dMaxXInMM, double& dMaxYInMM, double& dMaxZInMM)
{
    if (m_pCaseDefinition.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEDEFINITIONNOTINITIALIZED);

    auto pBuild = m_pDriverEnvironment->GetBuildJob(m_sBuildUUID);
    auto pToolpath = pBuild->CreateToolpathAccessor();
    auto pToolpathPart = pToolpath->FindPartByUUID(m_pCaseDefinition->getBuildItemUUID());

    auto pComponent = pToolpathPart->GetRootComponent();
    auto pBoundingBox = pComponent->CalculateBoundingBox ();
    auto minimumPoint = pBoundingBox->GetMinimum();
    auto maximumPoint = pBoundingBox->GetMaximum();

    dMinXInMM = minimumPoint.m_Coordinates[0];
    dMinYInMM = minimumPoint.m_Coordinates[1];
    dMinZInMM = minimumPoint.m_Coordinates[2];
    dMaxXInMM = maximumPoint.m_Coordinates[0];
    dMaxYInMM = maximumPoint.m_Coordinates[1];
    dMaxZInMM = maximumPoint.m_Coordinates[2];

}



void COpenFOAMCaseInstance::computePointInDomainNotOnGrid(double& dXInMM, double& dYInMM, double& dZInMM)
{
    auto pBuild = m_pDriverEnvironment->GetBuildJob(m_sBuildUUID);
    auto pToolpath = pBuild->CreateToolpathAccessor();
    auto pToolpathPart = pToolpath->FindPartByUUID(m_pCaseDefinition->getBuildItemUUID());

    auto pComponent = pToolpathPart->GetRootComponent();
    auto pBoundingBox = pComponent->CalculateBoundingBox();
    auto minimumPoint = pBoundingBox->GetMinimum();
    auto maximumPoint = pBoundingBox->GetMaximum();

    // TODO: Find inner point in mesh
    dXInMM = (minimumPoint.m_Coordinates[0] + maximumPoint.m_Coordinates[0]) * 0.5;
    dYInMM = (minimumPoint.m_Coordinates[1] + maximumPoint.m_Coordinates[1]) * 0.5;
    dZInMM = (minimumPoint.m_Coordinates[2] + maximumPoint.m_Coordinates[2]) * 0.5;
}


void COpenFOAMCaseInstance::writeSurfaceAsASCIISTL(COpenFOAMCaseSurfaceInstance* pSurface, LibMCEnv::CWorkingFileWriter* pWriterInstance, double dUnitFactorPerMM)
{
    if (pSurface == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException (LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);
    if (pWriterInstance == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_INVALIDPARAM);

    if (m_pCaseDefinition.get() == nullptr)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_CASEDEFINITIONNOTINITIALIZED);

    auto pBuild = m_pDriverEnvironment->GetBuildJob(m_sBuildUUID);
    auto pToolpath = pBuild->CreateToolpathAccessor();
    auto pToolpathPart = pToolpath->FindPartByUUID(m_pCaseDefinition->getBuildItemUUID());

    std::string sSurfaceIdentifier = pSurface->getIdentifier();

    auto pRootComponent = pToolpathPart->GetRootComponent();
    uint32_t nSolidCount = pRootComponent->GetSolidCount();

    if (nSolidCount != 1)
        throw ELibMCDriver_OpenFOAMInterfaceException(LIBMCDRIVER_OPENFOAM_ERROR_OPENFOAMDOMAINMUSTBECONNECTED);

    auto pSolidMesh = pRootComponent->GetSolidMesh(0);
    auto pCopiedMesh = pSolidMesh->CreateTriangleSetOfMesh(pSurface->getIdentifier ());

    std::vector<LibMCEnv::sMeshVertex3D> Vertices;
    pCopiedMesh->GetAllVertices(Vertices);
    
    std::vector<LibMCEnv::sMeshTriangle3D> Triangles;
    pCopiedMesh->GetAllTriangles(Triangles);

    pWriterInstance->WriteLine("solid " + sSurfaceIdentifier, true);
  
    for (auto& triangle : Triangles) {
        auto& vertex1 = Vertices.at(triangle.m_Vertices[0] - 1);
        auto& vertex2 = Vertices.at(triangle.m_Vertices[1] - 1);
        auto& vertex3 = Vertices.at(triangle.m_Vertices[2] - 1);

        pWriterInstance->WriteLine (" facet normal 0 0 -1", true);
        pWriterInstance->WriteLine ("  outer loop", true);
        pWriterInstance->WriteString   ("  vertex ");
        pWriterInstance->WriteFixedFloat(vertex1.m_Coordinates[0] * dUnitFactorPerMM, 6);
        pWriterInstance->WriteString(" ");
        pWriterInstance->WriteFixedFloat(vertex1.m_Coordinates[1] * dUnitFactorPerMM, 6);
        pWriterInstance->WriteString(" ");
        pWriterInstance->WriteFixedFloat(vertex1.m_Coordinates[2] * dUnitFactorPerMM, 6);
        pWriterInstance->WriteLine("", true);

        pWriterInstance->WriteString("  vertex ");
        pWriterInstance->WriteFixedFloat(vertex2.m_Coordinates[0] * dUnitFactorPerMM, 6);
        pWriterInstance->WriteString(" ");
        pWriterInstance->WriteFixedFloat(vertex2.m_Coordinates[1] * dUnitFactorPerMM, 6);
        pWriterInstance->WriteString(" ");
        pWriterInstance->WriteFixedFloat(vertex2.m_Coordinates[2] * dUnitFactorPerMM, 6);
        pWriterInstance->WriteLine("", true);

        pWriterInstance->WriteString("  vertex ");
        pWriterInstance->WriteFixedFloat(vertex3.m_Coordinates[0] * dUnitFactorPerMM, 6);
        pWriterInstance->WriteString(" ");
        pWriterInstance->WriteFixedFloat(vertex3.m_Coordinates[1] * dUnitFactorPerMM, 6);
        pWriterInstance->WriteString(" ");
        pWriterInstance->WriteFixedFloat(vertex3.m_Coordinates[2] * dUnitFactorPerMM, 6);
        pWriterInstance->WriteLine("", true);
        pWriterInstance->WriteLine   ("  endloop", true);
        pWriterInstance->WriteLine   (" endfacet", true);

    }

    pWriterInstance->WriteLine("endsolid " + sSurfaceIdentifier, true);
}

