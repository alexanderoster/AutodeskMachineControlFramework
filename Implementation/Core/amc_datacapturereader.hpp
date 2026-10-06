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


#ifndef __AMC_DATACAPTUREREADER
#define __AMC_DATACAPTUREREADER

#include "amc_datatableformat.hpp"
#include "libmcdata_dynamic.hpp"

#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

// Number of segment streams a reader keeps open at the same time.
#define DATACAPTUREREADER_MAXOPENSEGMENTS 8

namespace AMC {

	enum class eDataCaptureDownsampleMode : uint32_t {
		Stride = 1,
		MinMax = 2,
		Average = 3
	};

	struct sDataCaptureSegmentInfo {
		std::string m_sStreamUUID;
		uint64_t m_nFirstRow;
		uint64_t m_nRowCount;
		uint64_t m_nSizeInBytes;
		uint32_t m_nChunkCount;
		uint64_t m_nFirstChunkIndex;
		bool m_bIsPurged;
	};

	struct sDataCaptureChunkInfo {
		uint32_t m_nSegmentIndex;
		uint64_t m_nFirstRow;
		uint64_t m_nRowCount;
		bool m_bIsAvailable;
	};

	class CDataCaptureOpenSegment;
	typedef std::shared_ptr<CDataCaptureOpenSegment> PDataCaptureOpenSegment;


	/*************************************************************************************************************************
	 CDataCaptureReader reads a data capture lazily across its segments. The segment list is a snapshot of the
	 manifest at construction. A request for a column and a row range is resolved to the pages that overlap it;
	 segment footers are read on demand and a bounded number of segment streams stays open.

	 Errors are reported as EDataTableFormatException. An instance MUST only be used in a single thread.
	**************************************************************************************************************************/

	class CDataCaptureReader {
	private:

		LibMCData::PDataModel m_pDataModel;
		LibMCData::PStorage m_pStorage;

		std::string m_sUUID;
		std::string m_sIdentifier;
		std::string m_sName;

		std::vector<sDataTableV2Column> m_Columns;
		std::vector<uint32_t> m_EntrySizes;
		std::vector<sDataCaptureSegmentInfo> m_Segments;
		uint64_t m_nRowCount;
		uint64_t m_nChunkCount;

		std::map<uint32_t, PDataCaptureOpenSegment> m_OpenSegments;
		std::list<uint32_t> m_OpenSegmentOrder;

		uint64_t m_nPageReadCount;

		// The page that was read last stays decoded in the scratch buffer.
		std::vector<uint8_t> m_PageScratch;
		bool m_bPageCached;
		uint32_t m_nCachedSegmentIndex;
		uint32_t m_nCachedChunkIndex;
		uint32_t m_nCachedColumnIndex;

		const uint8_t* loadPage(CDataCaptureOpenSegment* pOpenSegment, uint32_t nSegmentIndex, uint32_t nChunkIndex, uint32_t nColumnIndex, uint64_t nChunkRowCount);

		void checkColumnIndex(uint32_t nColumnIndex) const;
		void checkRowRange(uint64_t nFromRow, uint64_t nRowCount) const;

		// Returns the index of the segment that holds the row.
		uint32_t findSegmentOfRow(uint64_t nRow) const;

		CDataCaptureOpenSegment* openSegment(uint32_t nSegmentIndex);

		// Calls the callback for every page that overlaps the row range, in row order, with the plain values of
		// the overlapping part: pointer, index of the first row relative to nFromRow and number of rows.
		void forEachPageInRange(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, const std::function<void(const uint8_t* pPlainValues, uint64_t nRangeOffset, uint64_t nCount)>& callback);

		double plainValueToDouble(uint32_t nColumnIndex, const uint8_t* pPlainValues, uint64_t nIndex) const;

	public:

		CDataCaptureReader(LibMCData::PDataModel pDataModel, LibMCData::PDataCapture pManifest);

		virtual ~CDataCaptureReader();

		std::string getUUID() const;

		std::string getIdentifier() const;

		std::string getName() const;

		uint64_t getRowCount() const;

		// Fails with InvalidRange if the row range is not part of the capture.
		void validateRowRange(uint64_t nFromRow, uint64_t nRowCount) const;

		// First row that is still stored. Equal to the row count if nothing is stored.
		uint64_t getFirstAvailableRow() const;

		uint32_t getColumnCount() const;

		const sDataTableV2Column& getColumn(uint32_t nColumnIndex) const;

		bool findColumn(const std::string& sIdentifier, uint32_t& nColumnIndex) const;

		// Fails with ColumnNotFound if the column does not exist.
		uint32_t getColumnIndex(const std::string& sIdentifier) const;

		// Quantisation step of the column as recorded in the stored segments. 0 if the column is not quantised.
		double getColumnQuantization(uint32_t nColumnIndex);

		uint32_t getSegmentCount() const;

		const sDataCaptureSegmentInfo& getSegment(uint32_t nSegmentIndex) const;

		uint64_t getChunkCount() const;

		sDataCaptureChunkInfo getChunkInfo(uint64_t nChunkIndex);

		// Minimum and maximum of a column in a chunk from the page directory. Returns false if the chunk
		// has been deleted or holds no comparable value.
		bool getChunkStatistics(uint64_t nChunkIndex, uint32_t nColumnIndex, double& dMinValue, double& dMaxValue);

		// Reads a row range as plain values of the column type into pTarget.
		void readColumnPlain(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, uint8_t* pTarget, uint64_t nTargetSize);

		// Reads a row range of any column as doubles.
		void readColumnAsDouble(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, double* pTarget);

		// Reads a row range of a Float or Double column as floats.
		void readColumnAsFloat(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, float* pTarget);

		// Reads a row range of an integer, bool or timestamp column as 64 bit integers.
		void readColumnAsInt64(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, int64_t* pTarget);

		// Number of buckets a downsampled read of the range produces.
		static uint64_t getDownsampleBucketCount(uint64_t nRowCount, uint32_t nTargetPoints);

		// Number of values a downsampled read of the range produces: two per bucket in MinMax mode, one otherwise.
		static uint64_t getDownsampleValueCount(uint64_t nRowCount, uint32_t nTargetPoints, eDataCaptureDownsampleMode mode);

		// Reduces a row range to buckets. pTarget receives getDownsampleValueCount values.
		// A bucket without a comparable value yields NaN.
		void readColumnDownsampled(uint32_t nColumnIndex, uint64_t nFromRow, uint64_t nRowCount, uint32_t nTargetPoints, eDataCaptureDownsampleMode mode, double* pTarget);

		// Indices of the stored chunks whose value range of the column overlaps [dMinValue, dMaxValue].
		std::vector<uint64_t> findChunksInValueRange(uint32_t nColumnIndex, double dMinValue, double dMaxValue);

		// Number of pages that have been read from storage so far.
		uint64_t getPageReadCount() const;

	};

	typedef std::shared_ptr<CDataCaptureReader> PDataCaptureReader;

}


#endif //__AMC_DATACAPTUREREADER
