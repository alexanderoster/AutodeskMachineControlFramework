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


Abstract: This is a stub class definition of CWorkingFile

*/

#include "libmcenv_workingfile.hpp"
#include "libmcenv_interfaceexception.hpp"
#include "libmcenv_workingfileprocess.hpp"

// Include custom headers here.
#include "common_utils.hpp"
#include "common_importstream_native.hpp"
#include <cstring>

using namespace LibMCEnv::Impl;





/*************************************************************************************************************************
 Class definition of CWorkingFile 
**************************************************************************************************************************/

CWorkingFile::CWorkingFile(const std::string & sFileName, AMC::WProcessDirectory pProcessDirectory)
    : m_pProcessDirectory(pProcessDirectory)
{

    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException (LIBMCENV_ERROR_WORKINGFILECEASEDTOEXIST);

    m_sFileName = sFileName;
    m_sAbsolutePath = pProcessDirectoryInstance->getAbsoluteFileName(sFileName);
}


CWorkingFile* CWorkingFile::makeFrom(CWorkingFile* pWorkingFile)
{
    if (pWorkingFile == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

    return new CWorkingFile (pWorkingFile->m_sFileName, pWorkingFile->m_pProcessDirectory);
}


std::shared_ptr<CWorkingFile> CWorkingFile::makeSharedFrom(CWorkingFile* pWorkingFile)
{
    return std::shared_ptr <CWorkingFile> (makeFrom (pWorkingFile));
}

std::string CWorkingFile::GetAbsoluteFileName()
{
    return m_sAbsolutePath;
}

LibMCEnv_uint64 CWorkingFile::GetSize()
{
    if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(m_sAbsolutePath))
        return 0;

    auto pStream = std::make_shared<AMCCommon::CImportStream_Native> (m_sAbsolutePath);
    return pStream->retrieveSize();
}

void CWorkingFile::ReadContent(LibMCEnv_uint64 nFileContentBufferSize, LibMCEnv_uint64* pFileContentNeededCount, LibMCEnv_uint8* pFileContentBuffer)
{
    if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(m_sAbsolutePath))
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGFILEDOESNOTEXIST);

    auto pStream = std::make_shared<AMCCommon::CImportStream_Native>(m_sAbsolutePath);
    uint64_t nFileSize = pStream->retrieveSize();

	if (pFileContentNeededCount != nullptr)
		*pFileContentNeededCount = nFileSize;

	if (pFileContentBuffer != nullptr) {
		if (nFileContentBufferSize < nFileSize)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

        if (nFileSize > 0)
		    pStream->readBuffer (pFileContentBuffer, nFileSize, true);
	}
}


std::string CWorkingFile::CalculateSHA2()
{
    return AMCCommon::CUtils::calculateSHA256FromFile(m_sAbsolutePath);
}


IWorkingFileProcess* CWorkingFile::ExecuteFile()
{
	return new CWorkingFileProcess(m_sAbsolutePath, m_pProcessDirectory);
}


bool CWorkingFile::IsManaged()
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        return false;

    return pProcessDirectoryInstance->fileIsMonitored (m_sFileName);
}

void CWorkingFile::MakeManaged()
{
    auto pProcessDirectoryInstance = m_pProcessDirectory.lock();
    if (pProcessDirectoryInstance.get() == nullptr)
        throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_WORKINGFILECEASEDTOEXIST);

    pProcessDirectoryInstance->addNewMonitoredFile(m_sFileName);
}

bool CWorkingFile::FileExists()
{
    return AMCCommon::CUtils::fileOrPathExistsOnDisk(m_sAbsolutePath);
}

bool CWorkingFile::DeleteFromDisk()
{
    if (AMCCommon::CUtils::fileOrPathExistsOnDisk(m_sAbsolutePath)) {
        AMCCommon::CUtils::deleteFileFromDisk(m_sAbsolutePath, false);

        return !AMCCommon::CUtils::fileOrPathExistsOnDisk(m_sAbsolutePath);
    }

    return true;
}

