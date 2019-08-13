#!/bin/sh

g++ builder.cpp -o builder -lshp

#rm -rf out naturalearth timezone db.zip
mkdir -p out
mkdir -p naturalearth; cd naturalearth
#wget https://www.naturalearthdata.com/http//www.naturalearthdata.com/download/10m/cultural/ne_10m_admin_0_countries_lakes.zip
#unzip ne_10m_admin_0_countries_lakes.zip
cd ..
#./builder C naturalearth/ne_10m_admin_0_countries_lakes ./out/country16.bin 16 "Made with Natural Earth, placed in the Public Domain."
#./builder C naturalearth/ne_10m_admin_0_countries_lakes ./out/country21.bin 21 "Made with Natural Earth, placed in the Public Domain."

mkdir timezone; cd timezone
#wget https://github.com/evansiroky/timezone-boundary-builder/releases/download/2018i/timezones.shapefile.zip
#unzip timezones.shapefile.zip
cd ..
#./builder T timezone/dist/combined-shapefile ./out/timezone16.bin 16 "Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License (ODbL)."
./builder T timezone/dist/combined-shapefile ./out/timezone21.bin 21 "Contains data from Natural Earth, placed in the Public Domain. Contains information from https://github.com/evansiroky/timezone-boundary-builder, which is made available here under the Open Database License (ODbL)."
#rm -rf naturalearth
#zip db.zip out/*
