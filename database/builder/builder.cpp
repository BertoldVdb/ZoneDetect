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

#include <shapefil.h>
#include <iostream>
#include <limits>
#include <fstream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <functional>
#include <math.h>
#include <tuple>

const double Inf = std::numeric_limits<float>::infinity();

std::unordered_map<std::string, std::string> alpha2ToName;
std::unordered_map<std::string, std::string> tzidToAlpha2;

void errorFatal(std::string what)
{
    std::cerr<<what<<"\n";
    exit(1);
}

void assert(bool mustBeTrue, std::string what){
    if(!mustBeTrue){
        errorFatal(what);
    }
}

uint64_t encodeSignedToUnsigned(int64_t valueIn){
    uint64_t value = valueIn * 2;
    if(valueIn < 0) {
        value = -valueIn * 2 + 1;
    }

    return value;
}

int encodeVariableLength(std::vector<uint8_t>& output, int64_t valueIn, bool handleNeg = true)
{
    uint64_t value = valueIn;

    if(handleNeg) {
        value = encodeSignedToUnsigned(valueIn);
    }

    int bytesUsed = 0;
    do {
        uint8_t byteOut = value & 0x7F;
        if(value >= 128) {
            byteOut |= 128;
        }
        output.push_back(byteOut);
        bytesUsed ++;
        value >>= 7;
    } while(value);

    return bytesUsed;
}


int64_t doubleToFixedPoint(double input, double scale, unsigned int precision = 32)
{
    if(input == Inf){
        return INT64_MAX;
    }
    if(input == -Inf){
	return INT64_MIN;
    }

    double inputScaled = input / scale;
    return inputScaled * pow(2, precision-1);

}

struct Point {
    Point(double lat = 0, double lon = 0, unsigned int precision = 32)
    {
        lat_ = doubleToFixedPoint(lat, 90, precision);
        lon_ = doubleToFixedPoint(lon, 180, precision);
    }

    std::tuple<int64_t, int64_t> value()
    {
        return std::make_tuple(lat_, lon_);
    }

    int encodePointBinary(std::vector<uint8_t>& output)
    {
        int bytesUsed = encodeVariableLength(output, lat_);
        bytesUsed += encodeVariableLength(output, lon_);

        return bytesUsed;
    }

    int64_t lat_;
    int64_t lon_;
};

struct PolygonData {
    Point boundingMin;
    Point boundingMax;
    std::vector<Point> points_;
    unsigned long fileIndex_ = 0;
    unsigned long metadataId_;

    void processPoint(const Point& p)
    {
        if(p.lat_ < boundingMin.lat_) {
            boundingMin.lat_ = p.lat_;
        }
        if(p.lon_ < boundingMin.lon_) {
            boundingMin.lon_ = p.lon_;
        }
        if(p.lat_ > boundingMax.lat_) {
            boundingMax.lat_ = p.lat_;
        }
        if(p.lon_ > boundingMax.lon_) {
            boundingMax.lon_ = p.lon_;
        }

        points_.push_back(p);
    }

    PolygonData(unsigned long id):
        boundingMin(Inf, Inf),
        boundingMax(-Inf, -Inf),
        metadataId_(id)
    {
    }
    
    uint64_t encodePointTo64(int64_t lat, int64_t lon){
        assert(lat || lon, "Tried to encode 0,0. This is not allowed");

        uint64_t latu=encodeSignedToUnsigned(lat);
        uint64_t lonu=encodeSignedToUnsigned(lon);
       
        assert(latu < (uint64_t)1<<32, "Unsigned lat overflow");
        assert(lonu < (uint64_t)1<<32, "Unsigned lat overflow");

        uint64_t point = 0;
        for(uint8_t i=31; i<=31; i--){
            point <<= 2;
            if(latu & (1<<i)){
                point |= 1;
            }
            if(lonu & (1<<i)){
                point |= 2;
            }
        }

        return point;
    }
    
    bool sameDirection(int64_t x1, int64_t y1, int64_t x2, int64_t y2){
        if((x1 > 0 && x2 < 0) || (x1 < 0 && x2 > 0)){
            return false;
        }
        if((y1 > 0 && y2 < 0) || (y1 < 0 && y2 > 0)){
            return false;
        }

        if(x1 == 0){
            return x2 == 0;
        }

        return y2 == (y1*x2/x1);
    }

    long encodeBinaryData(std::vector<uint8_t>& output)
    {
        bool first = true;
        int64_t latFixedPoint = 0, lonFixedPoint = 0;
        int64_t latFixedPointPrev, lonFixedPointPrev;

        int64_t diffLatAcc = 0, diffLonAcc = 0, diffLatPrev = 0, diffLonPrev = 0;

        for(Point point: points_){
            /* The points should first be rounded, and then the integer value is differentiated */
            latFixedPointPrev = latFixedPoint;
            lonFixedPointPrev = lonFixedPoint;
            std::tie(latFixedPoint, lonFixedPoint) = point.value();

            int64_t diffLat = latFixedPoint - latFixedPointPrev;
            int64_t diffLon = lonFixedPoint - lonFixedPointPrev;

            if(first) {
                /* First point is always encoded */
                encodeVariableLength(output, encodePointTo64(latFixedPoint, lonFixedPoint), false);
                
                first = false;
            } else {
                if(!sameDirection(diffLat, diffLon, diffLatPrev, diffLonPrev)) {
                    /* Encode accumulator */
                    if(diffLatAcc || diffLonAcc){                
                        encodeVariableLength(output, encodePointTo64(diffLatAcc, diffLonAcc), false);
                        
                        diffLatAcc = 0;
                        diffLonAcc = 0;
                    }
                }
                    
                diffLatAcc += diffLat;
                diffLonAcc += diffLon;
            }

            diffLatPrev = diffLat;
            diffLonPrev = diffLon;
        }
        
        /* Encode final point if needed */
        if(diffLonAcc || diffLatAcc) {
	    encodeVariableLength(output, encodePointTo64(diffLatAcc, diffLonAcc), false);
        }

        /* Encode stop marker */
        output.push_back(0);
        output.push_back(0);

        return 0;
    }
};

void encodeStringToBinary(std::vector<uint8_t>& output, std::string& input)
{
    encodeVariableLength(output, input.size(), false);
    for(unsigned int i=0; i<input.size(); i++) {
        output.push_back(input[i] ^ 0x80);
    }
}


std::unordered_map<std::string, uint64_t> usedStrings_;

struct MetaData {
    void encodeBinaryData(std::vector<uint8_t>& output)
    {
        for(std::string& str: data_) {
            if(str.length() >= 256) {
                std::cout << "Metadata string is too long\n";
                exit(1);
            }

            if(!usedStrings_.count(str)) {
                usedStrings_[str] = output.size();
                encodeStringToBinary(output, str);
            } else {
                encodeVariableLength(output, usedStrings_[str] + 256, false);
            }
        }
    }

    std::vector<std::string> data_;

    unsigned long fileIndex_;
};


std::vector<PolygonData*> polygons_;
std::vector<MetaData> metadata_;
std::vector<std::string> fieldNames_;


unsigned int decodeVariableLength(uint8_t* buffer, int64_t* result, bool handleNeg = true)
{
    int64_t value = 0;
    unsigned int i=0, shift = 0;

    do {
        value |= (buffer[i] & 0x7F) << shift;
        shift += 7;
    } while(buffer[i++] & 0x80);

    if(!handleNeg) {
        *result = value;
    } else {
        *result = (value & 1)?-(value/2):(value/2);
    }
    return i;
}

void readMetaDataTimezone(DBFHandle dataHandle)
{
    /* Specify field names */
    fieldNames_.push_back("TimezoneIdPrefix");
    fieldNames_.push_back("TimezoneId");
    fieldNames_.push_back("CountryAlpha2");
    fieldNames_.push_back("CountryName");

    /* Parse attribute names */
    for(int i = 0; i < DBFGetRecordCount(dataHandle); i++) {
        metadata_[i].data_.resize(4);
        for(int j = 0; j < DBFGetFieldCount(dataHandle); j++) {
            char fieldTitle[12];
            int fieldWidth, fieldDecimals;
            DBFFieldType eType = DBFGetFieldInfo(dataHandle, j, fieldTitle, &fieldWidth, &fieldDecimals);

            fieldTitle[11] = 0;
            std::string fieldTitleStr(fieldTitle);

            if( eType == FTString ) {
                if(fieldTitleStr == "tzid") {
                    std::string data = DBFReadStringAttribute(dataHandle, i, j);
                    size_t pos = data.find('/');
                    if (pos == std::string::npos) {
                        metadata_[i].data_.at(0) = data;
                    } else {
                        metadata_[i].data_.at(0) = data.substr(0, pos) + "/";
                        metadata_[i].data_.at(1) = data.substr(pos + 1, std::string::npos);
                    }
                    if(tzidToAlpha2.count(data)) {
                        metadata_[i].data_.at(2) = tzidToAlpha2[data];
                        if(alpha2ToName.count(metadata_[i].data_.at(2))) {
                            metadata_[i].data_.at(3) = alpha2ToName[metadata_[i].data_.at(2)];
                        } else {
                            std::cout<<metadata_[i].data_.at(2)<< " not found in alpha2ToName! ("<<data<<")\n";
                        }
                    } else {
                        std::cout<<data<<" not found in zoneToAlpha2!\n";
                    }
                }
            }
        }
    }
}

void readMetaDataNaturalEarthCountry(DBFHandle dataHandle)
{
    /* Specify field names */
    fieldNames_.push_back("Alpha2");
    fieldNames_.push_back("Alpha3");
    fieldNames_.push_back("Name");

    /* Parse attribute names */
    for(int i = 0; i < DBFGetRecordCount(dataHandle); i++) {
        metadata_[i].data_.resize(3);
        for(int j = 0; j < DBFGetFieldCount(dataHandle); j++) {
            char fieldTitle[12];
            int fieldWidth, fieldDecimals;
            DBFFieldType eType = DBFGetFieldInfo(dataHandle, j, fieldTitle, &fieldWidth, &fieldDecimals);

            fieldTitle[11] = 0;
            std::string fieldTitleStr(fieldTitle);

            if( eType == FTString ) {
                if(fieldTitleStr == "ISO_A2" || fieldTitleStr == "WB_A2") {
                    std::string tmp = DBFReadStringAttribute(dataHandle, i, j);
                    if(tmp != "-99") {
                        metadata_[i].data_.at(0) = tmp;
                    }
                } else if(fieldTitleStr == "ISO_A3" || fieldTitleStr == "WB_A3" || fieldTitleStr == "BRK_A3") {
                    std::string tmp = DBFReadStringAttribute(dataHandle, i, j);
                    if(tmp != "-99") {
                        metadata_[i].data_.at(1) = tmp;
                    }
                } else if(fieldTitleStr == "NAME_LONG") {
                    metadata_[i].data_.at(2) = DBFReadStringAttribute(dataHandle, i, j);
                }
            }

        }
    }
}

std::unordered_map<std::string, std::string> parseAlpha2ToName(DBFHandle dataHandle)
{
    std::unordered_map<std::string, std::string> result;

    for(int i = 0; i < DBFGetRecordCount(dataHandle); i++) {
        std::string alpha2, name;
        for(int j = 0; j < DBFGetFieldCount(dataHandle); j++) {
            char fieldTitle[12];
            int fieldWidth, fieldDecimals;
            DBFFieldType eType = DBFGetFieldInfo(dataHandle, j, fieldTitle, &fieldWidth, &fieldDecimals);

            fieldTitle[11] = 0;
            std::string fieldTitleStr(fieldTitle);

            if( eType == FTString ) {
                if(fieldTitleStr == "ISO_A2" || fieldTitleStr == "WB_A2") {
                    std::string tmp = DBFReadStringAttribute(dataHandle, i, j);
                    if(tmp != "-99" && alpha2 == "") {
                        alpha2 = tmp;
                    }
                } else if(fieldTitleStr == "NAME_LONG") {
                    name = DBFReadStringAttribute(dataHandle, i, j);
                }
            }
        }
        if(alpha2 != "") {
            result[alpha2]=name;
        }
    }

    result["GF"]="French Guiana";
    result["GP"]="Guadeloupe";
    result["BQ"]="Bonaire";
    result["MQ"]="Martinique";
    result["SJ"]="Svalbard and Jan Mayen Islands";
    result["NO"]="Norway";
    result["CX"]="Christmas Island";
    result["CC"]="Cocos Islands";
    result["YT"]="Mayotte";
    result["RE"]="Réunion";
    result["TK"]="Tokelau";

    return result;
}

std::unordered_map<std::string, std::string> parseTimezoneToAlpha2(std::string path)
{
    std::unordered_map<std::string, std::string> result;
    //TODO: Clean solution...
#include "zoneToAlpha.h"

    return result;
}

int main(int argc, char ** argv )
{
    if(argc != 6) {
        std::cout << "Wrong number of parameters\n";
        return 1;
    }

    tzidToAlpha2 = parseTimezoneToAlpha2("TODO");

    char tableType = argv[1][0];
    std::string path = argv[2];
    std::string outPath = argv[3];
    unsigned int precision = strtol(argv[4], NULL, 10);
    std::string notice = argv[5];

    DBFHandle dataHandle = DBFOpen("naturalearth/ne_10m_admin_0_countries_lakes", "rb" );
    alpha2ToName = parseAlpha2ToName(dataHandle);
    DBFClose(dataHandle);

    dataHandle = DBFOpen(path.c_str(), "rb" );
    if( dataHandle == NULL ) {
        errorFatal("Could not open attribute file\n");
    }

    metadata_.resize(DBFGetRecordCount(dataHandle));
    std::cout << "Reading "<<metadata_.size()<<" metadata records.\n";

    if(tableType == 'C') {
        readMetaDataNaturalEarthCountry(dataHandle);
    } else if(tableType == 'T') {
        readMetaDataTimezone(dataHandle);
    } else {
        std::cout << "Unknown table type\n";
        return 1;
    }

    DBFClose(dataHandle);

    SHPHandle shapeHandle = SHPOpen(path.c_str(), "rb");
    if( shapeHandle == NULL ) {
        errorFatal("Could not open shapefile\n");
    }

    int numEntities, shapeType, totalPolygons = 0;
    SHPGetInfo(shapeHandle, &numEntities, &shapeType, NULL, NULL);

    std::cout<<"Opened "<<SHPTypeName( shapeType )<< " file with "<<numEntities<<" entries.\n";

    for(int i = 0; i < numEntities; i++ ) {
        SHPObject *shapeObject;

        shapeObject = SHPReadObject( shapeHandle, i );
        if(shapeObject) {
            if(shapeObject->nSHPType != 3 && shapeObject->nSHPType != 5 &&
                    shapeObject->nSHPType != 13 && shapeObject->nSHPType != 15) {
                std::cout<<"Unsupported shape object ("<< SHPTypeName(shapeObject->nSHPType) <<")\n";
                continue;
            }

            int partIndex = 0;

            PolygonData* polygonData = nullptr;

            for(int j = 0; j < shapeObject->nVertices; j++ ) {
                if(j == 0 || j == shapeObject->panPartStart[partIndex]) {
                    totalPolygons++;

                    if(polygonData) {
                        /* Commit it */
                        polygons_.push_back(polygonData);
                    }
                    polygonData =  new PolygonData(i);

                    if(partIndex + 1 < shapeObject->nParts) {
                        partIndex++;
                    }
                }

                Point p(shapeObject->padfY[j], shapeObject->padfX[j], precision);
                polygonData->processPoint(p);

            }

            if(polygonData) {
                /* Commit it */
                polygons_.push_back(polygonData);
            }

            SHPDestroyObject(shapeObject);
        }
    }

    SHPClose(shapeHandle);

    std::cout<<"Parsed "<<totalPolygons<<" polygons.\n";

    /* Sort according to bounding box */
    std::sort(polygons_.begin(), polygons_.end(), [](PolygonData* a, PolygonData* b) {
        return a->boundingMin.lat_ < b->boundingMin.lat_;
    });

    /* Encode data section and store pointers */
    std::vector<uint8_t> outputData;
    for(PolygonData* polygon: polygons_) {
        polygon->fileIndex_ = outputData.size();
        polygon->encodeBinaryData(outputData);
    }
    std::cout << "Encoded data section into "<<outputData.size()<<" bytes.\n";

    /* Encode metadata */
    std::vector<uint8_t> outputMeta;
    for(MetaData& metadata: metadata_) {
        metadata.fileIndex_ = outputMeta.size();
        metadata.encodeBinaryData(outputMeta);
    }
    std::cout << "Encoded metadata into "<<outputMeta.size()<<" bytes.\n";

    /* Encode bounding boxes */
    std::vector<uint8_t> outputBBox;
    int64_t prevFileIndex = 0;
    int64_t prevMetaIndex = 0;
    for(PolygonData* polygon: polygons_) {
        polygon->boundingMin.encodePointBinary(outputBBox);
        polygon->boundingMax.encodePointBinary(outputBBox);

        encodeVariableLength(outputBBox, metadata_.at(polygon->metadataId_).fileIndex_ - prevMetaIndex);
        prevMetaIndex = metadata_[polygon->metadataId_].fileIndex_;

        encodeVariableLength(outputBBox, polygon->fileIndex_ - prevFileIndex, false);
        prevFileIndex = polygon->fileIndex_;
    }
    std::cout << "Encoded bounding box section into "<<outputBBox.size()<<" bytes.\n";

    /* Encode header */
    std::vector<uint8_t> outputHeader;
    outputHeader.push_back('P');
    outputHeader.push_back('L');
    outputHeader.push_back('B');
    outputHeader.push_back(tableType);
    outputHeader.push_back(1);
    outputHeader.push_back(precision);
    outputHeader.push_back(fieldNames_.size());
    for(unsigned int i=0; i<fieldNames_.size(); i++) {
        encodeStringToBinary(outputHeader, fieldNames_[i]);
    }
    encodeStringToBinary(outputHeader, notice);
    encodeVariableLength(outputHeader, outputBBox.size(), false);
    encodeVariableLength(outputHeader, outputMeta.size(), false);
    encodeVariableLength(outputHeader, outputData.size(), false);
    std::cout << "Encoded header into "<<outputHeader.size()<<" bytes.\n";

    FILE* outputFile = fopen(outPath.c_str(), "wb");
    fwrite(outputHeader.data(), 1, outputHeader.size(), outputFile);
    fwrite(outputBBox.data(), 1, outputBBox.size(), outputFile);
    fwrite(outputMeta.data(), 1, outputMeta.size(), outputFile);
    fwrite(outputData.data(), 1, outputData.size(), outputFile);
    fclose(outputFile);

}
