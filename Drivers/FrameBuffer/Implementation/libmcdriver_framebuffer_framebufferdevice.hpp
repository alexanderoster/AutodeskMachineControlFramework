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


Abstract: This is the class declaration of CFrameBufferAccess

*/


#ifndef __LIBMCDRIVER_FRAMEBUFFER_FRAMEBUFFERDEVICE
#define __LIBMCDRIVER_FRAMEBUFFER_FRAMEBUFFERDEVICE

#include "libmcdriver_framebuffer_interfaces.hpp"
#include "libmcdriver_framebuffer_framebufferinstance.hpp"


namespace LibMCDriver_FrameBuffer {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CFrameBufferAccess 
**************************************************************************************************************************/

class CFrameBufferDeviceInstance : public CFrameBufferInstance {
private:

	int m_nFBDeviceHandle;
	uint8_t* m_pFramebufferPtr;
	uint32_t m_nCurrentBufferIndex;
	bool m_bDoubleBufferingEnabled;

	uint32_t m_nVirtualResolutionY;
	uint32_t m_nMemorySize;

	uint32_t m_nScanLineLength;


public:
	
	CFrameBufferDeviceInstance (const std::string & sIdentifier, const std::string & sDevicePath);
	
	virtual ~CFrameBufferDeviceInstance ();
	
	virtual void flip() override;

	virtual bool usesDoubleBuffering() override;

};

typedef std::shared_ptr<CFrameBufferDeviceInstance> PFrameBufferDeviceInstance;

} // namespace Impl
} // namespace LibMCDriver_FrameBuffer

#endif // __LIBMCDRIVER_FRAMEBUFFER_FRAMEBUFFERDEVICE
