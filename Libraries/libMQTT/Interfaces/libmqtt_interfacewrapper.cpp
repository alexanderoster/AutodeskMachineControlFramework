/*++

Copyright (C) 2021 Autodesk Inc.

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


This file has been generated by the Automatic Component Toolkit (ACT) version 1.7.0-develop.

Abstract: This is an autogenerated C++ implementation file in order to allow easy
development of MQTT Library. The functions in this file need to be implemented. It needs to be generated only once.

Interface version: 1.0.0

*/

#include "libmqtt_abi.hpp"
#include "libmqtt_interfaces.hpp"
#include "libmqtt_interfaceexception.hpp"

#include <map>

using namespace LibMQTT::Impl;

LibMQTTResult handleLibMQTTException(IBase * pIBaseClass, ELibMQTTInterfaceException & Exception)
{
	LibMQTTResult errorCode = Exception.getErrorCode();

	if (pIBaseClass != nullptr)
		pIBaseClass->RegisterErrorMessage(Exception.what());

	return errorCode;
}

LibMQTTResult handleStdException(IBase * pIBaseClass, std::exception & Exception)
{
	LibMQTTResult errorCode = LIBMQTT_ERROR_GENERICEXCEPTION;

	if (pIBaseClass != nullptr)
		pIBaseClass->RegisterErrorMessage(Exception.what());

	return errorCode;
}

LibMQTTResult handleUnhandledException(IBase * pIBaseClass)
{
	LibMQTTResult errorCode = LIBMQTT_ERROR_GENERICEXCEPTION;

	if (pIBaseClass != nullptr)
		pIBaseClass->RegisterErrorMessage("Unhandled Exception");

	return errorCode;
}



/*************************************************************************************************************************
 Class implementation for Base
**************************************************************************************************************************/

/*************************************************************************************************************************
 Class implementation for MQTTContext
**************************************************************************************************************************/
LibMQTTResult libmqtt_mqttcontext_setiotendpoint(LibMQTT_MQTTContext pMQTTContext, const char * pEndPoint, LibMQTT_uint32 nPort)
{
	IBase* pIBaseClass = (IBase *)pMQTTContext;

	try {
		if (pEndPoint == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		std::string sEndPoint(pEndPoint);
		IMQTTContext* pIMQTTContext = dynamic_cast<IMQTTContext*>(pIBaseClass);
		if (!pIMQTTContext)
			throw ELibMQTTInterfaceException(LIBMQTT_ERROR_INVALIDCAST);
		
		pIMQTTContext->SetIotEndpoint(sEndPoint, nPort);

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_mqttcontext_setrootcertificate(LibMQTT_MQTTContext pMQTTContext, const char * pCertificateString)
{
	IBase* pIBaseClass = (IBase *)pMQTTContext;

	try {
		if (pCertificateString == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		std::string sCertificateString(pCertificateString);
		IMQTTContext* pIMQTTContext = dynamic_cast<IMQTTContext*>(pIBaseClass);
		if (!pIMQTTContext)
			throw ELibMQTTInterfaceException(LIBMQTT_ERROR_INVALIDCAST);
		
		pIMQTTContext->SetRootCertificate(sCertificateString);

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_mqttcontext_setclientinformation(LibMQTT_MQTTContext pMQTTContext, const char * pClientIdentifier, const char * pClientCertificateString, const char * pCertificatePrivateKeyString)
{
	IBase* pIBaseClass = (IBase *)pMQTTContext;

	try {
		if (pClientIdentifier == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		if (pClientCertificateString == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		if (pCertificatePrivateKeyString == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		std::string sClientIdentifier(pClientIdentifier);
		std::string sClientCertificateString(pClientCertificateString);
		std::string sCertificatePrivateKeyString(pCertificatePrivateKeyString);
		IMQTTContext* pIMQTTContext = dynamic_cast<IMQTTContext*>(pIBaseClass);
		if (!pIMQTTContext)
			throw ELibMQTTInterfaceException(LIBMQTT_ERROR_INVALIDCAST);
		
		pIMQTTContext->SetClientInformation(sClientIdentifier, sClientCertificateString, sCertificatePrivateKeyString);

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_mqttcontext_setmqtttopic(LibMQTT_MQTTContext pMQTTContext, const char * pMqttTopic)
{
	IBase* pIBaseClass = (IBase *)pMQTTContext;

	try {
		if (pMqttTopic == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		std::string sMqttTopic(pMqttTopic);
		IMQTTContext* pIMQTTContext = dynamic_cast<IMQTTContext*>(pIBaseClass);
		if (!pIMQTTContext)
			throw ELibMQTTInterfaceException(LIBMQTT_ERROR_INVALIDCAST);
		
		pIMQTTContext->SetMqttTopic(sMqttTopic);

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_mqttcontext_setmqttmessage(LibMQTT_MQTTContext pMQTTContext, const char * pMqttMessage)
{
	IBase* pIBaseClass = (IBase *)pMQTTContext;

	try {
		if (pMqttMessage == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		std::string sMqttMessage(pMqttMessage);
		IMQTTContext* pIMQTTContext = dynamic_cast<IMQTTContext*>(pIBaseClass);
		if (!pIMQTTContext)
			throw ELibMQTTInterfaceException(LIBMQTT_ERROR_INVALIDCAST);
		
		pIMQTTContext->SetMqttMessage(sMqttMessage);

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_mqttcontext_connect(LibMQTT_MQTTContext pMQTTContext)
{
	IBase* pIBaseClass = (IBase *)pMQTTContext;

	try {
		IMQTTContext* pIMQTTContext = dynamic_cast<IMQTTContext*>(pIBaseClass);
		if (!pIMQTTContext)
			throw ELibMQTTInterfaceException(LIBMQTT_ERROR_INVALIDCAST);
		
		pIMQTTContext->Connect();

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_mqttcontext_disconnect(LibMQTT_MQTTContext pMQTTContext)
{
	IBase* pIBaseClass = (IBase *)pMQTTContext;

	try {
		IMQTTContext* pIMQTTContext = dynamic_cast<IMQTTContext*>(pIBaseClass);
		if (!pIMQTTContext)
			throw ELibMQTTInterfaceException(LIBMQTT_ERROR_INVALIDCAST);
		
		pIMQTTContext->Disconnect();

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_mqttcontext_sendmessage(LibMQTT_MQTTContext pMQTTContext, const char * pMessageJSON)
{
	IBase* pIBaseClass = (IBase *)pMQTTContext;

	try {
		if (pMessageJSON == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		std::string sMessageJSON(pMessageJSON);
		IMQTTContext* pIMQTTContext = dynamic_cast<IMQTTContext*>(pIBaseClass);
		if (!pIMQTTContext)
			throw ELibMQTTInterfaceException(LIBMQTT_ERROR_INVALIDCAST);
		
		pIMQTTContext->SendMessage(sMessageJSON);

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}



/*************************************************************************************************************************
 Function table lookup implementation
**************************************************************************************************************************/

LibMQTTResult LibMQTT::Impl::LibMQTT_GetProcAddress (const char * pProcName, void ** ppProcAddress)
{
	if (pProcName == nullptr)
		return LIBMQTT_ERROR_INVALIDPARAM;
	if (ppProcAddress == nullptr)
		return LIBMQTT_ERROR_INVALIDPARAM;
	*ppProcAddress = nullptr;
	std::string sProcName (pProcName);
	
	if (sProcName == "libmqtt_mqttcontext_setiotendpoint") 
		*ppProcAddress = (void*) &libmqtt_mqttcontext_setiotendpoint;
	if (sProcName == "libmqtt_mqttcontext_setrootcertificate") 
		*ppProcAddress = (void*) &libmqtt_mqttcontext_setrootcertificate;
	if (sProcName == "libmqtt_mqttcontext_setclientinformation") 
		*ppProcAddress = (void*) &libmqtt_mqttcontext_setclientinformation;
	if (sProcName == "libmqtt_mqttcontext_setmqtttopic") 
		*ppProcAddress = (void*) &libmqtt_mqttcontext_setmqtttopic;
	if (sProcName == "libmqtt_mqttcontext_setmqttmessage") 
		*ppProcAddress = (void*) &libmqtt_mqttcontext_setmqttmessage;
	if (sProcName == "libmqtt_mqttcontext_connect") 
		*ppProcAddress = (void*) &libmqtt_mqttcontext_connect;
	if (sProcName == "libmqtt_mqttcontext_disconnect") 
		*ppProcAddress = (void*) &libmqtt_mqttcontext_disconnect;
	if (sProcName == "libmqtt_mqttcontext_sendmessage") 
		*ppProcAddress = (void*) &libmqtt_mqttcontext_sendmessage;
	if (sProcName == "libmqtt_getversion") 
		*ppProcAddress = (void*) &libmqtt_getversion;
	if (sProcName == "libmqtt_getlasterror") 
		*ppProcAddress = (void*) &libmqtt_getlasterror;
	if (sProcName == "libmqtt_releaseinstance") 
		*ppProcAddress = (void*) &libmqtt_releaseinstance;
	if (sProcName == "libmqtt_acquireinstance") 
		*ppProcAddress = (void*) &libmqtt_acquireinstance;
	if (sProcName == "libmqtt_injectcomponent") 
		*ppProcAddress = (void*) &libmqtt_injectcomponent;
	if (sProcName == "libmqtt_createcontext") 
		*ppProcAddress = (void*) &libmqtt_createcontext;
	
	if (*ppProcAddress == nullptr) 
		return LIBMQTT_ERROR_COULDNOTFINDLIBRARYEXPORT;
	return LIBMQTT_SUCCESS;
}

/*************************************************************************************************************************
 Global functions implementation
**************************************************************************************************************************/
LibMQTTResult libmqtt_getversion(LibMQTT_uint32 * pMajor, LibMQTT_uint32 * pMinor, LibMQTT_uint32 * pMicro)
{
	IBase* pIBaseClass = nullptr;

	try {
		if (!pMajor)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		if (!pMinor)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		if (!pMicro)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		CWrapper::GetVersion(*pMajor, *pMinor, *pMicro);

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_getlasterror(LibMQTT_Base pInstance, const LibMQTT_uint32 nErrorMessageBufferSize, LibMQTT_uint32* pErrorMessageNeededChars, char * pErrorMessageBuffer, bool * pHasError)
{
	IBase* pIBaseClass = nullptr;

	try {
		if ( (!pErrorMessageBuffer) && !(pErrorMessageNeededChars) )
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		if (pHasError == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		IBase* pIBaseClassInstance = (IBase *)pInstance;
		IBase* pIInstance = dynamic_cast<IBase*>(pIBaseClassInstance);
		if (!pIInstance)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDCAST);
		
		std::string sErrorMessage("");
		*pHasError = CWrapper::GetLastError(pIInstance, sErrorMessage);

		if (pErrorMessageNeededChars)
			*pErrorMessageNeededChars = (LibMQTT_uint32) (sErrorMessage.size()+1);
		if (pErrorMessageBuffer) {
			if (sErrorMessage.size() >= nErrorMessageBufferSize)
				throw ELibMQTTInterfaceException (LIBMQTT_ERROR_BUFFERTOOSMALL);
			for (size_t iErrorMessage = 0; iErrorMessage < sErrorMessage.size(); iErrorMessage++)
				pErrorMessageBuffer[iErrorMessage] = sErrorMessage[iErrorMessage];
			pErrorMessageBuffer[sErrorMessage.size()] = 0;
		}
		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_releaseinstance(LibMQTT_Base pInstance)
{
	IBase* pIBaseClass = nullptr;

	try {
		IBase* pIBaseClassInstance = (IBase *)pInstance;
		IBase* pIInstance = dynamic_cast<IBase*>(pIBaseClassInstance);
		if (!pIInstance)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDCAST);
		
		CWrapper::ReleaseInstance(pIInstance);

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_acquireinstance(LibMQTT_Base pInstance)
{
	IBase* pIBaseClass = nullptr;

	try {
		IBase* pIBaseClassInstance = (IBase *)pInstance;
		IBase* pIInstance = dynamic_cast<IBase*>(pIBaseClassInstance);
		if (!pIInstance)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDCAST);
		
		CWrapper::AcquireInstance(pIInstance);

		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_injectcomponent(const char * pNameSpace, LibMQTT_pvoid pSymbolAddressMethod)
{
	IBase* pIBaseClass = nullptr;

	try {
		if (pNameSpace == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		std::string sNameSpace(pNameSpace);
		
		bool bNameSpaceFound = false;
		
		
		if (!bNameSpaceFound)
			throw ELibMQTTInterfaceException(LIBMQTT_ERROR_COULDNOTLOADLIBRARY);
		
		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

LibMQTTResult libmqtt_createcontext(const char * pInstanceName, LibMQTT_MQTTContext * pInstance)
{
	IBase* pIBaseClass = nullptr;

	try {
		if (pInstanceName == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		if (pInstance == nullptr)
			throw ELibMQTTInterfaceException (LIBMQTT_ERROR_INVALIDPARAM);
		std::string sInstanceName(pInstanceName);
		IBase* pBaseInstance(nullptr);
		pBaseInstance = CWrapper::CreateContext(sInstanceName);

		*pInstance = (IBase*)(pBaseInstance);
		return LIBMQTT_SUCCESS;
	}
	catch (ELibMQTTInterfaceException & Exception) {
		return handleLibMQTTException(pIBaseClass, Exception);
	}
	catch (std::exception & StdException) {
		return handleStdException(pIBaseClass, StdException);
	}
	catch (...) {
		return handleUnhandledException(pIBaseClass);
	}
}

