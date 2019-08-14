#!/bin/sh

g++ builder.cpp -o builder -lshp

rm -rf out naturalearth timezone db.zip
mkdir -p out
mkdir -p out_v1
mkdir -p naturalearth; cd naturalearth
wget https://www.naturalearthdata.com/http//www.naturalearthdata.com/download/10m/cultural/ne_10m_admin_0_countries_lakes.zip
unzip ne_10m_admin_0_countries_lakes.zip
cd ..
./builder C naturalearth/ne_10m_admin_0_countries_lakes ./out/country16.bin 16 "Made with Natural Earth, placed in the Public Domain." 0
./builder C naturalearth/ne_10m_admin_0_countries_lakes ./out/country21.bin 21 "Made with Natural Earth, placed in the Public Domain." 0
./builder C naturalearth/ne_10m_admin_0_countries_lakes ./out_v1/country16.bin 16 "Made with Natural Earth, placed in the Public Domain." 1
./builder C naturalearth/ne_10m_admin_0_countries_lakes ./out_v1/country21.bin 21 "Made with Natural Earth, placed in the Public Domain." 1

mkdir timezone; cd timezone
wget https://github.com/evansiroky/timezone-boundary-builder/releases/download/2019b/timezones.shapefile.zip
unzip timezones.shapefile.zip
cd ..
./builder T timezone/dist/combined-shapefile ./out/timezone16.bin 16 "Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License (ODbL)." 0
./builder T timezone/dist/combined-shapefile ./out/timezone21.bin 21 "Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License (ODbL)." 0
./builder T timezone/dist/combined-shapefile ./out_v1/timezone16.bin 16 "Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License (ODbL)." 1
./builder T timezone/dist/combined-shapefile ./out_v1/timezone21.bin 21 "Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License (ODbL)." 1
rm -rf naturalearth
zip db.zip out/* out_v1/*
