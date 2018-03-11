# ZoneDetect

This is a C library that allows you to find an area a point belongs to using a database file. A typical example would be looking up the country or timezone given a latitude and longitude. The timezone database also contains the country information.

The API should be self-explanatory from zonedetect.h. A small demo is included (demo.c)

In the future (after cleanup) I will also commit the program used to convert shapefiles to the binary database files and an automated testsuite.

### Online API
You can test the library using an online API: [https://api.bertold.org/geozone](https://api.bertold.org/geozone)
It takes the following GET parameters:

* lat: Latitude.
* lon: Longitude.
* c: Set to one (c=1) to produce compact JSON.

For example: [https://api.bertold.org/geozone?lat=51&lon=5](https://api.bertold.org/geozone?lat=51&lon=5)  
Please do not use this API in an application that will result in a lot of server load :)
  
