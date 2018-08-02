'use strict'

let zd = require('./build/Release/zd.node')
zd.opendb("timezone21.bin")

exports.handler = function(event, context, callback) {
    let lat = parseFloat(event.queryStringParameters['lat']);
    let lon = parseFloat(event.queryStringParameters['lon']);
    let compact = event.queryStringParameters['c'] === "1";

    let replacer = function(key, val) {
        return val.toFixed ? Number(val.toFixed(6)) : val;
    }

    let result = JSON.stringify(zd.lookup(lat, lon, !compact), replacer, compact?0:2);

    let response = {
        statusCode: 200,
        headers: {
            "Cache-Control": "max-age=86400",
            "Access-Control-Allow-Origin": "*"
        },
        body: result
    };

    callback(null, response)
};

