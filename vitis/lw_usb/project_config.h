/* Project name project configuration file	*/

#ifndef _project_config_h_
#define _project_config_h_

//#include "../../../../lab6_2/lab6_2_workspace/mb_usb/src/lw_usb/GenericMacros.h"
//#include "../../../../lab6_2/lab6_2_workspace/mb_usb/src/lw_usb/GenericTypeDefs.h"
//#include "../../../../lab6_2/lab6_2_workspace/mb_usb/src/lw_usb/HID.h"
//#include "../../../../lab6_2/lab6_2_workspace/mb_usb/src/lw_usb/MAX3421E.h"
//#include "../../../../lab6_2/lab6_2_workspace/mb_usb/src/lw_usb/transfer.h"
//#include "../../../../lab6_2/lab6_2_workspace/mb_usb/src/lw_usb/usb_ch9.h"
//#include "../../../../lab6_2/lab6_2_workspace/mb_usb/src/lw_usb/USB.h"

#include "GenericMacros.h"
#include "GenericTypeDefs.h"
#include "HID.h"
#include "MAX3421E.h"
#include "transfer.h"
#include "usb_ch9.h"
#include "USB.h"

/* USB constants */
/* time in milliseconds */
#define USB_SETTLE_TIME 					200         //USB settle after reset
#define USB_XFER_TIMEOUT					5000        //USB transfer timeout

#define USB_NAK_LIMIT 200
#define USB_RETRY_LIMIT 3

#endif // _project_config_h
