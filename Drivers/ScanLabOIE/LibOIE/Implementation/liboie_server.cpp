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


Abstract: This is a stub class definition of CServer

*/

#include "liboie_server.hpp"
#include "liboie_interfaceexception.hpp"
#include "liboie_connectioniterator.hpp"

// Include custom headers here.
#include <brynet/net/EventLoop.hpp>
#include <brynet/net/TcpService.hpp>
#include <brynet/net/wrapper/ServiceBuilder.hpp>
#include <brynet/base/AppStatus.hpp>

using namespace LibOIE::Impl;

/*************************************************************************************************************************
 Class definition of CServer 
**************************************************************************************************************************/

#define OIE_MAXRULECOUNT (1024 * 1024 * 1024)

CServer::CServer()
    : m_nThreadCount (LIBOIE_THREADCOUNT_DEFAULT), 
    m_nReceiveBufferSize (LIBOIE_RECEIVEBUFFERSIZE_DEFAULT), 
    m_nAcceptRuleCounter (1),
    m_nConnectionCounter (1)
{

}


PConnectionHandler CServer::createConnectionHandler(const std::string & sIPAddress)
{
    std::lock_guard<std::mutex> lockGuard (m_ConnectionMutex);
    auto pConnectionHandler = std::make_shared<CConnectionHandler>(m_nConnectionCounter, sIPAddress);
    m_nConnectionCounter++;

    m_CurrentConnections.insert(std::make_pair (pConnectionHandler->getID (), pConnectionHandler));

    return pConnectionHandler;
}

void CServer::releaseConnectionHandler(const uint64_t nConnectionID)
{
    std::lock_guard<std::mutex> lockGuard(m_ConnectionMutex);

    m_CurrentConnections.erase(nConnectionID);

}

CConnectionHandler* CServer::findConnectionHandler(const uint64_t nConnectionID, bool bFailIfNotExisting)
{
    std::lock_guard<std::mutex> lockGuard(m_ConnectionMutex);

    auto iIter = m_CurrentConnections.find(nConnectionID);

    if (iIter != m_CurrentConnections.end()) {
        return iIter->second.get();
    }
       
    if (bFailIfNotExisting)
        throw ELibOIEInterfaceException(LIBOIE_ERROR_CONNECTIONNOTFOUND);

    return nullptr;

}


void CServer::Start(const std::string& sIPAddress, const LibOIE_uint32 nPort, const bool bIPv6) 
{

    if (m_pService != nullptr)
        throw ELibOIEInterfaceException(LIBOIE_ERROR_SERVICEALREADYRUNNING);
    if (m_pListener != nullptr)
        throw ELibOIEInterfaceException(LIBOIE_ERROR_SERVICEALREADYRUNNING);

    m_pService = brynet::net::TcpService::Create();

    m_pService->startWorkerThread(m_nThreadCount, nullptr);
    std::cout << "Listening!!" << std::endl;

    auto enterCallback = [this](const  brynet::net::TcpConnection::Ptr& session) {

        std::cout << "Client connected:" << session->getIP() << std::endl;
        auto pConnectionHandler = this->createConnectionHandler(session->getIP());
        
        session->setDataCallback([session, pConnectionHandler](const char* buffer, size_t len) {
            std::cout << "Data received: " << len << "!!" << std::endl;

            if (pConnectionHandler->needsToTerminate()) {
                session->postDisConnect();
                return len;
            }

            try {
                pConnectionHandler->receivedData((const uint8_t*)buffer, len);
            }
            catch (...) {
                session->postDisConnect();
            }

            return len;
        });

        session->setDisConnectCallback([this, pConnectionHandler](const  brynet::net::TcpConnection::Ptr& session) {
            std::cout << "Client disconnected: " << session->getIP() << std::endl;
            this->releaseConnectionHandler(pConnectionHandler->getID ());
        });

        // ?
        //session->setHeartBeat ()

    };


    m_pListener = std::make_shared<brynet::net::wrapper::ListenerBuilder>();
    m_pListener->configureService(m_pService)
        .configureSocketOptions({
            [](brynet::net::TcpSocket& socket) {
                socket.setNodelay();
            }
            })
        .configureConnectionOptions({
            brynet::net::AddSocketOption::WithMaxRecvBufferSize(m_nReceiveBufferSize),
            brynet::net::AddSocketOption::AddEnterCallback(enterCallback)
            })
                .configureListen([=](brynet::net::wrapper::BuildListenConfig config) {
                config.setAddr (bIPv6, sIPAddress, nPort);
            })
                .asyncRun();

}



void CServer::Stop()
{

    if (m_pListener != nullptr) {
        m_pListener->stop();
        m_pListener = nullptr;
    }

    if (m_pService != nullptr) {
        m_pService->stopWorkerThread();
        m_pService = nullptr;
    }

}

void CServer::SetThreadCount(const LibOIE_uint32 nThreadCount)
{
    if (nThreadCount < LIBOIE_THREADCOUNT_MIN)
        throw ELibOIEInterfaceException(LIBOIE_ERROR_INVALIDPARAM);
    if (nThreadCount > LIBOIE_THREADCOUNT_MAX)
        throw ELibOIEInterfaceException(LIBOIE_ERROR_INVALIDPARAM);
    m_nThreadCount = nThreadCount;
}

void CServer::SetBufferSize(const LibOIE_uint32 nRCVBufferSize)
{
    if (nRCVBufferSize < LIBOIE_RECEIVEBUFFERSIZE_MIN)
        throw ELibOIEInterfaceException(LIBOIE_ERROR_INVALIDPARAM);
    if (nRCVBufferSize > LIBOIE_RECEIVEBUFFERSIZE_MAX)
        throw ELibOIEInterfaceException(LIBOIE_ERROR_INVALIDPARAM);

    m_nReceiveBufferSize = nRCVBufferSize;
}

void CServer::CloseAllConnections()
{
	throw ELibOIEInterfaceException(LIBOIE_ERROR_NOTIMPLEMENTED);
}


LibOIE_uint32 CServer::AcceptDevice(const std::string & sDeviceName, const std::string & sApplicationName, const std::string & sVersionName)
{
    if (m_nAcceptRuleCounter >= OIE_MAXRULECOUNT)
        throw ELibOIEInterfaceException(LIBOIE_ERROR_RULEIDOVERFLOW);

    auto pAcceptRule = std::make_shared <CAcceptRule>(m_nAcceptRuleCounter, sDeviceName, sApplicationName, sVersionName);
    m_nAcceptRuleCounter++;

    m_AcceptRules.insert(std::make_pair (pAcceptRule->getRuleID (), pAcceptRule));
    return pAcceptRule->getRuleID();
   
}

void CServer::UnAcceptDevice(const LibOIE_uint32 nRuleID)
{
    m_AcceptRules.erase(nRuleID);
}

void CServer::ClearAcceptedDevices()
{
    m_AcceptRules.clear();
    m_nAcceptRuleCounter = 1;
}

IConnectionIterator * CServer::ListConnections()
{
	return new CConnectionIterator ();
}

void CServer::SetConnectionAcceptedCallback(const LibOIE::ConnectionAcceptedCallback pCallback, const LibOIE_pvoid pUserData)
{

}

void CServer::SetConnectionRejectedCallback(const LibOIE::ConnectionRejectedCallback pCallback, const LibOIE_pvoid pUserData)
{

}

