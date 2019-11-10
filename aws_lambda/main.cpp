#include <aws/lambda-runtime/runtime.h>
#include <string>
#include "json.hpp"
#include "../library/zonedetect.h"

using json = nlohmann::json;
using namespace aws::lambda_runtime;

ZoneDetect* zd;

invocation_response zd_handler(invocation_request const& request){
    try{
        auto body = json::parse(request.payload);

        if(body.count("queryStringParameters")){    
            auto param = body["queryStringParameters"];
    
            if(param.count("lat") && param.count("lon")){
                float lat = std::stof(param["lat"].get<std::string>(), nullptr);
                float lon = std::stof(param["lon"].get<std::string>(), nullptr);

                int compact = 0;
                if(param.count("c")){
                    compact = std::stoi(param["c"].get<std::string>());
                }
            
                int simple = 0;
                if(param.count("s")){
                    simple = std::stoi(param["s"].get<std::string>());
                }

                json body;
            
                if(!compact){
                    body["Notice"] = ZDGetNotice(zd); 
                }
            
                if(simple){
                    auto sr = ZDHelperSimpleLookupString(zd, lat, lon);
                    if(sr){
                        body["Result"] = sr;
                        free(sr);
                    }
                }else{
                    float safezone = 0;
                    auto results = ZDLookup(zd, lat, lon, &safezone);
                    if(results){
                        int index = 0;
                        while(results[index].lookupResult != ZD_LOOKUP_END) {
                            auto& zone = body["Zones"][index];
                            zone["Result"] = ZDLookupResultToString(results[index].lookupResult);
                        
                            if(results[index].data) {
                                for(unsigned int i = 0; i < results[index].numFields; i++) {
                                    if(results[index].fieldNames[i] && results[index].data[i]) {
                                        zone[results[index].fieldNames[i]] = results[index].data[i];
                                    }
                                }

                                if(zone.count("TimezoneId") && zone.count("TimezoneIdPrefix")){
                                    zone["TimezoneId"] = zone["TimezoneIdPrefix"].get<std::string>() + zone["TimezoneId"].get<std::string>();
                                    zone.erase("TimezoneIdPrefix");
                                }
                            }

                            index++;
                        }
                    }

                    ZDFreeResults(results);
                }

                json response;
                response["statusCode"] = 200;
                response["headers"]["Cache-Control"] = "max-age=86400";
                response["headers"]["Access-Control-Allow-Origin"] = "*";
                response["body"] = body.dump(compact?0:2);
            
                return invocation_response::success(response.dump(), "application/json");
            }
        }
    }catch(...){}
    
    return invocation_response::failure("Error", "Error");
}

int main(){
    zd = ZDOpenDatabase("timezone21.bin");
    if(!zd) {
        return 1;
    }

    run_handler(zd_handler);

    ZDCloseDatabase(zd);
    return 0;
}
