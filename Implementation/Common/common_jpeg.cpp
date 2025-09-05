/*++

Copyright (C) 2025 Autodesk Inc.

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

#include "common_jpeg.hpp"

#define JPEG_MAXSTREAMSIZE (1024UL * 1024UL * 1024UL)

#define STB_IMAGE_IMPLEMENTATION
#include "Libraries/stb_image/stb_image.h"

#define TJE_IMPLEMENTATION
#include "Libraries/TinyJPEG/tiny_jpeg.h"

#include <string>
#include <vector>
#include <stdexcept>

namespace AMCCommon {


	CJPEGImageDecoder::CJPEGImageDecoder(const uint8_t* pBuffer, uint64_t nBufferSize)
		: m_nWidth (0), m_nHeight (0), m_pJPEGData (nullptr), m_ChannelCount (eJPEGChannelCount::ccInvalid)
	{
		if ((nBufferSize > JPEG_MAXSTREAMSIZE) || (nBufferSize == 0))
			throw std::runtime_error("invalid JPEG stream size: " + std::to_string (nBufferSize));

		if (pBuffer == nullptr)
			throw std::runtime_error("invalid JPEG stream buffer"); 
		
		int nSizeX = 0;
		int nSizeY = 0;
		int nChannels = 0;
		m_pJPEGData = stbi_load_from_memory(pBuffer, (int)nBufferSize, &nSizeX, &nSizeY, &nChannels, 0);

		if (m_pJPEGData == nullptr)
			throw std::runtime_error("could not decode JPEG Data");

		if ((nSizeX <= 0) || (nSizeY <= 0)) {
			stbi_image_free(m_pJPEGData);
			m_pJPEGData = nullptr;
			throw std::runtime_error("invalid JPEG image size: " + std::to_string(nSizeX) + "/" + std::to_string(nSizeY));
		}

		m_nWidth = (uint32_t)nSizeX;
		m_nHeight = (uint32_t)nSizeY;

		switch (nChannels) {
			case 1:
				m_ChannelCount = eJPEGChannelCount::ccGray;
				break;
			case 2:
				m_ChannelCount = eJPEGChannelCount::ccGrayAlpha;
				break;
			case 3:
				m_ChannelCount = eJPEGChannelCount::ccRGB;
				break;
			case 4:
				m_ChannelCount = eJPEGChannelCount::ccRGBAlpha;
				break;
			default:
				stbi_image_free(m_pJPEGData);
				m_pJPEGData = nullptr;
				throw std::runtime_error("invalid JPEG channel count: " + std::to_string(nChannels));
		}

	}

	CJPEGImageDecoder::~CJPEGImageDecoder()
	{
		if (m_pJPEGData != nullptr) {
			stbi_image_free(m_pJPEGData);
			m_pJPEGData = nullptr;
		}
	}


	uint32_t CJPEGImageDecoder::getWidth()
	{
		return m_nWidth;
	}

	uint32_t CJPEGImageDecoder::getHeight()
	{
		return m_nHeight;
	}
	
	eJPEGChannelCount CJPEGImageDecoder::getChannelCount()
	{
		return m_ChannelCount;
	}

	void CJPEGImageDecoder::writeToBufferGreyScale8bit(std::vector<uint8_t>& buffer)
	{

		if (m_pJPEGData == nullptr)
			throw std::runtime_error("invalid JPEG pixel data");

		size_t nPixelCount = (size_t)m_nWidth * (size_t)m_nHeight;
		buffer.resize(nPixelCount);

		uint8_t* pSource = m_pJPEGData;
		uint8_t* pTarget = buffer.data();

		switch (m_ChannelCount) {
		case eJPEGChannelCount::ccGray:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				*pTarget = *pSource;
				pTarget++;
				pSource++;
			}
			break;

		case eJPEGChannelCount::ccGrayAlpha:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				*pTarget = *pSource;
				pTarget++;
				pSource++;
				pSource++; // Skip Alpha
			}
			break;

		case eJPEGChannelCount::ccRGB:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				uint8_t nRed = *pSource; pSource++;
				uint8_t nGreen = *pSource; pSource++;
				uint8_t nBlue = *pSource; pSource++;
				*pTarget = (nRed + nGreen + nBlue) / 3;
				pTarget++;
			}
			break;
		case eJPEGChannelCount::ccRGBAlpha:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				uint8_t nRed = *pSource; pSource++;
				uint8_t nGreen = *pSource; pSource++;
				uint8_t nBlue = *pSource; pSource++;
				pSource++; // Skip Alpha
				*pTarget = (nRed + nGreen + nBlue) / 3;
				pTarget++;
			}
			break;

		default:
			throw std::runtime_error ("invalid JPEG color channels : " + std::to_string((uint32_t) m_ChannelCount));
		}


	}

	void CJPEGImageDecoder::writeToBufferRGB24bit(std::vector<uint8_t>& buffer)
	{
		if (m_pJPEGData == nullptr)
			throw std::runtime_error("invalid JPEG pixel data");

		size_t nPixelCount = (size_t)m_nWidth * (size_t)m_nHeight;
		buffer.resize(nPixelCount * 3);

		uint8_t* pSource = m_pJPEGData;
		uint8_t* pTarget = buffer.data();

		switch (m_ChannelCount) {
		case eJPEGChannelCount::ccGray:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				uint8_t nGray = *pSource; pSource++;
				*pTarget = nGray; // Red
				pTarget++;
				*pTarget = nGray; // Green
				pTarget++;
				*pTarget = nGray; // Blue
				pTarget++;
			}
			break;

		case eJPEGChannelCount::ccGrayAlpha:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				uint8_t nGray = *pSource; pSource++;
				*pTarget = nGray; // Red
				pTarget++;
				*pTarget = nGray; // Green
				pTarget++;
				*pTarget = nGray; // Blue
				pTarget++;
				pSource++; // Skip Alpha
			}
			break;

		case eJPEGChannelCount::ccRGB:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				uint8_t nRed = *pSource; pSource++;
				uint8_t nGreen = *pSource; pSource++;
				uint8_t nBlue = *pSource; pSource++;
				*pTarget = nRed;
				pTarget++;
				*pTarget = nGreen;
				pTarget++;
				*pTarget = nBlue;
				pTarget++;
			}
			break;
		case eJPEGChannelCount::ccRGBAlpha:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				uint8_t nRed = *pSource; pSource++;
				uint8_t nGreen = *pSource; pSource++;
				uint8_t nBlue = *pSource; pSource++;
				pSource++; // Skip Alpha
				*pTarget = nRed;
				pTarget++;
				*pTarget = nGreen;
				pTarget++;
				*pTarget = nBlue;
				pTarget++;
			}
			break;

		default:
			throw std::runtime_error("invalid JPEG color channels : " + std::to_string((uint32_t)m_ChannelCount));
		}




	}
	
	void CJPEGImageDecoder::writeToBufferRGBA32bit(std::vector<uint8_t>& buffer)
	{
		if (m_pJPEGData == nullptr)
			throw std::runtime_error("invalid JPEG pixel data");

		size_t nPixelCount = (size_t)m_nWidth * (size_t)m_nHeight;
		buffer.resize(nPixelCount * 4);

		uint8_t* pSource = m_pJPEGData;
		uint8_t* pTarget = buffer.data();

		switch (m_ChannelCount) {
		case eJPEGChannelCount::ccGray:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				uint8_t nGray = *pSource; pSource++;
				*pTarget = nGray; // Red
				pTarget++;
				*pTarget = nGray; // Green
				pTarget++;
				*pTarget = nGray; // Blue
				pTarget++;
				*pTarget = 255; // Alpha
				pTarget++;
			}
			break;

		case eJPEGChannelCount::ccGrayAlpha:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				uint8_t nGray = *pSource; pSource++;
				uint8_t nAlpha = *pSource; pSource++;
				*pTarget = nGray; // Red
				pTarget++;
				*pTarget = nGray; // Green
				pTarget++;
				*pTarget = nGray; // Blue
				pTarget++;
				*pTarget = nAlpha; // Alpha
				pTarget++;
			}
			break;

		case eJPEGChannelCount::ccRGB:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				uint8_t nRed = *pSource; pSource++;
				uint8_t nGreen = *pSource; pSource++;
				uint8_t nBlue = *pSource; pSource++;
				*pTarget = nRed;
				pTarget++;
				*pTarget = nGreen;
				pTarget++;
				*pTarget = nBlue;
				pTarget++;
				*pTarget = 255; // Alpha
				pTarget++;
			}
			break;

		case eJPEGChannelCount::ccRGBAlpha:
			for (size_t nIndex = 0; nIndex < nPixelCount; nIndex++) {
				uint8_t nRed = *pSource; pSource++;
				uint8_t nGreen = *pSource; pSource++;
				uint8_t nBlue = *pSource; pSource++;
				uint8_t nAlpha = *pSource; pSource++;
				*pTarget = nRed;
				pTarget++;
				*pTarget = nGreen;
				pTarget++;
				*pTarget = nBlue;
				pTarget++;
				*pTarget = nAlpha; // Alpha
				pTarget++;
			}
			break;

		default:
			throw std::runtime_error("invalid JPEG color channels : " + std::to_string((uint32_t)m_ChannelCount));
		}


	}





	void jpeg_write_callback(void* context, void* data, int bytesToWrite)
	{
		if (context != nullptr) {
			auto pBuffer = (std::vector<uint8_t>*) context;

			if ((data != nullptr) && (bytesToWrite > 0)) {
				uint8_t* pSource = (uint8_t*) data;
				for (int nIndex = 0; nIndex < bytesToWrite; nIndex++) {
					pBuffer->push_back(*pSource);
					pSource++;
				}

			}
		}
	}

	CJPEGImageEncoder::CJPEGImageEncoder(uint32_t nWidth, uint32_t nHeight, eJPEGChannelCount channelCount, const uint8_t* pImageData, std::vector<uint8_t>& JPEGData, bool bThrowError)
		: m_nWidth (nWidth), m_nHeight (nHeight), m_ChannelCount (channelCount), m_JPEGData (JPEGData)
	{
		if (pImageData == nullptr)
			throw std::runtime_error("invalid JPEG Image data parameter");

		if ((nWidth <= 0) || (nHeight <= 0))
			throw std::runtime_error("invalid JPEG Image data size");

		int quality = 2;

		if (!tje_encode_with_func(jpeg_write_callback, (void*) &m_JPEGData, quality, (int)nWidth, (int)nHeight, (int)channelCount, pImageData)) {
	
			if (bThrowError) 
				throw std::runtime_error("could not encode JPEG data");

			m_JPEGData.clear();
		}
	}

	CJPEGImageEncoder::~CJPEGImageEncoder()
	{

	}

	uint32_t CJPEGImageEncoder::getWidth()
	{
		return m_nWidth;
	}

	uint32_t CJPEGImageEncoder::getHeight()
	{
		return m_nHeight;
	}

	eJPEGChannelCount CJPEGImageEncoder::getChannelCount()
	{
		return m_ChannelCount;
	}

	std::vector<uint8_t>& CJPEGImageEncoder::getJPEGData()
	{
		return m_JPEGData;
	}






}