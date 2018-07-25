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

#include <napi.h>
#include <vector> 
#include <cmath>
#include "zonedetect.h"
using namespace Napi;

ZoneDetect* zdTimezone;

Object formatOutput(Env env, ZoneDetect* database, float lat, float lon, bool withNotice) {
    Object obj = Object::New(env);

    if(lat > 90 || lat < -90 || lon > 180 || lon < -180 || std::isnan(lat) || std::isnan(lon)){
        return obj;
    }

    float safezone;
    ZoneDetectResult* results = ZDLookup(database, lat, lon, &safezone);
    if(!results){
        return obj;
    }

    unsigned int index = 0;
    std::vector<Object> sobjects;
    while(results[index].lookupResult != ZD_LOOKUP_END) {
        Object sobj = Object::New(env);
        sobj.Set(Napi::String::New(env, "Result"),
                 Napi::String::New(env, ZDLookupResultToString(results[index].lookupResult)));

        std::string finalTimezone;
        for(unsigned int i=0; i<results[index].numFields; i++){
	    if(!results[index].fieldNames || !results[index].data){
                continue;
            }

            if(results[index].fieldNames[i] && results[index].data[i]){
                /* Combine zone with prefix */
                if(std::string(results[index].fieldNames[i]) == "TimezoneId"){
                    finalTimezone += results[index].data[i];
                } else
                if(std::string(results[index].fieldNames[i]) == "TimezoneIdPrefix"){
                    finalTimezone = results[index].data[i] + finalTimezone;
                } else {
                    sobj.Set(Napi::String::New(env, results[index].fieldNames[i]),
                             Napi::String::New(env, results[index].data[i]));
                }
            }
        }
        sobj.Set(Napi::String::New(env, "TimezoneId"), Napi::String::New(env, finalTimezone));

        sobjects.push_back(sobj);
        index++;
    }
        
    ZDFreeResults(results);

    if(!index){
        std::string oceanTimezone = "Etc/GMT";
        int offsetGMT = (lon+187.5f)/15.0;
        offsetGMT -= 12;
        if(offsetGMT > 0){
            oceanTimezone += "-";
            oceanTimezone += std::to_string(offsetGMT);
        }
        if(offsetGMT < 0){
            oceanTimezone +=  "+";
            oceanTimezone += std::to_string(-offsetGMT);
        }
        
        Object sobj = Object::New(env);
        sobj.Set(Napi::String::New(env, "Result"), Napi::String::New(env, "In zone"));
        sobj.Set(Napi::String::New(env, "CountryName"), Napi::String::New(env, "High seas"));
        sobj.Set(Napi::String::New(env, "TimezoneId"), Napi::String::New(env, oceanTimezone));
        sobjects.push_back(sobj);
    }

    Array out = Array::New(env, sobjects.size());
    for(unsigned i=0; i<sobjects.size(); i++){
        out[i] = sobjects[i];
    }
    
    obj.Set(Napi::String::New(env, "Zones"), out);
    obj.Set(Napi::String::New(env, "Safezone"),
            Napi::Number::New(env, (index)?safezone:-1));
    

    std::string notice = ZDGetNotice(database);
    if(withNotice && notice != ""){
        obj.Set(Napi::String::New(env, "Notice"),
                Napi::String::New(env, notice));
    }

    return obj;
}

void OpenDB(const CallbackInfo& info) {
    std::string path;

    path = info[0].As<String>().Utf8Value();
    zdTimezone = ZDOpenDatabase(path.c_str());
}

void CloseDB(const CallbackInfo& info) {
    ZDCloseDatabase(zdTimezone);
    zdTimezone = nullptr;
}

Object Lookup(const CallbackInfo& info) {
    Env env = info.Env();
    float lat = info[0].As<Number>();
    float lon = info[1].As<Number>();
    bool withNotice = info[2].As<Boolean>();


    if(zdTimezone){
        auto country = formatOutput(env, zdTimezone, lat, lon, withNotice); 
        return country;
    }


    return Object::New(env);
}

Object Init(Env env, Object exports) {
    exports.Set("opendb", Function::New(env, OpenDB));
    exports.Set("closedb", Function::New(env, CloseDB));
    exports.Set("lookup", Function::New(env, Lookup));
    return exports;
}

NODE_API_MODULE(addon, Init)
