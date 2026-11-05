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


#ifndef __AMC_DATATABLEENCODING
#define __AMC_DATATABLEENCODING

#include <cstdint>
#include <cstddef>
#include <vector>

/*************************************************************************************************************************

 Page encodings and codecs of the chunked data table format (see amc_datatableformat.hpp).

 The encodings are restricted to ones that Parquet also has, so that a future Parquet writer is a
 serialisation pass without transcoding:

   PLAIN     raw little-endian value array
   DELTA     Parquet DELTA_BINARY_PACKED over 64 bit integers: blocks of 128 deltas in 4 miniblocks,
             each miniblock bit-packed with its own bit width after subtracting the minimum delta of the block
   BITPACK   one bit per value, least significant bit first. This is how Parquet stores PLAIN booleans.

 Codecs compress an encoded page as a whole: LZ4 block format (Parquet LZ4_RAW) and zlib deflate.

**************************************************************************************************************************/

#define DATATABLE_DELTA_BLOCKSIZE 128
#define DATATABLE_DELTA_MINIBLOCKCOUNT 4
#define DATATABLE_DELTA_MINIBLOCKSIZE (DATATABLE_DELTA_BLOCKSIZE / DATATABLE_DELTA_MINIBLOCKCOUNT)

namespace AMC {

	class CDataTableEncoding {
	public:

		// Appends the DELTA_BINARY_PACKED encoding of the values to the buffer.
		static void encodeDelta(const int64_t* pValues, size_t nValueCount, std::vector<uint8_t>& buffer);

		// Decodes exactly nValueCount values. Fails with an EDataTableFormatException if the data is
		// corrupt, ends early or holds a different number of values.
		static void decodeDelta(const uint8_t* pData, size_t nDataSize, int64_t* pValues, size_t nValueCount);

		// Appends one bit per value to the buffer. Any nonzero value is a set bit.
		static void encodeBitPack(const uint8_t* pValues, size_t nValueCount, std::vector<uint8_t>& buffer);

		// Decodes nValueCount bits into bytes that are 0 or 1.
		static void decodeBitPack(const uint8_t* pData, size_t nDataSize, uint8_t* pValues, size_t nValueCount);

		static size_t getBitPackSize(size_t nValueCount);

		static bool codecIsValid(uint32_t nCodec);

		// Compresses the data into the buffer, which is replaced. Returns false and leaves the buffer
		// undefined if the codec is NONE or the compressed data is not smaller than the input.
		static bool compress(uint32_t nCodec, const uint8_t* pData, size_t nDataSize, std::vector<uint8_t>& buffer);

		// Decompresses into pTarget. The decompressed size MUST be exactly nTargetSize.
		static void decompress(uint32_t nCodec, const uint8_t* pData, size_t nDataSize, uint8_t* pTarget, size_t nTargetSize);

	};

}


#endif //__AMC_DATATABLEENCODING
