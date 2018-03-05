/*
 * Copyright (c) 2018, Bertold Van den Bergh (vandenbergh@bertold.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the author nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR DISTRIBUTOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdint.h>

#ifndef _ZONEDETECT_H_
#define _ZONEDETECT_H_

typedef enum {
    ZD_LOOKUP_IGNORE = -3,
    ZD_LOOKUP_END = -2,
    ZD_LOOKUP_PARSE_ERROR = -1,
    ZD_LOOKUP_NOT_IN_ZONE = 0,
    ZD_LOOKUP_IN_ZONE = 1,
    ZD_LOOKUP_IN_EXCLUDED_ZONE = 2,
    ZD_LOOKUP_ON_BORDER_VERTEX = 3,
    ZD_LOOKUP_ON_BORDER_SEGMENT = 4
} ZDLookupResult;

typedef struct {
    ZDLookupResult lookupResult;

    uint32_t metaId;
    uint8_t numFields;
    char** fieldNames;
    char** data;
} ZoneDetectResult;

struct ZoneDetectOpaque;
typedef struct ZoneDetectOpaque ZoneDetect;

#ifdef __cplusplus
extern "C" {
#endif

ZoneDetect* ZDOpenDatabase(const char* path);
void        ZDCloseDatabase(ZoneDetect* library);

ZoneDetectResult* ZDLookup(ZoneDetect* library, float lat, float lon, float* safezone);
void              ZDFreeResults(ZoneDetectResult* results);

const char* ZDGetNotice(ZoneDetect* library);
uint8_t     ZDGetTableType(ZoneDetect* library);
const char* ZDLookupResultToString(ZDLookupResult result);

#ifdef __cplusplus
}
#endif

#endif
