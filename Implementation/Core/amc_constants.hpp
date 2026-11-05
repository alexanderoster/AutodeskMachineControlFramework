/*++

Copyright (C) 2024 Autodesk Inc.

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


#ifndef __AMC_CONSTANTS
#define __AMC_CONSTANTS

#include <memory>

#define AMC_PARAMETERUNITS_MINIMUM 1e-9
#define AMC_PARAMETERUNITS_MAXIMUM 1e+9
#define AMC_PARAMETERUNITS_DEFAULT 0.001

#define STATEJOURNAL_MAXVARIABLECOUNT  (16 * 1024 * 1024)
#define STATEJOURNAL_VARIABLE_MINUNITS 1.0E-9
#define STATEJOURNAL_VARIABLE_MAXUNITS 1.0E9

#define DATATABLE_MAXCOLUMNCOUNT (1024 * 1024)
#define DATATABLE_HEADERSIGNATURE 0x6A9B23E1
// Signature of the chunked columnar format (format version 2, see amc_datatableformat.hpp). It is
// deliberately distinct from the v1 signature: a reader without v2 support must fail cleanly instead
// of misreading a page directory as column data.
#define DATATABLE_HEADERSIGNATURE_V2 0x6A9B23E2
// Version field of the stream header. Files written before the field was populated carry 0 and
// are treated as format version 1. Readers reject any other value.
#define DATATABLE_FORMATVERSION_UNVERSIONED 0
#define DATATABLE_FORMATVERSION_1 1
#define DATATABLE_CURRENTFORMATVERSION DATATABLE_FORMATVERSION_1
#define DATATABLE_DEFAULTCSVSEPARATOR ";"
#define DATATABLE_DEFAULTCSVNEWLINE "\n"
#define DATATABLE_DEFAULTCSVMAXBYTESPERENTRY 32
#define DATATABLE_DEFAULTCSVCHUNKSIZE 4096
#define DATATABLE_MINCSVSEPARATOR 32
#define DATATABLE_MAXCSVSEPARATOR 127

// Column encodings. RAW is the only encoding of format version 1; readers reject any other value.
#define DATATABLE_ENCODINGTYPE_RAW 1

// Streaming data capture defaults and limits
#define DATACAPTURE_DEFAULTCHUNKROWCOUNT 65536
#define DATACAPTURE_MINCHUNKROWCOUNT 1
#define DATACAPTURE_MAXCHUNKROWCOUNT (16 * 1024 * 1024)
#define DATACAPTURE_DEFAULTSEGMENTSIZE (1024ULL * 1024ULL * 1024ULL)
#define DATACAPTURE_MINSEGMENTSIZE 1024ULL
#define DATACAPTURE_DEFAULTMAXQUEUEDCHUNKS 16
#define DATACAPTURE_MINMAXQUEUEDCHUNKS 1
#define DATACAPTURE_MAXMAXQUEUEDCHUNKS 1024
#define DATACAPTURE_SEGMENTMIMETYPE "application/amcf-datatable"

#define MICROSECONDS_PER_MILLISECOND 1000ULL
#define MICROSECONDS_PER_SECOND (1000ULL * 1000ULL)
#define MICROSECONDS_PER_MINUTE (60ULL * 1000ULL * 1000ULL)
#define MICROSECONDS_PER_HOUR (60ULL * 60ULL * 1000ULL * 1000ULL)
#define MICROSECONDS_PER_DAY (24ULL * 60ULL * 60ULL * 1000ULL * 1000ULL)

#define MESHENTITY_MAXVERTEXCOUNT (1024ULL * 1024ULL * 1024ULL)
#define MESHENTITY_MAXTRIANGLECOUNT (1024ULL * 1024ULL * 1024ULL)

#endif //__AMC_CONSTANTS

