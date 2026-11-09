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


Abstract: This is a stub class definition of CDriver_ScannerEmulator

*/

#include "libmcdriver_scanneremulator_driver_scanneremulator.hpp"
#include "libmcdriver_scanneremulator_scanner.hpp"
#include "libmcdriver_scanneremulator_scannerimage.hpp"
#include "libmcdriver_scanneremulator_interfaceexception.hpp"

#include <set>
#include <cmath>
#include <exception>

#define __STRINGIZE(x) #x
#define __STRINGIZE_VALUE_OF(x) __STRINGIZE(x)

#define SCANNEREMULATOR_CONFIGURATIONSCHEMA "http://schemas.autodesk.com/amc/scanneremulator/2026/11"
#define SCANNEREMULATOR_INFINITETIMEOUT 0xFFFFFFFF

using namespace LibMCDriver_ScannerEmulator;
using namespace LibMCDriver_ScannerEmulator::Impl;


/*************************************************************************************************************************
 Configuration parsing
**************************************************************************************************************************/

class CScannerEmulatorConfigurationNode {
private:
	LibMCEnv::CXMLDocumentNode* m_pNode;
	std::string m_sContext;

	[[noreturn]] void throwInvalidValue(const std::string& sAttributeName, const std::string& sValue)
	{
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, m_sContext + ": invalid value of attribute " + sAttributeName + ": '" + sValue + "'");
	}

public:

	CScannerEmulatorConfigurationNode(LibMCEnv::CXMLDocumentNode* pNode, const std::string& sContext, const std::set<std::string>& allowedAttributes)
		: m_pNode(pNode), m_sContext(sContext)
	{
		if (pNode == nullptr)
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);

		uint64_t nAttributeCount = pNode->GetAttributeCount();
		for (uint64_t nIndex = 0; nIndex < nAttributeCount; nIndex++) {
			auto pAttribute = pNode->GetAttribute(nIndex);
			std::string sAttributeName = pAttribute->GetName();
			if ((sAttributeName == "xmlns") || (sAttributeName.rfind("xmlns:", 0) == 0))
				continue;

			if (allowedAttributes.find(sAttributeName) == allowedAttributes.end())
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, m_sContext + ": unknown attribute " + sAttributeName);
		}
	}

	bool hasAttribute(const std::string& sAttributeName)
	{
		return m_pNode->HasAttribute("", sAttributeName);
	}

	std::string getString(const std::string& sAttributeName, bool bRequired, const std::string& sDefaultValue)
	{
		if (!hasAttribute(sAttributeName)) {
			if (bRequired)
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, m_sContext + ": missing attribute " + sAttributeName);
			return sDefaultValue;
		}

		return m_pNode->GetAttributeValue("", sAttributeName);
	}

	double getDouble(const std::string& sAttributeName, bool bRequired, double dDefaultValue)
	{
		if (!hasAttribute(sAttributeName) && !bRequired)
			return dDefaultValue;

		std::string sValue = getString(sAttributeName, true, "");
		size_t nCharactersParsed = 0;
		double dValue = 0.0;
		try {
			dValue = std::stod(sValue, &nCharactersParsed);
		}
		catch (std::exception&) {
			throwInvalidValue(sAttributeName, sValue);
		}

		if ((nCharactersParsed != sValue.length()) || !std::isfinite(dValue))
			throwInvalidValue(sAttributeName, sValue);

		return dValue;
	}

	int64_t getInteger(const std::string& sAttributeName, int64_t nMinValue, int64_t nMaxValue, int64_t nDefaultValue)
	{
		if (!hasAttribute(sAttributeName))
			return nDefaultValue;

		std::string sValue = getString(sAttributeName, true, "");
		size_t nCharactersParsed = 0;
		long long nValue = 0;
		try {
			nValue = std::stoll(sValue, &nCharactersParsed);
		}
		catch (std::exception&) {
			throwInvalidValue(sAttributeName, sValue);
		}

		if ((nCharactersParsed != sValue.length()) || (nValue < nMinValue) || (nValue > nMaxValue))
			throwInvalidValue(sAttributeName, sValue);

		return (int64_t)nValue;
	}

	uint64_t getUnsignedInteger(const std::string& sAttributeName, uint64_t nDefaultValue)
	{
		if (!hasAttribute(sAttributeName))
			return nDefaultValue;

		std::string sValue = getString(sAttributeName, true, "");
		if (sValue.empty() || (sValue.find_first_not_of("0123456789") != std::string::npos))
			throwInvalidValue(sAttributeName, sValue);

		size_t nCharactersParsed = 0;
		unsigned long long nValue = 0;
		try {
			nValue = std::stoull(sValue, &nCharactersParsed);
		}
		catch (std::exception&) {
			throwInvalidValue(sAttributeName, sValue);
		}

		if (nCharactersParsed != sValue.length())
			throwInvalidValue(sAttributeName, sValue);

		return (uint64_t)nValue;
	}

	bool getBool(const std::string& sAttributeName, bool bDefaultValue)
	{
		if (!hasAttribute(sAttributeName))
			return bDefaultValue;

		std::string sValue = getString(sAttributeName, true, "");
		if ((sValue == "true") || (sValue == "1"))
			return true;
		if ((sValue == "false") || (sValue == "0"))
			return false;

		throwInvalidValue(sAttributeName, sValue);
	}
};


static eFaultPersistence parseFaultPersistence(const std::string& sValue, const std::string& sContext)
{
	if (sValue == "latched")
		return eFaultPersistence::Latched;
	if (sValue == "transient")
		return eFaultPersistence::Transient;

	throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, sContext + ": invalid fault persistence: " + sValue);
}

static std::string scannerStateToString(eScannerState state)
{
	switch (state) {
	case eScannerState::Idle: return "idle";
	case eScannerState::Busy: return "busy";
	case eScannerState::Error: return "error";
	case eScannerState::Disconnected: return "disconnected";
	default: return "unknown";
	}
}


/*************************************************************************************************************************
 Class definition of CDriver_ScannerEmulator 
**************************************************************************************************************************/

CDriver_ScannerEmulator::CDriver_ScannerEmulator(const std::string& sName, LibMCEnv::PDriverEnvironment pDriverEnvironment)
	: m_sName(sName), m_pDriverEnvironment(pDriverEnvironment)
{
	if (pDriverEnvironment.get() == nullptr)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM);
}

CDriver_ScannerEmulator::~CDriver_ScannerEmulator()
{
}

PScannerInstance CDriver_ScannerEmulator::parseScannerConfiguration(LibMCEnv::CXMLDocumentNode* pScannerNode)
{
	CScannerEmulatorConfigurationNode scannerNode(pScannerNode, "scanner", { "name", "x1", "y1", "x2", "y2", "unitsinmm", "laserindex", "fastmode", "timescale", "outofviewport", "maxpower" });

	std::string sScannerName = scannerNode.getString("name", true, "");
	std::string sContext = "scanner " + sScannerName;

	CScannerEmulatorConfigurationNode namedScannerNode(pScannerNode, sContext, { "name", "x1", "y1", "x2", "y2", "unitsinmm", "laserindex", "fastmode", "timescale", "outofviewport", "maxpower" });

	sEmulatorViewport viewport;
	viewport.m_dX1 = namedScannerNode.getDouble("x1", true, 0.0);
	viewport.m_dY1 = namedScannerNode.getDouble("y1", true, 0.0);
	viewport.m_dX2 = namedScannerNode.getDouble("x2", true, 0.0);
	viewport.m_dY2 = namedScannerNode.getDouble("y2", true, 0.0);
	double dUnitsInMM = namedScannerNode.getDouble("unitsinmm", true, 0.0);

	auto pInstance = std::make_shared<CScannerInstance>(sScannerName, viewport, dUnitsInMM, true);

	pInstance->setLaserIndex((uint32_t)namedScannerNode.getInteger("laserindex", 0, 0xFFFFFFFFLL, 0));
	pInstance->setFastMode(namedScannerNode.getBool("fastmode", false));
	pInstance->setTimeScale(namedScannerNode.getDouble("timescale", false, 1.0));
	pInstance->setMaxPowerInWatts(namedScannerNode.getDouble("maxpower", false, 100.0));

	std::string sOutOfViewportPolicy = namedScannerNode.getString("outofviewport", false, "fail");
	if (sOutOfViewportPolicy == "fail")
		pInstance->setOutOfViewportPolicy(eOutOfViewportPolicy::Fail);
	else if (sOutOfViewportPolicy == "clamp")
		pInstance->setOutOfViewportPolicy(eOutOfViewportPolicy::Clamp);
	else if (sOutOfViewportPolicy == "warn")
		pInstance->setOutOfViewportPolicy(eOutOfViewportPolicy::Warn);
	else
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, sContext + ": invalid value of attribute outofviewport: '" + sOutOfViewportPolicy + "'");

	std::set<std::string> foundChildren;
	auto pChildren = pScannerNode->GetChildren();
	uint64_t nChildCount = pChildren->GetNodeCount();
	for (uint64_t nChildIndex = 0; nChildIndex < nChildCount; nChildIndex++) {
		auto pChildNode = pChildren->GetNode(nChildIndex);
		std::string sChildName = pChildNode->GetName();

		if (foundChildren.find(sChildName) != foundChildren.end())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, sContext + ": duplicate element " + sChildName);
		foundChildren.insert(sChildName);

		if (sChildName == "defaults") {
			CScannerEmulatorConfigurationNode defaultsNode(pChildNode.get(), sContext + "/defaults", { "jumpspeed", "markspeed", "power", "laserondelay", "laseroffdelay", "jumpdelay", "markdelay", "polygondelay" });
			auto defaults = pInstance->getDefaultSettings();

			pInstance->setDefaultJumpSpeed(defaultsNode.getDouble("jumpspeed", false, defaults.m_dJumpSpeed));
			pInstance->setDefaultMarkSpeed(defaultsNode.getDouble("markspeed", false, defaults.m_dMarkSpeed));
			pInstance->setDefaultPower(defaultsNode.getDouble("power", false, defaults.m_dPower));

			sEmulatorDelays delays;
			delays.m_dLaserOnDelay = defaultsNode.getDouble("laserondelay", false, defaults.m_Delays.m_dLaserOnDelay);
			delays.m_dLaserOffDelay = defaultsNode.getDouble("laseroffdelay", false, defaults.m_Delays.m_dLaserOffDelay);
			delays.m_dJumpDelay = defaultsNode.getDouble("jumpdelay", false, defaults.m_Delays.m_dJumpDelay);
			delays.m_dMarkDelay = defaultsNode.getDouble("markdelay", false, defaults.m_Delays.m_dMarkDelay);
			delays.m_dPolygonDelay = defaultsNode.getDouble("polygondelay", false, defaults.m_Delays.m_dPolygonDelay);
			pInstance->setDefaultDelays(delays);
		}
		else if (sChildName == "skywriting") {
			CScannerEmulatorConfigurationNode skywritingNode(pChildNode.get(), sContext + "/skywriting", { "mode", "timelag", "laseronshift", "nprev", "npost", "limit" });
			auto defaults = pInstance->getDefaultSettings();

			sEmulatorSkywriting skywriting;
			skywriting.m_Mode = (eSkywritingMode)skywritingNode.getInteger("mode", 0, 4, (int64_t)defaults.m_Skywriting.m_Mode);
			skywriting.m_dTimelag = skywritingNode.getDouble("timelag", false, defaults.m_Skywriting.m_dTimelag);
			skywriting.m_dLaserOnShift = skywritingNode.getDouble("laseronshift", false, defaults.m_Skywriting.m_dLaserOnShift);
			skywriting.m_dNPrev = skywritingNode.getDouble("nprev", false, defaults.m_Skywriting.m_dNPrev);
			skywriting.m_dNPost = skywritingNode.getDouble("npost", false, defaults.m_Skywriting.m_dNPost);
			skywriting.m_dLimit = skywritingNode.getDouble("limit", false, defaults.m_Skywriting.m_dLimit);
			pInstance->setDefaultSkywriting(skywriting);
		}
		else if (sChildName == "transformation") {
			CScannerEmulatorConfigurationNode transformationNode(pChildNode.get(), sContext + "/transformation", { "m11", "m12", "m21", "m22", "offsetx", "offsety" });

			pInstance->setTransformationMatrix(transformationNode.getDouble("m11", false, 1.0), transformationNode.getDouble("m12", false, 0.0),
				transformationNode.getDouble("m21", false, 0.0), transformationNode.getDouble("m22", false, 1.0));
			pInstance->setTransformationOffset(transformationNode.getDouble("offsetx", false, 0.0), transformationNode.getDouble("offsety", false, 0.0));
		}
		else if (sChildName == "faults") {
			CScannerEmulatorConfigurationNode faultsNode(pChildNode.get(), sContext + "/faults", { "seed", "slowdownfactor" });

			pInstance->setFaultSeed(faultsNode.getUnsignedInteger("seed", 0));
			pInstance->setSlowdownFactor(faultsNode.getDouble("slowdownfactor", false, pInstance->getSlowdownFactor()));

			auto pFaultNodes = pChildNode->GetChildren();
			uint64_t nFaultCount = pFaultNodes->GetNodeCount();
			for (uint64_t nFaultIndex = 0; nFaultIndex < nFaultCount; nFaultIndex++) {
				auto pFaultNode = pFaultNodes->GetNode(nFaultIndex);
				std::string sFaultContext = sContext + "/faults/fault[" + std::to_string(nFaultIndex) + "]";

				if (pFaultNode->GetName() != "fault")
					throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, sContext + "/faults: unknown element " + pFaultNode->GetName());

				CScannerEmulatorConfigurationNode faultNode(pFaultNode.get(), sFaultContext, { "type", "trigger", "persistence", "command", "time", "execution", "probability" });

				eFaultType faultType = CEmulatorFaultInjector::stringToFaultType(faultNode.getString("type", true, ""));
				eFaultPersistence persistence = parseFaultPersistence(faultNode.getString("persistence", false, "latched"), sFaultContext);
				std::string sTrigger = faultNode.getString("trigger", true, "");

				sFaultTrigger trigger = {};
				trigger.m_FaultType = faultType;
				trigger.m_Persistence = persistence;

				if (sTrigger == "immediate") {
					pInstance->injectFault(faultType, persistence);
				}
				else if (sTrigger == "atcommand") {
					trigger.m_TriggerType = eFaultTriggerType::AtCommand;
					if (!faultNode.hasAttribute("command"))
						throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, sFaultContext + ": missing attribute command");
					trigger.m_nCommandIndex = (uint32_t)faultNode.getInteger("command", 0, 0xFFFFFFFFLL, 0);
					pInstance->addFaultTrigger(trigger);
				}
				else if (sTrigger == "attime") {
					trigger.m_TriggerType = eFaultTriggerType::AtTime;
					trigger.m_dTimeInMicroseconds = faultNode.getDouble("time", true, 0.0);
					pInstance->addFaultTrigger(trigger);
				}
				else if (sTrigger == "atexecution") {
					trigger.m_TriggerType = eFaultTriggerType::AtExecution;
					if (!faultNode.hasAttribute("execution"))
						throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, sFaultContext + ": missing attribute execution");
					trigger.m_nExecutionCountdown = (uint32_t)faultNode.getInteger("execution", 1, 0xFFFFFFFFLL, 1);
					pInstance->addFaultTrigger(trigger);
				}
				else if (sTrigger == "random") {
					pInstance->setRandomFault(faultType, persistence, faultNode.getDouble("probability", true, 0.0));
				}
				else {
					throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, sFaultContext + ": invalid fault trigger: " + sTrigger);
				}
			}
		}
		else {
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, sContext + ": unknown element " + sChildName);
		}
	}

	return pInstance;
}

void CDriver_ScannerEmulator::registerScannerParameters(const std::string& sScannerName)
{
	std::string sPrefix = sScannerName + "_";

	m_pDriverEnvironment->RegisterStringParameter(sPrefix + "state", "Scanner state", "idle");
	m_pDriverEnvironment->RegisterBoolParameter(sPrefix + "busy", "Execution is running", false);
	m_pDriverEnvironment->RegisterDoubleParameter(sPrefix + "position_x", "Position X in scanner units", 0.0);
	m_pDriverEnvironment->RegisterDoubleParameter(sPrefix + "position_y", "Position Y in scanner units", 0.0);
	m_pDriverEnvironment->RegisterBoolParameter(sPrefix + "laser_on", "Laser is on", false);
	m_pDriverEnvironment->RegisterDoubleParameter(sPrefix + "progress", "Progress of the execution", 0.0);
	m_pDriverEnvironment->RegisterIntegerParameter(sPrefix + "command_index", "Command index of the execution", 0);
	m_pDriverEnvironment->RegisterIntegerParameter(sPrefix + "execution_count", "Number of executions", 0);
	m_pDriverEnvironment->RegisterDoubleParameter(sPrefix + "last_duration_ms", "Emulated duration of the last execution in ms", 0.0);
	m_pDriverEnvironment->RegisterBoolParameter(sPrefix + "position_ok", "Scan head position is ok", true);
	m_pDriverEnvironment->RegisterBoolParameter(sPrefix + "temperature_ok", "Scan head temperature is ok", true);
	m_pDriverEnvironment->RegisterBoolParameter(sPrefix + "power_ok", "Scan head power is ok", true);
	m_pDriverEnvironment->RegisterStringParameter(sPrefix + "fault", "Active fault", "");
	m_pDriverEnvironment->RegisterIntegerParameter(sPrefix + "fault_count", "Number of faults", 0);
}

void CDriver_ScannerEmulator::Configure(const std::string& sConfigurationString)
{
	if (sConfigurationString.empty())
		return;

	auto pXMLDocument = m_pDriverEnvironment->ParseXMLString(sConfigurationString);
	std::string sNameSpace = pXMLDocument->GetDefaultNamespace();
	if (sNameSpace != SCANNEREMULATOR_CONFIGURATIONSCHEMA)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, "invalid configuration schema: " + sNameSpace);

	auto pRootNode = pXMLDocument->GetRootNode();
	auto pChildren = pRootNode->GetChildren();
	uint64_t nChildCount = pChildren->GetNodeCount();

	std::map<std::string, PScannerInstance> configuredScanners;
	std::map<uint32_t, std::string> laserIndices;

	for (uint64_t nChildIndex = 0; nChildIndex < nChildCount; nChildIndex++) {
		auto pChildNode = pChildren->GetNode(nChildIndex);
		std::string sChildName = pChildNode->GetName();
		if (sChildName != "scanner")
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, "unknown element in driver configuration: " + sChildName);

		PScannerInstance pInstance;
		try {
			pInstance = parseScannerConfiguration(pChildNode.get());
		}
		catch (ELibMCDriver_ScannerEmulatorInterfaceException& Exception) {
			if (Exception.getErrorCode() == LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION)
				throw;
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, std::string("invalid scanner configuration: ") + Exception.what());
		}

		std::string sScannerName = pInstance->getName();
		if (configuredScanners.find(sScannerName) != configuredScanners.end())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, "duplicate scanner name: " + sScannerName);

		uint32_t nLaserIndex = pInstance->getLaserIndex();
		if (nLaserIndex != 0) {
			auto iIter = laserIndices.find(nLaserIndex);
			if (iIter != laserIndices.end())
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDCONFIGURATION, "laser index " + std::to_string(nLaserIndex) + " is used by scanner " + iIter->second + " and " + sScannerName);
			laserIndices.insert(std::make_pair(nLaserIndex, sScannerName));
		}

		configuredScanners.insert(std::make_pair(sScannerName, pInstance));
	}

	std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
	for (auto& scannerIter : configuredScanners) {
		if (m_Scanners.find(scannerIter.first) != m_Scanners.end())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERALREADYEXISTS, "scanner already exists: " + scannerIter.first);

		registerScannerParameters(scannerIter.first);
		m_Scanners.insert(scannerIter);
		m_ConfiguredScanners.push_back(scannerIter.second);
	}

	m_pDriverEnvironment->LogMessage("Scanner emulator configured with " + std::to_string(configuredScanners.size()) + " scanner(s)");
}

std::string CDriver_ScannerEmulator::GetName()
{
	return m_sName;
}

std::string CDriver_ScannerEmulator::GetType()
{
	return "scanneremulator";
}

void CDriver_ScannerEmulator::GetVersion(LibMCDriver_ScannerEmulator_uint32& nMajor, LibMCDriver_ScannerEmulator_uint32& nMinor, LibMCDriver_ScannerEmulator_uint32& nMicro, std::string& sBuild)
{
	nMajor = LIBMCDRIVER_SCANNEREMULATOR_VERSION_MAJOR;
	nMinor = LIBMCDRIVER_SCANNEREMULATOR_VERSION_MINOR;
	nMicro = LIBMCDRIVER_SCANNEREMULATOR_VERSION_MICRO;
	sBuild = __STRINGIZE_VALUE_OF(__GITHASH);
}

void CDriver_ScannerEmulator::QueryParameters()
{
	QueryParametersEx(m_pDriverEnvironment->CreateStatusUpdateSession());
}

void CDriver_ScannerEmulator::QueryParametersEx(LibMCEnv::PDriverStatusUpdateSession pDriverUpdateInstance)
{
	if (pDriverUpdateInstance.get() == nullptr)
		return;

	std::vector<PScannerInstance> configuredScanners;
	{
		std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
		configuredScanners = m_ConfiguredScanners;
	}

	for (auto pInstance : configuredScanners) {
		std::string sPrefix = pInstance->getName() + "_";
		auto status = pInstance->getStatus();

		pDriverUpdateInstance->SetStringParameter(sPrefix + "state", scannerStateToString(status.m_State));
		pDriverUpdateInstance->SetBoolParameter(sPrefix + "busy", status.m_bBusy);
		pDriverUpdateInstance->SetDoubleParameter(sPrefix + "position_x", status.m_dPositionX);
		pDriverUpdateInstance->SetDoubleParameter(sPrefix + "position_y", status.m_dPositionY);
		pDriverUpdateInstance->SetBoolParameter(sPrefix + "laser_on", status.m_bLaserOn);
		pDriverUpdateInstance->SetDoubleParameter(sPrefix + "progress", status.m_dProgress);
		pDriverUpdateInstance->SetIntegerParameter(sPrefix + "command_index", status.m_nCommandIndex);
		pDriverUpdateInstance->SetIntegerParameter(sPrefix + "execution_count", status.m_nExecutionCount);
		pDriverUpdateInstance->SetDoubleParameter(sPrefix + "last_duration_ms", status.m_dLastDurationInMilliseconds);
		pDriverUpdateInstance->SetBoolParameter(sPrefix + "position_ok", status.m_bPositionOK);
		pDriverUpdateInstance->SetBoolParameter(sPrefix + "temperature_ok", status.m_bTemperatureOK);
		pDriverUpdateInstance->SetBoolParameter(sPrefix + "power_ok", status.m_bPowerOK);
		pDriverUpdateInstance->SetStringParameter(sPrefix + "fault", CEmulatorFaultInjector::faultTypeToString(status.m_ActiveFault));
		pDriverUpdateInstance->SetIntegerParameter(sPrefix + "fault_count", status.m_nFaultCount);
	}
}

PScannerInstance CDriver_ScannerEmulator::findScannerInstance(const std::string& sName, bool bMustExist)
{
	std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
	auto iIter = m_Scanners.find(sName);
	if (iIter != m_Scanners.end())
		return iIter->second;

	if (bMustExist)
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERNOTFOUND, "scanner not found: " + sName);

	return nullptr;
}

IScanner * CDriver_ScannerEmulator::CreateScanner(const std::string & sName, const LibMCDriver_ScannerEmulator_double dX1, const LibMCDriver_ScannerEmulator_double dY1, const LibMCDriver_ScannerEmulator_double dX2, const LibMCDriver_ScannerEmulator_double dY2, const LibMCDriver_ScannerEmulator_double dUnitsInMM)
{
	sEmulatorViewport viewport = { dX1, dY1, dX2, dY2 };
	auto pInstance = std::make_shared<CScannerInstance>(sName, viewport, dUnitsInMM, false);

	{
		std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
		if (m_Scanners.find(sName) != m_Scanners.end())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERALREADYEXISTS, "scanner already exists: " + sName);
		m_Scanners.insert(std::make_pair(sName, pInstance));
	}

	return new CScanner(pInstance, m_pDriverEnvironment);
}

bool CDriver_ScannerEmulator::HasScanner(const std::string & sName)
{
	return findScannerInstance(sName, false).get() != nullptr;
}

IScanner * CDriver_ScannerEmulator::FindScanner(const std::string & sName)
{
	return new CScanner(findScannerInstance(sName, true), m_pDriverEnvironment);
}

void CDriver_ScannerEmulator::RemoveScanner(const std::string & sName)
{
	std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
	auto iIter = m_Scanners.find(sName);
	if (iIter == m_Scanners.end())
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERNOTFOUND, "scanner not found: " + sName);

	if (iIter->second->isFromConfiguration())
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_CANNOTREMOVECONFIGUREDSCANNER, "scanner is part of the driver configuration: " + sName);

	if (iIter->second->isBusy())
		throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERBUSY, "scanner is busy: " + sName);

	m_Scanners.erase(iIter);

	for (auto& imageIter : m_ScannerImages)
		imageIter.second->unassignScanner(sName);
}

LibMCDriver_ScannerEmulator_uint32 CDriver_ScannerEmulator::GetScannerCount()
{
	std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
	return (uint32_t)m_Scanners.size();
}

IScanner * CDriver_ScannerEmulator::GetScanner(const LibMCDriver_ScannerEmulator_uint32 nIndex)
{
	PScannerInstance pInstance;
	{
		std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
		if (nIndex >= m_Scanners.size())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_INVALIDPARAM, "invalid scanner index: " + std::to_string(nIndex));

		auto iIter = m_Scanners.begin();
		std::advance(iIter, nIndex);
		pInstance = iIter->second;
	}

	return new CScanner(pInstance, m_pDriverEnvironment);
}

void CDriver_ScannerEmulator::DrawLayerMultiLaser(const std::string & sStreamUUID, const LibMCDriver_ScannerEmulator_uint32 nLayerIndex, const bool bFailIfNonAssignedDataExists)
{
	std::map<uint32_t, PScannerInstance> scannersByLaserIndex;
	{
		std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
		for (auto& scannerIter : m_Scanners) {
			uint32_t nLaserIndex = scannerIter.second->getLaserIndex();
			if (nLaserIndex == 0)
				continue;

			auto iExisting = scannersByLaserIndex.find(nLaserIndex);
			if (iExisting != scannersByLaserIndex.end())
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_DUPLICATELASERINDEX, "laser index " + std::to_string(nLaserIndex) + " is used by scanner " + iExisting->second->getName() + " and " + scannerIter.first);

			scannersByLaserIndex.insert(std::make_pair(nLaserIndex, scannerIter.second));
		}
	}

	auto pToolpathAccessor = m_pDriverEnvironment->CreateToolpathAccessor(sStreamUUID);
	auto pLayer = pToolpathAccessor->LoadLayer(nLayerIndex);

	if (bFailIfNonAssignedDataExists) {
		uint32_t nSegmentCount = pLayer->GetSegmentCount();
		for (uint32_t nSegmentIndex = 0; nSegmentIndex < nSegmentCount; nSegmentIndex++) {
			uint32_t nLaserIndex = (uint32_t)std::round(pLayer->GetSegmentProfileDoubleValueDef(nSegmentIndex, "", "laserindex", 0.0));
			if (scannersByLaserIndex.find(nLaserIndex) == scannersByLaserIndex.end())
				throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_LASERINDEXHASNOASSIGNEDSCANNER, "segment " + std::to_string(nSegmentIndex) + " has laser index " + std::to_string(nLaserIndex) + " without scanner");
		}
	}

	std::vector<std::pair<PScannerInstance, std::shared_ptr<CEmulatorCommandList>>> scannerLists;
	for (auto& scannerIter : scannersByLaserIndex) {
		auto pCommandList = std::make_shared<CEmulatorCommandList>();
		CScanner::buildLayerCommandList(scannerIter.second.get(), pLayer.get(), bFailIfNonAssignedDataExists, *pCommandList);
		scannerLists.push_back(std::make_pair(scannerIter.second, pCommandList));
	}

	for (auto& scannerList : scannerLists)
		scannerList.first->clearList();

	std::vector<PScannerInstance> startedScanners;
	try {
		for (auto& scannerList : scannerLists) {
			if (scannerList.second->getCommandCount() == 0)
				continue;

			scannerList.first->appendToList(*scannerList.second);
			scannerList.first->executeList();
			startedScanners.push_back(scannerList.first);
		}
	}
	catch (...) {
		for (auto pStartedScanner : startedScanners)
			pStartedScanner->stopExecution();
		throw;
	}

	std::exception_ptr pFirstException;
	for (auto pStartedScanner : startedScanners) {
		try {
			CScanner::waitForInstance(pStartedScanner.get(), m_pDriverEnvironment.get(), SCANNEREMULATOR_INFINITETIMEOUT);
		}
		catch (...) {
			if (!pFirstException)
				pFirstException = std::current_exception();
		}
	}

	if (pFirstException)
		std::rethrow_exception(pFirstException);
}

IScannerImage * CDriver_ScannerEmulator::CreateScannerImage(const std::string & sName, const LibMCDriver_ScannerEmulator_double dX1, const LibMCDriver_ScannerEmulator_double dY1, const LibMCDriver_ScannerEmulator_double dX2, const LibMCDriver_ScannerEmulator_double dY2, const LibMCDriver_ScannerEmulator_uint32 nPixelCountX, const LibMCDriver_ScannerEmulator_uint32 nPixelCountY)
{
	CScannerImageInstance::validateName(sName);
	{
		std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
		if (m_ScannerImages.find(sName) != m_ScannerImages.end())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERIMAGEALREADYEXISTS, "scanner image already exists: " + sName);
	}

	sScannerImageWindow window = { dX1, dY1, dX2, dY2 };
	auto pInstance = std::make_shared<CScannerImageInstance>(sName, window, nPixelCountX, nPixelCountY);

	{
		std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
		if (m_ScannerImages.find(sName) != m_ScannerImages.end())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERIMAGEALREADYEXISTS, "scanner image already exists: " + sName);
		m_ScannerImages.insert(std::make_pair(sName, pInstance));
	}

	return new CScannerImage(pInstance);
}

bool CDriver_ScannerEmulator::HasScannerImage(const std::string & sName)
{
	std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
	return m_ScannerImages.find(sName) != m_ScannerImages.end();
}

IScannerImage * CDriver_ScannerEmulator::FindScannerImage(const std::string & sName)
{
	PScannerImageInstance pInstance;
	{
		std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
		auto iIter = m_ScannerImages.find(sName);
		if (iIter == m_ScannerImages.end())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERIMAGENOTFOUND, "scanner image not found: " + sName);
		pInstance = iIter->second;
	}

	return new CScannerImage(pInstance);
}

void CDriver_ScannerEmulator::RemoveScannerImage(const std::string & sName)
{
	PScannerImageInstance pInstance;
	{
		std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
		auto iIter = m_ScannerImages.find(sName);
		if (iIter == m_ScannerImages.end())
			throw ELibMCDriver_ScannerEmulatorInterfaceException(LIBMCDRIVER_SCANNEREMULATOR_ERROR_SCANNERIMAGENOTFOUND, "scanner image not found: " + sName);
		pInstance = iIter->second;
		m_ScannerImages.erase(iIter);
	}

	pInstance->detach();
}

LibMCDriver_ScannerEmulator_uint32 CDriver_ScannerEmulator::GetScannerImageCount()
{
	std::lock_guard<std::mutex> lockGuard(m_ScannerMutex);
	return (uint32_t)m_ScannerImages.size();
}

