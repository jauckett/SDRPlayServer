# SDRPlayServer
Simple rtl_tcp server for SDRPlay

Dependencies
  libusb, mirsdrapi-rsp (sdrplay.com to obtain mirsdrapi installer)



This implementation is a very basic single threaded rtl-tcp server that interfaces to the SDRPlay receiver.
It is meant to services a single client, such as SDR-Console, or websdr.




The code is experimental and a work in progress as of September 2016
TODO
  - implement sig handling for graceful shutdown
  - manage SDRPlay API frequency changes better
  - better error handling, program silently exits sometimes when client disconnects
  - SDRSharp seems to get out of sync on the IQ stream and renders noise with lots of imaging, from time to time
    SDR-Console does not have this problem



