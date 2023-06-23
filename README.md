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
Once the image has been built, run it by passing in your PJM username and password. If you don't have credentials, register to get some [here](https://sso.pjm.com/).
```
docker run -e "PJM_USERNAME=<your username>" -e "PJM_PASSWORD=<your password>" pjm-mg-util
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

Then to run the tool first set environment variables for username and password, then run the tool.
```
PJM_USERNAME=<your username>
PJM_PASSWORD=<your password>
pjm-mg-util
