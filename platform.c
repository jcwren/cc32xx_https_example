#include <stdio.h>

#include <ti/drivers/net/wifi/simplelink.h>
#include <ti/drivers/net/wifi/slnetifwifi.h>
#include <ti/drivers/SPI.h>

//
//  These includes are for the Board_initHook() support, if enabled
//
#include <ti/devices/cc32xx/inc/hw_types.h>
#include <ti/devices/cc32xx/driverlib/pin.h>
#include <ti/devices/cc32xx/driverlib/rom_map.h>
#include <ti/devices/cc32xx/driverlib/prcm.h>

#include "ti_drivers_config.h"
#include "uart_term.h"
#include "pthread.h"
#include "semaphore.h"
#include "certs.h"

#define APPLICATION_NAME     "HTTP GET"
#define DEVICE_ERROR         ("Device error, please refer \"DEVICE ERRORS CODES\" section in errors.h")
#define WLAN_ERROR           ("WLAN error, please refer \"WLAN ERRORS CODES\" section in errors.h")
#define SL_STOP_TIMEOUT      (200)
#define SPAWN_TASK_PRIORITY  (9)
#define SPAWN_STACK_SIZE     (4096 + 1024)
#define TASK_STACK_SIZE      (2048 + 1024)
#define SLNET_IF_WIFI_PRIO   (5)
#define SLNET_IF_WIFI_NAME   "CC32xx"
#undef  ENABLE_NWP_LOGS

#define arrsizeof(x) (sizeof (x) / sizeof (x [0]))
#define iarrsizeof(x) (int) (sizeof (x) / sizeof (x [0]))
#define elsizeof(x) (sizeof (x [0]))

//
//
//
static pthread_t httpThread;
static pthread_t spawn_thread ;
static int32_t mode;

//
//
//
extern void *httpTask (void *pvParameters);

#if ENABLE_NWP_LOGS
//
//  Maps pin 62 to output NWP debug info. This is binary async serial data at
//  921600/8/N/1, and will will look like garbage. Capture it using Putty,
//  TeraTerm, whatever. Just make sure the saved output doesn't have bit 7 set
//  low. TI Engineers will ask for this output if needed.
//
void Board_initHook (void)
{
  MAP_PRCMPeripheralClkEnable (PRCM_UARTA0, PRCM_RUN_MODE_CLK);
  MAP_PinTypeUART (PIN_62, PIN_MODE_1);
}
#endif

//
//
//
void printError (const char *func, char *errString, int code)
{
  UART_PRINT ("Error! Call to %s() returned error %d in function %s()\n", errString, code, func);

  while (1)
    ;
}

static _i32 st_ShowStorageInfo (void)
{
  _i32 ret = 0;
  _i32 size;
  _i32 used;
  _i32 avail;
  SlFsControlGetStorageInfoResponse_t storageInfo;

  UART_PRINT ("\nGet Storage Info:\n");

  if ((ret = sl_FsCtl ((SlFsCtl_e) SL_FS_CTL_GET_STORAGE_INFO, 0, NULL, NULL, 0, (_u8 *) &storageInfo, sizeof (SlFsControlGetStorageInfoResponse_t), NULL)) < 0)
    UART_PRINT ("sl_FsCtl error: %d\n");

  size = (storageInfo.DeviceUsage.DeviceBlocksCapacity *storageInfo.DeviceUsage.DeviceBlockSize) / 1024;
  UART_PRINT ("Total space: %dK\n\n", size);
  UART_PRINT ("Filestsyem      Size \tUsed \tAvail\t\n");

  size  = ((storageInfo.DeviceUsage.NumOfAvailableBlocksForUserFiles + storageInfo.DeviceUsage.NumOfAllocatedBlocks) * storageInfo.DeviceUsage.DeviceBlockSize) / 1024;
  used  = (storageInfo.DeviceUsage.NumOfAllocatedBlocks * storageInfo.DeviceUsage.DeviceBlockSize) / 1024;
  avail = (storageInfo.DeviceUsage.NumOfAvailableBlocksForUserFiles * storageInfo.DeviceUsage.DeviceBlockSize) / 1024;

  UART_PRINT ("%-15s %dK \t%dK \t%dK \t\n", "User", size, used, avail);

  size = (storageInfo.DeviceUsage.NumOfReservedBlocksForSystemfiles * storageInfo.DeviceUsage.DeviceBlockSize) / 1024;

  UART_PRINT ("%-15s %dK \n", "System", size);

  size = (storageInfo.DeviceUsage.NumOfReservedBlocks * storageInfo.DeviceUsage.DeviceBlockSize) / 1024;

  UART_PRINT ("%-15s %dK \n", "Reserved", size);
  UART_PRINT ("\n");
  UART_PRINT ("\n");

  UART_PRINT ("%-32s: %d \n", "Max number of files", storageInfo.FilesUsage.MaxFsFiles);
  UART_PRINT ("%-32s: %d \n", "Max number of system files", storageInfo.FilesUsage.MaxFsFilesReservedForSysFiles);
  UART_PRINT ("%-32s: %d \n", "Number of user files", storageInfo.FilesUsage.ActualNumOfUserFiles);
  UART_PRINT ("%-32s: %d \n", "Number of system files", storageInfo.FilesUsage.ActualNumOfSysFiles);
  UART_PRINT ("%-32s: %d \n", "Number of alert", storageInfo.FilesUsage.NumOfAlerts);
  UART_PRINT ("%-32s: %d \n", "Number Alert threshold", storageInfo.FilesUsage.NumOfAlertsThreshold);
  UART_PRINT ("%-32s: %d \n", "FAT write counter", storageInfo.FilesUsage.FATWriteCounter);

  UART_PRINT ("%-32s: ", "Bundle state");

  if (storageInfo.FilesUsage.Bundlestate == SL_FS_BUNDLE_STATE_STOPPED)
    UART_PRINT ("%s \n", "Stopped");
  else if (storageInfo.FilesUsage.Bundlestate == SL_FS_BUNDLE_STATE_STARTED)
    UART_PRINT ("%s \n", "Started");
  else if (storageInfo.FilesUsage.Bundlestate == SL_FS_BUNDLE_STATE_PENDING_COMMIT)
    UART_PRINT ("%s \n", "Commit pending");

  UART_PRINT ("\n");

  return ret;
}

#define MAX_FILE_ENTRIES 4

typedef struct
{
     SlFileAttributes_t attribute;
     char fileName [SL_FS_MAX_FILE_NAME_LENGTH];
}
slGetfileList_t;

static _i32 st_listFiles (_i32 numOfFiles, int bPrintDescription)
{
  int ret = SL_ERROR_BSD_ENOMEM;
  _i32 index = -1;
  _i32 fileCount = 0;
  slGetfileList_t *buffer = malloc (MAX_FILE_ENTRIES * sizeof (slGetfileList_t));

  UART_PRINT ("\nRead files list:\n");

  if (buffer)
  {
    while (numOfFiles > 0)
    {
      _i32 i;
      _i32 numOfEntries = (numOfFiles < MAX_FILE_ENTRIES) ? numOfFiles : MAX_FILE_ENTRIES;

      if ((ret = sl_FsGetFileList (&index, numOfEntries, sizeof (slGetfileList_t), (_u8*)buffer, SL_FS_GET_FILE_ATTRIBUTES)) < 0)
      {
        UART_PRINT ("sl_FsGetFileList error:  %d\n", ret);
        break;
      }

      if (ret == 0)
        break;

      for (i = 0; i < ret; i++)
      {
        UART_PRINT ("[%3d] ", ++fileCount);
        UART_PRINT ("%-40s  ", buffer [i].fileName);
        UART_PRINT ("%8d  ", buffer [i].attribute.FileMaxSize);
        UART_PRINT ("0x%04x\n", buffer [i].attribute.Properties);
      }
      numOfFiles -= ret;
    }

    UART_PRINT ("\n");

    if (bPrintDescription)
    {
      UART_PRINT ("File properties flags description:\n");
      UART_PRINT ("   0x%04x - Open file commit\n", SL_FS_INFO_MUST_COMMIT);
      UART_PRINT ("   0x%04x - Open bundle commit\n", SL_FS_INFO_BUNDLE_FILE);
      UART_PRINT ("   0x%04x - Pending file commit\n", SL_FS_INFO_PENDING_COMMIT);
      UART_PRINT ("   0x%04x - Pending bundle commit\n", SL_FS_INFO_PENDING_BUNDLE_COMMIT);
      UART_PRINT ("   0x%04x - Secure file\n", SL_FS_INFO_SECURE);
      UART_PRINT ("   0x%04x - Not fail-safe\n", SL_FS_INFO_NOT_FAILSAFE);
      UART_PRINT ("   0x%04x - System file\n", SL_FS_INFO_SYS_FILE);
      UART_PRINT ("   0x%04x - No valid image\n", SL_FS_INFO_NOT_VALID);
      UART_PRINT ("   0x%04x - Opened for public write\n", SL_FS_INFO_PUBLIC_WRITE);
      UART_PRINT ("   0x%04x - Opened for public read\n", SL_FS_INFO_PUBLIC_READ);
      UART_PRINT ("   0x%04x - Opened for read\n", SL_FS_INFO_OPEN_READ);
      UART_PRINT ("   0x%04x - Opened for read\n", SL_FS_INFO_OPEN_WRITE);
      UART_PRINT ("   0x%04x - File is not signed\n", SL_FS_INFO_NOSIGNATURE);
      UART_PRINT ("   0xc080 - (unused bits)\n\n");
    }

    free (buffer);
  }

  return ret;
}

static int writeCertFile (void)
{
#if (CA_ROOT_TYPE == CA_ROOT_TYPE_FILE)
  long deviceFileHandle;
  _i32 ret;
  _u32 masterToken = 0;

  UART_PRINT ("Attempt to write file '%s' with %u bytes...\n", CA_ROOT_FILE, caRootCertSize);

  if ((deviceFileHandle = sl_FsOpen ((unsigned char *) CA_ROOT_FILE, SL_FS_CREATE | SL_FS_OVERWRITE | SL_FS_CREATE_MAX_SIZE (8192), &masterToken)) < 0)
    printError (__func__, "sl_FsOpen", deviceFileHandle);
  if ((ret = sl_FsWrite (deviceFileHandle, 0, (unsigned char *) caRootCert, caRootCertSize)) < 0)
    printError (__func__, "sl_FsWrite", ret);
  if ((ret = sl_FsClose (deviceFileHandle, NULL, NULL , 0)) < 0)
    printError (__func__, "sl_FsClose", ret);

  UART_PRINT ("Wrote file '%s' successfully (%u bytes)\n", CA_ROOT_FILE, caRootCertSize);
#endif

  return 0;
}

/*!
  \brief          SimpleLinkNetAppEventHandler

  This handler gets called whenever a Netapp event is reported
  by the host driver / NWP. Here user can implement he's own logic
  for any of these events. This handler is used by 'network_terminal'
  application to show case the following scenarios:

  1. Handling IPv4 / IPv6 IP address acquisition.
  2. Handling IPv4 / IPv6 IP address Dropping.

  \param          pNetAppEvent     -   pointer to Netapp event data.

  \return         void

  \note           For more information, please refer to: user.h in the porting
  folder of the host driver and the  CC31xx/CC32xx
  NWP programmer's
  guide (SWRU455) section 5.7

 */
void SimpleLinkNetAppEventHandler (SlNetAppEvent_t *pNetAppEvent)
{
  int32_t status = 0;
  pthread_attr_t pAttrs;
  struct sched_param priParam;

  if (!pNetAppEvent)
    return;

  switch (pNetAppEvent->Id)
  {
    case SL_NETAPP_EVENT_IPV4_ACQUIRED :
    case SL_NETAPP_EVENT_IPV6_ACQUIRED :
      {
        if ((status = SlNetIf_init (0)) < 0)
          printError (__func__, "SlNetIf_init", status);
        if ((status = SlNetIf_add (SLNETIF_ID_1, SLNET_IF_WIFI_NAME, (const SlNetIf_Config_t *) &SlNetIfConfigWifi, SLNET_IF_WIFI_PRIO)) < 0)
          printError (__func__, "SlNetIf_add", status);
        if ((status = SlNetSock_init (0)) < 0)
          printError (__func__, "SlNetSock_init", status);
        if ((status = SlNetUtil_init (0)) < 0)
          printError (__func__, "SlNetUtil_init", status);

        st_ShowStorageInfo ();
        st_listFiles (50, 1);
        writeCertFile ();

        if (mode != ROLE_AP)
        {
          UART_PRINT ("[NETAPP EVENT] IP Acquired: IP=%d.%d.%d.%d, Gateway=%d.%d.%d.%d\n",
              SL_IPV4_BYTE (pNetAppEvent->Data.IpAcquiredV4.Ip, 3),
              SL_IPV4_BYTE (pNetAppEvent->Data.IpAcquiredV4.Ip, 2),
              SL_IPV4_BYTE (pNetAppEvent->Data.IpAcquiredV4.Ip, 1),
              SL_IPV4_BYTE (pNetAppEvent->Data.IpAcquiredV4.Ip, 0),
              SL_IPV4_BYTE (pNetAppEvent->Data.IpAcquiredV4.Gateway, 3),
              SL_IPV4_BYTE (pNetAppEvent->Data.IpAcquiredV4.Gateway, 2),
              SL_IPV4_BYTE (pNetAppEvent->Data.IpAcquiredV4.Gateway, 1),
              SL_IPV4_BYTE (pNetAppEvent->Data.IpAcquiredV4.Gateway, 0));

          pthread_attr_init (&pAttrs);
          priParam.sched_priority = 1;
          status = pthread_attr_setschedparam (&pAttrs, &priParam);
          status |= pthread_attr_setstacksize (&pAttrs, TASK_STACK_SIZE);

          if ((status = pthread_create(&httpThread, &pAttrs, httpTask, NULL)))
            printError (__func__, "pthread_create", status);
        }
      }
      break;

    default:
      UART_PRINT ("[NETAPP EVENT] Id=%d\n", pNetAppEvent->Id);
      break;
  }
}

/*!
  \brief          SimpleLinkFatalErrorEventHandler

  This handler gets called whenever a socket event is reported
  by the NWP / Host driver. After this routine is called, the user's
  application must restart the device in order to recover.

  \param          slFatalErrorEvent    -   pointer to fatal error event.

  \return         void

  \note           For more information, please refer to: user.h in the porting
  folder of the host driver and the  CC31xx/CC32xx NWP
  programmer's
  guide (SWRU455) section 17.9.

 */
void SimpleLinkFatalErrorEventHandler (SlDeviceFatal_t *slFatalErrorEvent)
{
  UART_PRINT ("[DEVICEFATAL EVENT] Id=%d\n", slFatalErrorEvent->Id);
}

/*!
  \brief          SimpleLinkNetAppRequestMemFreeEventHandler

  This handler gets called whenever the NWP is done handling with
  the buffer used in a NetApp request. This allows the use of
  dynamic memory with these requests.

  \param         pNetAppRequest     -   Pointer to NetApp request structure.

  \param         pNetAppResponse    -   Pointer to NetApp request Response.

  \note          For more information, please refer to: user.h in the porting
  folder of the host driver and the  CC31xx/CC32xx NWP
  programmer's
  guide (SWRU455) section 17.9.

  \return        void

 */
void SimpleLinkNetAppRequestMemFreeEventHandler (uint8_t *buffer)
{
  /* Unused in this application */
}

/*!
  \brief         SimpleLinkNetAppRequestEventHandler

  This handler gets called whenever a NetApp event is reported
  by the NWP / Host driver. User can write he's logic to handle
  the event here.

  \param         pNetAppRequest     -   Pointer to NetApp request structure.

  \param         pNetAppResponse    -   Pointer to NetApp request Response.

  \note          For more information, please refer to: user.h in the porting
  folder of the host driver and the  CC31xx/CC32xx NWP
  programmer's
  guide (SWRU455) section 17.9.

  \return         void

 */
void SimpleLinkNetAppRequestEventHandler (SlNetAppRequest_t *pNetAppRequest, SlNetAppResponse_t *pNetAppResponse)
{
  UART_PRINT ("[NETAPPREQUEST EVENT] AppID=%u, Type=%u\n", pNetAppRequest->AppId, pNetAppRequest->Type);
}

/*!
  \brief          SimpleLinkHttpServerEventHandler

  This handler gets called whenever a HTTP event is reported
  by the NWP internal HTTP server.

  \param          pHttpEvent       -   pointer to http event data.

  \param          pHttpEvent       -   pointer to http response.

  \return         void

  \note          For more information, please refer to: user.h in the porting
  folder of the host driver and the  CC31xx/CC32xx NWP
  programmer's
  guide (SWRU455) chapter 9.

 */
void SimpleLinkHttpServerEventHandler (SlNetAppHttpServerEvent_t *pHttpEvent, SlNetAppHttpServerResponse_t *pHttpResponse)
{
  UART_PRINT ("[HTTPSERVER EVENT] Event=%d\n", pHttpEvent->Event);
}

/*!
  \brief          SimpleLinkWlanEventHandler

  This handler gets called whenever a WLAN event is reported
  by the host driver / NWP. Here user can implement he's own logic
  for any of these events. This handler is used by 'network_terminal'
  application to show case the following scenarios:

  1. Handling connection / Disconnection.
  2. Handling Addition of station / removal.
  3. RX filter match handler.
  4. P2P connection establishment.

  \param          pWlanEvent       -   pointer to Wlan event data.

  \return         void

  \note          For more information, please refer to: user.h in the porting
  folder of the host driver and the  CC31xx/CC32xx
  NWP programmer's
  guide (SWRU455) sections 4.3.4, 4.4.5 and 4.5.5.

  \sa             cmdWlanConnectCallback, cmdEnableFilterCallback,
  cmdWlanDisconnectCallback,
  cmdP2PModecallback.

 */
void SimpleLinkWlanEventHandler (SlWlanEvent_t *pWlanEvent)
{
  UART_PRINT ("[WLAN EVENT] Id=%d\n", pWlanEvent->Id);
}

/*!
  \brief          SimpleLinkGeneralEventHandler

  This handler gets called whenever a general error is reported
  by the NWP / Host driver. Since these errors are not fatal,
  application can handle them.

  \param          pDevEvent    -   pointer to device error event.

  \return         void

  \note          For more information, please refer to: user.h in the porting
  folder of the host driver and the  CC31xx/CC32xx NWP
  programmer's
  guide (SWRU455) section 17.9.

 */
void SimpleLinkGeneralEventHandler (SlDeviceEvent_t *pDevEvent)
{
  UART_PRINT ("[GENERAL EVENT] ID=%d\n", pDevEvent->Id);
}

/*!
  \brief          SimpleLinkSockEventHandler

  This handler gets called whenever a socket event is reported
  by the NWP / Host driver.

  \param          SlSockEvent_t    -   pointer to socket event data.

  \return         void

  \note          For more information, please refer to: user.h in the porting
  folder of the host driver and the  CC31xx/CC32xx NWP
  programmer's
  guide (SWRU455) section 7.6.


 */
void SimpleLinkSockEventHandler (SlSockEvent_t *pSock)
{
  UART_PRINT ("[SOCK EVENT]: %d\n", pSock->Event);
}

void Connect (void)
{
  SlWlanSecParams_t secParams = {0};
  int16_t ret = 0;
  secParams.Key = (signed char*) SECURITY_KEY;
  secParams.KeyLen = strlen(SECURITY_KEY);
  secParams.Type = SECURITY_TYPE;

  UART_PRINT ("Connecting to : %s\n", SSID_NAME);

  if ((ret = sl_WlanConnect ((signed char *) SSID_NAME, strlen (SSID_NAME), 0, &secParams, 0)))
    printError (__func__, "sl_WlanConnect", ret);
}

static void DisplayBanner (char * AppName)
{
  UART_PRINT ("\n\n");
  UART_PRINT ("\t\t *************************************************\n");
  UART_PRINT ("\t\t            %s Application       \n", AppName);
  UART_PRINT ("\t\t *************************************************\n");
  UART_PRINT ("\n\n");
}

void mainThread (void *pvParameters)
{
  int32_t status = 0;
  pthread_attr_t pAttrs_spawn;
  struct sched_param priParam;

  SPI_init ();
  InitTerm ();

  DisplayBanner (APPLICATION_NAME);

  pthread_attr_init (&pAttrs_spawn);
  priParam.sched_priority = SPAWN_TASK_PRIORITY;
  status = pthread_attr_setschedparam (&pAttrs_spawn, &priParam);
  status |= pthread_attr_setstacksize (&pAttrs_spawn, SPAWN_STACK_SIZE);

  if ((status = pthread_create (&spawn_thread, &pAttrs_spawn, sl_Task, NULL)))
    printError (__func__, "pthread_create", status);

  if ((mode = sl_Start (0, 0, 0)) < 0)
    printError (__func__, "sl_Start", mode);

  if (mode != ROLE_STA)
  {
    if ((mode = sl_WlanSetMode (ROLE_STA)) < 0)
      printError (__func__, "sl_WlanSetMode", mode);

    if ((status = sl_Stop (SL_STOP_TIMEOUT)) < 0)
      printError (__func__, "sl_Stop", mode);

    if ((mode = sl_Start (0, 0, 0)) < 0)
      printError (__func__, "sl_Start", mode);
  }

  if (mode != ROLE_STA)
    printError (__func__, "Failed to configure device to it's default state", mode);

  Connect ();
}
