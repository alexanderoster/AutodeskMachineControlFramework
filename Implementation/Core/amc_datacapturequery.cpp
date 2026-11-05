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

#include "amc_datacapturequery.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace AMC {

	namespace {

		template <typename T>
		T loadPlainValue(const uint8_t* pData, uint64_t nIndex)
		{
			T value;
			memcpy(&value, pData + nIndex * sizeof(T), sizeof(T));
			return value;
		}

		// Appends a floating point number. JSON has no representation for NaN and infinity, they become null.
		void appendFloatingPoint(std::string& sOutput, double dValue, int nSignificantDigits, bool bJSON)
		{
			if (!std::isfinite(dValue)) {
				if (bJSON)
					sOutput += "null";
				else
					sOutput += std::isnan(dValue) ? "nan" : ((dValue > 0.0) ? "inf" : "-inf");
				return;
			}

			char buffer[48];
			snprintf(buffer, sizeof(buffer), "%.*g", nSignificantDigits, dValue);
			sOutput += buffer;
		}

		// Appends one plain value of a column. Integers are written exactly, floating point values
		// with as many digits as it takes to read them back unchanged.
		void appendPlainValue(std::string& sOutput, eDataTableV2DataType dataType, const uint8_t* pPlainValues, uint64_t nIndex, bool bJSON)
		{
			switch (dataType) {
			case eDataTableV2DataType::Double:
				appendFloatingPoint(sOutput, loadPlainValue<double>(pPlainValues, nIndex), 17, bJSON);
				break;
			case eDataTableV2DataType::Float:
				appendFloatingPoint(sOutput, (double)loadPlainValue<float>(pPlainValues, nIndex), 9, bJSON);
				break;
			case eDataTableV2DataType::Int32:
				sOutput += std::to_string(loadPlainValue<int32_t>(pPlainValues, nIndex));
				break;
			case eDataTableV2DataType::Uint32:
				sOutput += std::to_string(loadPlainValue<uint32_t>(pPlainValues, nIndex));
				break;
			case eDataTableV2DataType::Int64:
				sOutput += std::to_string(loadPlainValue<int64_t>(pPlainValues, nIndex));
				break;
			case eDataTableV2DataType::Uint64:
			case eDataTableV2DataType::Timestamp:
				sOutput += std::to_string(loadPlainValue<uint64_t>(pPlainValues, nIndex));
				break;
			case eDataTableV2DataType::Bool:
				if (bJSON)
					sOutput += (loadPlainValue<uint8_t>(pPlainValues, nIndex) != 0) ? "true" : "false";
				else
					sOutput += (loadPlainValue<uint8_t>(pPlainValues, nIndex) != 0) ? "1" : "0";
				break;
			default:
				throw EDataTableFormatException(eDataTableFormatError::InvalidDataType, "invalid datatable column data type");
			}
		}

		void appendJSONString(std::string& sOutput, const std::string& sValue)
		{
			sOutput += '"';
			for (char cChar : sValue) {
				switch (cChar) {
				case '"': sOutput += "\\\""; break;
				case '\\': sOutput += "\\\\"; break;
				case '\n': sOutput += "\\n"; break;
				case '\r': sOutput += "\\r"; break;
				case '\t': sOutput += "\\t"; break;
				default:
					if ((unsigned char)cChar < 0x20) {
						char buffer[8];
						snprintf(buffer, sizeof(buffer), "\\u%04x", (unsigned int)(unsigned char)cChar);
						sOutput += buffer;
					}
					else {
						sOutput += cChar;
					}
				}
			}
			sOutput += '"';
		}

		template <typename T>
		void appendBinaryValue(std::vector<uint8_t>& buffer, T value)
		{
			const uint8_t* pData = (const uint8_t*)&value;
			buffer.insert(buffer.end(), pData, pData + sizeof(T));
		}

	}


	CDataCaptureQuery::CDataCaptureQuery(PDataCaptureReader pReader, uint64_t nMaxValueCount)
		: m_pReader(pReader), m_nMaxValueCount(nMaxValueCount)
	{
		if (pReader.get() == nullptr)
			throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "data capture query has no reader");
	}

	eDataCaptureQueryDownsample CDataCaptureQuery::parseDownsample(const std::string& sValue)
	{
		if (sValue.empty() || (sValue == "none"))
			return eDataCaptureQueryDownsample::None;
		if (sValue == "stride")
			return eDataCaptureQueryDownsample::Stride;
		if (sValue == "minmax")
			return eDataCaptureQueryDownsample::MinMax;
		if (sValue == "average")
			return eDataCaptureQueryDownsample::Average;

		throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "invalid downsample mode: " + sValue + " (expected none, stride, minmax or average)");
	}

	std::string CDataCaptureQuery::downsampleToString(eDataCaptureQueryDownsample downsample)
	{
		switch (downsample) {
		case eDataCaptureQueryDownsample::Stride: return "stride";
		case eDataCaptureQueryDownsample::MinMax: return "minmax";
		case eDataCaptureQueryDownsample::Average: return "average";
		default: return "none";
		}
	}

	std::string CDataCaptureQuery::columnTypeToString(eDataTableV2DataType dataType)
	{
		switch (dataType) {
		case eDataTableV2DataType::Double: return "double";
		case eDataTableV2DataType::Int32: return "int32";
		case eDataTableV2DataType::Uint32: return "uint32";
		case eDataTableV2DataType::Int64: return "int64";
		case eDataTableV2DataType::Uint64: return "uint64";
		case eDataTableV2DataType::Float: return "float";
		case eDataTableV2DataType::Bool: return "bool";
		case eDataTableV2DataType::Timestamp: return "timestamp";
		default: return "unknown";
		}
	}

	CDataCaptureQuery::sResolvedQuery CDataCaptureQuery::resolveQuery(const sDataCaptureQuery& query)
	{
		sResolvedQuery resolved;

		if (query.m_ColumnIdentifiers.empty()) {
			for (uint32_t nColumnIndex = 0; nColumnIndex < m_pReader->getColumnCount(); nColumnIndex++)
				resolved.m_ColumnIndices.push_back(nColumnIndex);
		}
		else {
			for (auto& sIdentifier : query.m_ColumnIdentifiers)
				resolved.m_ColumnIndices.push_back(m_pReader->getColumnIndex(sIdentifier));
		}

		uint64_t nTotalRowCount = m_pReader->getRowCount();

		resolved.m_nFromRow = query.m_bHasFromRow ? query.m_nFromRow : m_pReader->getFirstAvailableRow();
		if (resolved.m_nFromRow > nTotalRowCount)
			throw EDataTableFormatException(eDataTableFormatError::InvalidRange, "fromrow " + std::to_string(resolved.m_nFromRow) + " exceeds the " + std::to_string(nTotalRowCount) + " rows of the data capture");

		resolved.m_nRowCount = query.m_bHasRowCount ? query.m_nRowCount : (nTotalRowCount - resolved.m_nFromRow);
		m_pReader->validateRowRange(resolved.m_nFromRow, resolved.m_nRowCount);

		resolved.m_Downsample = query.m_Downsample;
		resolved.m_nTargetPoints = query.m_nTargetPoints;
		resolved.m_nBucketCount = 0;
		resolved.m_nValuesPerColumn = resolved.m_nRowCount;

		if (resolved.m_Downsample != eDataCaptureQueryDownsample::None) {
			if ((resolved.m_nTargetPoints == 0) || (resolved.m_nTargetPoints > DATACAPTUREQUERY_MAXTARGETPOINTS))
				throw EDataTableFormatException(eDataTableFormatError::InvalidParam, "targetpoints must be between 1 and " + std::to_string(DATACAPTUREQUERY_MAXTARGETPOINTS));

			resolved.m_nBucketCount = CDataCaptureReader::getDownsampleBucketCount(resolved.m_nRowCount, resolved.m_nTargetPoints);
			resolved.m_nValuesPerColumn = (resolved.m_Downsample == eDataCaptureQueryDownsample::MinMax) ? (2 * resolved.m_nBucketCount) : resolved.m_nBucketCount;
		}

		// The size cap. A response is buffered as a whole, so an unbounded request must fail here
		// instead of trying to allocate the capture.
		uint64_t nColumnCount = resolved.m_ColumnIndices.size();
		if ((nColumnCount > 0) && (resolved.m_nValuesPerColumn > m_nMaxValueCount / nColumnCount))
			throw EDataTableFormatException(eDataTableFormatError::InvalidRange, "the request returns " + std::to_string(resolved.m_nValuesPerColumn) + " values in each of " + std::to_string(nColumnCount) + " columns, the limit is " + std::to_string(m_nMaxValueCount) + " values: reduce rowcount, or set downsample and targetpoints");

		return resolved;
	}

	uint64_t CDataCaptureQuery::bucketStart(const sResolvedQuery& resolved, uint64_t nBucketIndex)
	{
		// The same partition as CDataCaptureReader::readColumnDownsampled.
		uint64_t nQuotient = resolved.m_nRowCount / resolved.m_nBucketCount;
		uint64_t nRemainder = resolved.m_nRowCount % resolved.m_nBucketCount;
		return nBucketIndex * nQuotient + (nBucketIndex * nRemainder) / resolved.m_nBucketCount;
	}

	void CDataCaptureQuery::readDownsampledColumn(const sResolvedQuery& resolved, uint32_t nColumnIndex, std::vector<double>& values)
	{
		values.resize((size_t)resolved.m_nValuesPerColumn);
		if (values.empty())
			return;

		eDataCaptureDownsampleMode mode = eDataCaptureDownsampleMode::MinMax;
		if (resolved.m_Downsample == eDataCaptureQueryDownsample::Stride)
			mode = eDataCaptureDownsampleMode::Stride;
		else if (resolved.m_Downsample == eDataCaptureQueryDownsample::Average)
			mode = eDataCaptureDownsampleMode::Average;

		m_pReader->readColumnDownsampled(nColumnIndex, resolved.m_nFromRow, resolved.m_nRowCount, resolved.m_nTargetPoints, mode, values.data());
	}

	void CDataCaptureQuery::writeInfo(CJSONWriter& writer)
	{
		writer.addString("uuid", m_pReader->getUUID());
		writer.addString("identifier", m_pReader->getIdentifier());
		writer.addString("name", m_pReader->getName());
		writer.addInteger("rowcount", (int64_t)m_pReader->getRowCount());
		writer.addInteger("firstavailablerow", (int64_t)m_pReader->getFirstAvailableRow());
		writer.addInteger("columncount", (int64_t)m_pReader->getColumnCount());
		writer.addInteger("segmentcount", (int64_t)m_pReader->getSegmentCount());
		writer.addInteger("chunkcount", (int64_t)m_pReader->getChunkCount());

		uint64_t nStoredSize = 0;
		CJSONWriterArray segmentsArray(writer);
		for (uint32_t nSegmentIndex = 0; nSegmentIndex < m_pReader->getSegmentCount(); nSegmentIndex++) {
			auto& segment = m_pReader->getSegment(nSegmentIndex);
			if (!segment.m_bIsPurged)
				nStoredSize += segment.m_nSizeInBytes;

			CJSONWriterObject segmentObject(writer);
			segmentObject.addInteger("index", (int64_t)nSegmentIndex);
			segmentObject.addInteger("firstrow", (int64_t)segment.m_nFirstRow);
			segmentObject.addInteger("rowcount", (int64_t)segment.m_nRowCount);
			segmentObject.addInteger("chunkcount", (int64_t)segment.m_nChunkCount);
			segmentObject.addInteger("size", (int64_t)segment.m_nSizeInBytes);
			segmentObject.addBool("available", !segment.m_bIsPurged);
			segmentsArray.addObject(segmentObject);
		}
		writer.addInteger("storedsize", (int64_t)nStoredSize);

		// The value range of a column is the union of its page statistics in the stored segments.
		uint64_t nChunkCount = m_pReader->getChunkCount();
		CJSONWriterArray columnsArray(writer);
		for (uint32_t nColumnIndex = 0; nColumnIndex < m_pReader->getColumnCount(); nColumnIndex++) {
			auto& column = m_pReader->getColumn(nColumnIndex);

			bool bHasRange = false;
			double dColumnMin = 0.0;
			double dColumnMax = 0.0;
			for (uint64_t nChunkIndex = 0; nChunkIndex < nChunkCount; nChunkIndex++) {
				double dChunkMin = 0.0;
				double dChunkMax = 0.0;
				if (!m_pReader->getChunkStatistics(nChunkIndex, nColumnIndex, dChunkMin, dChunkMax))
					continue;

				if (!bHasRange || (dChunkMin < dColumnMin))
					dColumnMin = dChunkMin;
				if (!bHasRange || (dChunkMax > dColumnMax))
					dColumnMax = dChunkMax;
				bHasRange = true;
			}

			CJSONWriterObject columnObject(writer);
			columnObject.addString("name", column.m_sIdentifier);
			columnObject.addString("description", column.m_sDescription);
			columnObject.addString("type", columnTypeToString(column.m_DataType));
			columnObject.addDouble("quantization", m_pReader->getColumnQuantization(nColumnIndex));
			columnObject.addBool("hasrange", bHasRange);
			columnObject.addDouble("min", dColumnMin);
			columnObject.addDouble("max", dColumnMax);
			columnsArray.addObject(columnObject);
		}

		writer.addArray("columns", columnsArray);
		writer.addArray("segments", segmentsArray);
	}

	void CDataCaptureQuery::appendJSONMembers(const sDataCaptureQuery& query, std::string& sJSON)
	{
		auto resolved = resolveQuery(query);

		sJSON += "\"uuid\":";
		appendJSONString(sJSON, m_pReader->getUUID());
		sJSON += ",\"fromrow\":" + std::to_string(resolved.m_nFromRow);
		sJSON += ",\"rowcount\":" + std::to_string(resolved.m_nRowCount);
		sJSON += ",\"downsample\":";
		appendJSONString(sJSON, downsampleToString(resolved.m_Downsample));

		if (resolved.m_Downsample == eDataCaptureQueryDownsample::None) {
			sJSON += ",\"columns\":[";

			std::vector<uint8_t> plainValues;
			for (size_t nIndex = 0; nIndex < resolved.m_ColumnIndices.size(); nIndex++) {
				uint32_t nColumnIndex = resolved.m_ColumnIndices[nIndex];
				auto& column = m_pReader->getColumn(nColumnIndex);
				uint32_t nEntrySize = CDataTableV2::getEntrySize(column.m_DataType);

				plainValues.resize((size_t)(resolved.m_nRowCount * nEntrySize));
				m_pReader->readColumnPlain(nColumnIndex, resolved.m_nFromRow, resolved.m_nRowCount, plainValues.data(), plainValues.size());

				if (nIndex > 0)
					sJSON += ',';
				sJSON += "{\"name\":";
				appendJSONString(sJSON, column.m_sIdentifier);
				sJSON += ",\"type\":";
				appendJSONString(sJSON, columnTypeToString(column.m_DataType));
				sJSON += ",\"values\":[";
				for (uint64_t nRow = 0; nRow < resolved.m_nRowCount; nRow++) {
					if (nRow > 0)
						sJSON += ',';
					appendPlainValue(sJSON, column.m_DataType, plainValues.data(), nRow, true);
				}
				sJSON += "]}";
			}

			sJSON += ']';
			return;
		}

		// Downsampled: one bucket list per column, in the shape of the journal envelope.
		sJSON += ",\"count\":" + std::to_string(resolved.m_nBucketCount);
		sJSON += ",\"series\":[";

		std::vector<double> values;
		for (size_t nIndex = 0; nIndex < resolved.m_ColumnIndices.size(); nIndex++) {
			uint32_t nColumnIndex = resolved.m_ColumnIndices[nIndex];
			auto& column = m_pReader->getColumn(nColumnIndex);
			readDownsampledColumn(resolved, nColumnIndex, values);

			if (nIndex > 0)
				sJSON += ',';
			sJSON += "{\"name\":";
			appendJSONString(sJSON, column.m_sIdentifier);
			sJSON += ",\"type\":";
			appendJSONString(sJSON, columnTypeToString(column.m_DataType));
			sJSON += ",\"available\":true,\"buckets\":[";

			for (uint64_t nBucketIndex = 0; nBucketIndex < resolved.m_nBucketCount; nBucketIndex++) {
				if (nBucketIndex > 0)
					sJSON += ',';
				sJSON += "{\"row\":" + std::to_string(resolved.m_nFromRow + bucketStart(resolved, nBucketIndex));

				if (resolved.m_Downsample == eDataCaptureQueryDownsample::MinMax) {
					sJSON += ",\"min\":";
					appendFloatingPoint(sJSON, values[(size_t)(2 * nBucketIndex)], 17, true);
					sJSON += ",\"max\":";
					appendFloatingPoint(sJSON, values[(size_t)(2 * nBucketIndex + 1)], 17, true);
				}
				else {
					sJSON += ",\"value\":";
					appendFloatingPoint(sJSON, values[(size_t)nBucketIndex], 17, true);
				}
				sJSON += '}';
			}

			sJSON += "]}";
		}

		sJSON += ']';
	}

	std::string CDataCaptureQuery::buildCSV(const sDataCaptureQuery& query)
	{
		auto resolved = resolveQuery(query);
		const char cSeparator = ';';

		std::string sCSV;
		sCSV += "row";
		for (uint32_t nColumnIndex : resolved.m_ColumnIndices) {
			auto& sIdentifier = m_pReader->getColumn(nColumnIndex).m_sIdentifier;
			if (resolved.m_Downsample == eDataCaptureQueryDownsample::MinMax) {
				sCSV += cSeparator;
				sCSV += sIdentifier + "_min";
				sCSV += cSeparator;
				sCSV += sIdentifier + "_max";
			}
			else {
				sCSV += cSeparator;
				sCSV += sIdentifier;
			}
		}
		sCSV += '\n';

		if (resolved.m_Downsample == eDataCaptureQueryDownsample::None) {
			std::vector<std::vector<uint8_t>> columnValues(resolved.m_ColumnIndices.size());
			for (size_t nIndex = 0; nIndex < resolved.m_ColumnIndices.size(); nIndex++) {
				uint32_t nColumnIndex = resolved.m_ColumnIndices[nIndex];
				uint32_t nEntrySize = CDataTableV2::getEntrySize(m_pReader->getColumn(nColumnIndex).m_DataType);
				columnValues[nIndex].resize((size_t)(resolved.m_nRowCount * nEntrySize));
				m_pReader->readColumnPlain(nColumnIndex, resolved.m_nFromRow, resolved.m_nRowCount, columnValues[nIndex].data(), columnValues[nIndex].size());
			}

			for (uint64_t nRow = 0; nRow < resolved.m_nRowCount; nRow++) {
				sCSV += std::to_string(resolved.m_nFromRow + nRow);
				for (size_t nIndex = 0; nIndex < resolved.m_ColumnIndices.size(); nIndex++) {
					sCSV += cSeparator;
					appendPlainValue(sCSV, m_pReader->getColumn(resolved.m_ColumnIndices[nIndex]).m_DataType, columnValues[nIndex].data(), nRow, false);
				}
				sCSV += '\n';
			}

			return sCSV;
		}

		std::vector<std::vector<double>> columnValues(resolved.m_ColumnIndices.size());
		for (size_t nIndex = 0; nIndex < resolved.m_ColumnIndices.size(); nIndex++)
			readDownsampledColumn(resolved, resolved.m_ColumnIndices[nIndex], columnValues[nIndex]);

		size_t nValuesPerBucket = (resolved.m_Downsample == eDataCaptureQueryDownsample::MinMax) ? 2 : 1;
		for (uint64_t nBucketIndex = 0; nBucketIndex < resolved.m_nBucketCount; nBucketIndex++) {
			sCSV += std::to_string(resolved.m_nFromRow + bucketStart(resolved, nBucketIndex));
			for (size_t nIndex = 0; nIndex < resolved.m_ColumnIndices.size(); nIndex++) {
				for (size_t nValueIndex = 0; nValueIndex < nValuesPerBucket; nValueIndex++) {
					sCSV += cSeparator;
					appendFloatingPoint(sCSV, columnValues[nIndex][(size_t)nBucketIndex * nValuesPerBucket + nValueIndex], 17, false);
				}
			}
			sCSV += '\n';
		}

		return sCSV;
	}

	void CDataCaptureQuery::buildBinary(const sDataCaptureQuery& query, bool bFloat32, std::vector<uint8_t>& buffer)
	{
		auto resolved = resolveQuery(query);

		buffer.clear();
		buffer.reserve((size_t)(resolved.m_nValuesPerColumn * resolved.m_ColumnIndices.size() * (bFloat32 ? sizeof(float) : sizeof(double))));

		std::vector<double> values;
		for (uint32_t nColumnIndex : resolved.m_ColumnIndices) {
			if (resolved.m_Downsample == eDataCaptureQueryDownsample::None) {
				values.resize((size_t)resolved.m_nRowCount);
				if (!values.empty())
					m_pReader->readColumnAsDouble(nColumnIndex, resolved.m_nFromRow, resolved.m_nRowCount, values.data());
			}
			else {
				readDownsampledColumn(resolved, nColumnIndex, values);
			}

			for (double dValue : values) {
				if (bFloat32)
					appendBinaryValue<float>(buffer, (float)dValue);
				else
					appendBinaryValue<double>(buffer, dValue);
			}
		}
	}

}
