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

*/
#include "amcdata_databasemigrator_operationsequences.hpp"
#include "libmcdata_interfaceexception.hpp"

namespace AMCData {
		
	void CDatabaseMigrationClass_OperationSequences::increaseSchemaVersion(PSQLTransaction pTransaction, uint32_t nCurrentVersionIndex)
	{

		if (pTransaction.get() == nullptr)
			throw ELibMCDataInterfaceException(LIBMCDATA_ERROR_INVALIDPARAM);

		switch (nCurrentVersionIndex) {
		case 23: {
			// Published revisions of operation sequences. Rows are only ever inserted, never updated,
			// so that the exact sequence of every past build can be retrieved by its revision UUID.
			std::string sOperationSequences = "CREATE TABLE `operationsequences` (";
			sOperationSequences += "`uuid` varchar ( 64 ) UNIQUE NOT NULL,";
			sOperationSequences += "`statemachine` varchar ( 256 ) NOT NULL,";
			sOperationSequences += "`sequencename` varchar ( 256 ) NOT NULL,";
			sOperationSequences += "`revision` integer NOT NULL,";
			sOperationSequences += "`origin` integer NOT NULL,";
			sOperationSequences += "`content` text NOT NULL,";
			sOperationSequences += "`sha256` varchar ( 64 ) NOT NULL,";
			sOperationSequences += "`useruuid` varchar ( 64 ) NOT NULL,";
			sOperationSequences += "`timestamp` varchar ( 64 ) NOT NULL)";
			pTransaction->executeStatement(sOperationSequences);

			std::string sOperationSequencesIndex = "CREATE UNIQUE INDEX `idx_operationsequences_revision` ";
			sOperationSequencesIndex += "ON `operationsequences` (`statemachine`, `sequencename`, `revision`)";
			pTransaction->executeStatement(sOperationSequencesIndex);

			break;
		}

		}
	}



}

