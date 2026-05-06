#include "usb_hid.h"
#include "xparameters.h"
#include <string.h>

#if defined(XPAR_SPI_USB_DEVICE_ID)
#  define USB_SPI_ID XPAR_SPI_USB_DEVICE_ID
#  define USB_HID_SPI_PRESENT
#elif defined(XPAR_SPI_0_DEVICE_ID)
#  define USB_SPI_ID XPAR_SPI_0_DEVICE_ID
#  define USB_HID_SPI_PRESENT
#endif

#ifdef USB_HID_SPI_PRESENT

#include <xspi.h>

/* -----------------------------------------------------------------------
 * Type aliases matching Lab 6's GenericTypeDefs.h
 * --------------------------------------------------------------------- */
typedef unsigned char  BYTE;
typedef unsigned short WORD;
typedef unsigned char  BOOL;
#define FALSE 0U
#define TRUE  1U

/* -----------------------------------------------------------------------
 * MAX3421E register map - pre-shifted (reg_addr << 3), from Lab 6's MAX3421E.h.
 * SPI command byte: reg | 0x02 = write,  reg = read.
 * --------------------------------------------------------------------- */
#define rRCVFIFO      0x08u  /* 1<<3  Receive FIFO */
#define rSNDFIFO      0x10u  /* 2<<3  Send FIFO */
#define rSUDFIFO      0x20u  /* 4<<3  Setup Data FIFO */
#define rRCVBC        0x30u  /* 6<<3  Receive Byte Count */
#define rSNDBC        0x38u  /* 7<<3  Send Byte Count */
#define rUSBIRQ       0x68u  /* 13<<3 USB Interrupt Request */
#define bmOSCOKIRQ    0x01u  /*       Oscillator OK */
#define rUSBCTL       0x78u  /* 15<<3 USB Control */
#define bmCHIPRES     0x20u  /*       Chip Reset */
#define rPINCTL       0x88u  /* 17<<3 Pin Control */
#define bmFDUPSPI     0x10u  /*       Full-duplex SPI */
#define bmINTLEVEL    0x08u  /*       INT level (1=active-high) */
#define bmGPXB        0x02u  /*       GPX = bus-activity */
#define rREVISION     0x90u  /* 18<<3 Revision */
#define rIOPINS1      0xa0u  /* 20<<3 IO Pins 1 */
#define bmGPOUT0      0x01u  /*       GPOUT0 (VBUS switch) */
#define rHIRQ         0xc8u  /* 25<<3 Host Interrupt Request */
#define bmBUSEVENTIRQ 0x01u  /*       Bus Reset / Resume done */
#define bmRCVDAVIRQ   0x04u  /*       Receive Data Available */
#define bmSNDBAVIRQ   0x08u  /*       Send Buffer Available */
#define bmCONDETIRQ   0x20u  /*       Connection Detect (bit 5) */
#define bmFRAMEIRQ    0x40u  /*       1ms SOF Frame (bit 6) */
#define bmHXFRDNIRQ   0x80u  /*       Host Transfer Done */
#define rHIEN         0xd0u  /* 26<<3 Host Interrupt Enable */
#define bmCONDETIE    0x20u
#define rMODE         0xd8u  /* 27<<3 Mode */
#define bmHOST        0x01u
#define bmLOWSPEED    0x02u
#define bmSOFKAENAB   0x08u
#define bmSEPIRQ      0x10u
#define bmDMPULLDN    0x40u
#define bmDPPULLDN    0x80u
#define MODE_FS_HOST  (bmDPPULLDN|bmDMPULLDN|bmHOST|bmSOFKAENAB)
#define MODE_LS_HOST  (bmDPPULLDN|bmDMPULLDN|bmHOST|bmLOWSPEED|bmSOFKAENAB)
#define rPERADDR      0xe0u  /* 28<<3 Peripheral Address */
#define rHCTL         0xe8u  /* 29<<3 Host Control */
#define bmBUSRST      0x01u
#define bmSAMPLEBUS   0x04u
#define bmRCVTOG0     0x10u
#define bmRCVTOG1     0x20u
#define bmSNDTOG0     0x40u
#define bmSNDTOG1     0x80u
#define bmRCVTOGRD    0x10u  /* HRSL: received toggle */
#define bmJSTATUS     0x80u  /* HRSL: J-state (full-speed device) */
#define bmKSTATUS     0x40u  /* HRSL: K-state (low-speed device) */
#define rHXFR         0xf0u  /* 30<<3 Host Transfer */
#define tokSETUP      0x10u
#define tokIN         0x00u
#define tokINHS       0x80u  /* IN handshake (status stage) */
#define tokOUTHS      0xa0u  /* OUT handshake (status stage) */
#define rHRSL         0xf8u  /* 31<<3 Host Result */
#define hrSUCCESS     0x00u
#define hrNAK         0x04u
#define hrSTALL       0x05u
#define hrTIMEOUT     0x0eu

/* -----------------------------------------------------------------------
 * USB protocol constants (from Lab 6's transfer.h / usb_ch9.h)
 * --------------------------------------------------------------------- */
#define bmREQ_GET_DESCR  0x80u  /* D2H | Standard | Device */
#define bmREQ_SET        0x00u  /* H2D | Standard | Device */
#define bmREQ_HIDOUT     0x21u  /* H2D | Class    | Interface */

#define USB_REQUEST_SET_ADDRESS       5u
#define USB_REQUEST_GET_DESCRIPTOR    6u
#define USB_REQUEST_SET_CONFIGURATION 9u
#define USB_DESCRIPTOR_DEVICE         0x01u
#define USB_DESCRIPTOR_CONFIGURATION  0x02u
#define USB_DESCRIPTOR_INTERFACE      0x04u
#define USB_DESCRIPTOR_ENDPOINT       0x05u

#define HID_REQUEST_SET_PROTOCOL  0x0bu
#define BOOT_PROTOCOL             0x00u
#define HID_INTF                  0x03u
#define BOOT_INTF_SUBCLASS        0x01u
#define HID_PROTOCOL_KEYBOARD     0x01u

#define CONF_DESCR_LEN 9u
#define DEV_DESCR_LEN  18u

/* Transfer retry limits matching Lab 6's project_config.h */
#define USB_NAK_LIMIT   200u
#define USB_RETRY_LIMIT 3u

/*
 * Replaces XTmrCtr-based timeout from transfer.c.
 * At 100 MHz MicroBlaze, each SPI rd takes ~20 cycles; 500k iters >= 100 ms.
 */
#define USB_XFER_TIMEOUT_ITERS 500000u

/* -----------------------------------------------------------------------
 * Descriptor structs (minimal subset of usb_ch9.h)
 * bLength + bDescriptorType are at offset 0 in every descriptor, so
 * accessing via the config member is safe for any descriptor type.
 * --------------------------------------------------------------------- */
typedef struct {
    BYTE bLength; BYTE bDescriptorType;
    WORD bcdUSB; BYTE bDeviceClass; BYTE bDeviceSubClass;
    BYTE bDeviceProtocol; BYTE bMaxPacketSize0;
    WORD idVendor; WORD idProduct; WORD bcdDevice;
    BYTE iManufacturer; BYTE iProduct; BYTE iSerialNumber;
    BYTE bNumConfigurations;
} USB_DEV_DESCR;

typedef struct {
    BYTE bLength; BYTE bDescriptorType;
    WORD wTotalLength; BYTE bNumInterfaces; BYTE bConfigurationValue;
    BYTE iConfiguration; BYTE bmAttributes; BYTE bMaxPower;
} USB_CFG_DESCR;

typedef struct {
    BYTE bLength; BYTE bDescriptorType;
    BYTE bInterfaceNumber; BYTE bAlternateSetting; BYTE bNumEndpoints;
    BYTE bInterfaceClass; BYTE bInterfaceSubClass; BYTE bInterfaceProtocol;
    BYTE iInterface;
} USB_INTF_DESCR;

typedef struct {
    BYTE bLength; BYTE bDescriptorType;
    BYTE bEndpointAddress; BYTE bmAttributes;
    WORD wMaxPacketSize; BYTE bInterval;
} USB_EP_DESCR;

/* Union for easy descriptor parsing (from usb_ch9.h USB_DESCR) */
typedef union {
    BYTE          buf[80];
    USB_CFG_DESCR config;   /* .bLength always valid regardless of type */
    USB_INTF_DESCR iface;
    USB_EP_DESCR   ep;
} USB_DESCR;

/* USB setup packet - 8 bytes, matches Lab 6's Chapter 9 layout */
typedef struct {
    BYTE bmRequestType; BYTE bRequest;
    BYTE wValueLo; BYTE wValueHi;
    WORD wIndex; WORD wLength;
} SETUP_PKT;

/* Boot keyboard report (from HID.h BOOT_KBD_REPORT) */
typedef struct { BYTE mod; BYTE reserved; BYTE keycode[6]; } BOOT_KBD_REPORT;

/* Endpoint record (from transfer.h EP_RECORD) */
typedef struct {
    BYTE epAddr; BYTE Attr; WORD MaxPktSize;
    BYTE Interval; BYTE sndToggle; BYTE rcvToggle;
} EP_RECORD;

/* Device record (from transfer.h DEV_RECORD) */
typedef struct { EP_RECORD *epinfo; BYTE devclass; } DEV_RECORD;

/* -----------------------------------------------------------------------
 * Module state
 * Slots: 0 = addr-0 device during enumeration, 1 = keyboard after.
 * --------------------------------------------------------------------- */
#define USB_NUMDEVICES 2u
static XSpi       s_spi;
static EP_RECORD  s_dev0ep;
static EP_RECORD  s_hid_ep[2];   /* [0]=ctrl EP0, [1]=interrupt-IN */
static DEV_RECORD s_devtable[USB_NUMDEVICES + 1u];
static BYTE       s_hid_addr      = 0u;
static BYTE       s_hid_interface = 0u;

/* HID keycodes (from HID.h / HID.c) */
#define HID_LEFT  0x50u
#define HID_RIGHT 0x4fu
#define HID_SPACE 0x2cu
#define HID_ENTER 0x28u
#define HID_R     0x15u

static uint8_t s_left_held  = 0u;
static uint8_t s_right_held = 0u;
static uint8_t s_space_edge = 0u;
static uint8_t s_enter_edge = 0u;
static uint8_t s_r_edge     = 0u;
static BYTE    s_prev_keys[6] = {0u};

/* -----------------------------------------------------------------------
 * SPI primitives  (from MAX3421E.c: MAXreg_wr/rd, MAXbytes_wr/rd)
 * Uses XSP_MANUAL_SSELECT_OPTION; SetSlaveSelect called per transfer,
 * exactly as in the proven lab driver.
 * --------------------------------------------------------------------- */
static void MAXreg_wr(BYTE reg, BYTE val)
{
    BYTE wr[2] = { (BYTE)(reg | 0x02u), val }, rd[2];
    XSpi_SetSlaveSelect(&s_spi, 1u);
    XSpi_Transfer(&s_spi, wr, rd, 2u);
}

static BYTE MAXreg_rd(BYTE reg)
{
    BYTE wr[2] = { reg, 0x00u }, rd[2] = {0u, 0u};
    XSpi_SetSlaveSelect(&s_spi, 1u);
    XSpi_Transfer(&s_spi, wr, rd, 2u);
    return rd[1];
}

static void MAXbytes_wr(BYTE reg, BYTE nbytes, BYTE *data)
{
    BYTE wr[257], rd[257];
    BYTE i;
    wr[0] = (BYTE)(reg | 0x02u);
    for (i = 0u; i < nbytes; i++) wr[i + 1u] = data[i];
    XSpi_SetSlaveSelect(&s_spi, 1u);
    XSpi_Transfer(&s_spi, wr, rd, (unsigned int)nbytes + 1u);
}

static void MAXbytes_rd(BYTE reg, BYTE nbytes, BYTE *data)
{
    BYTE wr[257], rd[257];
    BYTE i;
    wr[0] = reg;
    for (i = 0u; i < nbytes; i++) wr[i + 1u] = 0x00u;
    XSpi_SetSlaveSelect(&s_spi, 1u);
    XSpi_Transfer(&s_spi, wr, rd, (unsigned int)nbytes + 1u);
    for (i = 0u; i < nbytes; i++) data[i] = rd[i + 1u];
}

/* -----------------------------------------------------------------------
 * XferDispatchPkt  (from transfer.c)
 * XTmrCtr timeout replaced with an iteration counter.
 * --------------------------------------------------------------------- */
static BYTE XferDispatchPkt(BYTE token, BYTE ep)
{
    BYTE     rcode;
    BYTE     nak_count   = 0u;
    BYTE     retry_count = 0u;
    unsigned int t;

    while (1) {
        MAXreg_wr(rHXFR, (BYTE)(token | ep));
        rcode = 0xffu;
        for (t = 0u; t < USB_XFER_TIMEOUT_ITERS; t++) {
            if (MAXreg_rd(rHIRQ) & bmHXFRDNIRQ) {
                MAXreg_wr(rHIRQ, bmHXFRDNIRQ);
                rcode = 0x00u;
                break;
            }
        }
        if (rcode != 0x00u) return rcode;

        rcode = (BYTE)(MAXreg_rd(rHRSL) & 0x0fu);
        if (rcode == hrNAK) {
            if (++nak_count >= USB_NAK_LIMIT) break;
            continue;
        }
        if (rcode == hrTIMEOUT) {
            if (++retry_count >= USB_RETRY_LIMIT) break;
            continue;
        }
        break;
    }
    return rcode;
}

/* -----------------------------------------------------------------------
 * XferInTransfer  (from transfer.c)
 * 'ep' is both the epinfo[] index and the USB endpoint number in HXFR.
 * Works because HID keyboards always use EP1 for interrupt-IN.
 * --------------------------------------------------------------------- */
static BYTE XferInTransfer(BYTE addr, BYTE ep,
                            WORD nbytes, BYTE *data, BYTE maxpktsize)
{
    BYTE rcode;
    BYTE pktsize;
    WORD xfrlen = 0u;

    MAXreg_wr(rHCTL, s_devtable[addr].epinfo[ep].rcvToggle);
    while (1) {
        rcode = XferDispatchPkt(tokIN, ep);
        if (rcode) return rcode;
        if ((MAXreg_rd(rHIRQ) & bmRCVDAVIRQ) == 0u) return 0xf0u;
        pktsize = MAXreg_rd(rRCVBC);
        MAXbytes_rd(rRCVFIFO, pktsize, data);
        data   += pktsize;
        MAXreg_wr(rHIRQ, bmRCVDAVIRQ);
        xfrlen += pktsize;
        if (pktsize < maxpktsize || xfrlen >= nbytes) {
            s_devtable[addr].epinfo[ep].rcvToggle =
                (MAXreg_rd(rHRSL) & bmRCVTOGRD) ? bmRCVTOG1 : bmRCVTOG0;
            return 0u;
        }
    }
}

/* -----------------------------------------------------------------------
 * XferCtrlReq  (from transfer.c: XferCtrlReq + XferCtrlData + XferCtrlND)
 * --------------------------------------------------------------------- */
static BYTE XferCtrlReq(BYTE addr, BYTE ep,
                         BYTE bmReqType, BYTE bRequest,
                         BYTE wValLo, BYTE wValHi,
                         WORD wInd, WORD nbytes, BYTE *dataptr)
{
    SETUP_PKT pkt;
    BYTE      rcode;

    pkt.bmRequestType = bmReqType;
    pkt.bRequest      = bRequest;
    pkt.wValueLo      = wValLo;
    pkt.wValueHi      = wValHi;
    pkt.wIndex        = wInd;
    pkt.wLength       = nbytes;

    MAXreg_wr(rPERADDR, addr);
    MAXbytes_wr(rSUDFIFO, 8u, (BYTE *)&pkt);
    rcode = XferDispatchPkt(tokSETUP, ep);
    if (rcode) return rcode;

    if (dataptr == NULL) {
        /* No data stage: status IN (device ACKs the request) */
        return XferDispatchPkt(tokINHS, ep);
    }
    if (bmReqType & 0x80u) {
        /* Data stage IN, then status OUT */
        s_devtable[addr].epinfo[ep].rcvToggle = bmRCVTOG1;
        rcode = XferInTransfer(addr, ep, nbytes, dataptr,
                               s_devtable[addr].epinfo[ep].MaxPktSize);
        if (rcode) return rcode;
        return XferDispatchPkt(tokOUTHS, ep);
    }
    return 0xffu;
}

/* Convenience macros matching transfer.h */
#define XferGetDevDescr(addr, nb, dp) \
    XferCtrlReq(addr, 0u, bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, \
                0x00u, USB_DESCRIPTOR_DEVICE, 0u, nb, dp)
#define XferGetConfDescr(addr, nb, conf, dp) \
    XferCtrlReq(addr, 0u, bmREQ_GET_DESCR, USB_REQUEST_GET_DESCRIPTOR, \
                (BYTE)(conf), USB_DESCRIPTOR_CONFIGURATION, 0u, nb, dp)
#define XferSetAddr(newaddr) \
    XferCtrlReq(0u, 0u, bmREQ_SET, USB_REQUEST_SET_ADDRESS, \
                newaddr, 0x00u, 0u, 0u, NULL)
#define XferSetConf(addr, conf) \
    XferCtrlReq(addr, 0u, bmREQ_SET, USB_REQUEST_SET_CONFIGURATION, \
                (BYTE)(conf), 0x00u, 0u, 0u, NULL)
#define XferSetProto(addr, intf, proto) \
    XferCtrlReq(addr, 0u, bmREQ_HIDOUT, HID_REQUEST_SET_PROTOCOL, \
                (BYTE)(proto), 0x00u, (WORD)(intf), 0u, NULL)

/* -----------------------------------------------------------------------
 * enumerate_keyboard  (from HID.c HIDKProbe, adapted)
 * Reads the full configuration descriptor, finds the HID boot-keyboard
 * interface and its interrupt-IN endpoint, then sets configuration and
 * boot protocol.  Fills s_hid_ep[1] and s_hid_interface on success.
 * --------------------------------------------------------------------- */
static BOOL enumerate_keyboard(BYTE addr)
{
    BYTE      rcode;
    BYTE      confvalue;
    WORD      total_length;
    BYTE      bigbuf[256];
    BYTE     *bp;
    USB_DESCR *dp;

    rcode = XferGetConfDescr(addr, CONF_DESCR_LEN, 0u, bigbuf);
    if (rcode) return FALSE;

    total_length = ((USB_CFG_DESCR *)bigbuf)->wTotalLength;
    if (total_length > 256u) total_length = 256u;
    confvalue    = ((USB_CFG_DESCR *)bigbuf)->bConfigurationValue;

    rcode = XferGetConfDescr(addr, total_length, 0u, bigbuf);
    if (rcode) return FALSE;

    bp = bigbuf;
    while (bp < bigbuf + total_length) {
        dp = (USB_DESCR *)bp;
        if (dp->config.bLength == 0u) break;

        if (dp->config.bDescriptorType != USB_DESCRIPTOR_INTERFACE) {
            bp += dp->config.bLength;
            continue;
        }
        /* Interface descriptor: must be HID boot keyboard */
        if (dp->iface.bInterfaceClass    != HID_INTF          ||
            dp->iface.bInterfaceSubClass != BOOT_INTF_SUBCLASS ||
            dp->iface.bInterfaceProtocol != HID_PROTOCOL_KEYBOARD)
            return FALSE;

        s_hid_interface = dp->iface.bInterfaceNumber;
        bp += dp->config.bLength;

        /* Scan for the interrupt-IN endpoint descriptor */
        while (bp < bigbuf + total_length) {
            dp = (USB_DESCR *)bp;
            if (dp->config.bLength == 0u) break;
            if (dp->config.bDescriptorType == USB_DESCRIPTOR_ENDPOINT) {
                s_hid_ep[1].epAddr     = (BYTE)(dp->ep.bEndpointAddress & 0x0fu);
                s_hid_ep[1].Attr       = dp->ep.bmAttributes;
                s_hid_ep[1].MaxPktSize = dp->ep.wMaxPacketSize;
                s_hid_ep[1].Interval   = dp->ep.bInterval;
                s_hid_ep[1].rcvToggle  = bmRCVTOG0;
                s_hid_ep[1].sndToggle  = bmSNDTOG0;

                rcode = XferSetConf(addr, confvalue);
                if (rcode) return FALSE;
                rcode = XferSetProto(addr, s_hid_interface, BOOT_PROTOCOL);
                return (rcode == 0u) ? TRUE : FALSE;
            }
            bp += dp->config.bLength;
        }
        return FALSE;
    }
    return FALSE;
}

/* -----------------------------------------------------------------------
 * connect_and_enumerate  (from USB_Task ATTACHED substates)
 * Runs once synchronously when a device is first detected.
 * --------------------------------------------------------------------- */
static BYTE connect_and_enumerate(void)
{
    BYTE         rcode;
    BYTE         busstate;
    USB_DEV_DESCR devdescr;
    unsigned int  i;

    /* Probe bus speed; set host mode accordingly (from MAX_busprobe) */
    busstate = (BYTE)(MAXreg_rd(rHRSL) & (bmJSTATUS | bmKSTATUS));
    MAXreg_wr(rMODE, (busstate == bmJSTATUS) ? MODE_FS_HOST : MODE_LS_HOST);

    /* Settle ~200 ms (from USB_ATTACHED_SUBSTATE_SETTLE) */
    for (i = 0u; i < 200000u; i++) { (void)MAXreg_rd(rHIRQ); }

    /* Bus reset (USB_ATTACHED_SUBSTATE_RESET_DEVICE) */
    MAXreg_wr(rHIRQ, bmBUSEVENTIRQ);
    MAXreg_wr(rHCTL, bmBUSRST);

    /* Wait for reset complete (USB_ATTACHED_SUBSTATE_WAIT_RESET_COMPLETE) */
    for (i = 0u; i < 1000000u; i++) {
        if ((MAXreg_rd(rHCTL) & bmBUSRST) == 0u) break;
    }

    /* Enable SOF, wait for first frame (USB_ATTACHED_SUBSTATE_WAIT_SOF) */
    MAXreg_wr(rMODE, (BYTE)(MAXreg_rd(rMODE) | bmSOFKAENAB));
    for (i = 0u; i < 1000000u; i++) {
        if (MAXreg_rd(rHIRQ) & bmFRAMEIRQ) break;
    }

    /* GET_DESCRIPTOR(Device, 8) at addr 0 to learn MaxPktSize0 */
    s_devtable[0].epinfo->MaxPktSize = 8u;
    rcode = XferGetDevDescr(0u, 8u, (BYTE *)&devdescr);
    if (rcode) return rcode;
    s_devtable[0].epinfo->MaxPktSize = devdescr.bMaxPacketSize0;

    /* Set up addr-1 slot before issuing SET_ADDRESS (from USB_Task) */
    s_hid_addr          = 1u;
    s_devtable[1].epinfo = s_devtable[0].epinfo;

    rcode = XferSetAddr(1u);
    if (rcode) return rcode;

    /* USB spec: >= 2 ms before first transaction at new address */
    for (i = 0u; i < 200000u; i++) { (void)MAXreg_rd(rHIRQ); }

    /* Enumerate as HID boot keyboard (HIDKProbe) */
    if (!enumerate_keyboard(s_hid_addr)) return 0xffu;

    /* Switch addr-1 endpoint table to the HID records */
    s_devtable[s_hid_addr].epinfo = s_hid_ep;
    return 0x00u;
}

/* -----------------------------------------------------------------------
 * parse_report  (from HID.c kbdPoll + main loop key-state logic)
 * --------------------------------------------------------------------- */
static void parse_report(const BOOT_KBD_REPORT *r)
{
    BYTE i, k, found, j;

    s_left_held  = 0u;
    s_right_held = 0u;
    for (i = 0u; i < 6u; i++) {
        if (r->keycode[i] == HID_LEFT)  s_left_held  = 1u;
        if (r->keycode[i] == HID_RIGHT) s_right_held = 1u;
    }
    for (i = 0u; i < 6u; i++) {
        k = r->keycode[i];
        if (k == 0u) continue;
        found = 0u;
        for (j = 0u; j < 6u; j++) {
            if (s_prev_keys[j] == k) { found = 1u; break; }
        }
        if (!found) {
            if (k == HID_SPACE) s_space_edge = 1u;
            if (k == HID_ENTER) s_enter_edge = 1u;
            if (k == HID_R)     s_r_edge     = 1u;
        }
    }
    memcpy(s_prev_keys, r->keycode, 6u);
}

/* -----------------------------------------------------------------------
 * Driver state machine
 * --------------------------------------------------------------------- */
typedef enum { ST_DISCONNECTED, ST_RUNNING, ST_ERROR } drv_state_t;
static drv_state_t s_drv_state = ST_DISCONNECTED;

/* -----------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------- */
int usb_hid_init(void)
{
    XSpi_Config *cfg;
    int          status;
    unsigned int i;

    cfg = XSpi_LookupConfig(USB_SPI_ID);
    if (!cfg) return -1;

    status = XSpi_CfgInitialize(&s_spi, cfg, cfg->BaseAddress);
    if (status != XST_SUCCESS) return -1;

    /* XSP_MANUAL_SSELECT_OPTION: SS managed per-transfer, as in lw_usb */
    status = XSpi_SetOptions(&s_spi, XSP_MASTER_OPTION | XSP_MANUAL_SSELECT_OPTION);
    if (status != XST_SUCCESS) return -1;

    XSpi_Start(&s_spi);
    XSpi_IntrGlobalDisable(&s_spi);

    /* Full-duplex SPI, active-high INT, GPX = bus-activity (from MAX3421E_init) */
    MAXreg_wr(rPINCTL, (BYTE)(bmFDUPSPI | bmINTLEVEL | bmGPXB));

    /* Software chip reset, wait for oscillator (from MAX3421E_reset) */
    MAXreg_wr(rUSBCTL, bmCHIPRES);
    MAXreg_wr(rUSBCTL, 0x00u);
    for (i = 0u; i < 1000000u; i++) {
        if (MAXreg_rd(rUSBIRQ) & bmOSCOKIRQ) break;
    }

    /* VBUS power on via GPOUT0 (from Vbus_power(ON)) */
    MAXreg_wr(rIOPINS1, (BYTE)(MAXreg_rd(rIOPINS1) | bmGPOUT0));

    /* Host mode: pull-downs, separate GPX IRQ; SOF added after connect */
    MAXreg_wr(rMODE, (BYTE)(bmDPPULLDN | bmDMPULLDN | bmHOST | bmSEPIRQ));
    MAXreg_wr(rHIEN, bmCONDETIE);
    MAXreg_wr(rHCTL, bmSAMPLEBUS);
    MAXreg_wr(rHIRQ, bmCONDETIRQ);  /* clear any stale event */

    /* Initialize device table (from USB_init) */
    s_dev0ep.MaxPktSize = 8u;
    s_dev0ep.sndToggle  = bmSNDTOG0;
    s_dev0ep.rcvToggle  = bmRCVTOG0;
    s_devtable[0].epinfo   = &s_dev0ep;
    s_devtable[0].devclass = 0u;
    s_devtable[1].epinfo   = NULL;
    s_devtable[1].devclass = 0u;

    s_drv_state = ST_DISCONNECTED;
    return 0;
}

void usb_hid_poll(void)
{
    BYTE hirq = MAXreg_rd(rHIRQ);

    if (s_drv_state == ST_DISCONNECTED) {
        if (hirq & bmCONDETIRQ) {
            MAXreg_wr(rHIRQ, bmCONDETIRQ);
            /* J or K state means a device is present; SE0 means nothing */
            if ((MAXreg_rd(rHRSL) & (bmJSTATUS | bmKSTATUS)) != 0u) {
                BYTE err = connect_and_enumerate();
                s_drv_state = (err == 0x00u) ? ST_RUNNING : ST_ERROR;
            }
        }
        return;
    }

    if (s_drv_state == ST_ERROR) return;

    /* Check for disconnect */
    if (hirq & bmCONDETIRQ) {
        MAXreg_wr(rHIRQ, bmCONDETIRQ);
        if ((MAXreg_rd(rHRSL) & (bmJSTATUS | bmKSTATUS)) == 0u) {
            s_left_held = s_right_held = 0u;
            s_space_edge = s_enter_edge = s_r_edge = 0u;
            memset(s_prev_keys, 0u, 6u);
            s_drv_state = ST_DISCONNECTED;
            return;
        }
    }

    /* Poll keyboard: one interrupt-IN per frame (from kbdPoll) */
    {
        BOOT_KBD_REPORT report;
        BYTE            addr  = s_hid_addr;
        BYTE            rcode;
        MAXreg_wr(rPERADDR, addr);
        rcode = XferInTransfer(addr, 1u, 8u, (BYTE *)&report,
                               s_devtable[addr].epinfo[1u].MaxPktSize);
        if (rcode == hrSUCCESS) {
            parse_report(&report);
        } else if (rcode == hrNAK) {
            /* No new data this frame; held state unchanged, no new edges */
        } else {
            s_drv_state = ST_ERROR;
        }
    }
}

uint8_t usb_hid_left_held(void)  { return s_left_held;  }
uint8_t usb_hid_right_held(void) { return s_right_held; }

uint8_t usb_hid_space_pressed(void)
    { uint8_t v = s_space_edge; s_space_edge = 0u; return v; }
uint8_t usb_hid_enter_pressed(void)
    { uint8_t v = s_enter_edge; s_enter_edge = 0u; return v; }
uint8_t usb_hid_r_pressed(void)
    { uint8_t v = s_r_edge; s_r_edge = 0u; return v; }

#else  /* !USB_HID_SPI_PRESENT -- stubs; game works via GPIO buttons only */

int     usb_hid_init(void)          { return -1; }
void    usb_hid_poll(void)          {}
uint8_t usb_hid_left_held(void)     { return 0u; }
uint8_t usb_hid_right_held(void)    { return 0u; }
uint8_t usb_hid_space_pressed(void) { return 0u; }
uint8_t usb_hid_enter_pressed(void) { return 0u; }
uint8_t usb_hid_r_pressed(void)     { return 0u; }

#endif /* USB_HID_SPI_PRESENT */
