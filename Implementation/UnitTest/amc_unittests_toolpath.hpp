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

#ifndef __AMCTEST_UNITTEST_TOOLPATH
#define __AMCTEST_UNITTEST_TOOLPATH

#include "amc_unittests.hpp"
#include "amc_toolpathlayerdata.hpp"
#include "amc_toolpathentity.hpp"
#include "amc_toolpathhandler.hpp"
#include "amc_toolpathpart.hpp"
#include "amc_scatterplot.hpp"
#include "amc_unittests_libmcdata.hpp"
#include "common_utils.hpp"

#include "lib3mf/lib3mf_dynamic.hpp"
#include "libmcdata_dynamic.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace AMCUnitTest {

	class CToolpathTestFixture {
	public:
		static std::string normalizePathSeparators(const std::string& sPath)
		{
			std::string result = sPath;
			for (auto& ch : result) {
				if (ch == '\\')
					ch = '/';
			}
			return result;
		}

		static std::string getRepoRoot()
		{
			std::string filePath = normalizePathSeparators(__FILE__);
			std::string marker = "Implementation/UnitTest/amc_unittests_toolpath.hpp";
			auto pos = filePath.find(marker);
			if (pos == std::string::npos)
				return "";
			return filePath.substr(0, pos);
		}

		static std::string getLib3MFPath()
		{
			std::string root = getRepoRoot();
#if defined(_WIN32)
			return root + "Artifacts/lib3mf/lib3mf_win64.dll";
#elif defined(__linux__)
#if defined(__aarch64__) || defined(__arm__)
			return root + "Artifacts/lib3mf/lib3mf_rpi.so";
#else
			return root + "Artifacts/lib3mf/lib3mf_linux64.so";
#endif
#else
			return root + "Artifacts/lib3mf/lib3mf_linux64.so";
#endif
		}

		static Lib3MF::PWrapper loadWrapper()
		{
			return Lib3MF::CWrapper::loadLibrary(getLib3MFPath());
		}

		static std::vector<uint8_t> buildToolpathBuffer(std::string& sToolpathUUID, std::string& sProfileUUID, std::string& sBuildItemUUID)
		{
			auto pWrapper = loadWrapper();
			auto pModel = pWrapper->CreateModel();

			auto pMesh = pModel->AddMeshObject();
			pMesh->SetUUID(AMCCommon::CUtils::createUUID());
			std::vector<Lib3MF::sPosition> vertices(3);
			vertices[0].m_Coordinates[0] = 0.0f;
			vertices[0].m_Coordinates[1] = 0.0f;
			vertices[0].m_Coordinates[2] = 0.0f;
			vertices[1].m_Coordinates[0] = 1.0f;
			vertices[1].m_Coordinates[1] = 0.0f;
			vertices[1].m_Coordinates[2] = 0.0f;
			vertices[2].m_Coordinates[0] = 0.0f;
			vertices[2].m_Coordinates[1] = 1.0f;
			vertices[2].m_Coordinates[2] = 0.0f;

			std::vector<Lib3MF::sTriangle> triangles(1);
			triangles[0].m_Indices[0] = 0;
			triangles[0].m_Indices[1] = 1;
			triangles[0].m_Indices[2] = 2;
			pMesh->SetGeometry(vertices, triangles);
			pMesh->SetName("ToolpathPart");

			Lib3MF::sTransform transform;
			for (uint32_t i = 0; i < 4; i++)
				for (uint32_t j = 0; j < 3; j++)
					transform.m_Fields[i][j] = 0.0f;
			transform.m_Fields[0][0] = 1.0f;
			transform.m_Fields[1][1] = 1.0f;
			transform.m_Fields[2][2] = 1.0f;
			auto pBuildItem = pModel->AddBuildItem(pMesh, transform);
			std::string buildItemUUID = AMCCommon::CUtils::createUUID();
			pBuildItem->SetUUID(buildItemUUID);
			bool bHasUUID = false;
			sBuildItemUUID = pBuildItem->GetUUID(bHasUUID);

			auto pToolpath = pModel->AddToolpath(0.01);
			sToolpathUUID = pToolpath->GetUUID();

			auto pProfile = pToolpath->AddProfile("default");
			pProfile->SetParameterDoubleValue("amc", "speed", 500.0);
			pProfile->SetParameterIntegerValue("amc", "count", 42);
			pProfile->AddModifier("amc", "speed", Lib3MF::eToolpathProfileModificationType::LinearModification, Lib3MF::eToolpathProfileModificationFactor::FactorF, 0.1, 0.9);
			sProfileUUID = pProfile->GetUUID();

			auto pWriter = pModel->QueryWriter("3mf");
			auto pLayer = pToolpath->AddLayer(100, "/toolpath/layer0.xml", pWriter);
			auto nProfileID = pLayer->RegisterProfile(pProfile);
			auto nPartID = pLayer->RegisterBuildItem(pBuildItem);

			pToolpath->RegisterCustomIntegerSegmentAttribute("amc", "intattr");
			pToolpath->RegisterCustomDoubleSegmentAttribute("amc", "doubleattr");

			pLayer->SetSegmentAttribute("amc", "intattr", "7");
			pLayer->SetSegmentAttribute("amc", "doubleattr", "1.25");
			std::vector<Lib3MF::sDiscretePosition2D> polyline(3);
			polyline[0].m_Coordinates[0] = 0;
			polyline[0].m_Coordinates[1] = 0;
			polyline[1].m_Coordinates[0] = 10;
			polyline[1].m_Coordinates[1] = 0;
			polyline[2].m_Coordinates[0] = 10;
			polyline[2].m_Coordinates[1] = 5;
			pLayer->WritePolylineDiscrete(nProfileID, nPartID, polyline);

			pLayer->SetSegmentAttribute("amc", "intattr", "9");
			pLayer->SetSegmentAttribute("amc", "doubleattr", "2.5");
			std::vector<Lib3MF::sDiscretePosition2D> loop(3);
			loop[0].m_Coordinates[0] = 1;
			loop[0].m_Coordinates[1] = 1;
			loop[1].m_Coordinates[0] = 1;
			loop[1].m_Coordinates[1] = 4;
			loop[2].m_Coordinates[0] = 4;
			loop[2].m_Coordinates[1] = 4;
			pLayer->WriteLoopDiscrete(nProfileID, nPartID, loop);

			pLayer->SetSegmentAttribute("amc", "intattr", "11");
			pLayer->SetSegmentAttribute("amc", "doubleattr", "3.5");
			std::vector<Lib3MF::sDiscreteHatch2D> hatches(2);
			hatches[0].m_Point1Coordinates[0] = 0;
			hatches[0].m_Point1Coordinates[1] = 0;
			hatches[0].m_Point2Coordinates[0] = 10;
			hatches[0].m_Point2Coordinates[1] = 0;
			hatches[1].m_Point1Coordinates[0] = 0;
			hatches[1].m_Point1Coordinates[1] = 5;
			hatches[1].m_Point2Coordinates[0] = 10;
			hatches[1].m_Point2Coordinates[1] = 5;

			std::vector<Lib3MF_double> factor1 = { 0.1, 0.2 };
			std::vector<Lib3MF_double> factor2 = { 0.3, 0.4 };
			std::vector<Lib3MF_uint32> subInterpolationCounts = { 1, 1 };
			std::vector<Lib3MF::sHatchModificationInterpolationData> interpolationData(2);
			interpolationData[0].m_Parameter = 0.1;
			interpolationData[0].m_Factor = 0.2;
			interpolationData[1].m_Parameter = 0.2;
			interpolationData[1].m_Factor = 0.3;
			pLayer->WriteHatchDataDiscreteWithNonlinearFactors(nProfileID, nPartID, hatches, factor1, factor2, subInterpolationCounts, interpolationData);

			auto pLayerCustom = pLayer->AddCustomData("amc", "layerinfo");
			pLayerCustom->GetRootNode()->AddAttribute("name", "layer0");

			auto pToolpathCustom = pToolpath->AddCustomData("amc", "job");
			pToolpathCustom->GetRootNode()->AddAttribute("name", "testjob");

			auto pAttachmentUnique = pModel->AddAttachment("/metadata/unique.bin", "urn:amc:unique");
			std::vector<Lib3MF_uint8> uniqueData = { 1, 2, 3, 4 };
			pAttachmentUnique->ReadFromBuffer(uniqueData);

			auto pAttachmentDuplicate1 = pModel->AddAttachment("/metadata/dup1.bin", "urn:amc:dup");
			std::vector<Lib3MF_uint8> dupData1 = { 5, 6 };
			pAttachmentDuplicate1->ReadFromBuffer(dupData1);
			auto pAttachmentDuplicate2 = pModel->AddAttachment("/metadata/dup2.bin", "urn:amc:dup");
			std::vector<Lib3MF_uint8> dupData2 = { 7, 8 };
			pAttachmentDuplicate2->ReadFromBuffer(dupData2);

			auto pThumbnail = pModel->CreatePackageThumbnailAttachment();
			pThumbnail->SetPath("/Thumbnail.png");
			std::vector<Lib3MF_uint8> thumbnailData = { 9, 9, 9 };
			pThumbnail->ReadFromBuffer(thumbnailData);

			pLayer->Finish();

			std::vector<Lib3MF_uint8> buffer;
			pWriter->WriteToBuffer(buffer);

			return std::vector<uint8_t>(buffer.begin(), buffer.end());
		}
	};

	class CUnitTestGroup_Toolpath : public CUnitTestGroup {
	public:

		std::string getTestGroupName() override
		{
			return "Toolpath";
		}

		void registerTests() override
		{
			registerTest("LayerProfileBasics", "CToolpathLayerProfile value and modifier handling", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Toolpath::testLayerProfileBasics, this));
			registerTest("LayerDataFromModel", "CToolpathLayerData conversion and accessors", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Toolpath::testLayerDataFromModel, this));
			registerTest("ToolpathPartBasics", "CToolpathPart wraps build items", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Toolpath::testToolpathPartBasics, this));
			registerTest("ToolpathEntityHandler", "CToolpathEntity/Handler metadata and load/unload paths", eUnitTestCategory::utMandatoryPass, std::bind(&CUnitTestGroup_Toolpath::testToolpathEntityHandler, this));
		}

		void initializeTests() override
		{
		}

	private:

		void testLayerProfileBasics()
		{
			AMC::CToolpathLayerProfile profile(7, "uuid", "profile");
			profile.addValue("amc", "speed", "123.5");
			profile.addValue("amc", "count", "42");
			profile.addModifier("amc", "speed", LibMCEnv::eToolpathProfileModificationType::LinearModification, LibMCEnv::eToolpathProfileModificationFactor::FactorF, 0.1, 0.9);

			assertTrue(profile.getUUID() == "uuid");
			assertTrue(profile.getName() == "profile");
			assertTrue(profile.getProfileIndex() == 7);

			assertTrue(profile.hasValue("amc", "speed"));
			assertTrue(profile.getValue("amc", "speed") == "123.5");
			assertTrue(profile.getValueDef("amc", "missing", "fallback") == "fallback");
			assertTrue(profile.getDoubleValue("amc", "speed") == 123.5);
			assertTrue(profile.getDoubleValueDef("amc", "missing", 1.25) == 1.25);
			assertTrue(profile.getIntegerValue("amc", "count") == 42);
			assertTrue(profile.getIntegerValueDef("amc", "missing", 7) == 7);

			assertTrue(profile.getModificationType("amc", "speed") == LibMCEnv::eToolpathProfileModificationType::LinearModification);
			LibMCEnv::eToolpathProfileModificationFactor factor = LibMCEnv::eToolpathProfileModificationFactor::Unknown;
			double dMin = 0.0;
			double dMax = 0.0;
			profile.getModificationInformation("amc", "speed", factor, dMin, dMax);
			assertTrue(factor == LibMCEnv::eToolpathProfileModificationFactor::FactorF);
			assertTrue(dMin == 0.1);
			assertTrue(dMax == 0.9);

			bool thrown = false;
			try {
				profile.getValue("amc", "missing");
			}
			catch (...) {
				thrown = true;
			}
			assertTrue(thrown, "Expected missing profile value to throw");

			assertTrue(AMC::CToolpathLayerData::getValueNameByType(LibMCEnv::eToolpathProfileValueType::Speed) == "laserspeed");
			thrown = false;
			try {
				AMC::CToolpathLayerData::getValueNameByType((LibMCEnv::eToolpathProfileValueType)0xFF);
			}
			catch (...) {
				thrown = true;
			}
			assertTrue(thrown, "Expected invalid profile value type to throw");
		}

		void testLayerDataFromModel()
		{
			std::string toolpathUUID;
			std::string profileUUID;
			std::string buildItemUUID;
			auto buffer = CToolpathTestFixture::buildToolpathBuffer(toolpathUUID, profileUUID, buildItemUUID);
			static_cast<void>(toolpathUUID);

			auto pWrapper = CToolpathTestFixture::loadWrapper();
			auto pModel = pWrapper->CreateModel();
			auto pReader = pModel->QueryReader("3mf");
			pReader->ReadFromBuffer(buffer);

			auto pToolpathIterator = pModel->GetToolpaths();
			assertTrue(pToolpathIterator->MoveNext());
			auto pToolpath = pToolpathIterator->GetCurrentToolpath();
			auto pLayerReader = pToolpath->ReadLayerData(0);

			std::vector<AMC::PToolpathCustomSegmentAttribute> customAttributes;
			customAttributes.push_back(std::make_shared<AMC::CToolpathCustomSegmentAttribute>(1, "amc", "intattr", LibMCEnv::eToolpathAttributeType::Integer));
			customAttributes.push_back(std::make_shared<AMC::CToolpathCustomSegmentAttribute>(2, "amc", "doubleattr", LibMCEnv::eToolpathAttributeType::Double));

			AMC::CToolpathLayerData layerData(pToolpath, pLayerReader, pToolpath->GetUnits(), (int32_t)pToolpath->GetLayerZMax(0), "testlayer", customAttributes);

			assertTrue(layerData.getUnits() == pToolpath->GetUnits());
			assertTrue(layerData.getZValue() == (int32_t)pToolpath->GetLayerZMax(0));
			assertTrue(layerData.getSegmentCount() == 3);

			assertTrue(layerData.getSegmentType(0) == LibMCEnv::eToolpathSegmentType::Polyline);
			assertTrue(layerData.getSegmentType(1) == LibMCEnv::eToolpathSegmentType::Polyline);
			assertTrue(layerData.getSegmentType(2) == LibMCEnv::eToolpathSegmentType::Hatch);

			assertTrue(layerData.getSegmentPointCount(0) == 3);
			assertTrue(layerData.getSegmentPointCount(1) == 4);
			assertTrue(layerData.getSegmentPointCount(2) == 4);

			std::vector<LibMCEnv::sPosition2D> points(4);
			layerData.storePointsToBufferInUnits(0, points.data());
			assertTrue(points[0].m_Coordinates[0] == 0);
			assertTrue(points[2].m_Coordinates[1] == 5);

			layerData.storePointsToBufferInUnits(1, points.data());
			assertTrue(points[0].m_Coordinates[0] == points[3].m_Coordinates[0]);
			assertTrue(points[0].m_Coordinates[1] == points[3].m_Coordinates[1]);

			std::vector<LibMCEnv::sHatch2D> hatches(2);
			layerData.storeHatchesToBufferInUnits(2, hatches.data());
			assertTrue(hatches[0].m_X1 == 0);
			assertTrue(hatches[1].m_Y2 == 5);

			int32_t nMinX = 0;
			int32_t nMinY = 0;
			int32_t nMaxX = 0;
			int32_t nMaxY = 0;
			layerData.calculateExtents(nMinX, nMinY, nMaxX, nMaxY);
			assertTrue(nMinX == 0);
			assertTrue(nMaxX == 10);
			assertTrue(nMinY == 0);
			assertTrue(nMaxY == 5);

			assertTrue(layerData.getSegmentProfileUUID(0) == profileUUID);
			assertTrue(layerData.getSegmentPartUUID(0) == buildItemUUID);
			assertTrue(layerData.getSegmentLocalPartID(0) == pLayerReader->GetSegmentPartID(0));

			auto pProfile = layerData.getSegmentProfile(0);
			assertTrue(pProfile->getUUID() == profileUUID);
			assertTrue(pProfile->getDoubleValue("amc", "speed") == 500.0);
			assertTrue(pProfile->getIntegerValue("amc", "count") == 42);

			uint32_t nAttributeID = 0;
			LibMCEnv::eToolpathAttributeType attributeType = LibMCEnv::eToolpathAttributeType::Unknown;
			assertTrue(layerData.findCustomSegmentAttribute("amc", "intattr", nAttributeID, attributeType));
			assertTrue(attributeType == LibMCEnv::eToolpathAttributeType::Integer);
			assertTrue(layerData.getSegmentIntegerAttribute(0, nAttributeID) == 7);

			assertTrue(layerData.findCustomSegmentAttribute("amc", "doubleattr", nAttributeID, attributeType));
			assertTrue(attributeType == LibMCEnv::eToolpathAttributeType::Double);
			assertTrue(layerData.getSegmentDoubleAttribute(2, nAttributeID) == 3.5);

			double dFactor1 = 0.0;
			double dFactor2 = 0.0;
			layerData.getHatchModificationFactors(2, 1, LibMCEnv::eToolpathProfileModificationFactor::FactorF, dFactor1, dFactor2);
			assertTrue(dFactor1 == 0.2);
			assertTrue(dFactor2 == 0.4);

			uint32_t nSubCount = 0;
			Lib3MF::sHatchModificationInterpolationData* pSubData = nullptr;
			layerData.getHatchSubinterpolationData(2, 0, nSubCount, pSubData);
			assertTrue(nSubCount == 1);
			assertTrue(pSubData->m_Parameter == 0.1);

			assertTrue(layerData.getMetaDataCount() == 1);
			std::string sLayerNameSpace;
			std::string sLayerName;
			layerData.getMetaDataInfo(0, sLayerNameSpace, sLayerName);
			assertTrue(sLayerNameSpace == "amc");
			assertTrue(sLayerName == "layerinfo");
			assertTrue(layerData.hasUniqueMetaData("amc", "layerinfo"));
			auto pLayerXML = layerData.findUniqueMetaData("amc", "layerinfo");
			assertTrue(pLayerXML->GetRootNode()->HasAttribute("name"));
		}

		void testToolpathPartBasics()
		{
			std::string toolpathUUID;
			std::string profileUUID;
			std::string buildItemUUID;
			auto buffer = CToolpathTestFixture::buildToolpathBuffer(toolpathUUID, profileUUID, buildItemUUID);
			static_cast<void>(toolpathUUID);
			static_cast<void>(profileUUID);

			auto pWrapper = CToolpathTestFixture::loadWrapper();
			auto pModel = pWrapper->CreateModel();
			auto pReader = pModel->QueryReader("3mf");
			pReader->ReadFromBuffer(buffer);

			auto pBuildItems = pModel->GetBuildItems();
			assertTrue(pBuildItems->MoveNext());
			auto pBuildItem = pBuildItems->GetCurrent();

			AMC::CToolpathPart part(pModel, pBuildItem, pWrapper);
			assertTrue(part.getUUID() == buildItemUUID);
			assertTrue(part.getModel().get() == pModel.get());
			assertTrue(part.getBuildItem().get() == pBuildItem.get());
			assertFalse(part.getName().empty());
		}

		void testToolpathEntityHandler()
		{
			std::string toolpathUUID;
			std::string profileUUID;
			std::string buildItemUUID;
			auto buffer = CToolpathTestFixture::buildToolpathBuffer(toolpathUUID, profileUUID, buildItemUUID);
			static_cast<void>(toolpathUUID);
			static_cast<void>(profileUUID);
			static_cast<void>(buildItemUUID);

			std::string sRootPath = "testoutput";
			if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(sRootPath))
				AMCCommon::CUtils::createDirectoryOnDisk(sRootPath);

			std::string sBasePath = sRootPath + "/toolpath_" + AMCCommon::CUtils::createUUID();
			AMCCommon::CUtils::createDirectoryOnDisk(sBasePath);
			std::string sDatabaseFile = sBasePath + "/toolpath.db";

			auto pDataWrapper = AMCUnitTest::loadLibMCDataInProcess();
			auto pDataModel = pDataWrapper->CreateDataModelInstance();
			pDataModel->InitialiseDatabase(sBasePath, LibMCData::eDataBaseType::SqLite, sDatabaseFile);

			auto pStorage = pDataModel->CreateStorage();
			std::string streamUUID = AMCCommon::CUtils::createUUID();
			std::vector<LibMCData_uint8> storageBuffer(buffer.begin(), buffer.end());
			pStorage->StoreNewStream(streamUUID, "toolpath.3mf", "application/3mf", storageBuffer, "", 0);

			AMC::CToolpathHandler handler(pDataModel);
			handler.setLibraryPath("lib3mf", CToolpathTestFixture::getLib3MFPath());

			assertTrue(handler.findToolpathEntity(streamUUID, false) == nullptr);
			bool thrown = false;
			try {
				handler.findToolpathEntity(streamUUID, true);
			}
			catch (...) {
				thrown = true;
			}
			assertTrue(thrown, "Expected missing entity to throw");

			auto* pEntity = handler.loadToolpathEntity(streamUUID);
			assertTrue(pEntity != nullptr);
			assertTrue(pEntity->getLayerCount() == 1);

			auto* pEntityAgain = handler.loadToolpathEntity(streamUUID);
			assertTrue(pEntityAgain == pEntity);

			handler.unloadToolpathEntity(streamUUID);
			assertTrue(handler.findToolpathEntity(streamUUID, false) != nullptr);
			handler.unloadToolpathEntity(streamUUID);
			assertTrue(handler.findToolpathEntity(streamUUID, false) == nullptr);

			pEntity = handler.loadToolpathEntity(streamUUID);
			assertTrue(pEntity->getMetaDataCount() == 1);
			std::string sNameSpace;
			std::string sName;
			pEntity->getMetaDataInfo(0, sNameSpace, sName);
			assertTrue(sNameSpace == "amc");
			assertTrue(sName == "job");
			assertTrue(pEntity->hasUniqueMetaData("amc", "job"));
			auto pXML = pEntity->findUniqueMetaData("amc", "job");
			assertTrue(pXML->GetRootNode()->HasAttribute("name"));

			assertTrue(pEntity->hasBinaryMetaData("/metadata/unique.bin"));
			std::vector<uint8_t> metaBuffer(4);
			uint64_t neededCount = 0;
			pEntity->getBinaryMetaData("/metadata/unique.bin", metaBuffer.size(), &neededCount, metaBuffer.data());
			assertTrue(neededCount == 4);
			assertTrue(metaBuffer[0] == 1);

			assertTrue(pEntity->getBinaryMetaDataAsString("/metadata/unique.bin").size() == 4);
			assertTrue(pEntity->getBinaryMetaDataRelationship("/metadata/unique.bin") == "urn:amc:unique");

			assertTrue(pEntity->hasUniqueBinaryMetaDataSchema("urn:amc:unique"));
			assertFalse(pEntity->hasUniqueBinaryMetaDataSchema("urn:amc:dup"));

			std::string sBinaryText = pEntity->getBinaryMetaDataAsStringBySchema("urn:amc:unique");
			assertTrue(sBinaryText.size() == 4);

			std::vector<uint8_t> thumbnail;
			std::string mimeType;
			assertTrue(pEntity->readThumbnail(thumbnail, mimeType));
			assertTrue(mimeType == "image/png");

			auto pScatterplot = std::make_shared<AMC::CScatterplot>(AMCCommon::CUtils::createUUID());
			handler.storeScatterplot(pScatterplot);
			assertTrue(handler.restoreScatterplot(pScatterplot->getUUID(), true).get() == pScatterplot.get());
		}

	};

}

#endif // __AMCTEST_UNITTEST_TOOLPATH
