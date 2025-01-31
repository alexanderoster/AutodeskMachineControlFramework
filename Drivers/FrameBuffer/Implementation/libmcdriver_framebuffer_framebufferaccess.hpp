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


#ifndef __LIBMCDRIVER_FRAMEBUFFER_FRAMEBUFFERACCESS
#define __LIBMCDRIVER_FRAMEBUFFER_FRAMEBUFFERACCESS

#include "libmcdriver_framebuffer_interfaces.hpp"

// Parent classes
#include "libmcdriver_framebuffer_driver.hpp"
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4250)
#endif

// Include custom headers here.
#include "libmcdriver_framebuffer_framebufferinstance.hpp"

namespace LibMCDriver_FrameBuffer {
namespace Impl {


/*************************************************************************************************************************
 Class declaration of CFrameBufferAccess 
**************************************************************************************************************************/

class CFrameBufferAccess : public virtual IFrameBufferAccess, public virtual CBase {
private:

	PFrameBufferInstance m_pInstance;

public:

	CFrameBufferAccess(PFrameBufferInstance pInstance);

	virtual ~CFrameBufferAccess();


	LibMCDriver_FrameBuffer_uint32 GetScreenWidth() override;

	LibMCDriver_FrameBuffer_uint32 GetScreenHeight() override;

	LibMCDriver_FrameBuffer::eFrameBufferBitDepth BitDepth() override;

	bool UsesDoubleBuffering() override;

	void Flip() override;

	void ClearScreen(const LibMCDriver_FrameBuffer::sColor RGBColor) override;

	void ClearScreenRGB(const LibMCDriver_FrameBuffer_uint8 nRed, const LibMCDriver_FrameBuffer_uint8 nGreen, const LibMCDriver_FrameBuffer_uint8 nBlue) override;

	void SetPixel(const LibMCDriver_FrameBuffer_int32 nX, const LibMCDriver_FrameBuffer_int32 nY, const LibMCDriver_FrameBuffer::sColor RGBColor) override;
	
	void SetPixelRGB(const LibMCDriver_FrameBuffer_int32 nX, const LibMCDriver_FrameBuffer_int32 nY, const LibMCDriver_FrameBuffer_uint8 nRed, const LibMCDriver_FrameBuffer_uint8 nGreen, const LibMCDriver_FrameBuffer_uint8 nBlue) override;

	void FillRectangle(const LibMCDriver_FrameBuffer_int32 nX1, const LibMCDriver_FrameBuffer_int32 nY1, const LibMCDriver_FrameBuffer_int32 nX2, const LibMCDriver_FrameBuffer_int32 nY2, const LibMCDriver_FrameBuffer::sColor RGBColor) override;

	void FillRectangleRGB(const LibMCDriver_FrameBuffer_int32 nX1, const LibMCDriver_FrameBuffer_int32 nY1, const LibMCDriver_FrameBuffer_int32 nX2, const LibMCDriver_FrameBuffer_int32 nY2, const LibMCDriver_FrameBuffer_uint8 nRed, const LibMCDriver_FrameBuffer_uint8 nGreen, const LibMCDriver_FrameBuffer_uint8 nBlue) override;

	void DrawImage(const LibMCDriver_FrameBuffer_int32 nX, const LibMCDriver_FrameBuffer_int32 nY, LibMCEnv::PImageData pImage) override;

};

} // namespace Impl
} // namespace LibMCDriver_Framebuffer

#ifdef _MSC_VER
#pragma warning(pop)
#endif
#endif // __LIBMCDRIVER_FRAMEBUFFER_FRAMEBUFFERACCESS
