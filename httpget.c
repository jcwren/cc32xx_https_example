#include <time.h>
#include <unistd.h>
#include "string.h"
#include <ti/net/http/httpclient.h>
#include <ti/net/slnetsock.h>
#include <ti/net/slnetif.h>
#include "semaphore.h"
#include "uart_term.h"
#include "certs.h"
#include "startsntp.h"

//
//
//
#define USE_SNTP

//
//
//
extern void printError (const char *func, char *errString, int code);

//
//
//
void *httpTask (void *pvParameters)
{
  HTTPClient_Handle httpClientHandle;
  int16_t statusCode;
  bool moreDataFlag = false;
  char data [256];
  int16_t ret;
  int16_t len = 0;

#if defined (USE_SNTP)
  startSNTP ();
#endif

  UART_PRINT ("Sending a HTTP GET request to '%s'\n", SITE_HOSTNAME);

  httpClientHandle = HTTPClient_create (&statusCode, 0);

  if (statusCode < 0)
    printError (__func__, "HTTPClient_create", statusCode);

  if ((ret = HTTPClient_setHeader (httpClientHandle, HTTPClient_HFIELD_REQ_USER_AGENT, SITE_USER_AGENT, strlen (SITE_USER_AGENT) + 1, HTTPClient_HFIELD_PERSISTENT)) < 0)
    printError (__func__, "HTTPClient_setHeader", ret);

#if (CA_ROOT_TYPE == CA_ROOT_TYPE_MEMORY)
  {
    int16_t secureRetVal;
    SlNetSockSecAttrib_t *secAttrib = SlNetSock_secAttribCreate ();

    if ((ret = SlNetIf_loadSecObj (SLNETIF_SEC_OBJ_TYPE_CERTIFICATE, ROOT_CA_CERT_FILE, strlen (ROOT_CA_CERT_FILE), caRootCert, caRootCertSize, SLNETIF_ID_1)) < 0)
      printError (__func__, "SlNetIf_loadSecOb", ret);
    if ((ret = SlNetSock_secAttribSet (secAttrib, SLNETSOCK_SEC_ATTRIB_PEER_ROOT_CA, ROOT_CA_CERT_FILE, sizeof (ROOT_CA_CERT_FILE))) < 0)
      printError (__func__, "SlNetSock_secAttribSet", ret);
    if ((ret = HTTPClient_connect2 (httpClientHandle, SITE_HOSTNAME, secAttrib, 0, &secureRetVal)) < 0)
      printError (__func__, "HTTPClient_connect2", ret);
  }
#else
  {
    HTTPClient_extSecParams httpClientSecParams;

    httpClientSecParams.rootCa     = CA_ROOT_FILE;
    httpClientSecParams.clientCert = NULL;
    httpClientSecParams.privateKey = NULL;

    if ((ret = HTTPClient_connect (httpClientHandle, SITE_HOSTNAME, &httpClientSecParams, 0)) < 0)
      printError (__func__, "HTTPClient_connect", ret);
  }
#endif

  if ((ret = HTTPClient_sendRequest (httpClientHandle, HTTP_METHOD_GET, SITE_REQUEST_URI, NULL,0, 0)) < 0)
    printError (__func__, "HTTPClient_sendRequest", ret);

  if (ret != HTTP_SC_OK)
    printError (__func__, "httpTask: cannot get status", ret);

  UART_PRINT ("HTTP Response Status Code: %d\n", ret);

  do
  {
    if ((ret = HTTPClient_readResponseBody(httpClientHandle, data, sizeof(data), &moreDataFlag)) < 0)
      printError (__func__, "HTTPClient_readResponseBody", ret);

    UART_PRINT ("%.*s", ret, data);
    len += ret;
  }
  while (moreDataFlag);

  UART_PRINT ("Received %d bytes of payload\n", len);

  if ((ret = HTTPClient_disconnect (httpClientHandle)) < 0)
    printError (__func__, "HTTPClient_disconnect", ret);

  HTTPClient_destroy (httpClientHandle);

  return 0;
}
