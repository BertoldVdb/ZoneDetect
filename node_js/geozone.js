var express = require('express');
var zd = require('node_binding')
var router = express.Router();

zd.opendb("../library/timezone21.bin")

router.get('/', function(req, res, next) {
    if (typeof req.query === 'undefined') {
        return res.send([]);
    }

    var lat = parseFloat(req.query.lat)
    var lon = parseFloat(req.query.lon)
    var compact = req.query.c === "1";

    var replacer = function(key, val) {
        return val.toFixed ? Number(val.toFixed(6)) : val;
    }

    var result = JSON.stringify(zd.lookup(lat, lon, !compact), replacer, compact?0:2)

    res.set('Access-Control-Allow-Origin', '*');    
    res.set('Content-Type', 'application/json');
    res.send(result)
});

module.exports = router;
