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


#ifndef __AMC_DATACAPTUREQUERY
#define __AMC_DATACAPTUREQUERY

#include "amc_datacapturereader.hpp"
#include "amc_jsonwriter.hpp"

#include <cstdint>
#include <string>
#include <vector>

// API responses are buffered in memory as a whole. A request that would return more values than
// this is rejected, so that a capture of tens of GB can not be pulled into one response.
#define DATACAPTUREQUERY_MAXVALUECOUNT 1000000

#define DATACAPTUREQUERY_DEFAULTTARGETPOINTS 1000
#define DATACAPTUREQUERY_MAXTARGETPOINTS 20000

namespace AMC {

	enum class eDataCaptureQueryDownsample : uint32_t {
		None = 0,
		Stride = 1,
		MinMax = 2,
		Average = 3
	};

	// A request for a part of a data capture.
	struct sDataCaptureQuery {

		// Identifiers of the columns to return, in this order. Empty means all columns.
		std::vector<std::string> m_ColumnIdentifiers;

		// First row. Default is the first row that is still stored.
		bool m_bHasFromRow = false;
		uint64_t m_nFromRow = 0;

		// Number of rows. Default is everything up to the end of the capture.
		bool m_bHasRowCount = false;
		uint64_t m_nRowCount = 0;

		eDataCaptureQueryDownsample m_Downsample = eDataCaptureQueryDownsample::None;
		uint32_t m_nTargetPoints = DATACAPTUREQUERY_DEFAULTTARGETPOINTS;
	};


	/*************************************************************************************************************************
	 CDataCaptureQuery turns a request for a part of a data capture into a response body: JSON, CSV or raw
	 little-endian arrays. It enforces the response size cap. The API handler is a thin layer on top of it.
	**************************************************************************************************************************/

	class CDataCaptureQuery {
	private:

		PDataCaptureReader m_pReader;
		uint64_t m_nMaxValueCount;

		struct sResolvedQuery {
			std::vector<uint32_t> m_ColumnIndices;
			uint64_t m_nFromRow;
			uint64_t m_nRowCount;
			eDataCaptureQueryDownsample m_Downsample;
			uint32_t m_nTargetPoints;

			// Number of buckets of a downsampled query, 0 otherwise.
			uint64_t m_nBucketCount;

			// Number of values per column in the response.
			uint64_t m_nValuesPerColumn;
		};

		sResolvedQuery resolveQuery(const sDataCaptureQuery& query);

		// First row of a bucket relative to the first row of the query.
		static uint64_t bucketStart(const sResolvedQuery& resolved, uint64_t nBucketIndex);

		void readDownsampledColumn(const sResolvedQuery& resolved, uint32_t nColumnIndex, std::vector<double>& values);

	public:

		CDataCaptureQuery(PDataCaptureReader pReader, uint64_t nMaxValueCount = DATACAPTUREQUERY_MAXVALUECOUNT);

		// Parses the downsample parameter of a request: empty, none, stride, minmax or average.
		static eDataCaptureQueryDownsample parseDownsample(const std::string& sValue);

		static std::string downsampleToString(eDataCaptureQueryDownsample downsample);

		static std::string columnTypeToString(eDataTableV2DataType dataType);

		// Adds schema, row counts, segments and the value range of every column to a JSON document.
		// Only manifest and segment footers are read.
		void writeInfo(CJSONWriter& writer);

		// Appends the members of a JSON object that holds the requested data, without the enclosing braces.
		// Without downsampling: "columns" with one value array per column.
		// With downsampling: "series" with one bucket list per column, in the shape of the journal envelope.
		void appendJSONMembers(const sDataCaptureQuery& query, std::string& sJSON);

		// Returns the requested data as CSV with a header line. The first column is the row index.
		std::string buildCSV(const sDataCaptureQuery& query);

		// Returns the requested data as raw little-endian arrays, one array per column after the other.
		// All arrays have the same length, which follows from the buffer size and the number of columns.
		void buildBinary(const sDataCaptureQuery& query, bool bFloat32, std::vector<uint8_t>& buffer);

	};

}


#endif //__AMC_DATACAPTUREQUERY
