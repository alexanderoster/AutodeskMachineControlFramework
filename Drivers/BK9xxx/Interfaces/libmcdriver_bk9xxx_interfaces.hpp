/*++

Copyright (C) 2023 Autodesk Inc.

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


This file has been generated by the Automatic Component Toolkit (ACT) version 1.8.0-develop.

Abstract: This is an autogenerated C++ header file in order to allow easy
development of MC Driver Beckhoff BK9xxx bus coupler. The implementer of MC Driver Beckhoff BK9xxx bus coupler needs to
derive concrete classes from the abstract classes in this header.

Interface version: 1.0.0

*/


#ifndef __LIBMCDRIVER_BK9XXX_CPPINTERFACES
#define __LIBMCDRIVER_BK9XXX_CPPINTERFACES

#include <string>
#include <memory>

#include "libmcdriver_bk9xxx_types.hpp"


#include "libmcenv_dynamic.hpp"

namespace LibMCDriver_BK9xxx {
namespace Impl {

/**
 Forward declarations of class interfaces
*/
class IBase;
class IDriver;
class IDriver_BK9xxx;



/*************************************************************************************************************************
 Parameter Cache definitions
**************************************************************************************************************************/

class ParameterCache {
	public:
		virtual ~ParameterCache() {}
};

template <class T1> class ParameterCache_1 : public ParameterCache {
	private:
		T1 m_param1;
	public:
		ParameterCache_1 (const T1 & param1)
			: m_param1 (param1)
		{
		}

		void retrieveData (T1 & param1)
		{
			param1 = m_param1;
		}
};

template <class T1, class T2> class ParameterCache_2 : public ParameterCache {
	private:
		T1 m_param1;
		T2 m_param2;
	public:
		ParameterCache_2 (const T1 & param1, const T2 & param2)
			: m_param1 (param1), m_param2 (param2)
		{
		}

		void retrieveData (T1 & param1, T2 & param2)
		{
			param1 = m_param1;
			param2 = m_param2;
		}
};

template <class T1, class T2, class T3> class ParameterCache_3 : public ParameterCache {
	private:
		T1 m_param1;
		T2 m_param2;
		T3 m_param3;
	public:
		ParameterCache_3 (const T1 & param1, const T2 & param2, const T3 & param3)
			: m_param1 (param1), m_param2 (param2), m_param3 (param3)
		{
		}

		void retrieveData (T1 & param1, T2 & param2, T3 & param3)
		{
			param1 = m_param1;
			param2 = m_param2;
			param3 = m_param3;
		}
};

template <class T1, class T2, class T3, class T4> class ParameterCache_4 : public ParameterCache {
	private:
		T1 m_param1;
		T2 m_param2;
		T3 m_param3;
		T4 m_param4;
	public:
		ParameterCache_4 (const T1 & param1, const T2 & param2, const T3 & param3, const T4 & param4)
			: m_param1 (param1), m_param2 (param2), m_param3 (param3), m_param4 (param4)
		{
		}

		void retrieveData (T1 & param1, T2 & param2, T3 & param3, T4 & param4)
		{
			param1 = m_param1;
			param2 = m_param2;
			param3 = m_param3;
			param4 = m_param4;
		}
};


/*************************************************************************************************************************
 Class interface for Base 
**************************************************************************************************************************/

class IBase {
private:
	std::unique_ptr<ParameterCache> m_ParameterCache;
public:
	/**
	* IBase::~IBase - virtual destructor of IBase
	*/
	virtual ~IBase() {};

	/**
	* IBase::ReleaseBaseClassInterface - Releases ownership of a base class interface. Deletes the reference, if necessary.
	* @param[in] pIBase - The base class instance to release
	*/
	static void ReleaseBaseClassInterface(IBase* pIBase)
	{
		if (pIBase) {
			pIBase->DecRefCount();
		}
	};

	/**
	* IBase::AcquireBaseClassInterface - Acquires shared ownership of a base class interface.
	* @param[in] pIBase - The base class instance to acquire
	*/
	static void AcquireBaseClassInterface(IBase* pIBase)
	{
		if (pIBase) {
			pIBase->IncRefCount();
		}
	};


	/**
	* IBase::GetLastErrorMessage - Returns the last error registered of this class instance
	* @param[out] sErrorMessage - Message of the last error registered
	* @return Has an error been registered already
	*/
	virtual bool GetLastErrorMessage(std::string & sErrorMessage) = 0;

	/**
	* IBase::ClearErrorMessages - Clears all registered messages of this class instance
	*/
	virtual void ClearErrorMessages() = 0;

	/**
	* IBase::RegisterErrorMessage - Registers an error message with this class instance
	* @param[in] sErrorMessage - Error message to register
	*/
	virtual void RegisterErrorMessage(const std::string & sErrorMessage) = 0;

	/**
	* IBase::IncRefCount - Increases the reference count of a class instance
	*/
	virtual void IncRefCount() = 0;

	/**
	* IBase::DecRefCount - Decreases the reference count of a class instance and free releases it, if the last reference has been removed
	* @return Has the object been released
	*/
	virtual bool DecRefCount() = 0;

	/**
	* IBase::_setCache - set parameter cache of object
	*/
	void _setCache(ParameterCache * pCache)
	{
		m_ParameterCache.reset(pCache);
	}

	/**
	* IBase::_getCache - returns parameter cache of object
	*/
	ParameterCache* _getCache()
	{
		return m_ParameterCache.get();
	}

};


/**
 Definition of a shared pointer class for IBase
*/
template<class T>
class IBaseSharedPtr : public std::shared_ptr<T>
{
public:
	explicit IBaseSharedPtr(T* t = nullptr)
		: std::shared_ptr<T>(t, IBase::ReleaseBaseClassInterface)
	{
		t->IncRefCount();
	}

	// Reset function, as it also needs to properly set the deleter.
	void reset(T* t = nullptr)
	{
		std::shared_ptr<T>::reset(t, IBase::ReleaseBaseClassInterface);
	}

	// Get-function that increases the Base class's reference count
	T* getCoOwningPtr()
	{
		T* t = this->get();
		t->IncRefCount();
		return t;
	}
};


typedef IBaseSharedPtr<IBase> PIBase;


/*************************************************************************************************************************
 Class interface for Driver 
**************************************************************************************************************************/

class IDriver : public virtual IBase {
public:
	/**
	* IDriver::Configure - Configures a driver with its specific configuration data.
	* @param[in] sConfigurationString - Configuration data of driver.
	*/
	virtual void Configure(const std::string & sConfigurationString) = 0;

	/**
	* IDriver::GetName - returns the name identifier of the driver
	* @return Name of the driver.
	*/
	virtual std::string GetName() = 0;

	/**
	* IDriver::GetType - returns the type identifier of the driver
	* @return Type of the driver.
	*/
	virtual std::string GetType() = 0;

	/**
	* IDriver::GetVersion - returns the version identifiers of the driver
	* @param[out] nMajor - Major version.
	* @param[out] nMinor - Minor version.
	* @param[out] nMicro - Micro version.
	* @param[out] sBuild - Build identifier.
	*/
	virtual void GetVersion(LibMCDriver_BK9xxx_uint32 & nMajor, LibMCDriver_BK9xxx_uint32 & nMinor, LibMCDriver_BK9xxx_uint32 & nMicro, std::string & sBuild) = 0;

	/**
	* IDriver::GetHeaderInformation - returns the header information
	* @param[out] sNameSpace - NameSpace of the driver.
	* @param[out] sBaseName - BaseName of the driver.
	*/
	virtual void GetHeaderInformation(std::string & sNameSpace, std::string & sBaseName) = 0;

	/**
	* IDriver::QueryParameters - Stores the driver parameters in the driver environment.
	*/
	virtual void QueryParameters() = 0;

};

typedef IBaseSharedPtr<IDriver> PIDriver;


/*************************************************************************************************************************
 Class interface for Driver_BK9xxx 
**************************************************************************************************************************/

class IDriver_BK9xxx : public virtual IDriver {
public:
	/**
	* IDriver_BK9xxx::SetToSimulationMode - Turns the driver into a simulation mode.
	*/
	virtual void SetToSimulationMode() = 0;

	/**
	* IDriver_BK9xxx::IsSimulationMode - Returns if the driver is in simulation mode.
	* @return Flag if driver is in simulation mode.
	*/
	virtual bool IsSimulationMode() = 0;

	/**
	* IDriver_BK9xxx::Connect - Connects to a BK9xxx bus coupler.
	* @param[in] sIPAddress - Address of BK9xxx coupler.
	* @param[in] nPort - Modbus TCP port of BK9xxx coupler. (Usually 502).
	* @param[in] nTimeOutInMs - Connection timeout in milliseconds.
	*/
	virtual void Connect(const std::string & sIPAddress, const LibMCDriver_BK9xxx_uint32 nPort, const LibMCDriver_BK9xxx_uint32 nTimeOutInMs) = 0;

	/**
	* IDriver_BK9xxx::GetIPAddress - Returns the IP Address of the connection.
	* @return Address of BK9xxx coupler.
	*/
	virtual std::string GetIPAddress() = 0;

	/**
	* IDriver_BK9xxx::GetPort - Returns the Port of the connection.
	* @return Modbus TCP port of BK9xxx coupler. (Usually 502).
	*/
	virtual LibMCDriver_BK9xxx_uint32 GetPort() = 0;

	/**
	* IDriver_BK9xxx::GetTimeOut - Returns the Timeout of the connection.
	* @return Connection timeout in milliseconds.
	*/
	virtual LibMCDriver_BK9xxx_uint32 GetTimeOut() = 0;

	/**
	* IDriver_BK9xxx::SetErrorRecoveryMode - Sets the connection recovery mode.
	* @param[in] nReconnectionDelayInMs - If connection is lost, how much time should be waited to reconnect in Milliseconds. Default is 300.
	* @param[in] nReconnectionCount - How many times should a reconnection be attempted. Default is 3.
	*/
	virtual void SetErrorRecoveryMode(const LibMCDriver_BK9xxx_uint32 nReconnectionDelayInMs, const LibMCDriver_BK9xxx_uint32 nReconnectionCount) = 0;

	/**
	* IDriver_BK9xxx::GetErrorRecoveryMode - Gets the connection recovery mode.
	* @param[out] nReconnectionDelayInMs - If connection is lost, how much time should be waited to reconnect in Milliseconds. Default is 300.
	* @param[in] nReconnectionCount - How many times should a reconnection be attempted. Default is 3.
	*/
	virtual void GetErrorRecoveryMode(LibMCDriver_BK9xxx_uint32 & nReconnectionDelayInMs, const LibMCDriver_BK9xxx_uint32 nReconnectionCount) = 0;

	/**
	* IDriver_BK9xxx::Disconnect - Disconnects from the Bus coupler.
	*/
	virtual void Disconnect() = 0;

	/**
	* IDriver_BK9xxx::ResetDevice - Resets the Bk9xxx bus coupler. Fails if not connected.
	*/
	virtual void ResetDevice() = 0;

	/**
	* IDriver_BK9xxx::Reconnect - Reconnects to the Bk9xxx bus coupler. Fails if Connect has never been called.
	*/
	virtual void Reconnect() = 0;

	/**
	* IDriver_BK9xxx::IsConnected - Returns if the the Bk9xxx bus coupler is connected.
	* @return Flag if BK9xxx is connected.
	*/
	virtual bool IsConnected() = 0;

	/**
	* IDriver_BK9xxx::GetErrorState - Returns the error state of the Bk9xxx bus coupler connection.
	* @return Error state of the connection.
	*/
	virtual LibMCDriver_BK9xxx::eErrorState GetErrorState() = 0;

	/**
	* IDriver_BK9xxx::VariableExists - Returns if any input or output exists by this name.
	* @param[in] sName - Name of variable.
	* @return Flag if value exists.
	*/
	virtual bool VariableExists(const std::string & sName) = 0;

	/**
	* IDriver_BK9xxx::GetVariableType - Returns type of a variable.
	* @param[in] sName - Name of variable.
	* @return Type of variable.
	*/
	virtual LibMCDriver_BK9xxx::eVariableType GetVariableType(const std::string & sName) = 0;

	/**
	* IDriver_BK9xxx::DigitalInputExists - Returns if a digital input exists.
	* @param[in] sName - Name of variable.
	* @return Flag if value exists.
	*/
	virtual bool DigitalInputExists(const std::string & sName) = 0;

	/**
	* IDriver_BK9xxx::DigitalOutputExists - Returns if a digital output exists.
	* @param[in] sName - Name of variable.
	* @return Flag if value exists.
	*/
	virtual bool DigitalOutputExists(const std::string & sName) = 0;

	/**
	* IDriver_BK9xxx::AnalogInputExists - Returns if an analog input exists.
	* @param[in] sName - Name of variable.
	* @return Flag if value exists.
	*/
	virtual bool AnalogInputExists(const std::string & sName) = 0;

	/**
	* IDriver_BK9xxx::AnalogOutputExists - Returns if an analog output exists.
	* @param[in] sName - Name of variable.
	* @return Flag if value exists.
	*/
	virtual bool AnalogOutputExists(const std::string & sName) = 0;

	/**
	* IDriver_BK9xxx::GetDigitalInput - Reads a value from an digital input variable. Fails if variable does not exist.
	* @param[in] sVariableName - Name of variable.
	* @return Result value.
	*/
	virtual bool GetDigitalInput(const std::string & sVariableName) = 0;

	/**
	* IDriver_BK9xxx::GetDigitalOutput - Reads a value from an digital output variable. Fails if variable does not exist.
	* @param[in] sVariableName - Name of variable.
	* @return Result value.
	*/
	virtual bool GetDigitalOutput(const std::string & sVariableName) = 0;

	/**
	* IDriver_BK9xxx::GetAnalogInputRaw - Reads a raw value from an analog input variable. Fails if variable does not exist.
	* @param[in] sVariableName - Name of variable.
	* @return Result value.
	*/
	virtual LibMCDriver_BK9xxx_uint32 GetAnalogInputRaw(const std::string & sVariableName) = 0;

	/**
	* IDriver_BK9xxx::GetAnalogOutputRaw - Reads a raw value from an analog output variable. Fails if variable does not exist.
	* @param[in] sVariableName - Name of variable.
	* @return Result value.
	*/
	virtual LibMCDriver_BK9xxx_uint32 GetAnalogOutputRaw(const std::string & sVariableName) = 0;

	/**
	* IDriver_BK9xxx::GetAnalogInput - Reads a scaled value from an analog input variable. Fails if variable does not exist.
	* @param[in] sVariableName - Name of variable.
	* @return Result value.
	*/
	virtual LibMCDriver_BK9xxx_double GetAnalogInput(const std::string & sVariableName) = 0;

	/**
	* IDriver_BK9xxx::GetAnalogOutput - Reads a scaled value from an analog output variable. Fails if variable does not exist.
	* @param[in] sVariableName - Name of variable.
	* @return Result value.
	*/
	virtual LibMCDriver_BK9xxx_double GetAnalogOutput(const std::string & sVariableName) = 0;

	/**
	* IDriver_BK9xxx::SetDigitalOutput - Writes a value to a digital output variable. Fails if variable does not exist.
	* @param[in] sVariableName - Name of variable.
	* @param[in] bValue - Value to be set.
	*/
	virtual void SetDigitalOutput(const std::string & sVariableName, const bool bValue) = 0;

	/**
	* IDriver_BK9xxx::SetAnalogOutputRaw - Writes a raw value to an analog output variable. Fails if variable does not exist.
	* @param[in] sVariableName - Name of variable.
	* @param[in] nValue - Value to be set.
	*/
	virtual void SetAnalogOutputRaw(const std::string & sVariableName, const LibMCDriver_BK9xxx_uint32 nValue) = 0;

	/**
	* IDriver_BK9xxx::SetAnalogOutput - Writes a scaled value to an analog output variable. Fails if variable does not exist.
	* @param[in] sVariableName - Name of variable.
	* @param[in] dValue - Value to be set.
	*/
	virtual void SetAnalogOutput(const std::string & sVariableName, const LibMCDriver_BK9xxx_double dValue) = 0;

};

typedef IBaseSharedPtr<IDriver_BK9xxx> PIDriver_BK9xxx;


/*************************************************************************************************************************
 Global functions declarations
**************************************************************************************************************************/
class CWrapper {
public:
	// Injected Components
	static LibMCEnv::PWrapper sPLibMCEnvWrapper;

	/**
	* Ilibmcdriver_bk9xxx::GetVersion - retrieves the binary version of this library.
	* @param[out] nMajor - returns the major version of this library
	* @param[out] nMinor - returns the minor version of this library
	* @param[out] nMicro - returns the micro version of this library
	*/
	static void GetVersion(LibMCDriver_BK9xxx_uint32 & nMajor, LibMCDriver_BK9xxx_uint32 & nMinor, LibMCDriver_BK9xxx_uint32 & nMicro);

	/**
	* Ilibmcdriver_bk9xxx::GetLastError - Returns the last error recorded on this object
	* @param[in] pInstance - Instance Handle
	* @param[out] sErrorMessage - Message of the last error
	* @return Is there a last error to query
	*/
	static bool GetLastError(IBase* pInstance, std::string & sErrorMessage);

	/**
	* Ilibmcdriver_bk9xxx::ReleaseInstance - Releases shared ownership of an Instance
	* @param[in] pInstance - Instance Handle
	*/
	static void ReleaseInstance(IBase* pInstance);

	/**
	* Ilibmcdriver_bk9xxx::AcquireInstance - Acquires shared ownership of an Instance
	* @param[in] pInstance - Instance Handle
	*/
	static void AcquireInstance(IBase* pInstance);

	/**
	* Ilibmcdriver_bk9xxx::CreateDriver - Creates a driver instance with a specific name.
	* @param[in] sName - Name of driver to be created.
	* @param[in] sType - Type of driver to be created.
	* @param[in] pDriverEnvironment - Environment of this driver.
	* @return New Driver instance
	*/
	static IDriver * CreateDriver(const std::string & sName, const std::string & sType, LibMCEnv::PDriverEnvironment pDriverEnvironment);

};

LibMCDriver_BK9xxxResult LibMCDriver_BK9xxx_GetProcAddress (const char * pProcName, void ** ppProcAddress);

} // namespace Impl
} // namespace LibMCDriver_BK9xxx

#endif // __LIBMCDRIVER_BK9XXX_CPPINTERFACES