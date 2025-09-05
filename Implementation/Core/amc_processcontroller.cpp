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

#include "amc_processcontroller.hpp"
#include "libmc_interfaceexception.hpp"

// Include custom headers here.
#include "Common/common_utils.hpp"

#define AMC_PROCESSCONTROLLER_MAXVARIABLECOUNT 1024
#define AMC_PROCESSCONTROLLER_PIPEREADBUFFER 4096

#include <vector>
#include <algorithm>
#include <functional>
#include <thread>
#include <array>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <fcntl.h>
#include <poll.h>
#endif

using namespace AMC;


void processControllerStdoutCallback(CProcessController* pController, const std::string& output) {

    if (pController == nullptr)
        return;

    std::string processed;
    processed.reserve(output.size());

    for (size_t i = 0; i < output.size(); ++i) {
        if (output[i] == '\r') {

            if (!processed.empty()) {
                pController->printToStdOut(processed);
                processed = "";
            }
        }
        else if (output[i] == '\n') {
            if (!processed.empty()) {
                pController->printToStdOut(processed);
                processed = "";
            }
        }
        else {
            processed += output[i];
        }
    }

}

void processControllerStderrCallback(CProcessController* pController, const std::string& output) {

    if (pController == nullptr)
        return;

    std::string processed;
    processed.reserve(output.size());

    for (size_t i = 0; i < output.size(); ++i) {
        if (output[i] == '\r') {
            // skip carriage return
            if (!processed.empty()) {
                pController->printToStdOut(processed);
                processed = "";
            }
        }
        else if (output[i] == '\n') {
            if (!processed.empty()) {
                pController->printToStdOut(processed);
                processed = "";
            }
        }
        else {
            processed += output[i];
        }
    }

}


#ifdef _WIN32

static void processControllerReadPipe(CProcessController* pController, HANDLE pipeHandle, std::function<void(CProcessController* pController, const std::string&)> callback)
{
    std::array<char, AMC_PROCESSCONTROLLER_PIPEREADBUFFER> readBufferUTF8;
    DWORD bytesRead = 0;

    while (true) {
        bool bSuccess = ReadFile(pipeHandle, readBufferUTF8.data (), (DWORD) (readBufferUTF8.size () - 1), &bytesRead, nullptr);
        if (!bSuccess || bytesRead == 0)
            break;

        if (bytesRead >= AMC_PROCESSCONTROLLER_PIPEREADBUFFER)
            throw ELibMCInterfaceException(LIBMC_ERROR_COULDNOTREADPROCESSPIPE);

        readBufferUTF8.at (bytesRead) = '\0';

        callback(pController, std::string (readBufferUTF8.data ()));
    }
}




#endif // _WIN32

CProcessController::CProcessController(const std::string& sAbsoluteExecutableName, WProcessDirectory pExecutableDirectory, AMCCommon::PChrono pGlobalChrono, AMC::PLogger pLogger)
	: m_sAbsoluteExecutableName (sAbsoluteExecutableName),
	m_nSystemStartTime(0),
	m_pGlobalChrono (pGlobalChrono),
	m_Status(eProcessControllerStatus::ProcessNotStarted),
    m_bTerminateThread (false),
    m_pLogger (pLogger),
    m_nExitCode (0),
    m_nTimeoutInMs (0),
    m_bVerboseLogging (false)


{
	if (pGlobalChrono.get() == nullptr)
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);
    if (pLogger.get() == nullptr)
        throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDPARAM);


	m_nSystemStartTime = pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970();
    m_pExecutableDirectory = pExecutableDirectory;
    m_pWorkingDirectory = pExecutableDirectory;
    m_sProcessSubsystemName = "process";

}

CProcessController::~CProcessController()
{

}


eProcessControllerStatus CProcessController::getStatus()
{
	return m_Status;
}



uint64_t CProcessController::getRunTimeInMilliseconds()
{
    return getRunTimeInMicroseconds() / 1000ULL;
}

uint64_t CProcessController::getRunTimeInMicroseconds()
{
    uint64_t nCurrentTime = m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970();
    if (nCurrentTime < m_nSystemStartTime)
        throw ELibMCInterfaceException(LIBMC_ERROR_PROCESSCONTROLTIMEISINVALID);

    uint64_t nRunTimeInMicroseconds = nCurrentTime - m_nSystemStartTime;
    return nRunTimeInMicroseconds;

}


void CProcessController::setWorkingDirectory(WProcessDirectory pDirectory)
{

   
    m_pWorkingDirectory = pDirectory;

}


void CProcessController::addEnvironmentVariable(const std::string & sVariableName, const std::string & sValue)
{
	if (m_EnvironmentVariables.size () >= AMC_PROCESSCONTROLLER_MAXVARIABLECOUNT)
		throw ELibMCInterfaceException(LIBMC_ERROR_REACHEDENVIRONMENTVARIABLELIMIT, sVariableName);

	if (m_Status != eProcessControllerStatus::ProcessNotStarted)
		throw ELibMCInterfaceException(LIBMC_ERROR_ENVIRONMENTVARIABLECHANGEAFTERSTART, sVariableName);

	if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString(sVariableName))
		throw ELibMCInterfaceException (LIBMC_ERROR_INVALIDENVIRONMENTVARIABLENAME, sVariableName);

	auto iIter = m_EnvironmentVariables.find(sVariableName);
	if (iIter != m_EnvironmentVariables.end())
		throw ELibMCInterfaceException(LIBMC_ERROR_ENVIRONMENTVARIABLEALREADYEXISTS, sVariableName);

	m_EnvironmentVariables.insert(std::make_pair (sVariableName, sValue));
	m_EnvironmentVariableKeys.push_back(sVariableName);
}

bool CProcessController::environmentVariableExists(const std::string & sVariableName)
{
	auto iIter = m_EnvironmentVariables.find(sVariableName);
	return (iIter != m_EnvironmentVariables.end());
}

void CProcessController::removeEnvironmentVariable(const std::string & sVariableName)
{
    if (m_Status != eProcessControllerStatus::ProcessNotStarted)
        throw ELibMCInterfaceException(LIBMC_ERROR_ENVIRONMENTVARIABLECHANGEAFTERSTART, sVariableName);

	m_EnvironmentVariables.erase(sVariableName);

	m_EnvironmentVariableKeys.erase (std::remove(m_EnvironmentVariableKeys.begin(), m_EnvironmentVariableKeys.end(), sVariableName), m_EnvironmentVariableKeys.end());

}

uint32_t CProcessController::getEnvironmentVariableCount()
{
	return (uint32_t)m_EnvironmentVariableKeys.size();
}

void CProcessController::getEnvironmentVariableByIndex(const uint32_t nVariableIndex, std::string & sVariableName, std::string & sValue)
{
	if ((size_t)nVariableIndex >= m_EnvironmentVariableKeys.size())
		throw ELibMCInterfaceException(LIBMC_ERROR_INVALIDENVIRONMENTVARIABLEINDEX, std::to_string (nVariableIndex) + "/" + std::to_string (m_EnvironmentVariableKeys.size()));

	sVariableName = m_EnvironmentVariableKeys.at(nVariableIndex);
	auto iIter = m_EnvironmentVariables.find(sVariableName);

	if (iIter == m_EnvironmentVariables.end ())
		throw ELibMCInterfaceException(LIBMC_ERROR_ENVIRONMENTVARIABLENOTFOUND, sVariableName);

	sValue = iIter->second;


}

void CProcessController::clearEnvironmentVariables()
{
    if (m_Status != eProcessControllerStatus::ProcessNotStarted)
        throw ELibMCInterfaceException(LIBMC_ERROR_ENVIRONMENTVARIABLECHANGEAFTERSTART);

	m_EnvironmentVariableKeys.clear();
	m_EnvironmentVariables.clear();
}


void CProcessController::startProcess(const std::string & sArgumentString, uint32_t nTimeOutInMs)
{

    if (m_Status != eProcessControllerStatus::ProcessNotStarted)
        throw ELibMCInterfaceException(LIBMC_ERROR_PROCESSHASALREADYBEENSTARTED);

    m_sArgumentString = sArgumentString;
    m_nTimeoutInMs = nTimeOutInMs;

    m_Status = eProcessControllerStatus::ProcessStarted;

#ifdef _WIN32
    m_Thread = std::make_unique<std::thread> (&CProcessController::processControllerRunProcessWinAPI, this);
#else 
    m_Thread = std::make_unique<std::thread>(&CProcessController::processControllerRunProcessLinux, this);
#endif // _WIN32

}

void CProcessController::terminateProcess()
{
    m_bTerminateThread = true;

    if (m_Thread.get() != nullptr) {
        if (m_Thread->joinable()) {
            m_Thread->join();
            m_Thread.reset();
        }
    }

    m_Status = eProcessControllerStatus::ProcessTerminated;
}


void CProcessController::processControllerRunProcessWinAPI ()
{

#ifdef _WIN32
    try {

        auto pExecutableDirectoryInstance = m_pExecutableDirectory.lock();
        if (pExecutableDirectoryInstance.get() == nullptr)
            throw ELibMCInterfaceException(LIBMC_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

        std::string sWorkingDirectory = pExecutableDirectoryInstance->getWorkingDirectory();

        if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(m_sAbsoluteExecutableName))
            throw ELibMCInterfaceException(LIBMC_ERROR_PROCESSEXECUTABLENOTFOUND, m_sAbsoluteExecutableName);
        if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(sWorkingDirectory))
            throw ELibMCInterfaceException(LIBMC_ERROR_WORKINGDIRECTORYNOTFOUND, m_sAbsoluteExecutableName);

        std::wstring sExecutablePathW = AMCCommon::CUtils::UTF8toUTF16(m_sAbsoluteExecutableName);
        std::wstring sWorkingDirectoryW = AMCCommon::CUtils::UTF8toUTF16(sWorkingDirectory);
        std::wstring sArgumentStringW = AMCCommon::CUtils::UTF8toUTF16(m_sArgumentString);

        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
        HANDLE hStdOutRead = 0;
        HANDLE hStdOutWrite = 0;
        HANDLE hStdErrRead = 0;
        HANDLE hStdErrWrite = 0;

        uint64_t nStartTime = m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970();

        try {

            if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0))
                throw ELibMCInterfaceException(LIBMC_ERROR_COULDNOTCREATEPROCESSPIPE, "stdout");

            if (!CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0))
                throw ELibMCInterfaceException(LIBMC_ERROR_COULDNOTCREATEPROCESSPIPE, "stderr");

            SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
            SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOW si = {};
            PROCESS_INFORMATION pi = {};
            si.cb = sizeof(si);
            si.dwFlags |= STARTF_USESTDHANDLES;
            si.hStdOutput = hStdOutWrite;
            si.hStdError = hStdErrWrite;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

            // Environment block
            std::wstring sEnvironmentBlockW;
            for (auto iEnvironmentVarIter : m_EnvironmentVariables) {
                std::string sEnvironmentVariable = iEnvironmentVarIter.first + "=" + iEnvironmentVarIter.second;
                std::wstring sEnvironmentVariableW = AMCCommon::CUtils::UTF8toUTF16(sEnvironmentVariable);
                sEnvironmentBlockW += sEnvironmentVariableW + L'\0';
            }
            sEnvironmentBlockW += L'\0';

            std::wstring commandLine = L"\"" + sExecutablePathW + L"\" " + sArgumentStringW;

            // Must be mutable
            std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
            cmdBuffer.push_back(L'\0');

            // Must be mutable
            std::vector<wchar_t> environmentBuffer(sEnvironmentBlockW.begin(), sEnvironmentBlockW.end());
            environmentBuffer.push_back(L'\0');


            BOOL bSuccess = CreateProcessW(
                sExecutablePathW.c_str(),
                cmdBuffer.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_UNICODE_ENVIRONMENT,
                (LPVOID)environmentBuffer.data(),
                sWorkingDirectoryW.c_str(),
                &si,
                &pi
            );

            CloseHandle(hStdOutWrite);
            hStdOutWrite = 0;

            CloseHandle(hStdErrWrite);
            hStdErrWrite = 0;

            if (!bSuccess) {
                throw ELibMCInterfaceException(LIBMC_ERROR_CREATEPROCESSFAILED, std::to_string(GetLastError()));
            }

            bool bTerminated = false;

            std::thread stdoutThread(processControllerReadPipe, this, hStdOutRead, processControllerStdoutCallback);
            std::thread stderrThread(processControllerReadPipe, this, hStdErrRead, processControllerStderrCallback);


            while (true) {

                uint64_t nCurrentTime = m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970();
                if (m_nTimeoutInMs > 0) {
                    uint64_t elapsedMs = (nCurrentTime - nStartTime) / 1000;
                    if (elapsedMs > m_nTimeoutInMs) {
                        m_pLogger->logMessage ("Process timed out after " + std::to_string(m_nTimeoutInMs) + " milliseconds. Terminating.", "process", AMC::eLogLevel::FatalError);

                        TerminateProcess(pi.hProcess, 1);
                        m_Status = eProcessControllerStatus::ProcessTimedOut;
                        break;
                    }
                }

                DWORD waitResult = WaitForSingleObject(pi.hProcess, 100); // 100ms polling

                if (waitResult == WAIT_OBJECT_0) {
                    break; // Process exited normally
                }

                if (m_bTerminateThread) {
                    TerminateProcess(pi.hProcess, 1);
                    bTerminated = true;
                    break;
                }

                Sleep(10); // Reduce CPU usage slightly
            }

            DWORD exitCode = 0;
            if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
                m_nExitCode = static_cast<int>(exitCode);
            }
            else {
                m_nExitCode = -1; // or special error value
            }

            stdoutThread.join();
            stderrThread.join();

            CloseHandle(hStdOutRead);
            hStdOutRead = 0;

            CloseHandle(hStdErrRead);
            hStdErrRead = 0;

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            if (m_Status == eProcessControllerStatus::ProcessStarted) {
                m_Status = eProcessControllerStatus::ProcessFinished;
            }


        }
        catch (std::exception &)
        {
            if (hStdOutRead != nullptr)
                CloseHandle(hStdOutRead);

            if (hStdOutWrite != nullptr)
                CloseHandle(hStdOutWrite);

            if (hStdErrRead != nullptr)
                CloseHandle(hStdErrRead);

            if (hStdErrWrite != nullptr)
                CloseHandle(hStdErrWrite);

            m_Status = eProcessControllerStatus::ProcessTerminated;

        }
    }
    catch (std::exception &) {
        m_Status = eProcessControllerStatus::ProcessTerminated;
    }
#else
    throw ELibMCInterfaceException(LIBMC_ERROR_PROCESSHANDLINGNOTIMPLEMENTEDONPLATFORM);
#endif //_WIN32

}

void CProcessController::processControllerRunProcessLinux() {
#ifndef _WIN32

    auto pExecutableDirectoryInstance = m_pExecutableDirectory.lock();
    if (pExecutableDirectoryInstance.get() == nullptr)
        throw ELibMCInterfaceException(LIBMC_ERROR_WORKINGDIRECTORYCEASEDTOEXIST);

    std::string sWorkingDirectory = pExecutableDirectoryInstance->getWorkingDirectory();

    if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(m_sAbsoluteExecutableName))
        throw ELibMCInterfaceException(LIBMC_ERROR_PROCESSEXECUTABLENOTFOUND, m_sAbsoluteExecutableName);
    if (!AMCCommon::CUtils::fileOrPathExistsOnDisk(sWorkingDirectory))
        throw ELibMCInterfaceException(LIBMC_ERROR_WORKINGDIRECTORYNOTFOUND, m_sAbsoluteExecutableName);


    int stdoutPipe[2];
    int stderrPipe[2];

    if (pipe(stdoutPipe) != 0)
        throw ELibMCInterfaceException(LIBMC_ERROR_COULDNOTCREATEPROCESSPIPE, "stdout");

    if (pipe(stderrPipe) != 0)
        throw ELibMCInterfaceException(LIBMC_ERROR_COULDNOTCREATEPROCESSPIPE, "stderr");

    pid_t pid = fork();
    if (pid < 0) {
        throw ELibMCInterfaceException(LIBMC_ERROR_CREATEPROCESSFAILED, "fork failed");
    }

    if (pid == 0) {
        // Child process
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);

        close(stdoutPipe[0]);
        close(stdoutPipe[1]);
        close(stderrPipe[0]);
        close(stderrPipe[1]);

        // Set working directory
        if (chdir(sWorkingDirectory.c_str()) != 0) {
            _exit(127);
        }

        // Build arguments
        std::vector<std::string> args = { m_sAbsoluteExecutableName };
        if (!m_sArgumentString.empty()) {
            std::istringstream iss(m_sArgumentString);
            std::string token;
            while (iss >> std::quoted(token))
                args.push_back(token);
        }

        std::vector<char*> argv;
        for (auto& arg : args)
            argv.push_back(const_cast<char*>(arg.c_str()));
        argv.push_back(nullptr);

        // Build environment
        std::vector<std::string> envStrings;
        for (auto& [key, value] : m_EnvironmentVariables)
            envStrings.push_back(key + "=" + value);
        std::vector<char*> envp;
        for (auto& env : envStrings)
            envp.push_back(const_cast<char*>(env.c_str()));
        envp.push_back(nullptr);

        execve(m_sAbsoluteExecutableName.c_str(), argv.data(), envp.data());
        _exit(127); // execve failed
    }

    // Parent process
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    int stdoutFd = stdoutPipe[0];
    int stderrFd = stderrPipe[0];

    std::array<char, AMC_PROCESSCONTROLLER_PIPEREADBUFFER> buffer;
    bool processRunning = true;
    uint64_t nStartTime = m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970();

    while (processRunning) {
        struct pollfd fds[2];
        fds[0].fd = stdoutFd;
        fds[0].events = POLLIN;
        fds[1].fd = stderrFd;
        fds[1].events = POLLIN;

        int ret = poll(fds, 2, 100); // 100 ms timeout
        if (ret > 0) {
            for (int i = 0; i < 2; ++i) {
                if (fds[i].revents & POLLIN) {
                    ssize_t count = read(fds[i].fd, buffer.data(), buffer.size() - 1);
                    if (count > 0) {
                        buffer[count] = '\0';
                        std::string output(buffer.data());
                        if (i == 0)
                            processControllerStdoutCallback(this, output);
                        else
                            processControllerStderrCallback(this, output);
                    }
                }
            }
        }

        // Check timeout
        if (m_nTimeoutInMs > 0) {
            uint64_t nCurrentTime = m_pGlobalChrono->getUTCTimeStampInMicrosecondsSince1970();
            if ((nCurrentTime - nStartTime) / 1000 > m_nTimeoutInMs) {
                m_pLogger->logMessage("Process timed out after " + std::to_string(m_nTimeoutInMs) + " milliseconds. Terminating.", "process", AMC::eLogLevel::FatalError);
                kill(pid, SIGKILL);
                m_Status = eProcessControllerStatus::ProcessTimedOut;
                processRunning = false;
            }
        }

        // Check termination
        if (m_bTerminateThread) {
            kill(pid, SIGTERM);
            m_Status = eProcessControllerStatus::ProcessTerminated;
            processRunning = false;
        }

        // Check if child exited
        int status = 0;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result > 0) {
            if (WIFEXITED(status)) {
                m_nExitCode = WEXITSTATUS(status);
            }
            else {
                m_nExitCode = -1;
            }
            processRunning = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(stdoutFd);
    close(stderrFd);

    if (m_Status == eProcessControllerStatus::ProcessStarted)
        m_Status = eProcessControllerStatus::ProcessFinished;

#else
    throw ELibMCInterfaceException(LIBMC_ERROR_PROCESSHANDLINGNOTIMPLEMENTEDONPLATFORM);
#endif
}


int32_t CProcessController::getExitCode()
{
    return m_nExitCode;
}


void CProcessController::printToStdOut(const std::string& sLine)
{
	m_StdOutBuffer.push_back(sLine);
    if (m_bVerboseLogging)
        m_pLogger->logMessage(sLine, m_sProcessSubsystemName, AMC::eLogLevel::Message);
}

void CProcessController::printToStdErr(const std::string& sLine)
{
	m_StdErrBuffer.push_back(sLine);
    if (m_bVerboseLogging)
        m_pLogger->logMessage(sLine, m_sProcessSubsystemName, AMC::eLogLevel::Warning);
}

void CProcessController::clearOutputBuffers()
{
	m_StdOutBuffer.clear();
	m_StdErrBuffer.clear();
}

void CProcessController::setVerboseLogging(bool bVerboseLogging)
{
    m_bVerboseLogging = bVerboseLogging;
}

