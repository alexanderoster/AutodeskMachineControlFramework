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

#include "libmcdriver_scanlabsmc_sdk.hpp"
#include "libmcdriver_scanlabsmc_interfaceexception.hpp"

#ifdef _WIN32
#include <windows.h>
#else // _WIN32
#include <dlfcn.h>
#endif // _WIN32


#include <vector>
#include <iostream>
#include <string>

using namespace LibMCDriver_ScanLabSMC::Impl;


#define SCANLAB_MAXDLLNAMELENGTH 1024 * 1024

#ifdef _WIN32
void* _loadScanLabSMCAddress (HMODULE hLibrary, const char * pSymbolName, bool bMandatory = true) {
	void * pFuncPtr = (void*) GetProcAddress(hLibrary, pSymbolName);
	if (bMandatory) {
		if (pFuncPtr == nullptr)
			throw ELibMCDriver_ScanLabSMCInterfaceException(LIBMCDRIVER_SCANLABSMC_ERROR_COULDNOTFINDLIBRARYEXPORT, "could not find library export: " + std::string(pSymbolName));
	}

	return pFuncPtr;
}
#else
void* _loadScanLabSMCAddress(void * hLibrary, const char* pSymbolName, bool bMandatory = true) {

	void* pFuncPtr = (void*) dlsym(hLibrary, pSymbolName);
	dlerror();
	if (bMandatory) {
		if (pFuncPtr == nullptr)
			throw ELibMCDriver_ScanLabSMCInterfaceException(LIBMCDRIVER_SCANLABSMC_ERROR_COULDNOTFINDLIBRARYEXPORT, "could not find library export: " + std::string(pSymbolName));
	}

	return pFuncPtr;
}
#endif


CScanLabSMCSDK::CScanLabSMCSDK(const std::string& sDLLNameUTF8)
	: m_LibraryHandle (nullptr), m_bIsInitialized (false)
{

	resetFunctionPtrs();

#ifdef _WIN32

	if (sDLLNameUTF8.length() > SCANLAB_MAXDLLNAMELENGTH)
		throw ELibMCDriver_ScanLabSMCInterfaceException(LIBMCDRIVER_SCANLABSMC_ERROR_INVALIDPARAM);

	// Convert filename to UTF16-string
	int nLength = (int)sDLLNameUTF8.length ();
	int nBufferSize = nLength * 2 + 2;
	std::vector<wchar_t> wsLibraryFileName(nBufferSize);
	int nResult = MultiByteToWideChar(CP_UTF8, 0, sDLLNameUTF8.c_str(), nLength, &wsLibraryFileName[0], nBufferSize);
	if (nResult == 0)
		throw ELibMCDriver_ScanLabSMCInterfaceException (LIBMCDRIVER_SCANLABSMC_ERROR_COULDNOTLOADLIBRARY);

	HMODULE hLibrary = LoadLibraryW(wsLibraryFileName.data());
	if (hLibrary == 0)
		throw ELibMCDriver_ScanLabSMCInterfaceException(LIBMCDRIVER_SCANLABSMC_ERROR_COULDNOTLOADLIBRARY);
#else // _WIN32
	void* hLibrary = dlopen(sDLLNameUTF8.c_str(), RTLD_LAZY);
	if (hLibrary == 0) {
		dlerror();
		throw ELibMCDriver_ScanLabSMCInterfaceException(LIBMCDRIVER_SCANLABSMC_ERROR_COULDNOTLOADLIBRARY);
	}
#endif // _WIN32


	this->slsc_cfg_initialize_from_file = (PScanLabSMCPtr_slsc_cfg_initialize_from_file)_loadScanLabSMCAddress(hLibrary, "slsc_cfg_initialize_from_file");
	this->slsc_cfg_delete = (PScanLabSMCPtr_slsc_cfg_delete)_loadScanLabSMCAddress(hLibrary, "slsc_cfg_delete");
	this->slsc_job_begin = (PScanLabSMCPtr_slsc_job_begin)_loadScanLabSMCAddress(hLibrary, "slsc_job_begin");
	this->slsc_job_end = (PScanLabSMCPtr_slsc_job_end)_loadScanLabSMCAddress(hLibrary, "slsc_job_end");
	this->slsc_job_jump = (PScanLabSMCPtr_slsc_job_jump)_loadScanLabSMCAddress(hLibrary, "slsc_job_jump");
	this->slsc_job_begin_polyline = (PScanLabSMCPtr_slsc_job_begin_polyline)_loadScanLabSMCAddress(hLibrary, "slsc_job_begin_polyline");
	this->slsc_job_end_polyline = (PScanLabSMCPtr_slsc_slsc_job_end_polyline)_loadScanLabSMCAddress(hLibrary, "slsc_job_end_polyline");
	this->slsc_job_line = (PScanLabSMCPtr_slsc_job_line)_loadScanLabSMCAddress(hLibrary, "slsc_job_line");
	this->slsc_ctrl_start_execution = (PScanLabSMCPtr_slsc_ctrl_start_execution)_loadScanLabSMCAddress(hLibrary, "slsc_ctrl_start_execution");
	this->slsc_ctrl_stop = (PScanLabSMCPtr_slsc_ctrl_stop)_loadScanLabSMCAddress(hLibrary, "slsc_ctrl_stop");
	this->slsc_ctrl_stop_controlled = (PScanLabSMCPtr_slsc_ctrl_stop_controlled)_loadScanLabSMCAddress(hLibrary, "slsc_ctrl_stop_controlled");

	m_LibraryHandle = (void*) hLibrary;
}


CScanLabSMCSDK::~CScanLabSMCSDK()
{

	if (m_LibraryHandle != nullptr) {
#ifdef _WIN32
		HMODULE hModule = (HMODULE)m_LibraryHandle;
		FreeLibrary(hModule);
#else // _WIN32
		dlclose(m_LibraryHandle);
#endif // _WIN32

		m_LibraryHandle = nullptr;
		resetFunctionPtrs();
	}
}

void CScanLabSMCSDK::initDLL()
{
	if (!m_bIsInitialized) {

		if (slsc_cfg_initialize_from_file == nullptr)
			throw std::runtime_error("RTC DLL not loaded");

		m_bIsInitialized = true;

	}
}

void CScanLabSMCSDK::checkError(uint32_t nRTCError)
{
	if (nRTCError != 0)
		throw std::runtime_error("RTC Error: " + std::to_string (nRTCError));
}


void CScanLabSMCSDK::resetFunctionPtrs()
{
	slsc_cfg_initialize_from_file = nullptr;
	slsc_cfg_delete = nullptr;
	slsc_job_begin = nullptr;
	slsc_job_end = nullptr;
	slsc_job_jump = nullptr;
	slsc_job_begin_polyline = nullptr;
	slsc_job_end_polyline = nullptr;
	slsc_job_line = nullptr;
	slsc_ctrl_start_execution = nullptr;
	slsc_ctrl_stop = nullptr;
	slsc_ctrl_stop_controlled = nullptr;

}
