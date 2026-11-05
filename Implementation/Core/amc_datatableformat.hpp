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


#ifndef __AMC_DATATABLEFORMAT
#define __AMC_DATATABLEFORMAT

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

/*************************************************************************************************************************

 Format version 2 of application/amcf-datatable: one chunked columnar segment.

   [ segment header ]   64 bytes, signature DATATABLE_HEADERSIGNATURE_V2
   [ chunk 0 ]          column 0 page | column 1 page | ... | column N-1 page
   [ chunk 1 ]          ...
   [ footer ]           footer header, column schema, page directory (chunk count x column count entries)
   [ trailer ]          16 bytes: footer length and magic. It locates the footer from the end of the stream.

 A segment is strictly append-only: nothing that has been written is ever rewritten, so a running
 hash over the stream stays valid and a writer never seeks back. Every page carries a small header
 with its own type and length, which keeps an interrupted segment recoverable by a forward scan.
 A segment without trailer is reported as truncated.

 All values are little-endian. Every column is fixed-width.

**************************************************************************************************************************/

#define DATATABLE_FORMATVERSION_2 2

#define DATATABLE_V2_PAGEMAGIC 0x47503244
#define DATATABLE_V2_FOOTERMAGIC 0x54463244
#define DATATABLE_V2_TRAILERMAGIC 0x52543244

// Page codecs. A page is stored uncompressed if compression does not make it smaller.
#define DATATABLE_V2_CODEC_NONE 0
#define DATATABLE_V2_CODEC_LZ4 1
#define DATATABLE_V2_CODEC_DEFLATE 2

// Page encodings, see amc_datatableencoding.hpp. PLAIN is a raw value array and has the same value as the
// RAW encoding of format version 1. A page falls back to PLAIN if its encoding does not make it smaller.
#define DATATABLE_V2_ENCODING_PLAIN 1
#define DATATABLE_V2_ENCODING_DELTA 2
#define DATATABLE_V2_ENCODING_BITPACK 3

#define DATATABLE_V2_PAGEFLAG_STATISTICSVALID 1

#define DATATABLE_V2_MAXCHUNKCOUNT (64 * 1024 * 1024)
#define DATATABLE_V2_MAXSTRINGLENGTH (1024 * 1024)

namespace AMC {

	// Column data types. The values are identical to LibMCEnv::eDataTableColumnType.
	enum class eDataTableV2DataType : uint32_t {
		Unknown = 0,
		Double = 1,
		Int32 = 2,
		Uint32 = 3,
		Int64 = 4,
		Uint64 = 5,
		Float = 6,
		Bool = 7,
		Timestamp = 8
	};

#pragma pack(push)
#pragma pack(1)

	struct sDataTableV2SegmentHeader {
		uint32_t m_nSignature;
		uint32_t m_nVersion;
		uint32_t m_nHeaderSize;
		uint32_t m_nFlags;
		uint32_t m_nSegmentIndex;
		uint32_t m_nColumnCount;
		uint64_t m_nFirstRowIndex;
		uint32_t m_nChunkRowCapacity;
		uint32_t m_nReserved[7];
	};

	struct sDataTableV2PageHeader {
		uint32_t m_nMagic;
		uint32_t m_nColumnIndex;
		uint32_t m_nRowCount;
		uint32_t m_nDataType;
		uint32_t m_nCodec;
		uint32_t m_nEncoding;
		uint32_t m_nCompressedSize;
		uint32_t m_nUncompressedSize;
	};

	// Minimum and maximum are 8 bytes typed by the column: the bit pattern of a double for Double and Float
	// columns, a signed 64 bit integer for Int32 and Int64 columns and an unsigned one for all others.
	struct sDataTableV2PageEntry {
		uint64_t m_nFileOffset;
		uint32_t m_nCompressedSize;
		uint32_t m_nUncompressedSize;
		uint32_t m_nRowCount;
		uint32_t m_nCodec;
		uint32_t m_nEncoding;
		uint64_t m_nMinValue;
		uint64_t m_nMaxValue;
		uint32_t m_nFlags;
		uint32_t m_nReserved;
	};

	struct sDataTableV2FooterHeader {
		uint32_t m_nMagic;
		uint32_t m_nColumnCount;
		uint32_t m_nChunkCount;
		uint32_t m_nSchemaSize;
		uint64_t m_nRowCount;
		uint64_t m_nFirstRowIndex;
		uint32_t m_nReserved[2];
	};

	// Followed by the identifier bytes and the description bytes.
	struct sDataTableV2SchemaEntry {
		uint32_t m_nDataType;
		uint32_t m_nDefaultEncoding;
		uint32_t m_nIdentifierLength;
		uint32_t m_nDescriptionLength;
		double m_dQuantizationScale;
		uint32_t m_nReserved[4];
	};

	struct sDataTableV2Trailer {
		uint64_t m_nFooterLength;
		uint32_t m_nReserved;
		uint32_t m_nMagic;
	};

#pragma pack(pop)

	static_assert(sizeof(sDataTableV2SegmentHeader) == 64, "datatable v2 segment header must be 64 bytes");
	static_assert(sizeof(sDataTableV2PageHeader) == 32, "datatable v2 page header must be 32 bytes");
	static_assert(sizeof(sDataTableV2PageEntry) == 52, "datatable v2 page directory entry must be 52 bytes");
	static_assert(sizeof(sDataTableV2FooterHeader) == 40, "datatable v2 footer header must be 40 bytes");
	static_assert(sizeof(sDataTableV2SchemaEntry) == 40, "datatable v2 schema entry must be 40 bytes");
	static_assert(sizeof(sDataTableV2Trailer) == 16, "datatable v2 trailer must be 16 bytes");


	enum class eDataTableFormatError : uint32_t {
		InvalidParam = 1,
		InvalidSignature = 2,
		UnsupportedVersion = 3,
		UnsupportedCodec = 4,
		UnsupportedEncoding = 5,
		InvalidDataType = 6,
		Truncated = 7,
		Corrupt = 8,
		InvalidRange = 9,
		SegmentPurged = 10,
		ColumnNotFound = 11,
		TypeMismatch = 12
	};

	class EDataTableFormatException : public std::runtime_error {
	private:
		eDataTableFormatError m_Error;

	public:
		EDataTableFormatException(eDataTableFormatError error, const std::string& sMessage);

		eDataTableFormatError getError() const;
	};


	struct sDataTableV2Column {
		std::string m_sIdentifier;
		std::string m_sDescription;
		eDataTableV2DataType m_DataType;

		// Encoding the pages of the column are written with. Single pages may fall back to PLAIN.
		uint32_t m_nEncoding = DATATABLE_V2_ENCODING_PLAIN;

		// Quantisation step of a Double column with DELTA encoding: values are stored as integer multiples
		// of the step. 0 means that the column is not quantised and stays bit-exact.
		double m_dQuantizationStep = 0.0;
	};

	// One chunk in its on-disk form: the page headers and payloads of all columns.
	struct sDataTableV2EncodedChunk {
		uint32_t m_nRowCount = 0;
		std::vector<uint8_t> m_Bytes;

		// One entry per column. m_nFileOffset is relative to the start of m_Bytes.
		std::vector<sDataTableV2PageEntry> m_Entries;
	};


	class CDataTableV2 {
	public:

		// Returns the width of one value in bytes. Fails for unknown data types.
		static uint32_t getEntrySize(eDataTableV2DataType dataType);

		static bool dataTypeIsValid(uint32_t nDataType);

		// Computes the typed minimum and maximum of a plain value array. Returns false if there is no
		// comparable value, which is the case for an empty array or a floating point array of NaNs only.
		static bool computeStatistics(eDataTableV2DataType dataType, const uint8_t* pData, uint32_t nRowCount, uint64_t& nMinValue, uint64_t& nMaxValue);

		// Converts a typed statistics value of a page directory entry to a double.
		static double statisticsValueToDouble(eDataTableV2DataType dataType, uint64_t nValue);

		// Returns the encoding a column of the given type is written with by default:
		// DELTA for integers and timestamps, BITPACK for bools, PLAIN for floating point values.
		static uint32_t getDefaultEncoding(eDataTableV2DataType dataType);

		// Returns if pages of a column may carry the encoding. DELTA on a Double column requires a quantisation step.
		static bool encodingIsValidForColumn(eDataTableV2DataType dataType, uint32_t nEncoding, double dQuantizationStep);
	};


	/*************************************************************************************************************************
	 CDataTableV2ChunkEncoder turns the plain value arrays of a chunk into pages: it applies the encoding of each
	 column, compresses the result and computes the page statistics. It keeps its scratch buffers between chunks.
	**************************************************************************************************************************/

	class CDataTableV2ChunkEncoder {
	private:
		std::vector<sDataTableV2Column> m_Columns;
		std::vector<uint32_t> m_EntrySizes;
		uint32_t m_nCodec;

		std::vector<int64_t> m_IntegerScratch;
		std::vector<double> m_DoubleScratch;
		std::vector<uint8_t> m_EncodedScratch;
		std::vector<uint8_t> m_CompressedScratch;

		bool quantize(const uint8_t* pData, uint32_t nRowCount, double dStep);
		void widenIntegers(eDataTableV2DataType dataType, const uint8_t* pData, uint32_t nRowCount);

	public:

		CDataTableV2ChunkEncoder(const std::vector<sDataTableV2Column>& columns, uint32_t nCodec);

		// columnData holds one pointer per column to nRowCount plain values of that column.
		void encodeChunk(const std::vector<const uint8_t*>& columnData, uint32_t nRowCount, sDataTableV2EncodedChunk& chunk);

	};


	/*************************************************************************************************************************
	 CDataTableV2SegmentBuilder serialises one segment. It produces bytes and keeps the directory, but does no I/O:
	 the caller appends every produced buffer to the segment stream, in the order of the calls.
	**************************************************************************************************************************/

	class CDataTableV2SegmentBuilder {
	private:
		std::vector<sDataTableV2Column> m_Columns;
		std::vector<uint32_t> m_EntrySizes;

		uint32_t m_nSegmentIndex;
		uint64_t m_nFirstRowIndex;
		uint32_t m_nChunkRowCapacity;

		uint64_t m_nCurrentSize;
		uint64_t m_nRowCount;
		uint32_t m_nChunkCount;
		bool m_bHeaderWritten;
		bool m_bFooterWritten;

		std::vector<sDataTableV2PageEntry> m_Directory;

		// Encoder of appendChunk: the encodings of the columns, no compression.
		CDataTableV2ChunkEncoder m_UncompressedEncoder;
		sDataTableV2EncodedChunk m_UncompressedChunk;

		uint64_t getSchemaSizeInBytes() const;

	public:

		CDataTableV2SegmentBuilder(const std::vector<sDataTableV2Column>& columns, uint32_t nSegmentIndex, uint64_t nFirstRowIndex, uint32_t nChunkRowCapacity);

		// Appends the segment header to the buffer. Must be the first call.
		void appendHeader(std::vector<uint8_t>& buffer);

		// Appends one chunk to the buffer: one page per column. columnData holds one pointer per column
		// to nRowCount plain values of that column. The pages are encoded, but not compressed.
		void appendChunk(const std::vector<const uint8_t*>& columnData, uint32_t nRowCount, std::vector<uint8_t>& buffer);

		// Appends a chunk that has been encoded by a CDataTableV2ChunkEncoder for the same columns.
		void appendEncodedChunk(const sDataTableV2EncodedChunk& chunk, std::vector<uint8_t>& buffer);

		// Appends footer and trailer to the buffer. Must be the last call.
		void appendFooter(std::vector<uint8_t>& buffer);

		// Size of a chunk of plain pages. No encoded chunk is larger.
		uint64_t getChunkSizeInBytes(uint32_t nRowCount) const;

		// Size of footer and trailer of a segment that holds nChunkCount chunks.
		uint64_t getFooterSizeInBytes(uint32_t nChunkCount) const;

		uint64_t getCurrentSize() const;

		uint64_t getRowCount() const;

		uint32_t getChunkCount() const;

		uint32_t getSegmentIndex() const;

		uint64_t getFirstRowIndex() const;

	};


	/*************************************************************************************************************************
	 CDataTableV2SegmentReader parses header, trailer and footer of a segment and reads single pages on request.
	 The constructor validates the complete directory against the stream size, so that a truncated or corrupted
	 segment is reported with an EDataTableFormatException and never yields garbage.
	**************************************************************************************************************************/

	class CDataTableV2SegmentReader {
	public:
		// Reads nSize bytes at the absolute stream offset nOffset into pBuffer.
		typedef std::function<void(uint64_t nOffset, uint64_t nSize, uint8_t* pBuffer)> ReadCallback;

	private:
		ReadCallback m_ReadCallback;
		uint64_t m_nStreamSize;
		uint64_t m_nFooterStart;

		sDataTableV2SegmentHeader m_Header;
		sDataTableV2FooterHeader m_FooterHeader;

		std::vector<sDataTableV2Column> m_Columns;
		std::vector<uint32_t> m_EntrySizes;
		std::vector<sDataTableV2PageEntry> m_Directory;

		std::vector<uint8_t> m_ReadScratch;
		std::vector<uint8_t> m_DecompressScratch;
		std::vector<int64_t> m_IntegerScratch;
		uint64_t m_nPageReadCount;

		void parseFooter(const std::vector<uint8_t>& footer);
		void validateDirectory();

	public:

		CDataTableV2SegmentReader(ReadCallback readCallback, uint64_t nStreamSize);

		uint32_t getSegmentIndex() const;

		uint64_t getFirstRowIndex() const;

		uint64_t getRowCount() const;

		uint32_t getChunkCount() const;

		uint32_t getColumnCount() const;

		const sDataTableV2Column& getColumn(uint32_t nColumnIndex) const;

		uint32_t getChunkRowCount(uint32_t nChunkIndex) const;

		const sDataTableV2PageEntry& getPageEntry(uint32_t nChunkIndex, uint32_t nColumnIndex) const;

		// Reads, decompresses and decodes one page into pTarget as plain values.
		void readPage(uint32_t nChunkIndex, uint32_t nColumnIndex, uint8_t* pTarget, uint64_t nTargetSize);

		// Number of pages that have been read from the stream. Reading the footer does not count.
		uint64_t getPageReadCount() const;

	};

}


#endif //__AMC_DATATABLEFORMAT
