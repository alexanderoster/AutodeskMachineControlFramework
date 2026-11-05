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

*/

#include "amc_datatableencoding.hpp"
#include "amc_datatableformat.hpp"

#include "Libraries/lz4/lz4.h"
#include "Libraries/zlib/zlib.h"

#include <cstring>

namespace AMC {

	namespace {

		void writeULEB128(std::vector<uint8_t>& buffer, uint64_t nValue)
		{
			while (nValue >= 0x80) {
				buffer.push_back((uint8_t)(nValue & 0x7F) | 0x80);
				nValue >>= 7;
			}
			buffer.push_back((uint8_t)nValue);
		}

		uint64_t readULEB128(const uint8_t* pData, size_t nDataSize, size_t& nPosition)
		{
			uint64_t nValue = 0;
			for (uint32_t nShift = 0; nShift < 70; nShift += 7) {
				if (nPosition >= nDataSize)
					throw EDataTableFormatException(eDataTableFormatError::Corrupt, "delta encoded page ends inside a number");

				uint8_t nByte = pData[nPosition];
				nPosition++;

				nValue |= ((uint64_t)(nByte & 0x7F)) << nShift;
				if ((nByte & 0x80) == 0)
					return nValue;
			}

			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "delta encoded page holds an invalid number");
		}

		uint64_t zigzagEncode(int64_t nValue)
		{
			return ((uint64_t)nValue << 1) ^ (uint64_t)(nValue >> 63);
		}

		int64_t zigzagDecode(uint64_t nValue)
		{
			return (int64_t)(nValue >> 1) ^ -(int64_t)(nValue & 1);
		}

		uint32_t bitWidthOf(uint64_t nValue)
		{
			uint32_t nWidth = 0;
			while (nValue != 0) {
				nWidth++;
				nValue >>= 1;
			}
			return nWidth;
		}

		// Packs DATATABLE_DELTA_MINIBLOCKSIZE values with nWidth bits each, least significant bit first.
		void packMiniblock(const uint64_t* pValues, uint32_t nWidth, std::vector<uint8_t>& buffer)
		{
			if (nWidth == 0)
				return;

			uint64_t nAccumulator = 0;
			uint32_t nAccumulatedBits = 0;

			for (uint32_t nIndex = 0; nIndex < DATATABLE_DELTA_MINIBLOCKSIZE; nIndex++) {
				uint64_t nValue = pValues[nIndex];
				uint32_t nRemainingBits = nWidth;

				while (nRemainingBits > 0) {
					uint32_t nTakeBits = 64 - nAccumulatedBits;
					if (nTakeBits > nRemainingBits)
						nTakeBits = nRemainingBits;

					uint64_t nPart = (nTakeBits == 64) ? nValue : (nValue & ((1ULL << nTakeBits) - 1));
					nAccumulator |= nPart << nAccumulatedBits;
					nAccumulatedBits += nTakeBits;
					nRemainingBits -= nTakeBits;
					nValue = (nTakeBits == 64) ? 0 : (nValue >> nTakeBits);

					if (nAccumulatedBits == 64) {
						for (uint32_t nByteIndex = 0; nByteIndex < 8; nByteIndex++)
							buffer.push_back((uint8_t)(nAccumulator >> (8 * nByteIndex)));
						nAccumulator = 0;
						nAccumulatedBits = 0;
					}
				}
			}

			// A miniblock is a whole number of bytes, because its size is a multiple of 8 values.
			for (uint32_t nByteIndex = 0; nByteIndex * 8 < nAccumulatedBits; nByteIndex++)
				buffer.push_back((uint8_t)(nAccumulator >> (8 * nByteIndex)));
		}

		// Reads bit-packed values from a byte range, least significant bit first.
		class CBitReader {
		private:
			const uint8_t* m_pData;
			size_t m_nBitPosition;

		public:
			explicit CBitReader(const uint8_t* pData)
				: m_pData(pData), m_nBitPosition(0)
			{
			}

			uint64_t read(uint32_t nWidth)
			{
				uint64_t nValue = 0;
				uint32_t nReadBits = 0;

				while (nReadBits < nWidth) {
					uint32_t nBitOffset = (uint32_t)(m_nBitPosition & 7);
					uint32_t nTakeBits = 8 - nBitOffset;
					if (nTakeBits > nWidth - nReadBits)
						nTakeBits = nWidth - nReadBits;

					uint64_t nPart = ((uint64_t)m_pData[m_nBitPosition >> 3] >> nBitOffset) & ((1ULL << nTakeBits) - 1);
					nValue |= nPart << nReadBits;

					nReadBits += nTakeBits;
					m_nBitPosition += nTakeBits;
				}

				return nValue;
			}
		};

	}


	void CDataTableEncoding::encodeDelta(const int64_t* pValues, size_t nValueCount, std::vector<uint8_t>& buffer)
	{
		if ((nValueCount > 0) && (pValues == nullptr))
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "delta encoding input is null");

		writeULEB128(buffer, DATATABLE_DELTA_BLOCKSIZE);
		writeULEB128(buffer, DATATABLE_DELTA_MINIBLOCKCOUNT);
		writeULEB128(buffer, nValueCount);
		writeULEB128(buffer, zigzagEncode((nValueCount > 0) ? pValues[0] : 0));

		size_t nDeltaCount = (nValueCount > 0) ? (nValueCount - 1) : 0;
		size_t nDeltaIndex = 0;

		uint64_t blockDeltas[DATATABLE_DELTA_BLOCKSIZE];

		while (nDeltaIndex < nDeltaCount) {
			size_t nBlockCount = nDeltaCount - nDeltaIndex;
			if (nBlockCount > DATATABLE_DELTA_BLOCKSIZE)
				nBlockCount = DATATABLE_DELTA_BLOCKSIZE;

			// Deltas use wrapping arithmetic, so that every pair of 64 bit values has a delta.
			int64_t nMinDelta = 0;
			for (size_t nIndex = 0; nIndex < nBlockCount; nIndex++) {
				uint64_t nDelta = (uint64_t)pValues[nDeltaIndex + nIndex + 1] - (uint64_t)pValues[nDeltaIndex + nIndex];
				blockDeltas[nIndex] = nDelta;
				if ((nIndex == 0) || ((int64_t)nDelta < nMinDelta))
					nMinDelta = (int64_t)nDelta;
			}

			writeULEB128(buffer, zigzagEncode(nMinDelta));

			// Missing values of the last block are padded with the minimum delta, which packs as zero.
			for (size_t nIndex = 0; nIndex < DATATABLE_DELTA_BLOCKSIZE; nIndex++) {
				if (nIndex < nBlockCount)
					blockDeltas[nIndex] -= (uint64_t)nMinDelta;
				else
					blockDeltas[nIndex] = 0;
			}

			uint8_t miniblockWidths[DATATABLE_DELTA_MINIBLOCKCOUNT];
			for (size_t nMiniblockIndex = 0; nMiniblockIndex < DATATABLE_DELTA_MINIBLOCKCOUNT; nMiniblockIndex++) {
				size_t nStart = nMiniblockIndex * DATATABLE_DELTA_MINIBLOCKSIZE;

				uint64_t nCombinedBits = 0;
				if (nStart < nBlockCount) {
					for (size_t nIndex = 0; nIndex < DATATABLE_DELTA_MINIBLOCKSIZE; nIndex++)
						nCombinedBits |= blockDeltas[nStart + nIndex];
				}

				miniblockWidths[nMiniblockIndex] = (uint8_t)bitWidthOf(nCombinedBits);
				buffer.push_back(miniblockWidths[nMiniblockIndex]);
			}

			// Miniblocks that hold no value at all are not written.
			for (size_t nMiniblockIndex = 0; nMiniblockIndex < DATATABLE_DELTA_MINIBLOCKCOUNT; nMiniblockIndex++) {
				size_t nStart = nMiniblockIndex * DATATABLE_DELTA_MINIBLOCKSIZE;
				if (nStart < nBlockCount)
					packMiniblock(blockDeltas + nStart, miniblockWidths[nMiniblockIndex], buffer);
			}

			nDeltaIndex += nBlockCount;
		}
	}

	void CDataTableEncoding::decodeDelta(const uint8_t* pData, size_t nDataSize, int64_t* pValues, size_t nValueCount)
	{
		if (pData == nullptr)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "delta decoding input is null");
		if ((nValueCount > 0) && (pValues == nullptr))
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "delta decoding output is null");

		size_t nPosition = 0;
		uint64_t nBlockSize = readULEB128(pData, nDataSize, nPosition);
		uint64_t nMiniblockCount = readULEB128(pData, nDataSize, nPosition);
		uint64_t nTotalCount = readULEB128(pData, nDataSize, nPosition);
		int64_t nFirstValue = zigzagDecode(readULEB128(pData, nDataSize, nPosition));

		if ((nBlockSize == 0) || (nBlockSize > 65536) || (nMiniblockCount == 0) || (nMiniblockCount > 256) || ((nBlockSize % nMiniblockCount) != 0))
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "delta encoded page has an invalid block layout");

		uint64_t nMiniblockSize = nBlockSize / nMiniblockCount;
		if ((nMiniblockSize % 8) != 0)
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "delta encoded page has an invalid miniblock size");

		if (nTotalCount != nValueCount)
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "delta encoded page holds a different number of values");

		if (nValueCount == 0)
			return;

		pValues[0] = nFirstValue;
		size_t nProducedCount = 1;

		while (nProducedCount < nValueCount) {
			int64_t nMinDelta = zigzagDecode(readULEB128(pData, nDataSize, nPosition));

			if (nMiniblockCount > nDataSize - nPosition)
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "delta encoded page ends inside a block header");
			const uint8_t* pWidths = pData + nPosition;
			nPosition += (size_t)nMiniblockCount;

			for (uint64_t nMiniblockIndex = 0; (nMiniblockIndex < nMiniblockCount) && (nProducedCount < nValueCount); nMiniblockIndex++) {
				uint32_t nWidth = pWidths[nMiniblockIndex];
				if (nWidth > 64)
					throw EDataTableFormatException(eDataTableFormatError::Corrupt, "delta encoded page has an invalid bit width");

				size_t nMiniblockBytes = (size_t)(nMiniblockSize * nWidth / 8);
				if (nMiniblockBytes > nDataSize - nPosition)
					throw EDataTableFormatException(eDataTableFormatError::Corrupt, "delta encoded page ends inside a miniblock");

				CBitReader bitReader(pData + nPosition);
				for (uint64_t nIndex = 0; (nIndex < nMiniblockSize) && (nProducedCount < nValueCount); nIndex++) {
					uint64_t nRelativeDelta = (nWidth > 0) ? bitReader.read(nWidth) : 0;
					pValues[nProducedCount] = (int64_t)((uint64_t)pValues[nProducedCount - 1] + (uint64_t)nMinDelta + nRelativeDelta);
					nProducedCount++;
				}

				nPosition += nMiniblockBytes;
			}
		}
	}

	size_t CDataTableEncoding::getBitPackSize(size_t nValueCount)
	{
		return (nValueCount + 7) / 8;
	}

	void CDataTableEncoding::encodeBitPack(const uint8_t* pValues, size_t nValueCount, std::vector<uint8_t>& buffer)
	{
		if ((nValueCount > 0) && (pValues == nullptr))
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "bit packing input is null");

		size_t nStart = buffer.size();
		buffer.resize(nStart + getBitPackSize(nValueCount), 0);

		for (size_t nIndex = 0; nIndex < nValueCount; nIndex++) {
			if (pValues[nIndex] != 0)
				buffer[nStart + (nIndex >> 3)] |= (uint8_t)(1 << (nIndex & 7));
		}
	}

	void CDataTableEncoding::decodeBitPack(const uint8_t* pData, size_t nDataSize, uint8_t* pValues, size_t nValueCount)
	{
		if (nDataSize != getBitPackSize(nValueCount))
			throw EDataTableFormatException(eDataTableFormatError::Corrupt, "bit packed page has an invalid size");
		if (nValueCount == 0)
			return;
		if ((pData == nullptr) || (pValues == nullptr))
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "bit packing buffer is null");

		for (size_t nIndex = 0; nIndex < nValueCount; nIndex++)
			pValues[nIndex] = (pData[nIndex >> 3] >> (nIndex & 7)) & 1;
	}

	bool CDataTableEncoding::codecIsValid(uint32_t nCodec)
	{
		return (nCodec == DATATABLE_V2_CODEC_NONE) || (nCodec == DATATABLE_V2_CODEC_LZ4) || (nCodec == DATATABLE_V2_CODEC_DEFLATE);
	}

	bool CDataTableEncoding::compress(uint32_t nCodec, const uint8_t* pData, size_t nDataSize, std::vector<uint8_t>& buffer)
	{
		if (!codecIsValid(nCodec))
			throw EDataTableFormatException(eDataTableFormatError::UnsupportedCodec, "unsupported datatable page codec: " + std::to_string(nCodec));

		if ((nCodec == DATATABLE_V2_CODEC_NONE) || (nDataSize == 0) || (pData == nullptr))
			return false;

		if (nCodec == DATATABLE_V2_CODEC_LZ4) {
			if (nDataSize > (size_t)LZ4_MAX_INPUT_SIZE)
				return false;

			int nBound = LZ4_compressBound((int)nDataSize);
			if (nBound <= 0)
				return false;

			buffer.resize((size_t)nBound);
			int nCompressedSize = LZ4_compress_default((const char*)pData, (char*)buffer.data(), (int)nDataSize, nBound);
			if ((nCompressedSize <= 0) || ((size_t)nCompressedSize >= nDataSize))
				return false;

			buffer.resize((size_t)nCompressedSize);
			return true;
		}

		// zlib deflate
		if (nDataSize > 0x7FFFFFFFULL)
			return false;

		uLongf nCompressedSize = compressBound((uLong)nDataSize);
		buffer.resize((size_t)nCompressedSize);
		int nResult = compress2(buffer.data(), &nCompressedSize, pData, (uLong)nDataSize, Z_DEFAULT_COMPRESSION);
		if ((nResult != Z_OK) || ((size_t)nCompressedSize >= nDataSize))
			return false;

		buffer.resize((size_t)nCompressedSize);
		return true;
	}

	void CDataTableEncoding::decompress(uint32_t nCodec, const uint8_t* pData, size_t nDataSize, uint8_t* pTarget, size_t nTargetSize)
	{
		if ((pData == nullptr) || (pTarget == nullptr))
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "decompression buffer is null");

		switch (nCodec) {
		case DATATABLE_V2_CODEC_NONE:
			if (nDataSize != nTargetSize)
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "uncompressed page has an invalid size");
			memcpy(pTarget, pData, nDataSize);
			break;

		case DATATABLE_V2_CODEC_LZ4: {
			if ((nDataSize > 0x7FFFFFFFULL) || (nTargetSize > 0x7FFFFFFFULL))
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "LZ4 page is too large");

			int nDecompressedSize = LZ4_decompress_safe((const char*)pData, (char*)pTarget, (int)nDataSize, (int)nTargetSize);
			if ((nDecompressedSize < 0) || ((size_t)nDecompressedSize != nTargetSize))
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "LZ4 page could not be decompressed");
			break;
		}

		case DATATABLE_V2_CODEC_DEFLATE: {
			if ((nDataSize > 0x7FFFFFFFULL) || (nTargetSize > 0x7FFFFFFFULL))
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "deflate page is too large");

			uLongf nDecompressedSize = (uLongf)nTargetSize;
			int nResult = uncompress(pTarget, &nDecompressedSize, pData, (uLong)nDataSize);
			if ((nResult != Z_OK) || ((size_t)nDecompressedSize != nTargetSize))
				throw EDataTableFormatException(eDataTableFormatError::Corrupt, "deflate page could not be decompressed");
			break;
		}

		default:
			throw EDataTableFormatException(eDataTableFormatError::UnsupportedCodec, "unsupported datatable page codec: " + std::to_string(nCodec));
		}
	}

}
