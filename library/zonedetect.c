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

#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>

#include "zonedetect.h"

struct ZoneDetectOpaque {
    int fd;
    uint32_t length;
    uint8_t* mapping;

    uint8_t tableType;
    uint8_t version;
    uint8_t precision;
    uint8_t numFields;

    char* notice;
    char** fieldNames;

    uint32_t bboxOffset;
    uint32_t metadataOffset;
    uint32_t dataOffset;
};

static int32_t ZDFloatToFixedPoint(float input, float scale, unsigned int precision) {
    float inputScaled = input / scale;
    return inputScaled * (float)(1 << (precision-1));
}

static unsigned int ZDDecodeVariableLengthUnsigned(ZoneDetect* library, uint32_t* index, uint32_t* result) {
    uint32_t value = 0;
    unsigned int i=0, shift = 0;

    if(*index >= library->length) {
        return 0;
    }

    uint8_t* buffer = library->mapping + *index;
    uint8_t* bufferEnd = library->mapping + library->length - 1;

    while(1) {
        value |= (buffer[i] & 0x7F) << shift;
        shift += 7;

        if(!(buffer[i] & 0x80)) {
            break;
        }

        i++;
        if(buffer + i > bufferEnd) {
            return 0;
        }
    }

    i++;
    *result = value;
    *index += i;
    return i;
}

static unsigned int ZDDecodeVariableLengthSigned(ZoneDetect* library, uint32_t* index, int32_t* result) {
    uint32_t value = 0;
    unsigned int retVal = ZDDecodeVariableLengthUnsigned(library, index, &value);
    *result = (value & 1)?-(value/2):(value/2);
    return retVal;
}

static char* ZDParseString(ZoneDetect* library, uint32_t* index) {
    uint32_t strLength;
    if(!ZDDecodeVariableLengthUnsigned(library, index, &strLength)) {
        return NULL;
    }

    uint32_t strOffset = *index;
    unsigned int remoteStr = 0;
    if(strLength >= 256) {
        strOffset = library->metadataOffset + strLength - 256;
        remoteStr = 1;

        if(!ZDDecodeVariableLengthUnsigned(library, &strOffset, &strLength)) {
            return NULL;
        }

        if(strLength > 256) {
            return NULL;
        }
    }

    char* str = malloc(strLength + 1);

    if(str) {
        unsigned int i;
        for(i=0; i<strLength; i++) {
            str[i] = library->mapping[strOffset+i] ^ 0x80;
        }
        str[strLength] = 0;
    }

    if(!remoteStr) {
        *index += strLength;
    }

    return str;
}

static int ZDParseHeader(ZoneDetect* library) {
    if(library->length < 7) {
        return -1;
    }

    if(memcmp(library->mapping, "PLB", 3)) {
        return -1;
    }

    library->tableType = library->mapping[3];
    library->version   = library->mapping[4];
    library->precision = library->mapping[5];
    library->numFields = library->mapping[6];

    if(library->version != 0) {
        return -1;
    }

    uint32_t index = 7;

    library->fieldNames = malloc(library->numFields * sizeof(char*));
    unsigned int i;
    for(i=0; i<library->numFields; i++) {
        library->fieldNames[i] = ZDParseString(library, &index);
    }

    library->notice = ZDParseString(library, &index);
    if(!library->notice) {
        return -1;
    }

    uint32_t tmp;
    /* Read section sizes */
    /* By memset: library->bboxOffset = 0 */

    if(!ZDDecodeVariableLengthUnsigned(library, &index, &tmp)) return -1;
    library->metadataOffset = tmp + library->bboxOffset;

    if(!ZDDecodeVariableLengthUnsigned(library, &index, &tmp))return -1;
    library->dataOffset = tmp + library->metadataOffset;

    if(!ZDDecodeVariableLengthUnsigned(library, &index, &tmp)) return -1;

    /* Add header size to everything */
    library->bboxOffset += index;
    library->metadataOffset += index;
    library->dataOffset += index;

    /* Verify file length */
    if(tmp + library->dataOffset != library->length) {
        return -2;
    }

    return 0;
}

static int ZDPointInBox(int32_t xl, int32_t x, int32_t xr, int32_t yl, int32_t y, int32_t yr) {
    if((xl <= x && x <= xr) || (xr <= x && x <= xl)) {
        if((yl <= y && y <= yr) || (yr <= y && y <= yl)) {
            return 1;
        }
    }

    return 0;
}

static ZDLookupResult ZDPointInPolygon(ZoneDetect* library, uint32_t polygonIndex, int32_t latFixedPoint, int32_t lonFixedPoint, uint64_t* distanceSqrMin) {
    uint32_t numVertices;
    int32_t pointLat = 0, pointLon = 0, diffLat = 0, diffLon = 0, firstLat = 0, firstLon = 0, prevLat = 0, prevLon = 0;
    lonFixedPoint -= 3;

    /* Read number of vertices */
    if(!ZDDecodeVariableLengthUnsigned(library, &polygonIndex, &numVertices)) return ZD_LOOKUP_PARSE_ERROR;
    if(numVertices > 1000000) return ZD_LOOKUP_PARSE_ERROR;

    int prevQuadrant = 0, winding = 0;

    uint32_t i;
    for(i=0; i<=numVertices; i++) {
        if(i<numVertices) {
            if(!ZDDecodeVariableLengthSigned(library, &polygonIndex, &diffLat)) return ZD_LOOKUP_PARSE_ERROR;
            if(!ZDDecodeVariableLengthSigned(library, &polygonIndex, &diffLon)) return ZD_LOOKUP_PARSE_ERROR;
            pointLat += diffLat;
            pointLon += diffLon;
            if(i==0) {
                firstLat = pointLat;
                firstLon = pointLon;
            }
        } else {
            /* The polygons should be closed, but just in case */
            pointLat = firstLat;
            pointLon = firstLon;
        }

        /* Check if point is ON the border */
        if(pointLat == latFixedPoint && pointLon == lonFixedPoint) {
            if(distanceSqrMin) *distanceSqrMin=0;
            return ZD_LOOKUP_ON_BORDER_VERTEX;
        }

        /* Find quadrant */
        int quadrant;
        if(pointLat>=latFixedPoint) {
            if(pointLon>=lonFixedPoint) {
                quadrant = 0;
            } else {
                quadrant = 1;
            }
        } else {
            if(pointLon>=lonFixedPoint) {
                quadrant = 3;
            } else {
                quadrant = 2;
            }
        }

        if(i>0) {
            int windingNeedCompare = 0, lineIsStraight = 0;
            float a = 0, b = 0;

            /* Calculate winding number */
            if(quadrant == prevQuadrant) {
                /* Do nothing */
            } else if(quadrant == (prevQuadrant + 1) % 4) {
                winding ++;
            } else if((quadrant + 1) % 4 == prevQuadrant) {
                winding --;
            } else {
                windingNeedCompare = 1;
            }

            /* Avoid horizontal and vertical lines */
            if((pointLon == prevLon || pointLat == prevLat)) {
                lineIsStraight = 1;
            }

            /* Calculate the parameters of y=ax+b if needed */
            if(!lineIsStraight && (distanceSqrMin || windingNeedCompare)) {
                a = ((float)pointLat - (float)prevLat)/((float)pointLon - prevLon);
                b = (float)pointLat - a*(float)pointLon;
            }

            /* Jumped two quadrants. */
            if(windingNeedCompare) {
                if(lineIsStraight) {
                    if(distanceSqrMin) *distanceSqrMin=0;
                    return ZD_LOOKUP_ON_BORDER_SEGMENT;
                }

                /* Check if the target is on the border */
                int32_t intersectLon = ((float)latFixedPoint - b)/a;
                if(intersectLon == lonFixedPoint) {
                    if(distanceSqrMin) *distanceSqrMin=0;
                    return ZD_LOOKUP_ON_BORDER_SEGMENT;
                }

                /* Ok, it's not. In which direction did we go round the target? */
                int sign = (intersectLon < lonFixedPoint)?2:-2;
                if(quadrant == 2 || quadrant == 3) {
                    winding += sign;
                } else {
                    winding -= sign;
                }
            }

            /* Calculate closest point on line (if needed) */
            if(distanceSqrMin) {
                float closestLon, closestLat;
                if(!lineIsStraight) {
                    closestLon=((float)lonFixedPoint+a*(float)latFixedPoint-a*b)/(a*a+1);
                    closestLat=(a*((float)lonFixedPoint+a*(float)latFixedPoint)+b)/(a*a+1);
                } else {
                    if(pointLon == prevLon) {
                        closestLon=pointLon;
                        closestLat=latFixedPoint;
                    } else {
                        closestLon=lonFixedPoint;
                        closestLat=pointLat;
                    }
                }

                int closestInBox = ZDPointInBox(pointLon, closestLon, prevLon, pointLat, closestLat, prevLat);

                int64_t diffLat, diffLon;
                if(closestInBox) {
                    /* Calculate squared distance to segment. */
                    diffLat = closestLat - latFixedPoint;
                    diffLon = (closestLon - lonFixedPoint);
                } else {
                    /*
                     * Calculate squared distance to vertices
                     * It is enough to check the current point since the polygon is closed.
                     */
                    diffLat = pointLat - latFixedPoint;
                    diffLon = (pointLon - lonFixedPoint);
                }

                /* Note: lon has half scale */
                uint64_t distanceSqr = diffLat*diffLat + diffLon*diffLon*4;
                if(distanceSqr < *distanceSqrMin) *distanceSqrMin = distanceSqr;
            }
        }

        prevQuadrant = quadrant;
        prevLat = pointLat;
        prevLon = pointLon;
    }

    if(winding == -4) {
        return ZD_LOOKUP_IN_ZONE;
    } else if(winding == 4) {
        return ZD_LOOKUP_IN_EXCLUDED_ZONE;
    } else if(winding == 0) {
        return ZD_LOOKUP_NOT_IN_ZONE;
    }

    /* Should not happen */
    if(distanceSqrMin) *distanceSqrMin=0;
    return ZD_LOOKUP_ON_BORDER_SEGMENT;
}

void ZDCloseDatabase(ZoneDetect* library) {
    if(library) {
        if(library->fieldNames) {
            unsigned int i;
            for(i=0; i<library->numFields; i++) {
                if(library->fieldNames[i]) {
                    free(library->fieldNames[i]);
                }
            }
            free(library->fieldNames);
        }
        if(library->notice) {
            free(library->notice);
        }
        if(library->mapping) {
            munmap(library->mapping, library->length);
        }
        if(library->fd >= 0) {
            close(library->fd);
        }
        free(library);
    }
}

ZoneDetect* ZDOpenDatabase(const char* path) {
    ZoneDetect* library = (ZoneDetect*)malloc(sizeof(*library));

    if(library) {
        memset(library, 0, sizeof(*library));

        library->fd = open(path, O_RDONLY | O_CLOEXEC);
        if(library->fd < 0) {
            goto fail;
        }

        library->length = lseek(library->fd, 0, SEEK_END);
        if(library->length <= 0) {
            goto fail;
        }
        lseek(library->fd, 0, SEEK_SET);

        library->mapping = mmap(NULL, library->length, PROT_READ, MAP_PRIVATE | MAP_FILE, library->fd, 0);
        if(!library->mapping) {
            goto fail;
        }

        /* Parse the header */
        if(ZDParseHeader(library)) {
            goto fail;
        }
    }

    return library;

fail:
    ZDCloseDatabase(library);
    return NULL;
}

ZoneDetectResult* ZDLookup(ZoneDetect* library, float lat, float lon, float* safezone) {
    int32_t latFixedPoint = ZDFloatToFixedPoint(lat, 90, library->precision);
    int32_t lonFixedPoint = ZDFloatToFixedPoint(lon, 180, library->precision);
    unsigned int numResults = 0;
    uint64_t distanceSqrMin=-1;

    /* Iterate over all polygons */
    uint32_t bboxIndex = library->bboxOffset;
    int32_t metadataIndex = 0;
    int32_t polygonIndex = 0;

    ZoneDetectResult* results = malloc(sizeof(ZoneDetectResult));
    if(!results) {
        return NULL;
    }

    while(bboxIndex < library->metadataOffset) {
        int32_t minLat, minLon, maxLat, maxLon, metadataIndexDelta;
        uint32_t polygonIndexDelta;
        if(!ZDDecodeVariableLengthSigned(library, &bboxIndex, &minLat)) break;
        if(!ZDDecodeVariableLengthSigned(library, &bboxIndex, &minLon)) break;
        if(!ZDDecodeVariableLengthSigned(library, &bboxIndex, &maxLat)) break;
        if(!ZDDecodeVariableLengthSigned(library, &bboxIndex, &maxLon)) break;
        if(!ZDDecodeVariableLengthSigned(library, &bboxIndex, &metadataIndexDelta)) break;
        if(!ZDDecodeVariableLengthUnsigned(library, &bboxIndex, &polygonIndexDelta)) break;

        metadataIndex+=metadataIndexDelta;
        polygonIndex+=polygonIndexDelta;

        if(latFixedPoint >= minLat) {
            if(latFixedPoint <= maxLat &&
                    lonFixedPoint >= minLon &&
                    lonFixedPoint <= maxLon) {

                /* Indices valid? */
                if(library->metadataOffset + metadataIndex >= library->dataOffset) continue;
                if(library->dataOffset + polygonIndex >= library->length) continue;

                ZDLookupResult lookupResult = ZDPointInPolygon(library, library->dataOffset + polygonIndex, latFixedPoint, lonFixedPoint, (safezone)?&distanceSqrMin:NULL);
                if(lookupResult == ZD_LOOKUP_PARSE_ERROR) {
                    break;
                } else if(lookupResult != ZD_LOOKUP_NOT_IN_ZONE) {
                    ZoneDetectResult* newResults = realloc(results, sizeof(ZoneDetectResult) * (numResults+2));

                    if(newResults) {
                        results = newResults;
                        results[numResults].metaId = metadataIndex;
                        results[numResults].numFields = library->numFields;
                        results[numResults].fieldNames = library->fieldNames;
                        results[numResults].lookupResult = lookupResult;

                        numResults++;
                    } else {
                        break;
                    }
                }
            }
        } else {
            /* The data is sorted along minLat */
            break;
        }
    }

    /* Clean up results */
    unsigned int i, j;
    for(i=0; i<numResults; i++) {
        int insideSum = 0;
        ZDLookupResult overrideResult = ZD_LOOKUP_IGNORE;
        for(j=i; j<numResults; j++) {
            if(results[i].metaId == results[j].metaId) {
                ZDLookupResult tmpResult = results[j].lookupResult;
                results[j].lookupResult = ZD_LOOKUP_IGNORE;

                /* This is the same result. Is it an exclusion zone? */
                if(tmpResult == ZD_LOOKUP_IN_ZONE) {
                    insideSum++;
                } else if(tmpResult == ZD_LOOKUP_IN_EXCLUDED_ZONE) {
                    insideSum--;
                } else {
                    /* If on the bodrder then the final result is on the border */
                    overrideResult = tmpResult;
                }

            }
        }

        if(overrideResult != ZD_LOOKUP_IGNORE) {
            results[i].lookupResult = overrideResult;
        } else {
            if(insideSum) {
                results[i].lookupResult = ZD_LOOKUP_IN_ZONE;
            }
        }
    }

    /* Remove zones to ignore */
    unsigned int newNumResults = 0;
    for(i=0; i<numResults; i++) {
        if(results[i].lookupResult != ZD_LOOKUP_IGNORE) {
            results[newNumResults] = results[i];
            newNumResults++;
        }
    }
    numResults = newNumResults;

    /* Lookup metadata */
    for(i=0; i<numResults; i++) {
        uint32_t tmpIndex = library->metadataOffset + results[i].metaId;
        results[i].data = malloc(library->numFields * sizeof(char*));
        if(results[i].data) {
            for(j=0; j<library->numFields; j++) {
                results[i].data[j] = ZDParseString(library, &tmpIndex);
            }
        }
    }

    /* Write end marker */
    results[numResults].lookupResult = ZD_LOOKUP_END;
    results[numResults].numFields = 0;
    results[numResults].fieldNames = NULL;
    results[numResults].data = NULL;

    if(safezone) {
        *safezone = sqrtf(distanceSqrMin) * 90 / (float)(1 << (library->precision-1));
    }

    return results;
}

void ZDFreeResults(ZoneDetectResult* results) {
    unsigned int index = 0;

    if(!results) {
        return;
    }

    while(results[index].lookupResult != ZD_LOOKUP_END) {
        if(results[index].data) {
            unsigned int i;
            for(i=0; i<results[index].numFields; i++) {
                if(results[index].data[i]) {
                    free(results[index].data[i]);
                }
            }
            free(results[index].data);
        }
        index++;
    }
    free(results);
}

const char* ZDGetNotice(ZoneDetect* library) {
    return library->notice;
}

uint8_t ZDGetTableType(ZoneDetect* library) {
    return library->tableType;
}

const char* ZDLookupResultToString(ZDLookupResult result) {
    switch(result) {
    case ZD_LOOKUP_IGNORE:
        return "Ignore";
    case ZD_LOOKUP_END:
        return "End";
    case ZD_LOOKUP_PARSE_ERROR:
        return "Parsing error";
    case ZD_LOOKUP_NOT_IN_ZONE:
        return "Not in zone";
    case ZD_LOOKUP_IN_ZONE:
        return "In zone";
    case ZD_LOOKUP_IN_EXCLUDED_ZONE:
        return "In excluded zone";
    case ZD_LOOKUP_ON_BORDER_VERTEX:
        return "Target point is border vertex";
    case ZD_LOOKUP_ON_BORDER_SEGMENT:
        return "Target point is on border";
    }

    return "Unknown";
}

