#pragma once

//
//  Edit as necessary to connect to your access point
//
#define SSID_NAME           "My AP SSID"
#define SECURITY_TYPE       SL_WLAN_SEC_TYPE_WPA_WPA2
#define SECURITY_KEY        "My_AP_Password"

//
//  Define the desired site and undef any others
//
#undef  SITE_EXAMPLEDOTCOM
#define SITE_APPLEDOTCOM

//
//  Select whether to use a certificate on the CC32xx file system or an
//  embedded certificate.
//
#define CA_ROOT_TYPE_FILE   0
#define CA_ROOT_TYPE_MEMORY 1

#define CA_ROOT_TYPE        CA_ROOT_TYPE_FILE

#if defined (SITE_EXAMPLEDOTCOM)
  #define SITE_HOSTNAME              "https://www.example.com"
  #define SITE_REQUEST_URI           "/"
  #define SITE_USER_AGENT            "HTTPClient (ARM; TI-RTOS)"
  #define SITE_ROOT_CA_COMMONNAME    "DigiCert Global Root CA"
#elif defined (SITE_APPLEDOTCOM)
  #define SITE_HOSTNAME              "https://apple.com"
  #define SITE_REQUEST_URI           "/"
  #define SITE_USER_AGENT            "HTTPClient (ARM; TI-RTOS)"
  #define SITE_ROOT_CA_COMMONNAME    "DigiCert High Assurance EV Root CA"
#else
  #error "No SITE_* define enabled!"
#endif

#define CA_ROOT_FILE "cert.pem"

extern unsigned char caRootCert [];
extern unsigned int caRootCertSize;
