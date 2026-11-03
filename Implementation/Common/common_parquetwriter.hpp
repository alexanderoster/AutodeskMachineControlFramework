/*++

Copyright (C) 2026 Autodesk Inc.

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

Abstract:

common_parquetwriter.hpp defines a writer for flat Apache Parquet files.

Supported subset: fixed-width columns (INT32, INT64, FLOAT, DOUBLE) without nulls,
PLAIN encoding, one data page per column chunk, UNCOMPRESSED or GZIP codec,
min/max statistics and the type defined column order.

*/

#ifndef __COMMON_PARQUETWRITER
#define __COMMON_PARQUETWRITER

#include <cstdint>
#include <string>
#include <vector>
#include <utility>

#define PARQUETWRITER_DEFAULTROWGROUPSIZE (1024ULL * 1024ULL)
#define PARQUETWRITER_MAXROWGROUPSIZE (64ULL * 1024ULL * 1024ULL)

namespace AMCCommon {

	enum class eParquetPhysicalType : int32_t {
		Int32 = 1,
		Int64 = 2,
		Float = 4,
		Double = 5
	};

	enum class eParquetCompressionCodec : int32_t {
		Uncompressed = 0,
		GZIP = 2
	};

	class CParquetWriter {
	private:

		struct sParquetColumn {
			std::string m_sName;
			eParquetPhysicalType m_PhysicalType;
			// PLAIN encoded values, little endian.
			std::vector<uint8_t> m_PlainData;
		};

		eParquetCompressionCodec m_Codec;
		uint64_t m_nRowGroupSize;
		uint64_t m_nRowCount;
		std::string m_sCreatedBy;

		std::vector<sParquetColumn> m_Columns;
		std::vector<std::pair<std::string, std::string>> m_KeyValueMetaData;

		sParquetColumn& addColumn(const std::string& sName, eParquetPhysicalType physicalType, uint64_t nRowCount);

	public:

		CParquetWriter(eParquetCompressionCodec codec = eParquetCompressionCodec::GZIP, uint64_t nRowGroupSize = PARQUETWRITER_DEFAULTROWGROUPSIZE);

		virtual ~CParquetWriter();

		// All columns must have the same number of rows. Column names must be unique.
		void addInt32Column(const std::string& sName, const std::vector<int32_t>& values);
		void addInt64Column(const std::string& sName, const std::vector<int64_t>& values);
		void addFloatColumn(const std::string& sName, const std::vector<float>& values);
		void addDoubleColumn(const std::string& sName, const std::vector<double>& values);

		void addKeyValueMetaData(const std::string& sKey, const std::string& sValue);

		// Should be of the form "<application> version <major.minor.patch>", which is what readers parse.
		void setCreatedBy(const std::string& sCreatedBy);

		uint64_t getRowCount() const;

		uint32_t getColumnCount() const;

		void writeToBuffer(std::vector<uint8_t>& buffer);

		static uint32_t getValueSize(eParquetPhysicalType physicalType);

	};

}

#endif //__COMMON_PARQUETWRITER
