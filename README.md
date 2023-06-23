# PJM Markets Gateway Utility
Command line tool written in C to query PJM Markets Gateway web service using 
[libcurl](https://curl.haxx.se/libcurl/) for HTTP client, [expat](https://libexpat.github.io/) 
to parse XML, and writes to command line. 

Details of the PJM Markets Gateway web service can be found [here](https://www.pjm.com/-/media/etools/emkt/external-interface-specification-guide-revision.ashx?la=en).

Written in C because I wanted to learn libcurl and expat.

## Installing
It is probably easiest to use docker/podman to run this.
```
docker build -t pjm-mg-util .
```

If docker/podman are not an option, compile from source. First, install dependencies (Fedora):
```
dnf install -y expat-devel libcurl-devel cmake
```

Checkout this repo and compile.
```
git clone https://github.com/mbheinen/pjm-mg-util
cd pjm-mg-util
mkdir build
cd build
cmake ..
make
sudo make install
```
