# cc32xx\_https\_example

Based on the `simplelink_cc32xx_sdk_3_30_01_02/examples/rtos/CC3235SF_LAUNCHXL/demos/httpget` example,
this demonstrates connecting to an HTTPS site.

## Example Summary

Two methods are provided for certificate management:
* Creating the certificate in the local file system
* Using a certificate embedded in the program

## Example Usage

This example presumes the use of CCS, although other IDEs may be used. The
author has only used CCS.

### FreeRTOS

This example uses the FreeRTOS operating system. You will need to install
FreeRTOS per the instructions in CCS.

### Configuring certificate mode and HTTPS site

Edit the `certs.h` file. To select using a certificate from the CC32xx's file
system, use the following define:
```
#define CA_ROOT_TYPE CA_ROOT_TYPE_FILE
```

To select using an embedded cerfificate:
```
#define CA_ROOT_TYPE CA_ROOT_TYPE_MEMORY
```

Two sites are provided for testing. To use the `https://www.example.com` site,
edit the `certs.h` file and set the `SITE_` #defines to:
```
#define SITE_EXAMPLEDOTCOM
#undef  SITE_APPLEDOTCOM
```

To use the `https://apple.com` site:
```
#undef  SITE_EXAMPLEDOTCOM
#define SITE_APPLEDOTCOM
```

You may add additional sites and certificates for additional testing. See
the [Certificate notes](#Certificate-notes) section at the end of this README.

### Configuring the access point

Edit the `certs.h` file and set the following #defines as applicable:
* `SSID_NAME`
* `SECURITY_TYPE`
* `SECURITY_KEY`

## Programming the image

Build the project, flash it by using the Uniflash tool for cc32xx,
or equivalently, run a debug session on the IDE of your choice.

## Serial connection

Open a serial port session using Term Term (Windows), minicom, tio, or screen
(Linux), miniterm.py (Linux, Mac OS), or whatever floats your boat.  The COM
port can be determined via Device Manager in Windows or via `ls /dev/tty*` in
Linux and Mac OS.

The connection should have the following connection settings:

    Baud-rate:    115200
    Data bits:         8
    Stop bits:         1
    Parity:         None
    Flow Control:   None

* Run the example by pressing the reset button or by running a debug session
through your IDE.

* The example then makes an HTTP GET call to "www.example.com" and prints
the HTTP response status and the number of bytes of data received.

## Application Design Details

This application uses a task for HTTP communication:

``httpTask`` is started after the CC32xx acquires an IP address.  It then
establishes a connection to the HTTPS server. Once waits for the IP address to
ip to be acquired and then creates a connection to the HTTP server.  a
connection is established, the application makes an HTTP GET method call using
the request URI. The response status code, header fields and body from the HTTP
server are processed to get response status code and data. The connection is
closed and resources are freed before the task exits.

## Certificate notes

The two certificates in `certs.c` were obtained by browsing to the respective
sites, finding the root certificate Common Name, then using PC's the operating
system utility and exporting the certificates as a `.pem` file, merging them
into `certs.c` and adding the double-quotes and linefeed characters. Note that
the TI examples use `\r\n` as a line terminator, which is not strictly
necessary.  `\n' (Unix-style line endings) work just fine.

It is possible that the `https://apple.com` certificate may not always be
correct. Apple has multiple root certificates, and it is not clear if going to
the top-level Apple site will always use the same certificate, or if the
Apple's load-balancing algorithm might result in connecting to a server that
has a different certificate than the one obtained for this example.

