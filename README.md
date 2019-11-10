# ZoneDetect

This is a C library that allows you to find an area a point belongs to using a database file. A typical example would be looking up the country or timezone given a latitude and longitude. The timezone database also contains the country information.

The API should be self-explanatory from zonedetect.h. A small demo is included (demo.c)

The databases are obtained from [here](https://github.com/evansiroky/timezone-boundary-builder) and converted to the format used by this library.

### Online API
You can test the library using an online API: [https://timezone.bertold.org/timezone](https://timezone.bertold.org/timezone)
It takes the following GET parameters:

* lat: Latitude.
* lon: Longitude.
* c: Set to one (c=1) to produce compact JSON.
* s: Set to one (s=1) to get only the timezone.

For example: [https://timezone.bertold.org/timezone?lat=51&lon=5](https://timezone.bertold.org/timezone?lat=51&lon=5)  
You are free to use this API for any application, but I am not responsible for the quality of service. Please contact me if your application requires reliability. 


### Demo
An online demo is available here: [https://cdn.bertold.org/demo/timezone.html](https://timezone.bertold.org/demo/timezone.html). Simple click anywhere on the map and see the result of the query.
