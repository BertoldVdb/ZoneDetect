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
