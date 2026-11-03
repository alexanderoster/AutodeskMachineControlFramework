/*++

Copyright (C) 2023 Autodesk Inc.

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


#include "amc_scatterplot.hpp"
#include "libmc_exceptiontypes.hpp"
#include "common_utils.hpp"
#include "common_parquetwriter.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <set>

#define SCATTERPLOT_CSVSEPARATOR ','
#define SCATTERPLOT_CSVNEWLINE '\n'

namespace AMC {

	namespace {

		struct sScatterplotExportColumn {
			std::string m_sName;
			const std::vector<double>* m_pValues;
		};

		void appendCSVText(std::vector<uint8_t>& buffer, const std::string& sText)
		{
			bool bNeedsQuotes = (sText.find_first_of(",\"\r\n") != std::string::npos);
			if (!bNeedsQuotes) {
				buffer.insert(buffer.end(), sText.begin(), sText.end());
				return;
			}

			buffer.push_back('"');
			for (char cChar : sText) {
				if (cChar == '"')
					buffer.push_back('"');
				buffer.push_back((uint8_t)cChar);
			}
			buffer.push_back('"');
		}

		std::vector<sScatterplotExportColumn> collectExportColumns(ChannelEntries& channelEntries)
		{
			std::vector<sScatterplotExportColumn> columns;
			std::set<std::string> usedNames = { "x", "y" };

			for (auto& channelIter : channelEntries) {
				for (auto& columnIter : channelIter.second) {
					std::string sBaseName = channelIter.first + "_" + columnIter.first;
					std::string sName = sBaseName;
					for (uint32_t nSuffix = 2; usedNames.find(sName) != usedNames.end(); nSuffix++)
						sName = sBaseName + "_" + std::to_string(nSuffix);

					usedNames.insert(sName);
					columns.push_back({ sName, &columnIter.second });
				}
			}

			return columns;
		}

		// Shortest representation that reads back to the same double. NaN is written as an empty cell.
		void appendCSVNumber(std::vector<uint8_t>& buffer, double dValue)
		{
			if (std::isnan(dValue))
				return;

			if (std::isinf(dValue)) {
				appendCSVText(buffer, (dValue > 0.0) ? "inf" : "-inf");
				return;
			}

			char numberBuffer[64];
#if defined(__cpp_lib_to_chars) && (__cpp_lib_to_chars >= 201611L)
			auto result = std::to_chars(numberBuffer, numberBuffer + sizeof(numberBuffer), dValue);
			buffer.insert(buffer.end(), numberBuffer, result.ptr);
#else
			int nLength = snprintf(numberBuffer, sizeof(numberBuffer), "%.15g", dValue);
			if (strtod(numberBuffer, nullptr) != dValue)
				nLength = snprintf(numberBuffer, sizeof(numberBuffer), "%.17g", dValue);
			if (nLength > 0)
				buffer.insert(buffer.end(), numberBuffer, numberBuffer + nLength);
#endif
		}

	}

	CScatterplot::CScatterplot(const std::string& sUUID)
		: m_sUUID (AMCCommon::CUtils::normalizeUUIDString (sUUID))
	{

	}

	CScatterplot::~CScatterplot()
	{

	}

	std::string CScatterplot::getUUID()
	{
		return m_sUUID;
	}

	void CScatterplot::clearData()
	{
		m_PointEntries.clear();
	}

	bool CScatterplot::isEmpty()
	{
		return m_PointEntries.empty ();
	}

	uint64_t CScatterplot::getEntryCount()
	{
		return m_PointEntries.size();
	}

	std::vector<sScatterplotEntry>& CScatterplot::getEntries()
	{
		return m_PointEntries;
	}

	ChannelEntries& CScatterplot::getChannelEntries()
	{
		return m_ChannelEntries;
	}

	void CScatterplot::getBoundaries(double& dMinX, double& dMinY, double& dMaxX, double& dMaxY)
	{
		dMinX = m_dMinX;
		dMinY = m_dMinY;
		dMaxX = m_dMaxX;
		dMaxY = m_dMaxY;
	}

	void CScatterplot::computeBoundaries()
	{
		size_t nPointCount = m_PointEntries.size();
		if (nPointCount > 0) {

			auto& point = m_PointEntries.at(0);
			m_dMinX = point.m_dX;
			m_dMinY = point.m_dY;
			m_dMaxX = point.m_dX;
			m_dMaxY = point.m_dY;

			for (size_t nIndex = 1; nIndex < nPointCount; nIndex++) {
				auto& point = m_PointEntries.at(nIndex);
				if (point.m_dX < m_dMinX)
					m_dMinX = point.m_dX;
				if (point.m_dY < m_dMinY)
					m_dMinY = point.m_dY;
				if (point.m_dX > m_dMaxX)
					m_dMaxX = point.m_dX;
				if (point.m_dY > m_dMaxY)
					m_dMaxY = point.m_dY;
			}


		}
		else {
			m_dMinX = 0.0;
			m_dMinY = 0.0;
			m_dMaxX = 0.0;
			m_dMaxY = 0.0;

		}
	}

	void CScatterplot::writeCSVToBuffer(std::vector<uint8_t>& buffer)
	{
		auto columns = collectExportColumns(m_ChannelEntries);
		size_t nPointCount = m_PointEntries.size();

		buffer.clear();
		buffer.reserve((nPointCount + 1) * (columns.size() + 2) * 12);

		appendCSVText(buffer, "x");
		buffer.push_back(SCATTERPLOT_CSVSEPARATOR);
		appendCSVText(buffer, "y");
		for (auto& column : columns) {
			buffer.push_back(SCATTERPLOT_CSVSEPARATOR);
			appendCSVText(buffer, column.m_sName);
		}
		buffer.push_back(SCATTERPLOT_CSVNEWLINE);

		for (size_t nPointIndex = 0; nPointIndex < nPointCount; nPointIndex++) {
			auto& point = m_PointEntries[nPointIndex];
			appendCSVNumber(buffer, point.m_dX);
			buffer.push_back(SCATTERPLOT_CSVSEPARATOR);
			appendCSVNumber(buffer, point.m_dY);

			for (auto& column : columns) {
				buffer.push_back(SCATTERPLOT_CSVSEPARATOR);
				if (nPointIndex < column.m_pValues->size())
					appendCSVNumber(buffer, (*column.m_pValues)[nPointIndex]);
			}
			buffer.push_back(SCATTERPLOT_CSVNEWLINE);
		}
	}

	void CScatterplot::writeParquetToBuffer(std::vector<uint8_t>& buffer)
	{
		auto columns = collectExportColumns(m_ChannelEntries);
		size_t nPointCount = m_PointEntries.size();

		AMCCommon::CParquetWriter writer(AMCCommon::eParquetCompressionCodec::GZIP);

		{
			std::vector<double> xValues(nPointCount);
			std::vector<double> yValues(nPointCount);
			for (size_t nPointIndex = 0; nPointIndex < nPointCount; nPointIndex++) {
				xValues[nPointIndex] = m_PointEntries[nPointIndex].m_dX;
				yValues[nPointIndex] = m_PointEntries[nPointIndex].m_dY;
			}
			writer.addDoubleColumn("x", xValues);
			writer.addDoubleColumn("y", yValues);
		}

		for (auto& column : columns) {
			if (column.m_pValues->size() == nPointCount) {
				writer.addDoubleColumn(column.m_sName, *column.m_pValues);
			}
			else {
				std::vector<double> paddedValues(nPointCount, std::numeric_limits<double>::quiet_NaN());
				size_t nCopyCount = std::min(nPointCount, column.m_pValues->size());
				std::copy(column.m_pValues->begin(), column.m_pValues->begin() + nCopyCount, paddedValues.begin());
				writer.addDoubleColumn(column.m_sName, paddedValues);
			}
		}

		writer.addKeyValueMetaData("amcf.scatterplot.uuid", m_sUUID);
		writer.writeToBuffer(buffer);
	}

}


