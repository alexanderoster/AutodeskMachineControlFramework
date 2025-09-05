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


Abstract: This is a stub class definition of CWorkingDirectory

*/

#include "libmcenv_workingdirectory.hpp"
#include "libmcenv_interfaceexception.hpp"
#include "libmcenv_workingfile.hpp"
#include "libmcenv_workingfileiterator.hpp"
#include "libmcenv_workingfilewriter.hpp"

// Include custom headers here.
#include "common_utils.hpp"
#include "common_exportstream_native.hpp"

using namespace LibMCEnv::Impl;

/*************************************************************************************************************************
 Class definition of CWorkingDirectory 
**************************************************************************************************************************/
CWorkingDirectory::CWorkingDirectory(AMC::PProcessDirectoryStructure pProcessDirectoryStructure, AMC::WProcessDirectory pProcessDirectory, AMC::PResourcePackage pDriverResourcePackage, AMC::PResourcePackage pMachineResourcePackage, AMCCommon::PChrono pGlobalChrono, AMC::PLogger pLogger)
    : m_pDriverResourcePackage (pDriverResourcePackage), 
    m_pMachineResourcePackage (pMachineResourcePackage),
    m_sTempFileNamePrefix ("amcf_"),
    m_pProcessDirectoryStructure (pProcessDirectoryStructure),
    m_pProcessDirectory (pProcessDirectory)

{
    if (pDriverResourcePackage.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
    if (pMachineResourcePackage.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
    if (pProcessDirectoryStructure.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

    /*

    m_pWorkingFileMonitor = std::make_shared<CWorkingFileMonitor> (sWorkingDirectoryPath, pGlobalChrono, pLogger); */

}


CWorkingDirectory::~CWorkingDirectory()
{
    try {

        auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
        if (pProcessDirectoryInstance.get() != nullptr) {
            pProcessDirectoryInstance->cleanUpDirectory(nullptr);
        }
    }
    catch (...) {

    }
    m_pProcessDirectory.reset();
    m_pProcessDirectoryStructure = nullptr;
}


bool CWorkingDirectory::IsActive()
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    return pProcessDirectoryInstance->isActive();
}

std::string CWorkingDirectory::GetAbsoluteFilePath()
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    return pProcessDirectoryInstance->getWorkingDirectory ();
}

AMC::WProcessDirectory CWorkingDirectory::getProcessDirectory()
{
    return m_pProcessDirectory;
}


IWorkingDirectory* CWorkingDirectory::CreateSubDirectory(const std::string& sDirectoryName)
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

	auto pSubDirectory = pProcessDirectoryInstance->createSubDirectory(sDirectoryName);

	return new CWorkingDirectory(m_pProcessDirectoryStructure, pSubDirectory, m_pDriverResourcePackage, m_pMachineResourcePackage, m_pProcessDirectoryStructure->getGlobalChrono(), m_pProcessDirectoryStructure->getLogger());

}


IWorkingFile * CWorkingDirectory::StoreCustomData(const std::string & sFileName, const LibMCEnv_uint64 nDataBufferBufferSize, const LibMCEnv_uint8 * pDataBufferBuffer)
{
    if (pDataBufferBuffer == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
 
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    std::string sAbsoluteFileName = pProcessDirectoryInstance->getAbsoluteFileName (sFileName);
    pProcessDirectoryInstance->addNewMonitoredFile(sFileName);

    auto pStream = std::make_unique<AMCCommon::CExportStream_Native>(sAbsoluteFileName);
    if (nDataBufferBufferSize > 0)
        pStream->writeBuffer(pDataBufferBuffer, nDataBufferBufferSize);
    pStream.reset();

    return new CWorkingFile (sFileName, m_pProcessDirectory);
}


IWorkingFile* CWorkingDirectory::StoreCustomString(const std::string& sFileName, const std::string& sDataString)
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    std::string sAbsoluteFileName = pProcessDirectoryInstance->getAbsoluteFileName(sFileName);
    pProcessDirectoryInstance->addNewMonitoredFile(sFileName);

    auto pStream = std::make_unique<AMCCommon::CExportStream_Native>(sAbsoluteFileName);
    if (sDataString.length () > 0)
        pStream->writeBuffer(sDataString.c_str (), sDataString.length ());
    pStream.reset();

    return new CWorkingFile(sFileName, m_pProcessDirectory);
}


IWorkingFile * CWorkingDirectory::StoreDriverData(const std::string & sFileName, const std::string & sIdentifier)
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    std::string sAbsoluteFileName = pProcessDirectoryInstance->getAbsoluteFileName(sFileName);
    pProcessDirectoryInstance->addNewMonitoredFile(sFileName);

    std::vector<uint8_t> Buffer;
    m_pDriverResourcePackage->readEntry(sIdentifier, Buffer);

    auto pStream = std::make_unique<AMCCommon::CExportStream_Native>(sAbsoluteFileName);
    if (Buffer.size () > 0)
        pStream->writeBuffer(Buffer.data(), Buffer.size ());
    pStream.reset();

    return new CWorkingFile(sFileName, m_pProcessDirectory);

}


IWorkingFile* CWorkingDirectory::StoreMachineResourceData(const std::string& sFileName, const std::string& sIdentifier)
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    std::string sAbsoluteFileName = pProcessDirectoryInstance->getAbsoluteFileName(sFileName);
    pProcessDirectoryInstance->addNewMonitoredFile(sFileName);

    std::vector<uint8_t> Buffer;
    m_pMachineResourcePackage->readEntry(sIdentifier, Buffer);

    auto pStream = std::make_unique<AMCCommon::CExportStream_Native>(sAbsoluteFileName);
    if (Buffer.size() > 0)
        pStream->writeBuffer(Buffer.data(), Buffer.size());
    pStream.reset();

    return new CWorkingFile(sFileName, m_pProcessDirectory);
}


std::string CWorkingDirectory::generateFileNameForExtension(const std::string& sExtension)
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);


    std::string sFilteredExtension = sExtension;
    if (!sFilteredExtension.empty()) {
        if (sExtension.length () > 64)
            throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_TEMPFILEEXTENSIONEXCEEDS64CHARACTERS);

        if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString (sExtension))
            throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_TEMPFILEEXTENSIONCONTAINSINVALIDCHARACTERS);

        sFilteredExtension = "." + sExtension;
    }
    
    uint32_t nMaxIterations = 64;
    for (uint32_t nIndex = 0; nIndex < nMaxIterations; nIndex++) {
        std::string sFileName = m_sTempFileNamePrefix + AMCCommon::CUtils::createUUID() + sFilteredExtension;
        std::string sAbsoluteFileName = pProcessDirectoryInstance->getAbsoluteFileName(sFileName);

        if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(sAbsoluteFileName))
            return sFileName;
    }

    throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COULDNOTGENERATETEMPFILENAME);

}

IWorkingFile* CWorkingDirectory::StoreCustomDataInTempFile(const std::string& sExtension, const LibMCEnv_uint64 nDataBufferBufferSize, const LibMCEnv_uint8* pDataBufferBuffer)
{
    std::string sFileName = generateFileNameForExtension(sExtension);
    return StoreCustomData(sFileName, nDataBufferBufferSize, pDataBufferBuffer);
}

IWorkingFile* CWorkingDirectory::StoreCustomStringInTempFile(const std::string& sExtension, const std::string& sDataString)
{
    std::string sFileName = generateFileNameForExtension(sExtension);
    return StoreCustomString(sFileName, sDataString);
}

IWorkingFile* CWorkingDirectory::StoreDriverDataInTempFile(const std::string& sExtension, const std::string& sIdentifier)
{
    std::string sFileName = generateFileNameForExtension(sExtension);
    return StoreDriverData (sFileName, sIdentifier);

}


IWorkingFile* CWorkingDirectory::StoreMachineResourceDataInTempFile(const std::string& sExtension, const std::string& sIdentifier)
{
    std::string sFileName = generateFileNameForExtension(sExtension);
    return StoreMachineResourceData(sFileName, sIdentifier);

}


bool CWorkingDirectory::CleanUp()
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    pProcessDirectoryInstance->cleanUpDirectory (nullptr);

    return !HasUnmanagedFiles();
}

IWorkingFile* CWorkingDirectory::AddManagedFile(const std::string& sFileName)
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    pProcessDirectoryInstance->addNewMonitoredFile(sFileName);

    return new CWorkingFile(sFileName, m_pProcessDirectory);
}

IWorkingFile* CWorkingDirectory::AddManagedTempFile(const std::string& sExtension)
{
    std::string sFileName = generateFileNameForExtension(sExtension);
    return AddManagedFile(sFileName);

}


bool CWorkingDirectory::HasUnmanagedFiles()
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    auto fileNames = AMCCommon::CUtils::findContentOfDirectory(pProcessDirectoryInstance->getWorkingDirectory(), true, false);
    for (auto sFileName : fileNames) {
        if (!pProcessDirectoryInstance->fileIsMonitored(sFileName))
            return true;
    }

    return false;
}

IWorkingFileIterator* CWorkingDirectory::RetrieveUnmanagedFiles()
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    auto pIterator = std::make_unique<CWorkingFileIterator>();

    auto fileNames = AMCCommon::CUtils::findContentOfDirectory(pProcessDirectoryInstance->getWorkingDirectory(), true, false);
    for (auto sFileName : fileNames) {
        if (!pProcessDirectoryInstance->fileIsMonitored (sFileName))
            pIterator->AddWorkingFile(std::make_shared<CWorkingFile>(sFileName, m_pProcessDirectory));
    }

    return pIterator.release();
}

IWorkingFileIterator* CWorkingDirectory::RetrieveManagedFiles()
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);


    auto pIterator = std::make_unique<CWorkingFileIterator>();

    auto fileNames = pProcessDirectoryInstance->getFileNames ();
    for (auto sFileName : fileNames) {
        pIterator->AddWorkingFile(std::make_shared<CWorkingFile>(sFileName, m_pProcessDirectory));
    }

    return pIterator.release();

}

IWorkingFileIterator* CWorkingDirectory::RetrieveAllFiles()
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    auto pIterator = std::make_unique<CWorkingFileIterator>();

    auto fileNames = AMCCommon::CUtils::findContentOfDirectory (pProcessDirectoryInstance->getWorkingDirectory(), true, false);
    for (auto sFileName : fileNames) {
        pIterator->AddWorkingFile(std::make_shared<CWorkingFile>(sFileName, m_pProcessDirectory));
    }

    return pIterator.release();
}

IWorkingFileWriter* CWorkingDirectory::AddBufferedWriter(const std::string& sFileName, const LibMCEnv_uint32 nBufferSizeInkB)
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    auto pInstance = pProcessDirectoryInstance->addNewFileWriter(sFileName, nBufferSizeInkB);
    return new CWorkingFileWriter (pInstance, m_pProcessDirectory);
}

IWorkingFileWriter* CWorkingDirectory::AddBufferedWriterTempFile(const std::string& sExtension, const LibMCEnv_uint32 nBufferSizeInkB)
{
    std::string sFileName = generateFileNameForExtension(sExtension);
    return AddBufferedWriter(sFileName, nBufferSizeInkB);
}


