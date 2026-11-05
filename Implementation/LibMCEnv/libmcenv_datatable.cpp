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


Abstract: This is a stub class definition of CDataTable

*/

#include "libmcenv_datatable.hpp"
#include "libmcenv_interfaceexception.hpp"

#include "libmcenv_datatablewriteoptions.hpp"
#include "libmcenv_datatablecsvwriteoptions.hpp"
#include "libmcenv_datatablescatterplotoptions.hpp"
#include "libmcenv_scatterplot.hpp"

// Include custom headers here.
#include "common_utils.hpp"
#include <vector>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <cmath>

#include "amc_constants.hpp"
#include "amc_scatterplot.hpp"
#include "amc_datatableformat.hpp"

using namespace LibMCEnv::Impl;

namespace {

	// Clears a data table during stack unwinding, so that a failed load never leaves a partially loaded table behind.
	class CDataTableClearGuard {
	private:
		CDataTable* m_pTable;
		bool m_bArmed;

	public:
		explicit CDataTableClearGuard(CDataTable* pTable)
			: m_pTable(pTable), m_bArmed(true)
		{
		}

		~CDataTableClearGuard()
		{
			if (m_bArmed)
				m_pTable->Clear();
		}

		void dismiss()
		{
			m_bArmed = false;
		}
	};

}

LibMCEnvResult CDataTable::formatErrorToResult(AMC::eDataTableFormatError formatError)
{
	switch (formatError) {
	case AMC::eDataTableFormatError::InvalidSignature: return LIBMCENV_ERROR_INVALIDDATATABLESIGNATURE;
	case AMC::eDataTableFormatError::UnsupportedVersion: return LIBMCENV_ERROR_UNSUPPORTEDDATATABLEVERSION;
	case AMC::eDataTableFormatError::UnsupportedCodec: return LIBMCENV_ERROR_UNSUPPORTEDDATATABLECODEC;
	case AMC::eDataTableFormatError::UnsupportedEncoding: return LIBMCENV_ERROR_UNSUPPORTEDDATATABLEENCODING;
	case AMC::eDataTableFormatError::InvalidDataType: return LIBMCENV_ERROR_INVALIDCOLUMNDATATYPE;
	case AMC::eDataTableFormatError::Truncated: return LIBMCENV_ERROR_DATATABLESEGMENTTRUNCATED;
	case AMC::eDataTableFormatError::Corrupt: return LIBMCENV_ERROR_INVALIDDATATABLESEGMENT;
	case AMC::eDataTableFormatError::InvalidRange: return LIBMCENV_ERROR_INVALIDDATATABLEROWRANGE;
	case AMC::eDataTableFormatError::SegmentPurged: return LIBMCENV_ERROR_DATACAPTURESEGMENTPURGED;
	case AMC::eDataTableFormatError::ColumnNotFound: return LIBMCENV_ERROR_DATACAPTURECOLUMNNOTFOUND;
	case AMC::eDataTableFormatError::TypeMismatch: return LIBMCENV_ERROR_DATATABLECOLUMNTYPEMISMATCH;
	default: return LIBMCENV_ERROR_INVALIDPARAM;
	}
}

CDataTableColumn* CDataTable::getColumnByIndex(uint32_t nColumnIndex)
{
	if (nColumnIndex >= m_Columns.size())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNINDEX);

	return m_Columns.at(nColumnIndex).get();
}

CDataTableColumn* CDataTable::findColumnByIdentifier(const std::string& sIdentifier)
{
	return findColumn(sIdentifier, false);
}

void CDataTable::setColumnPlainData(const std::string& sIdentifier, uint64_t nRowCount, const std::function<void(uint8_t* pPlainValues, uint64_t nSizeInBytes)>& fillCallback)
{
	auto pColumn = findColumn(sIdentifier, true);

	uint8_t* pPlainValues = pColumn->resizeRawData(nRowCount);
	if (nRowCount > 0) {
		fillCallback(pPlainValues, nRowCount * pColumn->getEntrySizeInBytes());
		pColumn->rawDataChanged();
	}

	updateMaxRowCount();
}


#pragma pack(push)
#pragma pack(1)


struct sLibMCDataTableStreamHeader {
	uint32_t m_nSignature;
	uint32_t m_nVersion;
	uint64_t m_nColumnTableStart;
	uint32_t m_nColumnCount;
	uint32_t m_nReserved[8];
};


struct sLibMCDataTableColumnHeader {
	uint32_t m_nColumnDataType;
	uint32_t m_nEncodingType;
	uint64_t m_nColumnDataStart;
	uint64_t m_nIdentifierStart;
	uint32_t m_nIdentifierLength;
	uint64_t m_nDescriptionStart;
	uint32_t m_nDescriptionLength;
	uint64_t m_nEntryCount;
	uint32_t m_nReserved[8];
};



#pragma pack(pop)


void CDataTableColumn::writeUint64ToBufferFixedDigits(uint64_t nValue, uint32_t nFixedDigits, std::vector<char>& buffer, size_t& nBufferPosition)
{
	if (nFixedDigits > 0) {

		uint64_t nDigitValue = nValue;

		for (int32_t nIndex = nFixedDigits - 1; nIndex >= 0; nIndex--) {
			uint8_t nDigit = (uint8_t)(nDigitValue % 10);
			nDigitValue /= 10;
			buffer.at(nBufferPosition + nIndex) = nDigit + '0';
		}

		nBufferPosition += nFixedDigits;
	}
}

void CDataTableColumn::writeUint64ToBuffer(uint64_t nValue, std::vector<char>& buffer, size_t& nBufferPosition)
{
	uint64_t nDigitValue = nValue;
	int32_t nDigits = 1;
	while (nDigitValue >= 10) {
		nDigits++;
		nDigitValue /= 10;
	}

	writeUint64ToBufferFixedDigits (nValue, nDigits, buffer, nBufferPosition);

}

void CDataTableColumn::writeQuantizedDoubleToBuffer(double dValue, uint64_t nQuantizationFactor, uint32_t nQuantizationDigits, std::vector<char>& buffer, size_t& nBufferPosition)
{
	uint64_t nQuantizedValue;

	if (dValue >= 0.0) {
		nQuantizedValue = (uint64_t)round(dValue * (double)nQuantizationFactor);
	}
	else {
		nQuantizedValue = (uint64_t)round((-dValue) * (double)nQuantizationFactor);

		buffer.at(nBufferPosition) = '-';
		nBufferPosition++;
	}

	uint64_t nIntegerValue = nQuantizedValue / nQuantizationFactor;
	uint64_t nFracValue = nQuantizedValue % nQuantizationFactor;

	writeUint64ToBuffer(nIntegerValue, buffer, nBufferPosition);
	buffer.at(nBufferPosition) = '.';
	nBufferPosition++;
	writeUint64ToBufferFixedDigits(nFracValue, nQuantizationDigits, buffer, nBufferPosition);
}


class CDataTableColumn_Double : public CDataTableColumn 
{
private:

	std::vector <double> m_Rows;
	uint64_t m_nCSVQuantizationFactor;
	uint32_t m_nCSVQuantizationDigits;


public:

	CDataTableColumn_Double (const std::string& sIdentifier, const std::string& sDescription)
		: CDataTableColumn (sIdentifier, sDescription)
	{
		m_nCSVQuantizationDigits = 3;
		m_nCSVQuantizationFactor = 1ULL;
		for (uint32_t nIndex = 0; nIndex < m_nCSVQuantizationDigits; nIndex++)
			m_nCSVQuantizationFactor *= 10;


	}

	virtual ~CDataTableColumn_Double()
	{

	}

	size_t getRowCount()
	{
		return m_Rows.size();
	}

	LibMCEnv::eDataTableColumnType getColumnType()
	{
		return LibMCEnv::eDataTableColumnType::DoubleColumn;
	}

	void copyDataTo(double* pBuffer, size_t nBufferSize)
	{
		if (nBufferSize > 0) {
			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			size_t nCopyCount = nBufferSize;
			if (nCopyCount > m_Rows.size())
				nCopyCount = m_Rows.size();

			for (size_t nIndex = 0; nIndex < nCopyCount; nIndex++)
				pBuffer[nIndex] = m_Rows.at(nIndex);

			// Fill up buffer with 0.0 as value, when buffer is larger than the column's rowcount
			for (size_t nFillIndex = nCopyCount; nFillIndex < nBufferSize; nFillIndex++)
				pBuffer[nFillIndex] = 0.0;
		}
	}

	void copyDataFrom(const double* pBuffer, size_t nBufferSize)
	{
		m_Rows.resize(nBufferSize);

		if (nBufferSize > 0) {

			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			for (size_t nIndex = 0; nIndex < nBufferSize; nIndex++)
				m_Rows.at(nIndex) = pBuffer[nIndex];

		}
	}

	void writeCSVValue(size_t nRowIndex, std::vector<char>& buffer, size_t& nBufferPosition) override
	{
		if (nRowIndex < m_Rows.size()) {

			double dValue = m_Rows[nRowIndex];
			uint64_t nQuantizedValue;


			if (dValue >= 0.0) {
				nQuantizedValue = (uint64_t)round(dValue * (double)m_nCSVQuantizationFactor);

			} else {
				nQuantizedValue = (uint64_t)round((-dValue) * (double)m_nCSVQuantizationFactor);

				buffer.at(nBufferPosition) = '-';
				nBufferPosition++;
			} 

			uint64_t nIntegerValue = nQuantizedValue / m_nCSVQuantizationFactor;
			uint64_t nFracValue = nQuantizedValue % m_nCSVQuantizationFactor;

			writeUint64ToBuffer(nIntegerValue, buffer, nBufferPosition);
			buffer.at(nBufferPosition) = '.';
			nBufferPosition++;
			writeUint64ToBufferFixedDigits(nFracValue, m_nCSVQuantizationDigits, buffer, nBufferPosition);

		} else {
			buffer.at(nBufferPosition) = '0';
			nBufferPosition++;

		}
	}

	size_t getEntrySizeInBytes() override
	{
		return sizeof(double);
	}

	uint8_t* resizeRawData(uint64_t nEntryCount) override
	{
		m_Rows.resize(nEntryCount);
		return (uint8_t*)m_Rows.data();
	}

	void WriteDataToStream(ITempStreamWriter* pWriter) override
	{
		if (pWriter == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (m_Rows.size() > 0) {
			pWriter->WriteData(m_Rows.size() * getEntrySizeInBytes (), (uint8_t*)m_Rows.data());
		}
	}

	void ReadDataFromStream(IStreamReader* pReader, uint64_t nEntryCount) override
	{
		if (pReader == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);
 
		if (nEntryCount > 0) {
			m_Rows.resize(nEntryCount);
			size_t nBufferSize = m_Rows.size() * getEntrySizeInBytes();

			pReader->ReadData (nBufferSize, nBufferSize, nullptr, (uint8_t*) m_Rows.data () );

		} else {
			m_Rows.resize(0);
		}
	}

	void fillScatterplotXCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto & entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();
		
		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto & entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dX = m_Rows.at (nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dX = 0.0;
			}
		}
		

	}

	void fillScatterplotYCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dY = m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dY = 0.0;
			}
		}

	}

	void fillScatterplotChannel(AMC::CScatterplot* pScatterplot, const std::string& sChannel, const std::string& sColumn, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& channelEntries = pScatterplot->getChannelEntries();
		auto channelIter = channelEntries.find(sChannel);

		if (channelIter == channelEntries.end()) {
			channelEntries[sChannel][sColumn] = std::vector<double>();
		}
		else {
			auto& columnEntries = channelIter->second;

			if (columnEntries.find(sColumn) == columnEntries.end())
				columnEntries[sColumn] = std::vector<double>();
			else {
				std::string sException = "The channel = " + sChannel + " with the column = " + sColumn + " already exists";
				throw std::runtime_error(sException.c_str());
			}
		}

		auto& vecColumn = channelEntries[sChannel][sColumn];

		vecColumn.resize(m_Rows.size());

		for (size_t nIndex = 0; nIndex < m_Rows.size(); nIndex++) {
			vecColumn[nIndex] = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
		}
	}

};



class CDataTableColumn_Uint32 : public CDataTableColumn
{
private:

	std::vector <uint32_t> m_Rows;

public:

	CDataTableColumn_Uint32(const std::string& sIdentifier, const std::string& sDescription)
		: CDataTableColumn(sIdentifier, sDescription)
	{

	}

	virtual ~CDataTableColumn_Uint32()
	{

	}

	size_t getRowCount()
	{
		return m_Rows.size();
	}

	LibMCEnv::eDataTableColumnType getColumnType()
	{
		return LibMCEnv::eDataTableColumnType::Uint32Column;
	}

	void copyDataTo(uint32_t* pBuffer, size_t nBufferSize)
	{
		if (nBufferSize > 0) {
			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			size_t nCopyCount = nBufferSize;
			if (nCopyCount > m_Rows.size())
				nCopyCount = m_Rows.size();

			for (size_t nIndex = 0; nIndex < nCopyCount; nIndex++)
				pBuffer[nIndex] = m_Rows.at(nIndex);

			// Fill up buffer with 0.0 as value, when buffer is larger than the column's rowcount
			for (size_t nFillIndex = nCopyCount; nFillIndex < nBufferSize; nFillIndex++)
				pBuffer[nFillIndex] = 0;
		}
	}

	void copyDataFrom(const uint32_t* pBuffer, size_t nBufferSize)
	{
		m_Rows.resize(nBufferSize);

		if (nBufferSize > 0) {

			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			for (size_t nIndex = 0; nIndex < nBufferSize; nIndex++)
				m_Rows.at(nIndex) = pBuffer[nIndex];

		}
	}

	void writeCSVValue(size_t nRowIndex, std::vector<char>& buffer, size_t& nBufferPosition) override
	{
		if (nRowIndex < m_Rows.size()) {

			uint32_t nValue = m_Rows[nRowIndex];
			writeUint64ToBuffer(nValue, buffer, nBufferPosition);

			/*std::string sValue = std::to_string(m_Rows[nRowIndex]);
			for (auto ch : sValue) {
				buffer.at(nBufferPosition) = ch;
				nBufferPosition++;

			} */

		}
		else {
			buffer.at(nBufferPosition) = '0';
			nBufferPosition++;

		}
	}

	size_t getEntrySizeInBytes() override
	{
		return sizeof(uint32_t);
	}

	uint8_t* resizeRawData(uint64_t nEntryCount) override
	{
		m_Rows.resize(nEntryCount);
		return (uint8_t*)m_Rows.data();
	}

	void WriteDataToStream(ITempStreamWriter* pWriter) override
	{
		if (pWriter == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (m_Rows.size() > 0) {
			pWriter->WriteData(m_Rows.size() * getEntrySizeInBytes(), (uint8_t*)m_Rows.data());
		}
	}

	void ReadDataFromStream(IStreamReader* pReader, uint64_t nEntryCount) override
	{
		if (pReader == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (nEntryCount > 0) {
			m_Rows.resize(nEntryCount);
			size_t nBufferSize = m_Rows.size() * getEntrySizeInBytes();

			pReader->ReadData(nBufferSize, nBufferSize, nullptr, (uint8_t*)m_Rows.data());

		}
		else {
			m_Rows.resize(0);
		}
	}

	void fillScatterplotXCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dX = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dX = 0.0;
			}
		}


	}

	void fillScatterplotYCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dY = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dY = 0.0;
			}
		}

	}

	void fillScatterplotChannel(AMC::CScatterplot* pScatterplot, const std::string& sChannel, const std::string& sColumn, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto & channelEntries = pScatterplot->getChannelEntries();
		auto channelIter = channelEntries.find(sChannel);

		if (channelIter == channelEntries.end()) {
			channelEntries[sChannel][sColumn] = std::vector<double>();
		}
		else {
			auto & columnEntries = channelIter->second;

			if (columnEntries.find(sColumn) == columnEntries.end())
				columnEntries[sColumn] = std::vector<double>();
			else {
				std::string sException = "The channel = " + sChannel + " with the column = " + sColumn + " already exists";
				throw std::runtime_error(sException.c_str());
			}
		}

		auto & vecColumn = channelEntries[sChannel][sColumn];

		vecColumn.resize(m_Rows.size());

		for (size_t nIndex = 0; nIndex < m_Rows.size(); nIndex++) {
			vecColumn[nIndex] = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
		}
	}

};


class CDataTableColumn_Uint64 : public CDataTableColumn
{
private:

	std::vector <uint64_t> m_Rows;

public:

	CDataTableColumn_Uint64(const std::string& sIdentifier, const std::string& sDescription)
		: CDataTableColumn(sIdentifier, sDescription)
	{

	}

	virtual ~CDataTableColumn_Uint64()
	{

	}

	size_t getRowCount()
	{
		return m_Rows.size();
	}

	LibMCEnv::eDataTableColumnType getColumnType()
	{
		return LibMCEnv::eDataTableColumnType::Uint64Column;
	}

	void copyDataTo(uint64_t* pBuffer, size_t nBufferSize)
	{
		if (nBufferSize > 0) {
			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			size_t nCopyCount = nBufferSize;
			if (nCopyCount > m_Rows.size())
				nCopyCount = m_Rows.size();

			for (size_t nIndex = 0; nIndex < nCopyCount; nIndex++)
				pBuffer[nIndex] = m_Rows.at(nIndex);

			// Fill up buffer with 0.0 as value, when buffer is larger than the column's rowcount
			for (size_t nFillIndex = nCopyCount; nFillIndex < nBufferSize; nFillIndex++)
				pBuffer[nFillIndex] = 0;
		}
	}

	void copyDataFrom(const uint64_t* pBuffer, size_t nBufferSize)
	{
		m_Rows.resize(nBufferSize);

		if (nBufferSize > 0) {

			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			for (size_t nIndex = 0; nIndex < nBufferSize; nIndex++)
				m_Rows.at(nIndex) = pBuffer[nIndex];

		}
	}

	void writeCSVValue(size_t nRowIndex, std::vector<char>& buffer, size_t& nBufferPosition) override
	{
		if (nRowIndex < m_Rows.size()) {
			uint64_t nValue = m_Rows[nRowIndex];
			writeUint64ToBuffer(nValue, buffer, nBufferPosition);

		}
		else {
			buffer.at(nBufferPosition) = '0';
			nBufferPosition++;

		}
	}

	size_t getEntrySizeInBytes() override
	{
		return sizeof(uint64_t);
	}

	uint8_t* resizeRawData(uint64_t nEntryCount) override
	{
		m_Rows.resize(nEntryCount);
		return (uint8_t*)m_Rows.data();
	}

	void WriteDataToStream(ITempStreamWriter* pWriter) override
	{
		if (pWriter == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (m_Rows.size() > 0) {
			pWriter->WriteData(m_Rows.size() * getEntrySizeInBytes(), (uint8_t*)m_Rows.data());
		}
	}

	void ReadDataFromStream(IStreamReader* pReader, uint64_t nEntryCount) override
	{
		if (pReader == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (nEntryCount > 0) {
			m_Rows.resize(nEntryCount);
			size_t nBufferSize = m_Rows.size() * getEntrySizeInBytes();

			pReader->ReadData(nBufferSize, nBufferSize, nullptr, (uint8_t*)m_Rows.data());

		}
		else {
			m_Rows.resize(0);
		}
	}

	void fillScatterplotXCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dX = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dX = 0.0;
			}
		}


	}

	void fillScatterplotYCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dY = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dY = 0.0;
			}
		}

	}

	void fillScatterplotChannel(AMC::CScatterplot* pScatterplot, const std::string& sChannel, const std::string& sColumn, double dScaleFactor, double dOffset) override
	{
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_NOTIMPLEMENTED);
	}

};


class CDataTableColumn_Int32 : public CDataTableColumn
{
private:

	std::vector <int32_t> m_Rows;

public:

	CDataTableColumn_Int32(const std::string& sIdentifier, const std::string& sDescription)
		: CDataTableColumn(sIdentifier, sDescription)
	{

	}

	virtual ~CDataTableColumn_Int32()
	{

	}

	size_t getRowCount()
	{
		return m_Rows.size();
	}

	LibMCEnv::eDataTableColumnType getColumnType()
	{
		return LibMCEnv::eDataTableColumnType::Int32Column;
	}

	void copyDataTo(int32_t* pBuffer, size_t nBufferSize)
	{
		if (nBufferSize > 0) {
			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			size_t nCopyCount = nBufferSize;
			if (nCopyCount > m_Rows.size())
				nCopyCount = m_Rows.size();

			for (size_t nIndex = 0; nIndex < nCopyCount; nIndex++)
				pBuffer[nIndex] = m_Rows.at(nIndex);

			// Fill up buffer with 0.0 as value, when buffer is larger than the column's rowcount
			for (size_t nFillIndex = nCopyCount; nFillIndex < nBufferSize; nFillIndex++)
				pBuffer[nFillIndex] = 0;
		}
	}

	void copyDataFrom(const int32_t* pBuffer, size_t nBufferSize)
	{
		m_Rows.resize(nBufferSize);

		if (nBufferSize > 0) {

			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			for (size_t nIndex = 0; nIndex < nBufferSize; nIndex++)
				m_Rows.at(nIndex) = pBuffer[nIndex];

		}
	}

	void writeCSVValue(size_t nRowIndex, std::vector<char>& buffer, size_t& nBufferPosition) override
	{
		if (nRowIndex < m_Rows.size()) {
			int32_t nValue = m_Rows[nRowIndex];
			if (nValue >= 0) {
				writeUint64ToBuffer((uint64_t) nValue, buffer, nBufferPosition);
			}
			else {
				buffer.at(nBufferPosition) = '-';
				nBufferPosition++;

				writeUint64ToBuffer((uint64_t)(- nValue), buffer, nBufferPosition);
			}

		}
		else {
			buffer.at(nBufferPosition) = '0';
			nBufferPosition++;

		}
	}

	size_t getEntrySizeInBytes() override
	{
		return sizeof(int32_t);
	}

	uint8_t* resizeRawData(uint64_t nEntryCount) override
	{
		m_Rows.resize(nEntryCount);
		return (uint8_t*)m_Rows.data();
	}

	void WriteDataToStream(ITempStreamWriter* pWriter) override
	{
		if (pWriter == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (m_Rows.size() > 0) {
			pWriter->WriteData(m_Rows.size() * getEntrySizeInBytes(), (uint8_t*)m_Rows.data());
		}
	}

	void ReadDataFromStream(IStreamReader* pReader, uint64_t nEntryCount) override
	{
		if (pReader == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (nEntryCount > 0) {
			m_Rows.resize(nEntryCount);
			size_t nBufferSize = m_Rows.size() * getEntrySizeInBytes();

			pReader->ReadData(nBufferSize, nBufferSize, nullptr, (uint8_t*)m_Rows.data());

		}
		else {
			m_Rows.resize(0);
		}
	}

	void fillScatterplotXCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dX = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dX = 0.0;
			}
		}


	}

	void fillScatterplotYCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dY = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dY = 0.0;
			}
		}
	}

	void fillScatterplotChannel(AMC::CScatterplot* pScatterplot, const std::string& sChannel, const std::string& sColumn, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& channelEntries = pScatterplot->getChannelEntries();
		auto channelIter = channelEntries.find(sChannel);

		if (channelIter == channelEntries.end()) {
			channelEntries[sChannel][sColumn] = std::vector<double>();
		}
		else {
			auto& columnEntries = channelIter->second;

			if (columnEntries.find(sColumn) == columnEntries.end())
				columnEntries[sColumn] = std::vector<double>();
			else {
				std::string sException = "The channel = " + sChannel + " with the column = " + sColumn + " already exists";
				throw std::runtime_error(sException.c_str());
			}
		}

		auto& vecColumn = channelEntries[sChannel][sColumn];

		vecColumn.resize(m_Rows.size());

		for (size_t nIndex = 0; nIndex < m_Rows.size(); nIndex++) {
			vecColumn[nIndex] = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
		}
	}
};


class CDataTableColumn_Int64 : public CDataTableColumn
{
private:

	std::vector <int64_t> m_Rows;

public:

	CDataTableColumn_Int64(const std::string& sIdentifier, const std::string& sDescription)
		: CDataTableColumn(sIdentifier, sDescription)
	{

	}

	virtual ~CDataTableColumn_Int64()
	{

	}

	size_t getRowCount()
	{
		return m_Rows.size();
	}

	LibMCEnv::eDataTableColumnType getColumnType()
	{
		return LibMCEnv::eDataTableColumnType::Int64Column;
	}

	void copyDataTo(int64_t* pBuffer, size_t nBufferSize)
	{
		if (nBufferSize > 0) {
			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			size_t nCopyCount = nBufferSize;
			if (nCopyCount > m_Rows.size())
				nCopyCount = m_Rows.size();

			for (size_t nIndex = 0; nIndex < nCopyCount; nIndex++)
				pBuffer[nIndex] = m_Rows.at(nIndex);

			// Fill up buffer with 0.0 as value, when buffer is larger than the column's rowcount
			for (size_t nFillIndex = nCopyCount; nFillIndex < nBufferSize; nFillIndex++)
				pBuffer[nFillIndex] = 0;
		}
	}

	void copyDataFrom(const int64_t* pBuffer, size_t nBufferSize)
	{
		m_Rows.resize(nBufferSize);

		if (nBufferSize > 0) {

			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			for (size_t nIndex = 0; nIndex < nBufferSize; nIndex++)
				m_Rows.at(nIndex) = pBuffer[nIndex];

		}
	}

	void writeCSVValue(size_t nRowIndex, std::vector<char>& buffer, size_t& nBufferPosition) override
	{
		if (nRowIndex < m_Rows.size()) {
			int64_t nValue = m_Rows[nRowIndex];
			if (nValue >= 0) {
				writeUint64ToBuffer((uint64_t)nValue, buffer, nBufferPosition);
			}
			else {
				buffer.at(nBufferPosition) = '-';
				nBufferPosition++;

				writeUint64ToBuffer((uint64_t)(-nValue), buffer, nBufferPosition);
			}

		}
		else {
			buffer.at(nBufferPosition) = '0';
			nBufferPosition++;

		}

	}

	size_t getEntrySizeInBytes() override
	{
		return sizeof(int64_t);
	}

	uint8_t* resizeRawData(uint64_t nEntryCount) override
	{
		m_Rows.resize(nEntryCount);
		return (uint8_t*)m_Rows.data();
	}

	void WriteDataToStream(ITempStreamWriter* pWriter) override
	{
		if (pWriter == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (m_Rows.size() > 0) {
			pWriter->WriteData(m_Rows.size() * getEntrySizeInBytes(), (uint8_t*)m_Rows.data());
		}
	}

	void ReadDataFromStream(IStreamReader* pReader, uint64_t nEntryCount) override
	{
		if (pReader == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (nEntryCount > 0) {
			m_Rows.resize(nEntryCount);
			size_t nBufferSize = m_Rows.size() * getEntrySizeInBytes();

			pReader->ReadData(nBufferSize, nBufferSize, nullptr, (uint8_t*)m_Rows.data());

		}
		else {
			m_Rows.resize(0);
		}
	}

	void fillScatterplotXCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dX = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dX = 0.0;
			}
		}


	}

	void fillScatterplotYCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dY = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dY = 0.0;
			}
		}

	}

	void fillScatterplotChannel(AMC::CScatterplot* pScatterplot, const std::string& sChannel, const std::string& sColumn, double dScaleFactor, double dOffset) override
	{
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_NOTIMPLEMENTED);
	}

};


/*************************************************************************************************************************
 Fixed-width column template for the column types added in format phase 0 (Float, Bool, Timestamp).
 Rows are held in a flat std::vector<T> and written to the stream as a raw value array, exactly like
 the five original column types. Bit-packing and delta encoding arrive with format version 2.
**************************************************************************************************************************/

template <typename T>
class CDataTableColumn_FixedWidth : public CDataTableColumn
{
protected:

	std::vector <T> m_Rows;

public:

	CDataTableColumn_FixedWidth(const std::string& sIdentifier, const std::string& sDescription)
		: CDataTableColumn(sIdentifier, sDescription)
	{

	}

	virtual ~CDataTableColumn_FixedWidth()
	{

	}

	size_t getRowCount() override
	{
		return m_Rows.size();
	}

	void copyDataTo(T* pBuffer, size_t nBufferSize)
	{
		if (nBufferSize > 0) {
			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			size_t nCopyCount = nBufferSize;
			if (nCopyCount > m_Rows.size())
				nCopyCount = m_Rows.size();

			for (size_t nIndex = 0; nIndex < nCopyCount; nIndex++)
				pBuffer[nIndex] = m_Rows.at(nIndex);

			// Fill up buffer with 0 as value, when buffer is larger than the row count of the column
			for (size_t nFillIndex = nCopyCount; nFillIndex < nBufferSize; nFillIndex++)
				pBuffer[nFillIndex] = (T)0;
		}
	}

	virtual void copyDataFrom(const T* pBuffer, size_t nBufferSize)
	{
		m_Rows.resize(nBufferSize);

		if (nBufferSize > 0) {

			if (pBuffer == nullptr)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNBUFFERISNULL);

			for (size_t nIndex = 0; nIndex < nBufferSize; nIndex++)
				m_Rows.at(nIndex) = pBuffer[nIndex];

		}
	}

	size_t getEntrySizeInBytes() override
	{
		return sizeof(T);
	}

	uint8_t* resizeRawData(uint64_t nEntryCount) override
	{
		m_Rows.resize(nEntryCount);
		return (uint8_t*)m_Rows.data();
	}

	void WriteDataToStream(ITempStreamWriter* pWriter) override
	{
		if (pWriter == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (m_Rows.size() > 0) {
			pWriter->WriteData(m_Rows.size() * getEntrySizeInBytes(), (uint8_t*)m_Rows.data());
		}
	}

	void ReadDataFromStream(IStreamReader* pReader, uint64_t nEntryCount) override
	{
		if (pReader == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		if (nEntryCount > 0) {
			m_Rows.resize(nEntryCount);
			size_t nBufferSize = m_Rows.size() * getEntrySizeInBytes();

			pReader->ReadData(nBufferSize, nBufferSize, nullptr, (uint8_t*)m_Rows.data());

		}
		else {
			m_Rows.resize(0);
		}
	}

	void fillScatterplotXCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dX = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dX = 0.0;
			}
		}
	}

	void fillScatterplotYCoordinates(AMC::CScatterplot* pScatterplot, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& entries = pScatterplot->getEntries();
		size_t entryCount = entries.size();
		size_t rowCount = m_Rows.size();

		for (size_t nIndex = 0; nIndex < entryCount; nIndex++) {
			auto& entry = entries.at(nIndex);
			if (nIndex < rowCount) {
				entry.m_dY = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
			}
			else {
				entry.m_dY = 0.0;
			}
		}
	}

	void fillScatterplotChannel(AMC::CScatterplot* pScatterplot, const std::string& sChannel, const std::string& sColumn, double dScaleFactor, double dOffset) override
	{
		if (pScatterplot == nullptr)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

		auto& channelEntries = pScatterplot->getChannelEntries();
		auto channelIter = channelEntries.find(sChannel);

		if (channelIter == channelEntries.end()) {
			channelEntries[sChannel][sColumn] = std::vector<double>();
		}
		else {
			auto& columnEntries = channelIter->second;

			if (columnEntries.find(sColumn) == columnEntries.end())
				columnEntries[sColumn] = std::vector<double>();
			else {
				std::string sException = "The channel = " + sChannel + " with the column = " + sColumn + " already exists";
				throw std::runtime_error(sException.c_str());
			}
		}

		auto& vecColumn = channelEntries[sChannel][sColumn];

		vecColumn.resize(m_Rows.size());

		for (size_t nIndex = 0; nIndex < m_Rows.size(); nIndex++) {
			vecColumn[nIndex] = (double)m_Rows.at(nIndex) * dScaleFactor + dOffset;
		}
	}

};


class CDataTableColumn_Float : public CDataTableColumn_FixedWidth<float>
{
private:

	uint64_t m_nCSVQuantizationFactor;
	uint32_t m_nCSVQuantizationDigits;

public:

	CDataTableColumn_Float(const std::string& sIdentifier, const std::string& sDescription)
		: CDataTableColumn_FixedWidth<float>(sIdentifier, sDescription)
	{
		// Same CSV export rounding as the double column.
		m_nCSVQuantizationDigits = 3;
		m_nCSVQuantizationFactor = 1ULL;
		for (uint32_t nIndex = 0; nIndex < m_nCSVQuantizationDigits; nIndex++)
			m_nCSVQuantizationFactor *= 10;
	}

	LibMCEnv::eDataTableColumnType getColumnType() override
	{
		return LibMCEnv::eDataTableColumnType::FloatColumn;
	}

	void writeCSVValue(size_t nRowIndex, std::vector<char>& buffer, size_t& nBufferPosition) override
	{
		if (nRowIndex < m_Rows.size()) {
			writeQuantizedDoubleToBuffer((double)m_Rows[nRowIndex], m_nCSVQuantizationFactor, m_nCSVQuantizationDigits, buffer, nBufferPosition);
		}
		else {
			buffer.at(nBufferPosition) = '0';
			nBufferPosition++;
		}
	}

};


class CDataTableColumn_Bool : public CDataTableColumn_FixedWidth<uint8_t>
{
private:

	void normalizeRows()
	{
		for (auto& nValue : m_Rows)
			nValue = (nValue != 0) ? 1 : 0;
	}

public:

	CDataTableColumn_Bool(const std::string& sIdentifier, const std::string& sDescription)
		: CDataTableColumn_FixedWidth<uint8_t>(sIdentifier, sDescription)
	{

	}

	LibMCEnv::eDataTableColumnType getColumnType() override
	{
		return LibMCEnv::eDataTableColumnType::BoolColumn;
	}

	// Values cross the ABI as bytes; any nonzero byte is true and is stored as 1.
	void copyDataFrom(const uint8_t* pBuffer, size_t nBufferSize) override
	{
		CDataTableColumn_FixedWidth<uint8_t>::copyDataFrom(pBuffer, nBufferSize);
		normalizeRows();
	}

	void ReadDataFromStream(IStreamReader* pReader, uint64_t nEntryCount) override
	{
		CDataTableColumn_FixedWidth<uint8_t>::ReadDataFromStream(pReader, nEntryCount);
		normalizeRows();
	}

	void rawDataChanged() override
	{
		normalizeRows();
	}

	void writeCSVValue(size_t nRowIndex, std::vector<char>& buffer, size_t& nBufferPosition) override
	{
		if ((nRowIndex < m_Rows.size()) && (m_Rows[nRowIndex] != 0)) {
			buffer.at(nBufferPosition) = '1';
		}
		else {
			buffer.at(nBufferPosition) = '0';
		}
		nBufferPosition++;
	}

};


class CDataTableColumn_Timestamp : public CDataTableColumn_FixedWidth<uint64_t>
{
public:

	CDataTableColumn_Timestamp(const std::string& sIdentifier, const std::string& sDescription)
		: CDataTableColumn_FixedWidth<uint64_t>(sIdentifier, sDescription)
	{

	}

	LibMCEnv::eDataTableColumnType getColumnType() override
	{
		return LibMCEnv::eDataTableColumnType::TimestampColumn;
	}

	// Timestamps are microseconds and are exported as plain integers.
	void writeCSVValue(size_t nRowIndex, std::vector<char>& buffer, size_t& nBufferPosition) override
	{
		if (nRowIndex < m_Rows.size()) {
			writeUint64ToBuffer(m_Rows[nRowIndex], buffer, nBufferPosition);
		}
		else {
			buffer.at(nBufferPosition) = '0';
			nBufferPosition++;
		}
	}

};


CDataTableColumn::CDataTableColumn(const std::string& sIdentifier, const std::string& sDescription)
	: m_sIdentifier (sIdentifier), m_sDescription (sDescription)
{
	if (sIdentifier.empty())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNIDENTIFIEREMPTY);

	if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString (sIdentifier))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNIDENTIFIER, "invalid column identifier: " + sIdentifier);

}

CDataTableColumn::~CDataTableColumn()
{

}

std::string CDataTableColumn::getIdentifier()
{
	return m_sIdentifier;
}

std::string CDataTableColumn::getDescription()
{
	return m_sDescription;
}

void CDataTableColumn::rawDataChanged()
{
}

const uint8_t* CDataTableColumn::getRawData()
{
	// Resizing to the current row count changes nothing and returns the value array.
	return resizeRawData(getRowCount());
}

void CDataTableColumn::setDescription(const std::string& sDescription)
{
	m_sDescription = sDescription;
}


CDataTable::CDataTable(AMC::PToolpathHandler pToolpathHandler)
	: m_nMaxRowCount (0), m_pToolpathHandler (pToolpathHandler)
{
	if (pToolpathHandler.get() == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

}

CDataTable::~CDataTable()
{

}

CDataTable* CDataTable::makeFromStream(IStreamReader * pStreamReader, AMC::PToolpathHandler pToolpathHandler)
{
	auto pDataTable = std::make_unique<CDataTable>(pToolpathHandler);
	pDataTable->LoadFromStream(pStreamReader);
	return pDataTable.release();
}


void CDataTable::AddColumn(const std::string& sIdentifier, const std::string& sDescription, const LibMCEnv::eDataTableColumnType eColumnType)
{
	addColumnEx(sIdentifier, sDescription, eColumnType);
}

PDataTableColumn CDataTable::addColumnEx(const std::string& sIdentifier, const std::string& sDescription, const LibMCEnv::eDataTableColumnType eColumnType)
{
	if (sIdentifier.empty())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNIDENTIFIEREMPTY);

	if (!AMCCommon::CUtils::stringIsValidAlphanumericNameString(sIdentifier))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNIDENTIFIER, "invalid column identifier: " + sIdentifier);

	if (HasColumn (sIdentifier))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DUPLICATECOLUMNIDENTIFIER, "duplicate column identifier: " + sIdentifier);

	if (m_Columns.size () >= DATATABLE_MAXCOLUMNCOUNT)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATATABLEHASTOOMANYCOLUMS);

	PDataTableColumn pColumn;

	switch (eColumnType) {
	case LibMCEnv::eDataTableColumnType::DoubleColumn:
		pColumn = std::make_shared<CDataTableColumn_Double>(sIdentifier, sDescription);
		break;
	case LibMCEnv::eDataTableColumnType::Uint32Column:
		pColumn = std::make_shared<CDataTableColumn_Uint32>(sIdentifier, sDescription);
		break;
	case LibMCEnv::eDataTableColumnType::Uint64Column:
		pColumn = std::make_shared<CDataTableColumn_Uint64>(sIdentifier, sDescription);
		break;
	case LibMCEnv::eDataTableColumnType::Int32Column:
		pColumn = std::make_shared<CDataTableColumn_Int32>(sIdentifier, sDescription);
		break;
	case LibMCEnv::eDataTableColumnType::Int64Column:
		pColumn = std::make_shared<CDataTableColumn_Int64>(sIdentifier, sDescription);
		break;
	case LibMCEnv::eDataTableColumnType::FloatColumn:
		pColumn = std::make_shared<CDataTableColumn_Float>(sIdentifier, sDescription);
		break;
	case LibMCEnv::eDataTableColumnType::BoolColumn:
		pColumn = std::make_shared<CDataTableColumn_Bool>(sIdentifier, sDescription);
		break;
	case LibMCEnv::eDataTableColumnType::TimestampColumn:
		pColumn = std::make_shared<CDataTableColumn_Timestamp>(sIdentifier, sDescription);
		break;
	default:
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNDATATYPE);
	}

	m_Columns.push_back(pColumn);
	m_ColumnMap.insert(std::make_pair (sIdentifier, pColumn));

	return pColumn;
}

void CDataTable::RemoveColumn(const std::string & sIdentifier)
{
	auto pFoundColumn = findColumn(sIdentifier, true);

	m_ColumnMap.erase(sIdentifier);
	m_Columns.erase(std::remove_if(m_Columns.begin(), m_Columns.end(), [pFoundColumn](PDataTableColumn & pColumn) {
		return (pFoundColumn ==  pColumn.get ());
	}), m_Columns.end());

}

bool CDataTable::HasColumn(const std::string & sIdentifier)
{
	auto iIter = m_ColumnMap.find(sIdentifier);
	return iIter != m_ColumnMap.end();
}

CDataTableColumn* CDataTable::findColumn(const std::string& sIdentifier, bool bMustExist)
{
	auto iIter = m_ColumnMap.find(sIdentifier);
	if (iIter == m_ColumnMap.end()) {
		if (bMustExist)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNIDENTIFIERNOTFOUND, "column identifier not found: " + sIdentifier);

		return nullptr;
	}

	return iIter->second.get();

}

LibMCEnv_uint32 CDataTable::GetRowCount()
{
	// Kept for ABI compatibility. Row counts above the uint32 range are truncated here;
	// GetRowCount64 returns the full value.
	return (uint32_t) m_nMaxRowCount;
}

LibMCEnv_uint64 CDataTable::GetRowCount64()
{
	return (uint64_t) m_nMaxRowCount;
}

LibMCEnv_uint32 CDataTable::GetColumnCount()
{
	return (uint32_t)m_Columns.size();
}

std::string CDataTable::GetColumnIdentifier(const LibMCEnv_uint32 nColumnIndex)
{
	if (nColumnIndex >= m_Columns.size ())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNINDEX);

	return m_Columns.at(nColumnIndex)->getIdentifier();
}

std::string CDataTable::GetColumnDescription(const LibMCEnv_uint32 nColumnIndex)
{
	if (nColumnIndex >= m_Columns.size())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNINDEX);

	return m_Columns.at(nColumnIndex)->getDescription();
}

LibMCEnv::eDataTableColumnType CDataTable::GetColumnType(const LibMCEnv_uint32 nColumnIndex)
{
	if (nColumnIndex >= m_Columns.size())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCOLUMNINDEX);

	return m_Columns.at(nColumnIndex)->getColumnType();
}

void CDataTable::GetColumnInformation(const std::string & sIdentifier, std::string & sDescription, LibMCEnv::eDataTableColumnType & eColumnType)
{
	auto pColumn = findColumn(sIdentifier, true);
	sDescription = pColumn->getDescription();
	eColumnType = pColumn->getColumnType();
}

void CDataTable::GetDoubleColumnValues(const std::string & sIdentifier, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_double * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);

	auto pDoubleColumn = dynamic_cast<CDataTableColumn_Double*> (pColumn);

	if (pDoubleColumn == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEDOUBLE, "column is not of type double: " + sIdentifier);

	if (pValuesNeededCount != nullptr)
		*pValuesNeededCount = m_nMaxRowCount;

	if (pValuesBuffer != nullptr) 
	{
		if (nValuesBufferSize < m_nMaxRowCount)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

		pDoubleColumn->copyDataTo(pValuesBuffer, m_nMaxRowCount);
	}

}

void CDataTable::GetInt32ColumnValues(const std::string & sIdentifier, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_int32 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);

	auto pInt32Column = dynamic_cast<CDataTableColumn_Int32*> (pColumn);

	if (pInt32Column == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEINT32, "column is not of type int32: " + sIdentifier);

	if (pValuesNeededCount != nullptr)
		*pValuesNeededCount = m_nMaxRowCount;

	if (pValuesBuffer != nullptr)
	{
		if (nValuesBufferSize < m_nMaxRowCount)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

		pInt32Column->copyDataTo(pValuesBuffer, m_nMaxRowCount);
	}
}

void CDataTable::GetInt64ColumnValues(const std::string & sIdentifier, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_int64 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);

	auto pInt64Column = dynamic_cast<CDataTableColumn_Int64*> (pColumn);

	if (pInt64Column == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEINT64, "column is not of type int64: " + sIdentifier);

	if (pValuesNeededCount != nullptr)
		*pValuesNeededCount = m_nMaxRowCount;

	if (pValuesBuffer != nullptr)
	{
		if (nValuesBufferSize < m_nMaxRowCount)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

		pInt64Column->copyDataTo(pValuesBuffer, m_nMaxRowCount);
	}
}

void CDataTable::GetUint32ColumnValues(const std::string & sIdentifier, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_uint32 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);

	auto pUint32Column = dynamic_cast<CDataTableColumn_Uint32*> (pColumn);

	if (pUint32Column == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEUINT32, "column is not of type uint32: " + sIdentifier);

	if (pValuesNeededCount != nullptr)
		*pValuesNeededCount = m_nMaxRowCount;

	if (pValuesBuffer != nullptr)
	{
		if (nValuesBufferSize < m_nMaxRowCount)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

		pUint32Column->copyDataTo(pValuesBuffer, m_nMaxRowCount);
	}
}

void CDataTable::GetUint64ColumnValues(const std::string & sIdentifier, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_uint64 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);

	auto pUint64Column = dynamic_cast<CDataTableColumn_Uint64*> (pColumn);

	if (pUint64Column == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEUINT64, "column is not of type uint64: " + sIdentifier);

	if (pValuesNeededCount != nullptr)
		*pValuesNeededCount = m_nMaxRowCount;

	if (pValuesBuffer != nullptr)
	{
		if (nValuesBufferSize < m_nMaxRowCount)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

		pUint64Column->copyDataTo(pValuesBuffer, m_nMaxRowCount);
	}
}

void CDataTable::SetDoubleColumnValues(const std::string & sIdentifier, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_double * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pDoubleColumn = dynamic_cast<CDataTableColumn_Double*> (pColumn);

	if (pDoubleColumn == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEDOUBLE, "column is not of type double: " + sIdentifier);

	pDoubleColumn->copyDataFrom(pValuesBuffer, nValuesBufferSize);

	updateMaxRowCount();
}

void CDataTable::SetInt32ColumnValues(const std::string & sIdentifier, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_int32 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pInt32Column = dynamic_cast<CDataTableColumn_Int32*> (pColumn);

	if (pInt32Column == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEINT32, "column is not of type int32: " + sIdentifier);

	pInt32Column->copyDataFrom(pValuesBuffer, nValuesBufferSize);

	updateMaxRowCount();
}

void CDataTable::SetInt64ColumnValues(const std::string & sIdentifier, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_int64 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pInt64Column = dynamic_cast<CDataTableColumn_Int64*> (pColumn);

	if (pInt64Column == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEINT64, "column is not of type int64: " + sIdentifier);

	pInt64Column->copyDataFrom(pValuesBuffer, nValuesBufferSize);

	updateMaxRowCount();
}

void CDataTable::SetUint32ColumnValues(const std::string & sIdentifier, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint32 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pUint32Column = dynamic_cast<CDataTableColumn_Uint32*> (pColumn);

	if (pUint32Column == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEUINT32, "column is not of type uint32: " + sIdentifier);

	pUint32Column->copyDataFrom(pValuesBuffer, nValuesBufferSize);

	updateMaxRowCount();
}

void CDataTable::SetUint64ColumnValues(const std::string & sIdentifier, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint64 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pUint64Column = dynamic_cast<CDataTableColumn_Uint64*> (pColumn);

	if (pUint64Column == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEUINT64, "column is not of type uint64: " + sIdentifier);

	pUint64Column->copyDataFrom(pValuesBuffer, nValuesBufferSize);

	updateMaxRowCount();
}


void CDataTable::GetFloatColumnValues(const std::string & sIdentifier, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_single * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pFloatColumn = dynamic_cast<CDataTableColumn_Float*> (pColumn);

	if (pFloatColumn == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEFLOAT, "column is not of type float: " + sIdentifier);

	if (pValuesNeededCount != nullptr)
		*pValuesNeededCount = m_nMaxRowCount;

	if (pValuesBuffer != nullptr)
	{
		if (nValuesBufferSize < m_nMaxRowCount)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

		pFloatColumn->copyDataTo(pValuesBuffer, m_nMaxRowCount);
	}
}

void CDataTable::GetBoolColumnValues(const std::string & sIdentifier, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_uint8 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pBoolColumn = dynamic_cast<CDataTableColumn_Bool*> (pColumn);

	if (pBoolColumn == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEBOOL, "column is not of type bool: " + sIdentifier);

	if (pValuesNeededCount != nullptr)
		*pValuesNeededCount = m_nMaxRowCount;

	if (pValuesBuffer != nullptr)
	{
		if (nValuesBufferSize < m_nMaxRowCount)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

		pBoolColumn->copyDataTo(pValuesBuffer, m_nMaxRowCount);
	}
}

void CDataTable::GetTimestampColumnValues(const std::string & sIdentifier, LibMCEnv_uint64 nValuesBufferSize, LibMCEnv_uint64* pValuesNeededCount, LibMCEnv_uint64 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pTimestampColumn = dynamic_cast<CDataTableColumn_Timestamp*> (pColumn);

	if (pTimestampColumn == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPETIMESTAMP, "column is not of type timestamp: " + sIdentifier);

	if (pValuesNeededCount != nullptr)
		*pValuesNeededCount = m_nMaxRowCount;

	if (pValuesBuffer != nullptr)
	{
		if (nValuesBufferSize < m_nMaxRowCount)
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_BUFFERTOOSMALL);

		pTimestampColumn->copyDataTo(pValuesBuffer, m_nMaxRowCount);
	}
}

void CDataTable::SetFloatColumnValues(const std::string & sIdentifier, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_single * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pFloatColumn = dynamic_cast<CDataTableColumn_Float*> (pColumn);

	if (pFloatColumn == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEFLOAT, "column is not of type float: " + sIdentifier);

	pFloatColumn->copyDataFrom(pValuesBuffer, nValuesBufferSize);

	updateMaxRowCount();
}

void CDataTable::SetBoolColumnValues(const std::string & sIdentifier, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint8 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pBoolColumn = dynamic_cast<CDataTableColumn_Bool*> (pColumn);

	if (pBoolColumn == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPEBOOL, "column is not of type bool: " + sIdentifier);

	pBoolColumn->copyDataFrom(pValuesBuffer, nValuesBufferSize);

	updateMaxRowCount();
}

void CDataTable::SetTimestampColumnValues(const std::string & sIdentifier, const LibMCEnv_uint64 nValuesBufferSize, const LibMCEnv_uint64 * pValuesBuffer)
{
	auto pColumn = findColumn(sIdentifier, true);
	auto pTimestampColumn = dynamic_cast<CDataTableColumn_Timestamp*> (pColumn);

	if (pTimestampColumn == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_COLUMNISNOTOFTYPETIMESTAMP, "column is not of type timestamp: " + sIdentifier);

	pTimestampColumn->copyDataFrom(pValuesBuffer, nValuesBufferSize);

	updateMaxRowCount();
}

void CDataTable::updateMaxRowCount()
{
	m_nMaxRowCount = 0;
	for (auto& iColumn : m_Columns) {
		size_t nRowCount = iColumn->getRowCount();
		if (nRowCount > m_nMaxRowCount)
			m_nMaxRowCount = nRowCount;
	}
}

IDataTableWriteOptions* CDataTable::CreateWriteOptions()
{
	return new CDataTableWriteOptions ();
}

IDataTableCSVWriteOptions* CDataTable::CreateCSVWriteOptions()
{
	return new CDataTableCSVWriteOptions();
}


void CDataTable::WriteCSVToStream(ITempStreamWriter* pWriter, IDataTableCSVWriteOptions* pOptions) 
{
	if (pWriter == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	std::string sSeparator = DATATABLE_DEFAULTCSVSEPARATOR;
	std::string sNewLine = DATATABLE_DEFAULTCSVNEWLINE;
	size_t nMaxBytesPerEntry = DATATABLE_DEFAULTCSVMAXBYTESPERENTRY;
	size_t nChunkSize = DATATABLE_DEFAULTCSVCHUNKSIZE;

	if (pOptions != nullptr) {
		sSeparator = pOptions->GetSeparator();
	}

	if (sSeparator.length () != 1)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCSVSEPARATOR);

	char cSeparator = sSeparator.at(0);
	if ((cSeparator < DATATABLE_MINCSVSEPARATOR) || (cSeparator > DATATABLE_MAXCSVSEPARATOR))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDCSVSEPARATOR);
	

	std::stringstream sHeader;
	auto iIter = m_Columns.begin();
	while (iIter != m_Columns.end()) {
		sHeader << (*iIter)->getDescription ();
		iIter++;

		if (iIter != m_Columns.end())
			sHeader << sSeparator;
	}

	pWriter->WriteLine(sHeader.str ());

	size_t nRowCount = m_nMaxRowCount;
	size_t nBufferSize = nChunkSize * nMaxBytesPerEntry * (m_Columns.size() + 1);
	std::vector<char> buffer (nBufferSize);

	size_t nChunkCount = (nRowCount + nChunkSize - 1) / nChunkSize;

	for (size_t nChunkIndex = 0; nChunkIndex < nChunkCount; nChunkIndex++) {

		size_t nChunkPosition = 0;

		for (size_t nRowIndexInChunk = 0; nRowIndexInChunk < nChunkSize; nRowIndexInChunk++) {
			size_t nRowIndex = nChunkIndex * nChunkSize + nRowIndexInChunk;
			if (nRowIndex >= m_nMaxRowCount)
				break;
   
			auto iIter = m_Columns.begin();
			while (iIter != m_Columns.end()) {

				(*iIter)->writeCSVValue(nRowIndex, buffer, nChunkPosition);

				if (nChunkPosition + 1 >= nBufferSize)
					throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATATABLECSVBUFFEROVERFLOW);

				iIter++;

				if (iIter != m_Columns.end()) {
					buffer.at(nChunkPosition) = cSeparator;
					nChunkPosition++;
				}

			}

			if (nChunkPosition + 1 >= nBufferSize)
				throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATATABLECSVBUFFEROVERFLOW);

			for (auto ch : sNewLine) {
				buffer.at(nChunkPosition) = ch;
				nChunkPosition++;
			}

		}

		pWriter->WriteData(nChunkPosition, (uint8_t*)buffer.data());
	}



}

void CDataTable::WriteDataToStream(ITempStreamWriter* pWriter, IDataTableWriteOptions* pOptions)
{
	if (pWriter == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	sLibMCDataTableStreamHeader header;
	memset((void*)&header, 0, sizeof(header));

	uint64_t currentDataStart = sizeof (header);
	header.m_nSignature = DATATABLE_HEADERSIGNATURE;
	header.m_nVersion = DATATABLE_CURRENTFORMATVERSION;
	header.m_nColumnTableStart = currentDataStart;
	header.m_nColumnCount = (uint32_t)m_Columns.size();
	pWriter->WriteData(sizeof (header), (uint8_t*)&header);

	currentDataStart += m_Columns.size() * sizeof(sLibMCDataTableColumnHeader);

	for (auto pColumn : m_Columns) {
		std::string sIdentifier = pColumn->getIdentifier();
		std::string sDescription = pColumn->getDescription();

		sLibMCDataTableColumnHeader columnHeader;
		memset((void*)&columnHeader, 0, sizeof(columnHeader));

		columnHeader.m_nIdentifierStart = currentDataStart;
		columnHeader.m_nIdentifierLength = (uint32_t)sIdentifier.length();
		currentDataStart += columnHeader.m_nIdentifierLength;

		columnHeader.m_nDescriptionStart = currentDataStart;
		columnHeader.m_nDescriptionLength = (uint32_t)sDescription.length();
		currentDataStart += columnHeader.m_nDescriptionLength;

		columnHeader.m_nEntryCount = pColumn->getRowCount();
		columnHeader.m_nColumnDataType = (uint32_t)pColumn->getColumnType();
		columnHeader.m_nEncodingType = (uint32_t)DATATABLE_ENCODINGTYPE_RAW;
		columnHeader.m_nColumnDataStart = currentDataStart;
		
		currentDataStart += columnHeader.m_nEntryCount * pColumn->getEntrySizeInBytes();

		pWriter->WriteData(sizeof(columnHeader), (uint8_t*)&columnHeader);
	}

	for (auto pColumn : m_Columns) {
		std::string sIdentifier = pColumn->getIdentifier();
		std::string sDescription = pColumn->getDescription();		
		if (sIdentifier.size() > 0)
			pWriter->WriteData(sIdentifier.length(), (const uint8_t*)sIdentifier.c_str());
		if (sDescription.size() > 0)
			pWriter->WriteData(sDescription.length(), (const uint8_t*)sDescription.c_str());

		pColumn->WriteDataToStream(pWriter);
	}

}

void CDataTable::LoadFromStream(IStreamReader* pStream)
{
	Clear();

	if (pStream == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	uint64_t nStreamSize = pStream->GetSize();

	// The signature selects the format: the chunked columnar format (version 2) carries its own
	// signature, so that it is never interpreted as a version 1 column table.
	uint32_t nSignature = 0;
	pStream->Seek(0);
	pStream->ReadData(sizeof(nSignature), sizeof(nSignature), nullptr, (uint8_t*)&nSignature);

	if (nSignature == DATATABLE_HEADERSIGNATURE_V2)
		loadFromStreamV2(pStream, nStreamSize);
	else
		loadFromStreamV1(pStream, nStreamSize);
}

void CDataTable::loadFromStreamV2(IStreamReader* pStream, uint64_t nStreamSize)
{
	CDataTableClearGuard clearOnFailure(this);

	try {

		AMC::CDataTableV2SegmentReader segmentReader([pStream](uint64_t nOffset, uint64_t nSize, uint8_t* pBuffer) {
			if (nSize > 0) {
				pStream->Seek(nOffset);
				pStream->ReadData(nSize, nSize, nullptr, pBuffer);
			}
		}, nStreamSize);

		uint64_t nRowCount = segmentReader.getRowCount();
		uint32_t nChunkCount = segmentReader.getChunkCount();
		uint32_t nColumnCount = segmentReader.getColumnCount();

		for (uint32_t nColumnIndex = 0; nColumnIndex < nColumnCount; nColumnIndex++) {
			auto& column = segmentReader.getColumn(nColumnIndex);
			auto pColumn = addColumnEx(column.m_sIdentifier, column.m_sDescription, (LibMCEnv::eDataTableColumnType)column.m_DataType);

			if (nRowCount > 0) {
				uint64_t nEntrySize = pColumn->getEntrySizeInBytes();
				uint8_t* pColumnData = pColumn->resizeRawData(nRowCount);

				uint64_t nRowOffset = 0;
				for (uint32_t nChunkIndex = 0; nChunkIndex < nChunkCount; nChunkIndex++) {
					segmentReader.readPage(nChunkIndex, nColumnIndex, pColumnData + nRowOffset * nEntrySize, (nRowCount - nRowOffset) * nEntrySize);
					nRowOffset += segmentReader.getChunkRowCount(nChunkIndex);
				}

				pColumn->rawDataChanged();
			}
		}

	}
	catch (const AMC::EDataTableFormatException& formatException) {
		throw ELibMCEnvInterfaceException(formatErrorToResult(formatException.getError()), formatException.what());
	}

	clearOnFailure.dismiss();

	updateMaxRowCount();
}

void CDataTable::loadFromStreamV1(IStreamReader* pStream, uint64_t nStreamSize)
{
	if (nStreamSize < sizeof(sLibMCDataTableStreamHeader))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATATABLESIGNATURE, "stream is too small for a datatable");

	sLibMCDataTableStreamHeader header;
	memset((void*)&header, 0, sizeof(header));

	pStream->Seek(0);
	pStream->ReadData(sizeof(header), sizeof(header), nullptr, (uint8_t*)&header);

	if (header.m_nSignature != DATATABLE_HEADERSIGNATURE)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATATABLESIGNATURE);

	// Files written before the version field was populated carry 0 and are format version 1.
	if ((header.m_nVersion != DATATABLE_FORMATVERSION_UNVERSIONED) && (header.m_nVersion != DATATABLE_FORMATVERSION_1))
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_UNSUPPORTEDDATATABLEVERSION, "unsupported datatable format version: " + std::to_string(header.m_nVersion));

	if (header.m_nColumnCount >= DATATABLE_MAXCOLUMNCOUNT)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_DATATABLEHASTOOMANYCOLUMS);

	if (header.m_nColumnCount > 0) {

		if (header.m_nColumnTableStart < sizeof(header))
			throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATATABLECOLUMNHEADER, "column table overlaps the stream header");

		std::vector<sLibMCDataTableColumnHeader> columnHeaders;
		columnHeaders.resize(header.m_nColumnCount);

		size_t nBufferSize = sizeof(sLibMCDataTableColumnHeader) * (size_t)header.m_nColumnCount;

		pStream->Seek(header.m_nColumnTableStart);
		pStream->ReadData(nBufferSize, nBufferSize, nullptr, (uint8_t*)columnHeaders.data());

		// Never leave a partially loaded table behind: if any check or read below throws,
		// the guard clears the table during stack unwinding.
		CDataTableClearGuard clearOnFailure(this);

		{

			for (auto& columnHeader : columnHeaders) {

				// RAW is the only encoding of format version 1. Anything else would be read as garbage.
				if (columnHeader.m_nEncodingType != DATATABLE_ENCODINGTYPE_RAW)
					throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_UNSUPPORTEDDATATABLEENCODING, "unsupported datatable column encoding: " + std::to_string(columnHeader.m_nEncodingType));

				// Validate all offsets against the stream size before allocating or reading anything,
				// so that a truncated or corrupted stream fails with a datatable-specific error.
				if ((columnHeader.m_nIdentifierStart > nStreamSize) || (columnHeader.m_nIdentifierLength > nStreamSize - columnHeader.m_nIdentifierStart))
					throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATATABLECOLUMNHEADER, "column identifier exceeds the end of the stream");
				if ((columnHeader.m_nDescriptionStart > nStreamSize) || (columnHeader.m_nDescriptionLength > nStreamSize - columnHeader.m_nDescriptionStart))
					throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATATABLECOLUMNHEADER, "column description exceeds the end of the stream");

				std::vector<uint8_t> identifierBytes;
				std::vector<uint8_t> descriptionBytes;

				identifierBytes.resize((size_t)columnHeader.m_nIdentifierLength + 1);
				descriptionBytes.resize((size_t)columnHeader.m_nDescriptionLength + 1);

				if (columnHeader.m_nIdentifierLength > 0) {
					pStream->Seek(columnHeader.m_nIdentifierStart);
					pStream->ReadData(columnHeader.m_nIdentifierLength, columnHeader.m_nIdentifierLength, nullptr, identifierBytes.data());
				}

				if (columnHeader.m_nDescriptionLength > 0) {
					pStream->Seek(columnHeader.m_nDescriptionStart);
					pStream->ReadData(columnHeader.m_nDescriptionLength, columnHeader.m_nDescriptionLength, nullptr, descriptionBytes.data());
				}

				identifierBytes.at(columnHeader.m_nIdentifierLength) = 0;
				descriptionBytes.at(columnHeader.m_nDescriptionLength) = 0;

				std::string sIdentifier((char*)identifierBytes.data());
				std::string sDescription((char*)descriptionBytes.data());

				auto pColumn = addColumnEx(sIdentifier, sDescription, (LibMCEnv::eDataTableColumnType)columnHeader.m_nColumnDataType);

				uint64_t nEntrySize = pColumn->getEntrySizeInBytes();
				if (columnHeader.m_nColumnDataStart > nStreamSize)
					throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATATABLECOLUMNHEADER, "column data of " + sIdentifier + " starts beyond the end of the stream");
				if (columnHeader.m_nEntryCount > (nStreamSize - columnHeader.m_nColumnDataStart) / nEntrySize)
					throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDDATATABLECOLUMNHEADER, "column data of " + sIdentifier + " exceeds the end of the stream");

				pStream->Seek(columnHeader.m_nColumnDataStart);
				pColumn->ReadDataFromStream(pStream, columnHeader.m_nEntryCount);

			}

		}

		clearOnFailure.dismiss();

		updateMaxRowCount();

	}
}

void CDataTable::Clear()
{
	m_Columns.clear();
	m_ColumnMap.clear();

	updateMaxRowCount();
}

IDataTableScatterPlotOptions* CDataTable::CreateScatterPlotOptions()
{
	return new CDataTableScatterPlotOptions();
}

IScatterPlot* CDataTable::CalculateScatterPlot(IDataTableScatterPlotOptions* pScatterPlotInput)
{
	if (pScatterPlotInput == nullptr)
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_INVALIDPARAM);

	std::string sNewUUID = AMCCommon::CUtils::createUUID();

	size_t nPointCount = m_nMaxRowCount;

	std::string sXAxisColumn = pScatterPlotInput->GetXAxisColumn();
	if (sXAxisColumn.empty ())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_NOXAXISCOLUMNGIVEN);
	double dXAxisScaleFactor = pScatterPlotInput->GetXAxisScaling();
	double dXAxisOffset = pScatterPlotInput->GetXAxisOffset();

	std::string sYAxisColumn = pScatterPlotInput->GetYAxisColumn();
	if (sYAxisColumn.empty())
		throw ELibMCEnvInterfaceException(LIBMCENV_ERROR_NOYAXISCOLUMNGIVEN);
	double dYAxisScaleFactor = pScatterPlotInput->GetYAxisScaling();
	double dYAxisOffset = pScatterPlotInput->GetYAxisOffset();

	auto pXAxisColumn = findColumn(sXAxisColumn, true);
	auto pYAxisColumn = findColumn(sYAxisColumn, true);

	auto pScatterPlotInstance = std::make_shared<AMC::CScatterplot>(sNewUUID);
	auto& entries = pScatterPlotInstance->getEntries();
	entries.resize(nPointCount);

	pXAxisColumn->fillScatterplotXCoordinates(pScatterPlotInstance.get(), dXAxisScaleFactor, dXAxisOffset);
	pYAxisColumn->fillScatterplotYCoordinates(pScatterPlotInstance.get(), dYAxisScaleFactor, dYAxisOffset);

	pScatterPlotInstance->computeBoundaries();

	auto pDataChannelsIterator = pScatterPlotInput->ListDataChannels();
	while (pDataChannelsIterator->MoveNext()) {
		auto pDataChannel = dynamic_cast<IScatterPlotDataChannel*>(pDataChannelsIterator->GetCurrent());
		auto sChannelIdentifier = pDataChannel->GetChannelIdentifier();

		auto pDataColumnsIterator = pDataChannel->ListScatterPlotDataColumns();
		while (pDataColumnsIterator->MoveNext()){
			auto pDataColumn = dynamic_cast<IScatterPlotDataColumn*>(pDataColumnsIterator->GetCurrent());
			auto sColumnIdentifier = pDataColumn->GetColumnIdentifier();

			auto pColumn = findColumn(sColumnIdentifier, false);
			if (pColumn)
				pColumn->fillScatterplotChannel(pScatterPlotInstance.get(), sChannelIdentifier, sColumnIdentifier, pDataColumn->GetScaleFactor(), pDataColumn->GetOffsetFactor());
		}
	}

	m_pToolpathHandler->storeScatterplot(pScatterPlotInstance);

	return new CScatterPlot (pScatterPlotInstance);
}

