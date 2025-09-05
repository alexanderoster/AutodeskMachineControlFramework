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


Abstract: This is a stub class definition of CWorkingFileProcess

*/

#include "amc_processdirectory.hpp"
#include "libmc_interfaceexception.hpp"

// Include custom headers here.
#include "Common/common_utils.hpp"
#include <cstring>

#define WORKINGFILEBUFFER_MINIMUMSIZEINKB 1
#define WORKINGFILEBUFFER_MAXIMUMSIZEINKB (1024 * 1024)

using namespace AMC;

CProcessDirectoryWriter::CProcessDirectoryWriter(const std::string& sLocalFileName, const std::string& sAbsoluteFileName, uint32_t nMemoryBufferSizeInKB)
    : m_sLocalFileName(sLocalFileName), m_sAbsoluteFileName(sAbsoluteFileName), m_nPositionInBuffer(0), m_nBytesWritten(0)
{
    if ((nMemoryBufferSizeInKB < WORKINGFILEBUFFER_MINIMUMSIZEINKB) ||
        (nMemoryBufferSizeInKB > WORKINGFILEBUFFER_MAXIMUMSIZEINKB)) {

        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDWRITEBUFFERSIZE, std::to_string(nMemoryBufferSizeInKB));
    }

    m_MemoryBuffer.resize(nMemoryBufferSizeInKB * 1024);

    m_pExportStream = std::make_shared <AMCCommon::CExportStream_Native>(sAbsoluteFileName);

}

CProcessDirectoryWriter::~CProcessDirectoryWriter()
{
    finish();
}

std::string CProcessDirectoryWriter::getAbsoluteFileName()
{
    return m_sAbsoluteFileName;
}

std::string CProcessDirectoryWriter::getLocalFileName()
{
    return m_sLocalFileName;
}

void CProcessDirectoryWriter::writeData(const uint8_t* pData, uint64_t nSize)
{
    uint64_t nBufferSize = m_MemoryBuffer.size();

    if (nSize == 0)
        return;

    if (pData == nullptr)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

    const uint8_t* pSource = pData;

    uint64_t nBytesLeft = nSize;
    while (nBytesLeft > 0) {
        if (m_nPositionInBuffer >= nBufferSize)
            throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDWRITEBUFFFERPOSITION, std::to_string(m_nPositionInBuffer));

        uint64_t nBytesAvailable = nBufferSize - m_nPositionInBuffer;
        uint64_t nBytesToCopy;

        if (nBytesLeft > nBytesAvailable)
            nBytesToCopy = nBytesAvailable;
        else
            nBytesToCopy = nBytesLeft;

        memcpy((void*)&m_MemoryBuffer.at(m_nPositionInBuffer), (void*)pSource, nBytesToCopy);
        pSource += nBytesToCopy;
        m_nPositionInBuffer += nBytesToCopy;

        if (m_nPositionInBuffer >= nBufferSize)
            flushBuffer();

        nBytesLeft -= nBytesToCopy;
    }
}

void CProcessDirectoryWriter::flushBuffer()
{
    if (m_nPositionInBuffer > 0) {
        if (m_pExportStream.get() == nullptr)
            throw ELibMCInterfaceException(LIBMC_ERROR_CANNOTWRITETOFINISHEDFILE);

        m_pExportStream->writeBuffer(m_MemoryBuffer.data(), m_nPositionInBuffer);
    }

    m_nPositionInBuffer = 0;
}

void CProcessDirectoryWriter::finish()
{
    flushBuffer();

    if (m_pExportStream.get() != nullptr) {
        m_pExportStream = nullptr;
    }
}

bool CProcessDirectoryWriter::isFinished()
{
    return (m_pExportStream.get() == nullptr);
}


uint64_t CProcessDirectoryWriter::getWrittenBytes()
{
    return m_nBytesWritten;
}


CProcessDirectory::CProcessDirectory(CProcessDirectoryStructure* pOwner, const std::string& sWorkingDirectory)
    : m_sWorkingDirectory(sWorkingDirectory), m_bIsActive(true), m_pOwner(pOwner)
{
    if (sWorkingDirectory.empty())
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
    if (pOwner == nullptr)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

    if (!AMCCommon::CUtils::fileOrPathExistsOnDisk (sWorkingDirectory))
        throw ELibMCInterfaceException(LIBMC_ERROR_PROCESSWORKINGDIRECTORYDOESNOTEXIST);
}

CProcessDirectory::~CProcessDirectory()
{
    try {
        cleanUpDirectory(nullptr);
    }
    catch (std::exception&) {
        // We do not throw exceptions in destructors, so we just ignore the error.
        // This is a best-effort cleanup.
    }
}

std::string CProcessDirectory::getWorkingDirectory()
{
    return m_sWorkingDirectory;
}


std::string CProcessDirectory::getAbsoluteFileName(const std::string& sFileName)
{
    if (!m_bIsActive)
        throw ELibMCInterfaceException(LIBMC_ERROR_WORKINGDIRECTORYHASBEENCLEANED, m_sWorkingDirectory);

    if (sFileName.find(":") != std::string::npos)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCHARACTERINFILENAME, sFileName);
    if (sFileName.find(">") != std::string::npos)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCHARACTERINFILENAME, sFileName);
    if (sFileName.find("<") != std::string::npos)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCHARACTERINFILENAME, sFileName);
    if (sFileName.find("\"") != std::string::npos)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCHARACTERINFILENAME, sFileName);
    if (sFileName.find("/") != std::string::npos)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCHARACTERINFILENAME, sFileName);
    if (sFileName.find("\\") != std::string::npos)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCHARACTERINFILENAME, sFileName);
    if (sFileName.find("|") != std::string::npos)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCHARACTERINFILENAME, sFileName);
    if (sFileName.find("?") != std::string::npos)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCHARACTERINFILENAME, sFileName);
    if (sFileName.find("*") != std::string::npos)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCHARACTERINFILENAME);
    if (sFileName.find("..") != std::string::npos)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDCHARACTERINFILENAME);

    return m_sWorkingDirectory + "/" + sFileName;
}

PProcessDirectory CProcessDirectory::createSubDirectory(const std::string& sDirectoryName)
{
	std::string sAbsoluteDirectoryName = getAbsoluteFileName(sDirectoryName);
    if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(sAbsoluteDirectoryName)) {
        AMCCommon::CUtils::createDirectoryOnDisk(sAbsoluteDirectoryName);
    } else {
        throw ELibMCInterfaceException(LIBMC_ERROR_DIRECTORYALREADYEXISTS, sAbsoluteDirectoryName);
    }
    auto pSubDirectory = std::make_shared<CProcessDirectory>(m_pOwner, sAbsoluteDirectoryName);
    m_SubDirectories.insert(std::make_pair(sDirectoryName, pSubDirectory));
	return pSubDirectory;

}

void CProcessDirectory::addNewMonitoredFile(const std::string& sFileName)
{
    if (!m_bIsActive)
        throw ELibMCInterfaceException(LIBMC_ERROR_WORKINGDIRECTORYHASBEENCLEANED, m_sWorkingDirectory);

    // Check if file name has the right character set.
    getAbsoluteFileName(sFileName);

    // Store file name
    m_MonitoredFileNames.insert(sFileName);
}

PProcessDirectoryWriter CProcessDirectory::addNewFileWriter(const std::string& sFileName, uint32_t nMemoryBufferSize)
{
    if (!m_bIsActive)
        throw ELibMCInterfaceException(LIBMC_ERROR_WORKINGDIRECTORYHASBEENCLEANED, m_sWorkingDirectory);

    std::string sAbsoluteFileName = getAbsoluteFileName(sFileName);

    auto pInstance = std::make_shared <CProcessDirectoryWriter>(sFileName, sAbsoluteFileName, nMemoryBufferSize);
    m_WriterInstances.insert(std::make_pair(sFileName, pInstance));

    addNewMonitoredFile(sFileName);

    return pInstance;
}


bool CProcessDirectory::fileIsMonitored(const std::string& sFileName)
{
    if (!m_bIsActive)
        throw ELibMCInterfaceException(LIBMC_ERROR_WORKINGDIRECTORYHASBEENCLEANED, m_sWorkingDirectory);

    auto iIter = m_MonitoredFileNames.find(sFileName);
    return (iIter != m_MonitoredFileNames.end());
}

void CProcessDirectory::cleanUpDirectory(AMC::CLogger* pLoggerForUnmanagedFileWarnings)
{

    m_SubDirectories.clear();

    if (AMCCommon::CUtils::fileOrPathExistsOnDisk(m_sWorkingDirectory)) {
        for (auto sFileName : m_MonitoredFileNames) {
            auto sAbsoluteFileName = getAbsoluteFileName(sFileName);
            if (AMCCommon::CUtils::fileOrPathExistsOnDisk(sAbsoluteFileName)) {
                try
                {
                    AMCCommon::CUtils::deleteFileFromDisk(sAbsoluteFileName, true);
                }
                catch (std::exception& E)
                {
                    if (pLoggerForUnmanagedFileWarnings != nullptr)
                        pLoggerForUnmanagedFileWarnings->logMessage("Could not delete temporary file: " + sAbsoluteFileName + "(" + E.what() + ")", LOG_SUBSYSTEM_WORKINGDIRECTORIES, AMC::eLogLevel::Warning);
                }

            }
        }
    }


    try {
        if (AMCCommon::CUtils::fileOrPathExistsOnDisk(m_sWorkingDirectory)) {
            AMCCommon::CUtils::deleteDirectoryFromDisk(m_sWorkingDirectory, true);
        }
    }
    catch (std::exception& E)
    {
        if (pLoggerForUnmanagedFileWarnings != nullptr)
            pLoggerForUnmanagedFileWarnings->logMessage("Could not delete working directory: " + m_sWorkingDirectory + "(" + E.what() + ")", LOG_SUBSYSTEM_WORKINGDIRECTORIES, AMC::eLogLevel::Warning);
    }

    m_MonitoredFileNames.clear();
    m_bIsActive = false;

}

bool CProcessDirectory::isActive()
{
    return m_bIsActive;
}


std::set<std::string> CProcessDirectory::getFileNames()
{
    return m_MonitoredFileNames;
}

AMCCommon::PChrono CProcessDirectory::getGlobalChrono()
{
    return m_pOwner->getGlobalChrono();
}

AMC::PLogger CProcessDirectory::getLogger()
{
	return m_pOwner->getLogger();
}


CProcessDirectoryStructure::CProcessDirectoryStructure(const std::string& sBasePath, AMCCommon::PChrono pGlobalChrono, AMC::PLogger pLogger)
    : m_pGlobalChrono (pGlobalChrono), m_pLogger (pLogger)
{
    if (pGlobalChrono.get() == nullptr)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
    if (pLogger.get() == nullptr)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);

    auto sWorkingDirectoryPath = AMCCommon::CUtils::findTemporaryFileName(sBasePath, "amcf_", "", 1024);
    AMCCommon::CUtils::createDirectoryOnDisk(sWorkingDirectoryPath);

    m_pRootDirectory = std::make_shared<CProcessDirectory> (this, sWorkingDirectoryPath);
}

CProcessDirectoryStructure::~CProcessDirectoryStructure()
{

}

PProcessDirectory CProcessDirectoryStructure::getRootDirectory()
{
    return m_pRootDirectory;
}

AMCCommon::PChrono CProcessDirectoryStructure::getGlobalChrono()
{
    return m_pGlobalChrono;
}

AMC::PLogger CProcessDirectoryStructure::getLogger()
{
    return m_pLogger;
}

