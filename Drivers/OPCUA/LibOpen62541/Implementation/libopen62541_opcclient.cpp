/*++

Copyright (C) 2024 Autodesk Inc.

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


Abstract: This is a stub class definition of COPCClient

*/

#include "libopen62541_opcclient.hpp"
#include "libopen62541_interfaceexception.hpp"

#include <array>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <open62541/client.h>

using namespace LibOpen62541::Impl;

/*************************************************************************************************************************
 Class definition of COPCClient 
**************************************************************************************************************************/

static std::string trimString(const std::string & sValue)
{
	size_t nStart = 0;
	while ((nStart < sValue.size()) && std::isspace(static_cast<unsigned char>(sValue[nStart])))
		nStart++;

	size_t nEnd = sValue.size();
	while ((nEnd > nStart) && std::isspace(static_cast<unsigned char>(sValue[nEnd - 1])))
		nEnd--;

	return sValue.substr(nStart, nEnd - nStart);
}

static std::vector<std::string> splitFields(const std::string & sValue)
{
	std::vector<std::string> result;
	std::string current;
	for (char ch : sValue) {
		if (ch == ',') {
			std::string token = trimString(current);
			if (!token.empty())
				result.push_back(token);
			current.clear();
		}
		else {
			current.push_back(ch);
		}
	}
	std::string token = trimString(current);
	if (!token.empty())
		result.push_back(token);

	return result;
}

static std::string escapeJSONString(const std::string & sValue)
{
	std::string sResult;
	sResult.reserve(sValue.size());
	for (unsigned char ch : sValue) {
		switch (ch) {
			case '\\': sResult += "\\\\"; break;
			case '"': sResult += "\\\""; break;
			case '\b': sResult += "\\b"; break;
			case '\f': sResult += "\\f"; break;
			case '\n': sResult += "\\n"; break;
			case '\r': sResult += "\\r"; break;
			case '\t': sResult += "\\t"; break;
			default:
				if (ch < 0x20) {
					std::ostringstream stream;
					stream << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(ch);
					sResult += stream.str();
				}
				else {
					sResult.push_back(static_cast<char>(ch));
				}
				break;
		}
	}
	return sResult;
}

static std::string nodeIdToString(const UA_NodeId & nodeId)
{
	std::ostringstream stream;
	stream << "ns=" << nodeId.namespaceIndex << ";";
	switch (nodeId.identifierType) {
		case UA_NODEIDTYPE_NUMERIC:
			stream << "i=" << nodeId.identifier.numeric;
			break;
		case UA_NODEIDTYPE_STRING:
			if (nodeId.identifier.string.data != nullptr) {
				std::string value((char*)nodeId.identifier.string.data, nodeId.identifier.string.length);
				stream << "s=" << value;
			}
			else {
				stream << "s=";
			}
			break;
		case UA_NODEIDTYPE_GUID: {
			const UA_Guid & guid = nodeId.identifier.guid;
			stream << "g=" << std::hex << std::setw(8) << std::setfill('0') << guid.data1 << "-";
			stream << std::setw(4) << guid.data2 << "-" << std::setw(4) << guid.data3 << "-";
			for (int i = 0; i < 2; i++)
				stream << std::setw(2) << static_cast<unsigned int>(guid.data4[i]);
			stream << "-";
			for (int i = 2; i < 8; i++)
				stream << std::setw(2) << static_cast<unsigned int>(guid.data4[i]);
			break;
		}
		case UA_NODEIDTYPE_BYTESTRING:
			stream << "b=";
			if (nodeId.identifier.byteString.data != nullptr) {
				for (size_t i = 0; i < nodeId.identifier.byteString.length; i++) {
					stream << std::hex << std::setw(2) << std::setfill('0')
						<< static_cast<unsigned int>(nodeId.identifier.byteString.data[i]);
				}
			}
			break;
		default:
			stream << "unknown";
			break;
	}
	return stream.str();
}

static std::string variantToJSON(const UA_Variant * pVariant)
{
	if (pVariant == nullptr || pVariant->data == nullptr)
		return "null";

#ifdef UA_ENABLE_JSON_ENCODING
	UA_String output = UA_STRING_NULL;
	if (UA_print(pVariant, &UA_TYPES[UA_TYPES_VARIANT], &output) == UA_STATUSCODE_GOOD) {
		std::string result;
		if (output.data != nullptr)
			result.assign((char*)output.data, output.length);
		UA_String_clear(&output);
		if (!result.empty())
			return result;
	}
#endif

	if (!UA_Variant_isScalar(pVariant))
		return "null";

	switch (pVariant->type->typeIndex) {
		case UA_TYPES_BOOLEAN:
			return (*(UA_Boolean*)pVariant->data) ? "true" : "false";
		case UA_TYPES_SBYTE:
			return std::to_string(*(UA_SByte*)pVariant->data);
		case UA_TYPES_BYTE:
			return std::to_string(*(UA_Byte*)pVariant->data);
		case UA_TYPES_INT16:
			return std::to_string(*(UA_Int16*)pVariant->data);
		case UA_TYPES_UINT16:
			return std::to_string(*(UA_UInt16*)pVariant->data);
		case UA_TYPES_INT32:
			return std::to_string(*(UA_Int32*)pVariant->data);
		case UA_TYPES_UINT32:
			return std::to_string(*(UA_UInt32*)pVariant->data);
		case UA_TYPES_INT64:
			return std::to_string(*(UA_Int64*)pVariant->data);
		case UA_TYPES_UINT64:
			return std::to_string(*(UA_UInt64*)pVariant->data);
		case UA_TYPES_FLOAT: {
			std::ostringstream stream;
			stream << std::setprecision(9) << *(UA_Float*)pVariant->data;
			return stream.str();
		}
		case UA_TYPES_DOUBLE: {
			std::ostringstream stream;
			stream << std::setprecision(17) << *(UA_Double*)pVariant->data;
			return stream.str();
		}
		case UA_TYPES_STRING: {
			UA_String * pValue = (UA_String*)pVariant->data;
			std::string result;
			if (pValue->data != nullptr)
				result.assign((char*)pValue->data, pValue->length);
			return "\"" + escapeJSONString(result) + "\"";
		}
		case UA_TYPES_LOCALIZEDTEXT: {
			UA_LocalizedText * pValue = (UA_LocalizedText*)pVariant->data;
			std::string locale;
			std::string text;
			if (pValue->locale.data != nullptr)
				locale.assign((char*)pValue->locale.data, pValue->locale.length);
			if (pValue->text.data != nullptr)
				text.assign((char*)pValue->text.data, pValue->text.length);
			return "{\"locale\":\"" + escapeJSONString(locale) + "\",\"text\":\"" + escapeJSONString(text) + "\"}";
		}
		case UA_TYPES_NODEID: {
			UA_NodeId * pValue = (UA_NodeId*)pVariant->data;
			return "\"" + escapeJSONString(nodeIdToString(*pValue)) + "\"";
		}
		default:
			return "null";
	}
}

void COPCClient::EventNotificationCallback(UA_Client * pClient, UA_UInt32 nSubscriptionID, void * pSubscriptionContext, UA_UInt32 nMonitoredItemID, void * pMonitoredItemContext, size_t nEventFields, UA_Variant * pEventFields)
{
	(void)pClient;
	(void)pSubscriptionContext;
	auto pContext = reinterpret_cast<COPCClient::sMonitoredItemContext*>(pMonitoredItemContext);
	if (pContext == nullptr || pContext->m_pClient == nullptr)
		return;

	std::ostringstream stream;
	stream << "{";
	for (size_t i = 0; i < nEventFields; i++) {
		std::string fieldName;
		if (i < pContext->m_FieldNames.size())
			fieldName = pContext->m_FieldNames[i];
		else
			fieldName = "field" + std::to_string(i);

		stream << "\"" << escapeJSONString(fieldName) << "\":";
		stream << variantToJSON(&pEventFields[i]);
		if (i + 1 < nEventFields)
			stream << ",";
	}
	stream << "}";

	COPCClient::sEventEntry entry;
	entry.m_nSubscriptionID = nSubscriptionID;
	entry.m_nMonitoredItemID = nMonitoredItemID;
	entry.m_sEventJSON = stream.str();

	{
		std::lock_guard<std::mutex> lock(pContext->m_pClient->m_EventMutex);
		pContext->m_pClient->m_EventQueue.push_back(std::move(entry));
	}
}

COPCClient::COPCClient()
	: m_Client (nullptr),
	m_SecurityMode (LibOpen62541::eUASecurityMode::None) 
{

}

COPCClient::~COPCClient()
{
	if (m_Client != nullptr) {
		UA_Client_delete(m_Client);
		m_Client = nullptr;
	}
}

void COPCClient::EnableEncryption(const std::string& sLocalCertificate, const std::string& sPrivateKey, const LibOpen62541::eUASecurityMode eSecurityMode)
{
	m_sCertificate = sLocalCertificate;
	m_sPrivateKey = sPrivateKey;
	m_SecurityMode = eSecurityMode;
}

void COPCClient::DisableEncryption()
{
	m_sCertificate = "";
	m_sPrivateKey = "";
	m_SecurityMode = LibOpen62541::eUASecurityMode::None;
}

void COPCClient::ConnectUserName(const std::string & sEndPointURL, const std::string & sUsername, const std::string & sPassword, const std::string & sApplicationURL)
{
	UA_StatusCode statusCode;

	m_Client = UA_Client_new();
	if (m_Client == nullptr)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTCREATEOPCUACLIENT);

	try {

		UA_ClientConfig* pConfig = UA_Client_getConfig(m_Client);

		UA_ClientConfig_setDefault(pConfig);

		switch (m_SecurityMode) {
			case LibOpen62541::eUASecurityMode::None:
				pConfig->securityMode = UA_MESSAGESECURITYMODE_NONE;
				break;
			case LibOpen62541::eUASecurityMode::Sign:
				pConfig->securityMode = UA_MESSAGESECURITYMODE_SIGN;
				break;
			case LibOpen62541::eUASecurityMode::SignAndEncrypt:
				pConfig->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;
				break;
			default:
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_INVALIDSECURITYMODE);
		}

		UA_String_clear(&pConfig->clientDescription.applicationUri);
		pConfig->clientDescription.applicationUri = UA_STRING_ALLOC(sApplicationURL.c_str ());

		if (!m_sCertificate.empty()) {
			auto certificateString = UA_String_fromChars(m_sCertificate.c_str());
			auto privateKeyString = UA_String_fromChars(m_sPrivateKey.c_str());

			statusCode = UA_ClientConfig_setDefaultEncryption(pConfig, certificateString, privateKeyString, nullptr, 0, nullptr, 0);

			UA_String_clear(&certificateString);
			UA_String_clear(&privateKeyString);
		}
		else {
			statusCode = UA_ClientConfig_setDefault(pConfig);
		}

		if (statusCode != UA_STATUSCODE_GOOD)
			throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTSENDOPCUACLIENTCONFIG, "could not send OPCUA Client Config (" + std::to_string (statusCode) + ")");
		
		statusCode = UA_Client_connectUsername(m_Client, sEndPointURL.c_str (), sUsername.c_str (), sPassword.c_str ());
		if (statusCode != UA_STATUSCODE_GOOD)
			throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTCONNECT, "could not connect to OPCUA endpoint " + sEndPointURL +  " (" + std::to_string (statusCode) + ")");



	}
	catch (...)
	{
		Disconnect();
		throw;
	}


}

void COPCClient::Disconnect()
{
	if (m_Client != nullptr) {
		UA_Client_delete(m_Client);
		m_Client = nullptr;
	}

	{
		std::lock_guard<std::mutex> lock(m_EventMutex);
		m_EventQueue.clear();
	}
	m_MonitoredItemContexts.clear();
}

bool COPCClient::IsConnected()
{
	return (m_Client != nullptr);
}

LibOpen62541_int64 COPCClient::ReadInteger(const LibOpen62541_uint32 nNameSpace, const std::string & sNodeName, const LibOpen62541::eUAIntegerType eNodeType)
{
	UA_StatusCode statusCode;
	int64_t nResultValue = 0;

	if (m_Client == nullptr)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_NOTCONNECTED, "OPCUA client is not connected");

	UA_Variant* variantValue = UA_Variant_new();
	try {
		std::vector<char> charBuffer (sNodeName.begin (), sNodeName.end ());
		charBuffer.push_back(0);

		statusCode = UA_Client_readValueAttribute(m_Client, UA_NODEID_STRING(nNameSpace, charBuffer.data()), variantValue);
		if (statusCode != UA_STATUSCODE_GOOD) 
			throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTREADVALUE, "could not read value " + sNodeName + " (" + std::to_string(statusCode) + ")");

		switch (eNodeType) {
			case eUAIntegerType::UAUInt8: {
				if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_BYTE]))
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be Uint8 (" + std::to_string(statusCode) + ")");

				UA_Byte* pTypedValue = (UA_Byte*)variantValue->data;
				if (pTypedValue == nullptr)
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

				nResultValue = *pTypedValue;
				break;
			}

			case eUAIntegerType::UAUInt16: {

				if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_UINT16])) 
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be Uint16 (" + std::to_string(statusCode) + ")");

				UA_UInt16* pTypedValue = (UA_UInt16*)variantValue->data;
				if (pTypedValue == nullptr)
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

				nResultValue = *pTypedValue;
				break;
			}

			case eUAIntegerType::UAUInt32: {
				if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_UINT32]))
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be Uint32 (" + std::to_string(statusCode) + ")");

				UA_UInt32* pTypedValue = (UA_UInt32*)variantValue->data;
				if (pTypedValue == nullptr)
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

				nResultValue = *pTypedValue;
				break;
			}

			case eUAIntegerType::UAUInt64: {
				if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_UINT64]))
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be Uint64 (" + std::to_string(statusCode) + ")");

				UA_UInt64* pTypedValue = (UA_UInt64*)variantValue->data;
				if (pTypedValue == nullptr)
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

				nResultValue = *pTypedValue;
				break;
			}

			case eUAIntegerType::UAInt8: {
				if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_SBYTE]))
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be Int8 (" + std::to_string(statusCode) + ")");

				UA_SByte* pTypedValue = (UA_SByte*)variantValue->data;
				if (pTypedValue == nullptr)
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

				nResultValue = *pTypedValue;
				break;
			}

			case eUAIntegerType::UAInt16: {
				if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_INT16]))
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be Int16 (" + std::to_string(statusCode) + ")");

				UA_Int16* pTypedValue = (UA_Int16*)variantValue->data;
				if (pTypedValue == nullptr)
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

				nResultValue = *pTypedValue;
				break;
			}

			case eUAIntegerType::UAInt32: {
				if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_INT32]))
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be Int32 (" + std::to_string(statusCode) + ")");

				UA_Int32* pTypedValue = (UA_Int32*)variantValue->data;
				if (pTypedValue == nullptr)
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

				nResultValue = *pTypedValue;
				break;
			}

			case eUAIntegerType::UAInt64: {
				if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_INT64]))
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be Int64 (" + std::to_string(statusCode) + ")");

				UA_Int64* pTypedValue = (UA_Int64*)variantValue->data;
				if (pTypedValue == nullptr)
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

				nResultValue = *pTypedValue;
				break;
			}

			default:
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_INVALIDINTEGERNODETYPE);

		}

		UA_Variant* valueToDelete = variantValue;
		variantValue = nullptr;

		UA_Variant_delete(valueToDelete);

	}
	catch (...) {
		if (variantValue != nullptr)
			UA_Variant_delete(variantValue);

		throw;
	}

	return nResultValue;


}

LibOpen62541_double COPCClient::ReadDouble(const LibOpen62541_uint32 nNameSpace, const std::string & sNodeName, const LibOpen62541::eUADoubleType eNodeType)
{
	UA_StatusCode statusCode;
	double dResultValue = 0;

	if (m_Client == nullptr)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_NOTCONNECTED, "OPCUA client is not connected");

	UA_Variant* variantValue = UA_Variant_new();
	try {
		std::vector<char> charBuffer(sNodeName.begin(), sNodeName.end());
		charBuffer.push_back(0);

		statusCode = UA_Client_readValueAttribute(m_Client, UA_NODEID_STRING(nNameSpace, charBuffer.data()), variantValue);
		if (statusCode != UA_STATUSCODE_GOOD)
			throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTREADVALUE, "could not read value " + sNodeName + " (" + std::to_string(statusCode) + ")");

		switch (eNodeType) {
			case eUADoubleType::UAFloat32: {
				if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_FLOAT]))
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be Float (" + std::to_string(statusCode) + ")");

				UA_Float* pTypedValue = (UA_Float*) variantValue->data;
				if (pTypedValue == nullptr)
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

				dResultValue = *pTypedValue;
				break;
			}

			case eUADoubleType::UADouble64: {
				if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_FLOAT]))
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be Float (" + std::to_string(statusCode) + ")");

				UA_Float* pTypedValue = (UA_Float*)variantValue->data;
				if (pTypedValue == nullptr)
					throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

				dResultValue = *pTypedValue;
				break;
			}

			default:
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_INVALIDFLOATNODETYPE);

		}

		UA_Variant* valueToDelete = variantValue;
		variantValue = nullptr;

		UA_Variant_delete(valueToDelete);

	}
	catch (...) {
		if (variantValue != nullptr)
			UA_Variant_delete(variantValue);

		throw;
	}

	return dResultValue;


}

std::string COPCClient::ReadString(const LibOpen62541_uint32 nNameSpace, const std::string & sNodeName)
{
	UA_StatusCode statusCode;
	std::string sResultValue;

	if (m_Client == nullptr)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_NOTCONNECTED, "OPCUA client is not connected");

	UA_Variant* variantValue = UA_Variant_new();
	try {
		std::vector<char> charBuffer(sNodeName.begin(), sNodeName.end());
		charBuffer.push_back(0);

		statusCode = UA_Client_readValueAttribute(m_Client, UA_NODEID_STRING(nNameSpace, charBuffer.data()), variantValue);
		if (statusCode != UA_STATUSCODE_GOOD)
			throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTREADVALUE, "could not read value " + sNodeName + " (" + std::to_string(statusCode) + ")");

		if (!UA_Variant_hasScalarType(variantValue, &UA_TYPES[UA_TYPES_STRING]))
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_READVALUEISOFINCORRECTTYPE, "read value is of incorrect type. " + sNodeName + " was supposed to be String (" + std::to_string(statusCode) + ")");

		UA_String* pTypedValue = (UA_String*)variantValue->data;
		if (pTypedValue == nullptr)
			throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAVARIANTDATAISNULL, "OPCUA variant data is null for " + sNodeName);

		const char * pDataChar = (const char*) pTypedValue->data;
		if (pDataChar == nullptr)
			throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUASTRINGDATAISNULL, "OPCUA String data is null: " + sNodeName);

		sResultValue = std::string(pDataChar, pDataChar + pTypedValue->length);

		UA_Variant* valueToDelete = variantValue;
		variantValue = nullptr;

		UA_Variant_delete(valueToDelete);

	}
	catch (...) {
		if (variantValue != nullptr)
			UA_Variant_delete(variantValue);

		throw;
	}

	return sResultValue;
}

void COPCClient::WriteInteger(const LibOpen62541_uint32 nNameSpace, const std::string & sNodeName, const LibOpen62541::eUAIntegerType eNodeType, const LibOpen62541_int64 nValue)
{
	std::array<uint8_t, 8> writeDataBuffer;
	for (auto & value : writeDataBuffer)
		value = 0;

	if (m_Client == nullptr)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_NOTCONNECTED, "OPCUA client is not connected");

	UA_DataType* pDataType;
	switch (eNodeType) {
		case eUAIntegerType::UAUInt8: {
			pDataType = &UA_TYPES[UA_TYPES_BYTE];
			if ((nValue < 0) || (nValue >= 256))
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAWRITEINTEGEROUTOFBOUNDS, "OPCUA write integer out of bounds: " + std::to_string (nValue));

			uint8_t* pTargetPtr = writeDataBuffer.data();
			*pTargetPtr = (uint8_t) nValue;

			break;
		}

		case eUAIntegerType::UAUInt16: {
			pDataType = &UA_TYPES[UA_TYPES_UINT16];
			if ((nValue < 0) || (nValue >= 65536))
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAWRITEINTEGEROUTOFBOUNDS, "OPCUA write integer out of bounds" + std::to_string(nValue));

			uint16_t* pTargetPtr = (uint16_t *) writeDataBuffer.data();
			*pTargetPtr = (uint16_t)nValue;

			break;
		}

		case eUAIntegerType::UAUInt32: {
			pDataType = &UA_TYPES[UA_TYPES_UINT32];
			if ((nValue < 0) || (nValue > 0x100000000LL))
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAWRITEINTEGEROUTOFBOUNDS, "OPCUA write integer out of bounds" + std::to_string(nValue));

			uint32_t* pTargetPtr = (uint32_t*)writeDataBuffer.data();
			*pTargetPtr = (uint32_t)nValue;

			break;
		}

		case eUAIntegerType::UAUInt64: {
			pDataType = &UA_TYPES[UA_TYPES_UINT64];

			if (nValue < 0)
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAWRITEINTEGEROUTOFBOUNDS, "OPCUA write integer out of bounds" + std::to_string(nValue));

			uint64_t* pTargetPtr = (uint64_t*)writeDataBuffer.data();
			*pTargetPtr = (uint64_t)nValue;

			break;
		}

		case eUAIntegerType::UAInt8: {
			pDataType = &UA_TYPES[UA_TYPES_BYTE];
			if ((nValue < -127) || (nValue >= 128))
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAWRITEINTEGEROUTOFBOUNDS, "OPCUA write integer out of bounds: " + std::to_string(nValue));

			int8_t* pTargetPtr = (int8_t*)writeDataBuffer.data();
			*pTargetPtr = (int8_t)nValue;

			break;
		}

		case eUAIntegerType::UAInt16: {
			pDataType = &UA_TYPES[UA_TYPES_INT16];
			if ((nValue < -32767) || (nValue >= 32768))
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAWRITEINTEGEROUTOFBOUNDS, "OPCUA write integer out of bounds" + std::to_string(nValue));

			int16_t* pTargetPtr = (int16_t*)writeDataBuffer.data();
			*pTargetPtr = (int16_t)nValue;

			break;
		}

		case eUAIntegerType::UAInt32: {
			pDataType = &UA_TYPES[UA_TYPES_INT32];
			if ((nValue < INT32_MIN) || (nValue > INT32_MAX))
				throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_OPCUAWRITEINTEGEROUTOFBOUNDS, "OPCUA write integer out of bounds" + std::to_string(nValue));

			int32_t* pTargetPtr = (int32_t*)writeDataBuffer.data();
			*pTargetPtr = (int32_t)nValue;

			break;
		}

		case eUAIntegerType::UAInt64: {
			pDataType = &UA_TYPES[UA_TYPES_INT64];

			int64_t* pTargetPtr = (int64_t*)writeDataBuffer.data();
			*pTargetPtr = (int64_t)nValue;

			break;
		}

		default:
			throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_INVALIDINTEGERNODETYPE);

	}

	UA_WriteRequest wReq;
	UA_WriteRequest_init(&wReq);
	wReq.nodesToWrite = UA_WriteValue_new();
	wReq.nodesToWriteSize = 1;
	wReq.nodesToWrite[0].nodeId = UA_NODEID_STRING_ALLOC(nNameSpace, sNodeName.c_str ());
	wReq.nodesToWrite[0].attributeId = UA_ATTRIBUTEID_VALUE;
	wReq.nodesToWrite[0].value.hasValue = true;
	wReq.nodesToWrite[0].value.value.storageType = UA_VARIANT_DATA_NODELETE; /* do not free the integer on deletion */

	wReq.nodesToWrite[0].value.value.type = pDataType;
	wReq.nodesToWrite[0].value.value.data = writeDataBuffer.data();

	UA_WriteResponse wResp = UA_Client_Service_write(m_Client, wReq);

	auto serviceResult = wResp.responseHeader.serviceResult;
	UA_WriteRequest_clear(&wReq);
	UA_WriteResponse_clear(&wResp);

	if (serviceResult != UA_STATUSCODE_GOOD)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTWRITEVALUE, "could not write value to " + sNodeName);

}

void COPCClient::WriteDouble(const LibOpen62541_uint32 nNameSpace, const std::string & sNodeName, const LibOpen62541::eUADoubleType eNodeType, const LibOpen62541_double dValue)
{
	std::array<uint8_t, 8> writeDataBuffer;
	for (auto& value : writeDataBuffer)
		value = 0;

	if (m_Client == nullptr)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_NOTCONNECTED, "OPCUA client is not connected");

	UA_DataType* pDataType;
	switch (eNodeType) {
	case eUADoubleType::UAFloat32: {
		pDataType = &UA_TYPES[UA_TYPES_FLOAT];
		float* pTargetPtr = (float*)writeDataBuffer.data();
		*pTargetPtr = (float)dValue;
		break;
	}

	case eUADoubleType::UADouble64: {
		pDataType = &UA_TYPES[UA_TYPES_DOUBLE];
		double* pTargetPtr = (double*)writeDataBuffer.data();
		*pTargetPtr = (double)dValue;
		break;
	}


	default:
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_INVALIDFLOATNODETYPE);
	}

	UA_WriteRequest wReq;
	UA_WriteRequest_init(&wReq);
	wReq.nodesToWrite = UA_WriteValue_new();
	wReq.nodesToWriteSize = 1;
	wReq.nodesToWrite[0].nodeId = UA_NODEID_STRING_ALLOC(nNameSpace, sNodeName.c_str());
	wReq.nodesToWrite[0].attributeId = UA_ATTRIBUTEID_VALUE;
	wReq.nodesToWrite[0].value.hasValue = true;
	wReq.nodesToWrite[0].value.value.storageType = UA_VARIANT_DATA_NODELETE; /* do not free the float on deletion */

	wReq.nodesToWrite[0].value.value.type = pDataType;
	wReq.nodesToWrite[0].value.value.data = writeDataBuffer.data();

	UA_WriteResponse wResp = UA_Client_Service_write(m_Client, wReq);

	auto serviceResult = wResp.responseHeader.serviceResult;
	UA_WriteRequest_clear(&wReq);
	UA_WriteResponse_clear(&wResp);

	if (serviceResult != UA_STATUSCODE_GOOD)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTWRITEVALUE, "could not write value to " + sNodeName);
}

void COPCClient::WriteString(const LibOpen62541_uint32 nNameSpace, const std::string & sNodeName, const std::string & sValue)
{

	if (m_Client == nullptr)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_NOTCONNECTED, "OPCUA client is not connected");

	UA_String stringValue = UA_String_fromChars(sValue.c_str());

	UA_WriteRequest wReq;
	UA_WriteRequest_init(&wReq);
	wReq.nodesToWrite = UA_WriteValue_new();
	wReq.nodesToWriteSize = 1;
	wReq.nodesToWrite[0].nodeId = UA_NODEID_STRING_ALLOC(nNameSpace, sNodeName.c_str());
	wReq.nodesToWrite[0].attributeId = UA_ATTRIBUTEID_VALUE;
	wReq.nodesToWrite[0].value.hasValue = true;
	
	UA_Variant_setScalar(&wReq.nodesToWrite[0].value.value, &stringValue, &UA_TYPES[UA_TYPES_STRING]);

	UA_WriteResponse wResp = UA_Client_Service_write(m_Client, wReq);

	auto serviceResult = wResp.responseHeader.serviceResult;
	UA_WriteRequest_clear(&wReq);
	UA_WriteResponse_clear(&wResp);

	if (serviceResult != UA_STATUSCODE_GOOD)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTWRITEVALUE, "could not write value to " + sNodeName);

}

void COPCClient::CreateEventSubscription(const LibOpen62541_uint32 nNameSpace, const std::string & sNodeName, const std::string & sSelectFields, const LibOpen62541_double dPublishingInterval, const LibOpen62541_uint32 nQueueSize, const bool bDiscardOldest, LibOpen62541_uint32 & nSubscriptionID, LibOpen62541_uint32 & nMonitoredItemID)
{
	if (m_Client == nullptr)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_NOTCONNECTED, "OPCUA client is not connected");

	std::vector<std::string> fields = splitFields(sSelectFields);
	if (fields.empty()) {
		fields = { "EventId", "EventType", "SourceNode", "SourceName", "Time", "ReceiveTime", "Message", "Severity" };
	}

	UA_CreateSubscriptionRequest request = UA_CreateSubscriptionRequest_default();
	if (dPublishingInterval > 0.0)
		request.requestedPublishingInterval = dPublishingInterval;

	UA_CreateSubscriptionResponse response = UA_Client_Subscriptions_create(m_Client, request, nullptr, nullptr, nullptr);
	if (response.responseHeader.serviceResult != UA_STATUSCODE_GOOD)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTCONNECT, "could not create event subscription (" + std::to_string(response.responseHeader.serviceResult) + ")");

	UA_EventFilter filter;
	UA_EventFilter_init(&filter);
	filter.selectClausesSize = fields.size();
	filter.selectClauses = (UA_SimpleAttributeOperand*)UA_Array_new(filter.selectClausesSize, &UA_TYPES[UA_TYPES_SIMPLEATTRIBUTEOPERAND]);
	for (size_t i = 0; i < fields.size(); i++) {
		UA_SimpleAttributeOperand_init(&filter.selectClauses[i]);
		filter.selectClauses[i].typeDefinitionId = UA_NODEID_NUMERIC(0, UA_NS0ID_BASEEVENTTYPE);
		filter.selectClauses[i].attributeId = UA_ATTRIBUTEID_VALUE;
		filter.selectClauses[i].browsePathSize = 1;
		filter.selectClauses[i].browsePath = (UA_QualifiedName*)UA_Array_new(1, &UA_TYPES[UA_TYPES_QUALIFIEDNAME]);
		filter.selectClauses[i].browsePath[0] = UA_QUALIFIEDNAME_ALLOC(0, fields[i].c_str());
	}

	UA_MonitoredItemCreateRequest monRequest = UA_MonitoredItemCreateRequest_default(UA_NODEID_NULL);
	std::vector<char> charBuffer(sNodeName.begin(), sNodeName.end());
	charBuffer.push_back(0);
	monRequest.itemToMonitor.nodeId = UA_NODEID_STRING(nNameSpace, charBuffer.data());
	monRequest.itemToMonitor.attributeId = UA_ATTRIBUTEID_EVENTNOTIFIER;
	monRequest.requestedParameters.queueSize = nQueueSize;
	monRequest.requestedParameters.discardOldest = bDiscardOldest;
	monRequest.requestedParameters.filter.encoding = UA_EXTENSIONOBJECT_DECODED;
	monRequest.requestedParameters.filter.content.decoded.type = &UA_TYPES[UA_TYPES_EVENTFILTER];
	monRequest.requestedParameters.filter.content.decoded.data = &filter;

	auto pContext = std::make_shared<sMonitoredItemContext>();
	pContext->m_pClient = this;
	pContext->m_FieldNames = fields;

	UA_MonitoredItemCreateResult result = UA_Client_MonitoredItems_createEvent(
		m_Client, response.subscriptionId, UA_TimestampsToReturn_Both, monRequest,
		pContext.get(), COPCClient::EventNotificationCallback, nullptr);

	UA_EventFilter_clear(&filter);

	if (result.statusCode != UA_STATUSCODE_GOOD) {
		UA_Client_Subscriptions_deleteSingle(m_Client, response.subscriptionId);
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTCONNECT, "could not create event monitored item (" + std::to_string(result.statusCode) + ")");
	}

	nSubscriptionID = response.subscriptionId;
	nMonitoredItemID = result.monitoredItemId;

	m_MonitoredItemContexts.insert({ nMonitoredItemID, pContext });
}

void COPCClient::DeleteEventSubscription(const LibOpen62541_uint32 nSubscriptionID, const LibOpen62541_uint32 nMonitoredItemID)
{
	if (m_Client == nullptr)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_NOTCONNECTED, "OPCUA client is not connected");

	UA_StatusCode statusCode = UA_Client_MonitoredItems_deleteSingle(m_Client, nSubscriptionID, nMonitoredItemID);
	if (statusCode != UA_STATUSCODE_GOOD)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTCONNECT, "could not delete monitored item (" + std::to_string(statusCode) + ")");

	statusCode = UA_Client_Subscriptions_deleteSingle(m_Client, nSubscriptionID);
	if (statusCode != UA_STATUSCODE_GOOD)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTCONNECT, "could not delete subscription (" + std::to_string(statusCode) + ")");

	m_MonitoredItemContexts.erase(nMonitoredItemID);
}

void COPCClient::PollEvent(const LibOpen62541_uint32 nTimeoutMS, bool & bHasEvent, LibOpen62541_uint32 & nSubscriptionID, LibOpen62541_uint32 & nMonitoredItemID, std::string & sEventJSON)
{
	if (m_Client == nullptr)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_NOTCONNECTED, "OPCUA client is not connected");

	UA_StatusCode statusCode = UA_Client_run_iterate(m_Client, (UA_UInt16)nTimeoutMS);
	if (statusCode != UA_STATUSCODE_GOOD)
		throw ELibOpen62541InterfaceException(LIBOPEN62541_ERROR_COULDNOTCONNECT, "could not process OPCUA events (" + std::to_string(statusCode) + ")");

	{
		std::lock_guard<std::mutex> lock(m_EventMutex);
		if (m_EventQueue.empty()) {
			bHasEvent = false;
			nSubscriptionID = 0;
			nMonitoredItemID = 0;
			sEventJSON.clear();
			return;
		}

		sEventEntry entry = std::move(m_EventQueue.front());
		m_EventQueue.pop_front();

		bHasEvent = true;
		nSubscriptionID = entry.m_nSubscriptionID;
		nMonitoredItemID = entry.m_nMonitoredItemID;
		sEventJSON = std::move(entry.m_sEventJSON);
	}
}
