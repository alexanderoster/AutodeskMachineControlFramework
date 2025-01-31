/*++

Copyright (C) 2019 3MF Consortium

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Abstract:

NMR_PortableZIPWriter.cpp implements a portable and fast writer of ZIP files

--*/

#include "common_portablezipwriter.hpp"
#include "common_utils.hpp"
#include "common_portablezipwritertypes.hpp"
#include "common_exportstream_zip.hpp"
#include <stdexcept>

namespace AMCCommon {

	CPortableZIPWriter::CPortableZIPWriter(PExportStream pExportStream, bool bWriteZIP64)
	{
		if (pExportStream.get() == nullptr)
			throw std::runtime_error("invalid export stream param");

		m_pExportStream = pExportStream;
		m_nCurrentEntryKey = 0;
		m_nNextEntryKey = 1;
		m_pCurrentEntry = nullptr;
		m_bIsFinished = false;
		m_bWriteZIP64 = bWriteZIP64;

		if (m_bWriteZIP64) {
			m_nVersionMade = ZIPFILEVERSIONNEEDEDZIP64;
			m_nVersionNeeded = ZIPFILEVERSIONNEEDEDZIP64;
		}
		else {
			m_nVersionMade = ZIPFILEVERSIONNEEDED;
			m_nVersionNeeded = ZIPFILEVERSIONNEEDED;
		}

		if (m_pExportStream->getPosition() != 0)
			throw std::runtime_error("export stream not empty");

	}

	CPortableZIPWriter::~CPortableZIPWriter()
	{
		if (!m_bIsFinished)
			writeDirectory();
	}

	PExportStream CPortableZIPWriter::createEntry(const std::string sName, uint64_t nUnixTimeStamp)
	{
		if (m_bIsFinished)
			throw std::runtime_error("zip already finished");
		// Finish old entry state
		closeEntry();

		// Initialize new entry state
		m_nCurrentEntryKey = m_nNextEntryKey;
		m_nNextEntryKey++;
		if (m_nNextEntryKey >= ZIPFILEMAXENTRIES)
			throw std::runtime_error("zip entry overflow");

		// Convert Name to UTF8
		std::string sFilteredName = AMCCommon::CUtils::removeLeadingPathDelimiter(sName);
		std::string sUTF8Name = sFilteredName;
		if (sUTF8Name.length() > ZIPFILEMAXFILENAMELENGTH)
			throw std::runtime_error("invalid zip name");
		uint32_t nNameLength = (uint32_t)sUTF8Name.length();

		// Convert Timestamp to File Date
		uint32_t nFileDate = 0;
		uint16_t nLastModTime = nFileDate % 65536;
		uint16_t nLastModDate = nFileDate / 65536;

		// Write local file header
		ZIPLOCALFILEHEADER LocalHeader;
		LocalHeader.m_nSignature = ZIPFILEHEADERSIGNATURE;
		LocalHeader.m_nVersion = m_nVersionNeeded;
		LocalHeader.m_nGeneralPurposeFlags = 0;
		LocalHeader.m_nCompressionMethod = ZIPFILECOMPRESSION_DEFLATED;
		LocalHeader.m_nLastModTime = nLastModTime;
		LocalHeader.m_nLastModDate = nLastModDate;
		LocalHeader.m_nCRC32 = 0;
		LocalHeader.m_nCompressedSize = 0;
		LocalHeader.m_nUnCompressedSize = 0;
		LocalHeader.m_nFileNameLength = nNameLength;
		LocalHeader.m_nExtraFieldLength = 0;// sizeof(ZIPLOCALFILEADDITIONALHEADER) + sizeof(ZIPLOCALFILEEXTENDEDINFORMATIONFIELD);

		if (m_bWriteZIP64) {
			LocalHeader.m_nExtraFieldLength += sizeof(ZIP64EXTRAINFORMATIONFIELD);
		}

		ZIP64EXTRAINFORMATIONFIELD zip64ExtraInformation;
		zip64ExtraInformation.m_nTag = ZIPFILEDATAZIP64EXTENDEDINFORMATIONEXTRAFIELD;
		zip64ExtraInformation.m_nFieldSize = sizeof(ZIP64EXTRAINFORMATIONFIELD) - 4;
		zip64ExtraInformation.m_nCompressedSize = 0;
		zip64ExtraInformation.m_nUncompressedSize = 0;

		// Write data to ZIP stream
		uint64_t nFilePosition = m_pExportStream->getPosition();
		
		// prepare byte-buffer for big-endian machines

		m_pExportStream->writeBuffer(&LocalHeader, sizeof(LocalHeader));
		m_pExportStream->writeBuffer(sUTF8Name.c_str(), nNameLength);
		uint64_t nExtInfoPosition = m_pExportStream->getPosition();
		if (m_bWriteZIP64) {
			// prepare byte-buffer for big-endian machines
			m_pExportStream->writeBuffer(&zip64ExtraInformation, sizeof(zip64ExtraInformation));
		}

		uint64_t nDataPosition = m_pExportStream->getPosition();

		// create list entry
		m_pCurrentEntry = std::make_shared<CPortableZIPWriterEntry>(sUTF8Name, nLastModTime, nLastModDate, nFilePosition, nExtInfoPosition, nDataPosition);
		m_Entries.push_back(m_pCurrentEntry);

		// Return new ZIP Entry stream
		m_pCurrentStream = std::make_shared<CExportStream_ZIP>(this, m_nCurrentEntryKey);
		return m_pCurrentStream;
	}

	void CPortableZIPWriter::closeEntry()
	{
		if (m_bIsFinished)
			throw std::runtime_error("zip already finished");

		if (m_pCurrentEntry.get() != nullptr) {
			// finish current stream writing
			CExportStream_ZIP * pZipStream = dynamic_cast<CExportStream_ZIP *>(m_pCurrentStream.get());
			if (pZipStream == nullptr)
				throw std::runtime_error("no export stream");
			pZipStream->flushZIPStream();

			// Write CRC and Size
			ZIPLOCALFILEDESCRIPTOR FileDescriptor;
			FileDescriptor.m_nCRC32 = m_pCurrentEntry->getCRC32();
			if (m_bWriteZIP64) {
				FileDescriptor.m_nCompressedSize =0xFFFFFFFF;
				FileDescriptor.m_nUnCompressedSize = 0xFFFFFFFF;
			}
			else {
				if ((m_pCurrentEntry->getCompressedSize() > ZIPFILEMAXIMUMSIZENON64) ||
					(m_pCurrentEntry->getUncompressedSize() > ZIPFILEMAXIMUMSIZENON64))
					throw std::runtime_error("zip entry too large for 64bit");
				FileDescriptor.m_nCompressedSize = (uint32_t)m_pCurrentEntry->getCompressedSize();
				FileDescriptor.m_nUnCompressedSize = (uint32_t)m_pCurrentEntry->getUncompressedSize();
			}

			ZIP64EXTRAINFORMATIONFIELD zip64ExtraInformation;
			zip64ExtraInformation.m_nTag = ZIPFILEDATAZIP64EXTENDEDINFORMATIONEXTRAFIELD;
			zip64ExtraInformation.m_nFieldSize = sizeof(ZIP64EXTRAINFORMATIONFIELD) - 4;
			zip64ExtraInformation.m_nCompressedSize = m_pCurrentEntry->getCompressedSize();
			zip64ExtraInformation.m_nUncompressedSize = m_pCurrentEntry->getUncompressedSize();
			
			// Write File Descriptor to file
			m_pExportStream->seekPosition(m_pCurrentEntry->getFilePosition() + ZIPFILEDESCRIPTOROFFSET, true);
			
			// prepare byte-buffer for big-endian machines
			m_pExportStream->writeBuffer(&FileDescriptor, sizeof(FileDescriptor));

			if (m_bWriteZIP64) {
				// Write Extra Information to file
				m_pExportStream->seekPosition(m_pCurrentEntry->getExtInfoPosition(), true);

				// prepare byte-buffer for big-endian machines
				m_pExportStream->writeBuffer(&zip64ExtraInformation, sizeof(zip64ExtraInformation));
			}

			// Reset file pointer
			m_pExportStream->seekFromEnd(0, true);
		}

		m_pCurrentStream = nullptr;
		m_pCurrentEntry = nullptr;
		m_nCurrentEntryKey = 0;
	}

	void CPortableZIPWriter::calculateChecksum(uint32_t nEntryKey, const void * pBuffer, uint32_t cbUncompressedBytes)
	{
		if (m_pCurrentEntry.get() == nullptr)
			throw std::runtime_error("invalid zip entry");

		if (pBuffer == nullptr)
			throw std::runtime_error("invalid param");

		if (nEntryKey != m_nCurrentEntryKey)
			throw std::runtime_error("invalid zip entry key");
		
		if (cbUncompressedBytes > 0) {
			m_pCurrentEntry->calculateChecksum(pBuffer, cbUncompressedBytes);
			m_pCurrentEntry->increaseUncompressedSize(cbUncompressedBytes);
		}
	}


	void CPortableZIPWriter::writeDeflatedBuffer(uint32_t nEntryKey, const void * pBuffer, uint32_t cbCompressedBytes)
	{
		if (m_pCurrentEntry.get() == nullptr)
			throw std::runtime_error("invalid zip entry");

		if (pBuffer == nullptr)
			throw std::runtime_error("invalid param");

		if (nEntryKey != m_nCurrentEntryKey)
			throw std::runtime_error("invalid zip entry key");

		if (cbCompressedBytes > 0) {
			m_pExportStream->writeBuffer(pBuffer, cbCompressedBytes);
			m_pCurrentEntry->increaseCompressedSize(cbCompressedBytes);
		}
	}

	uint64_t CPortableZIPWriter::getCurrentSize(uint32_t nEntryKey)
	{
		if (m_pCurrentEntry.get() == nullptr)
			throw std::runtime_error("invalid zip entry");

		if (nEntryKey != m_nCurrentEntryKey)
			throw std::runtime_error("invalid zip entry key");
		return m_pCurrentEntry->getUncompressedSize();
	}


	void CPortableZIPWriter::writeDirectory()
	{
		closeEntry();

		uint64_t nCentralDirStartPos = m_pExportStream->getPosition();
		auto iIterator = m_Entries.begin();
		while (iIterator != m_Entries.end()) {
			PPortableZIPWriterEntry pEntry = *iIterator;
			std::string sUTF8Name = pEntry->getUTF8Name();
			uint32_t nNameLength = (uint32_t)sUTF8Name.length();
			
			ZIPCENTRALDIRECTORYFILEHEADER DirectoryHeader;
			DirectoryHeader.m_nSignature = ZIPFILECENTRALHEADERSIGNATURE;
			DirectoryHeader.m_nVersionMade = m_nVersionMade;
			DirectoryHeader.m_nVersionNeeded = m_nVersionNeeded;
			DirectoryHeader.m_nGeneralPurposeFlags = 0;
			DirectoryHeader.m_nCompressionMethod = ZIPFILECOMPRESSION_DEFLATED;
			DirectoryHeader.m_nLastModTime = pEntry->getLastModTime();
			DirectoryHeader.m_nLastModDate = pEntry->getLastModDate();
			DirectoryHeader.m_nCRC32 = pEntry->getCRC32();
			DirectoryHeader.m_nCompressedSize = 0;
			DirectoryHeader.m_nUnCompressedSize = 0;
			DirectoryHeader.m_nFileNameLength = nNameLength;
			DirectoryHeader.m_nExtraFieldLength = 0;
			DirectoryHeader.m_nFileCommentLength = 0;
			DirectoryHeader.m_nDiskNumberStart = 0;
			DirectoryHeader.m_nInternalFileAttributes = 0;
			DirectoryHeader.m_nExternalFileAttributes = ZIPFILEEXTERNALFILEATTRIBUTES;
			DirectoryHeader.m_nRelativeOffsetOfLocalHeader = (uint32_t) pEntry->getFilePosition();

			uint64_t nRelativeOffsetOfLocalHeader = pEntry->getFilePosition();
			if (m_bWriteZIP64) {
				DirectoryHeader.m_nCompressedSize = 0xFFFFFFFF;
				DirectoryHeader.m_nUnCompressedSize = 0xFFFFFFFF;
				DirectoryHeader.m_nExtraFieldLength += sizeof(ZIP64EXTRAINFORMATIONFIELD) + sizeof(nRelativeOffsetOfLocalHeader);
				DirectoryHeader.m_nRelativeOffsetOfLocalHeader = 0xFFFFFFFF;
			}
			else {
				if ((m_pCurrentEntry->getCompressedSize() > ZIPFILEMAXIMUMSIZENON64) ||
					(m_pCurrentEntry->getUncompressedSize() > ZIPFILEMAXIMUMSIZENON64))
					throw std::runtime_error("zip entry too large for 64bit");
				DirectoryHeader.m_nCompressedSize = (uint32_t)pEntry->getCompressedSize();
				DirectoryHeader.m_nUnCompressedSize = (uint32_t)pEntry->getUncompressedSize();
			}

			// prepare byte-buffer for big-endian machines
			m_pExportStream->writeBuffer(&DirectoryHeader, (uint64_t) sizeof(DirectoryHeader));
			m_pExportStream->writeBuffer(sUTF8Name.c_str(), nNameLength);
			
			if (m_bWriteZIP64) {
				ZIP64EXTRAINFORMATIONFIELD zip64ExtraInformation;
				zip64ExtraInformation.m_nTag = ZIPFILEDATAZIP64EXTENDEDINFORMATIONEXTRAFIELD;
				zip64ExtraInformation.m_nFieldSize = sizeof(ZIP64EXTRAINFORMATIONFIELD) - 4 + sizeof(nRelativeOffsetOfLocalHeader);
				zip64ExtraInformation.m_nCompressedSize = pEntry->getCompressedSize();
				zip64ExtraInformation.m_nUncompressedSize = pEntry->getUncompressedSize();

				// prepare byte-buffer for big-endian machines
				m_pExportStream->writeBuffer(&zip64ExtraInformation, sizeof(zip64ExtraInformation));
				m_pExportStream->writeBuffer(&nRelativeOffsetOfLocalHeader, sizeof(nRelativeOffsetOfLocalHeader));
			}
			
			iIterator++;
		}

		uint64_t nEndOfCentralDir64StartPos = m_pExportStream->getPosition();

		// [ZIP64 end of central directory record]
		ZIP64ENDOFCENTRALDIRHEADER EndHeader64;
		EndHeader64.m_nSignature = ZIP64FILEENDOFCENTRALDIRRECORDSIGNATURE;
		EndHeader64.m_nEndOfCentralDirHeaderRecord = (uint64_t)(sizeof(ZIP64ENDOFCENTRALDIRHEADER) - 12);
		EndHeader64.m_nVersionMade = m_nVersionMade;
		EndHeader64.m_nVersionNeeded = m_nVersionNeeded;
		EndHeader64.m_nNumberOfDisk = 0;
		EndHeader64.m_nNumberOfDiskOfCentralDirectory = 0;
		EndHeader64.m_nTotalNumberOfEntriesOnThisDisk = m_Entries.size();
		EndHeader64.m_nTotalNumberOfEntriesInCentralDirectory = m_Entries.size();
		EndHeader64.m_nSizeOfCentralDirectory = (uint64_t)(m_pExportStream->getPosition() - nCentralDirStartPos);
		EndHeader64.m_nOffsetOfCentralDirectoryWithRespectToDisk = nCentralDirStartPos;
		
		// [ZIP64 end of central directory locator]
		ZIP64ENDOFCENTRALDIRLOCATOR EndLocator64;
		EndLocator64.m_nSignature = ZIP64FILEENDOFCENTRALDIRLOCATORSIGNATURE;
		EndLocator64.m_nNumberOfDiskWithStartOfZIP64EOCentralDir = 0;
		EndLocator64.m_nTotalNumberOfDisk = 1;
		EndLocator64.m_nRelativeOffset = nEndOfCentralDir64StartPos;
		
		ZIPENDOFCENTRALDIRHEADER EndHeader;
		EndHeader.m_nSignature = ZIPFILEENDOFCENTRALDIRSIGNATURE;
		EndHeader.m_nNumberOfDisk = 0;
		EndHeader.m_nRelativeNumberOfDisk = 0;
		EndHeader.m_nNumberOfEntriesOfDisk = (uint16_t) m_Entries.size();
		EndHeader.m_nNumberOfEntriesOfDirectory = (uint16_t)m_Entries.size();
		EndHeader.m_nSizeOfCentralDirectory = (uint32_t)(m_pExportStream->getPosition() - nCentralDirStartPos);
		EndHeader.m_nOffsetOfCentralDirectory = (uint32_t)nCentralDirStartPos;
		EndHeader.m_nCommentLength = 0;

		if (m_bWriteZIP64) {
			EndHeader.m_nOffsetOfCentralDirectory = 0xFFFFFFFF;
			// prepare byte-buffer for big-endian machines
			m_pExportStream->writeBuffer(&EndHeader64, sizeof(EndHeader64));
			m_pExportStream->writeBuffer(&EndLocator64, sizeof(EndLocator64));
		}
		// prepare byte-buffer for big-endian machines
		m_pExportStream->writeBuffer(&EndHeader, (uint64_t) sizeof(EndHeader));

		m_bIsFinished = true;
	}


}
