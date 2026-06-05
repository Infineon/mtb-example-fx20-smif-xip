/***************************************************************************//**
 * \file cy_usb_descriptors.c
 * \version 1.0
 *
 * This file contains the USB descriptors for the FX SMIF XIP application
 *
 *******************************************************************************
 * \copyright
 * (c) (2026), Cypress Semiconductor Corporation (an Infineon company) or
 * an affiliate of Cypress Semiconductor Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *******************************************************************************/

#include "cy_usb_descriptors.h"
#include "cy_pdl.h"
#include "cy_usb_app.h"
#include "cy_usb_common.h"
#include "cy_usb_usbd.h"
#include "cy_usbhs_cal_drv.h"


/* Standard device descriptor for USB 3.1 */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSB30DeviceDscr[]
__attribute__((aligned(32))) = {
    0x12,		/* Descriptor size */
    0x01,		/* Device descriptor type */
    0x20, 0x03, /* USB 3.2 */
    0xEF,		/* Device class */
    0x02,		/* Device Sub-class */
    0x01,		/* Device protocol */
    0x09,		/* Maxpacket size for EP0 : 2^9 */
    CY_USB_GET_LSB(CY_USB_VENDOR_ID),
    CY_USB_GET_MSB(CY_USB_VENDOR_ID), /* Vendor ID */
    CY_USB_GET_LSB(CY_USB_PRODUCT_ID),
    CY_USB_GET_MSB(CY_USB_PRODUCT_ID), /* Product ID */
    0x00, 0x00, /* Device release number */
    0x01,		/* Manufacture string index */
    0x02,		/* Product string index */
    0x00,		/* Serial number string index */
    0x01		/* Number of configurations */
};

/* Standard device descriptor for USB 2.0 */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSB20DeviceDscr[]
__attribute__((aligned(32))) = {
    0x12,		/* Descriptor size */
    0x01,		/* Device descriptor type */
    0x00, 0x02, /* USB 2.00 */
    0x00,		/* Device class */
    0x00,		/* Device sub-class */
    0x00,		/* Device protocol */
    0x40,		/* Maxpacket size for EP0 : 64 bytes */
    CY_USB_GET_LSB(CY_USB_VENDOR_ID),
    CY_USB_GET_MSB(CY_USB_VENDOR_ID), /* Vendor ID */
    CY_USB_GET_LSB(CY_USB_PRODUCT_ID),
    CY_USB_GET_MSB(CY_USB_PRODUCT_ID), /* Product ID */
    0x00, 0x00, /* Device release number */
    0x01,		/* Manufacture string index */
    0x02,		/* Product string index */
    0x00,		/* Serial number string index */
    0x01		/* Number of configurations */
};

/* Binary device object store descriptor for USB 2.x operation. */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBBOSDscr_HS[]
__attribute__((aligned(32))) = {
    0x05,		/* Descriptor size */
    0x0F,		/* BOS descriptor type */
    0x0C, 0x00, /* Length of this descriptor and all sub descriptors */
    0x01,		/* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07,					/* Descriptor size */
    0x10,					/* Device capability type descriptor */
    CY_USB2_EXTN_CAPB_TYPE, /* USB 2.0 extension capability type */
    0x02, 0x00, 0x00,
    0x00 /* Supported device level features: LPM support  */
};

/* Binary device object store descriptor for SuperSpeed and SuperSpeedPlus
 * operation. */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBBOSDscr_SS[]
__attribute__((aligned(32))) = {
    0x05,		/* Descriptor size */
    0x0F,		/* BOS descriptor type */
    0x16, 0x00, /* Length of this descriptor and all sub descriptors */
    0x02,		/* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07,					/* Descriptor size */
    0x10,					/* Device capability type descriptor */
    CY_USB2_EXTN_CAPB_TYPE, /* USB 2.0 extension capability type */
    0x02, 0x00, 0x00,
    0x00, /* Supported device level features: LPM support  */

    /* SuperSpeed device capability */
    0x0A,				 /* Descriptor size */
    0x10,				 /* Device capability type descriptor */
    CY_SS_USB_CAPB_TYPE, /* SuperSpeed device capability type */
    0x00,				 /* Supported device level features  */
    0x0E, 0x00, /* Speeds supported by the device : SS, HS and FS */
    0x03,		/* Functionality support */
    0x00,		/* U1 Device Exit latency */
    0x00, 0x00	/* U2 Device Exit latency */
};

/* Binary Object Store (BOS) Descriptor to be used in SuperSpeedPlus connection.
*/
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBBOSDscr_SSP[] __attribute__((
            aligned(32))) = {
    0x05,		/* Descriptor size */
    0x0F,		/* BOS descriptor. */
    0x2A, 0x00, /* Length of this descriptor and all sub-descriptors */
    0x03,		/* Number of device capability descriptors */

    /* USB 2.0 extension */
    0x07, /* Descriptor size */
    0x10, /* Device capability type descriptor */
    0x02, /* USB 2.0 extension capability type */
    0x1E, 0x64, 0x00,
    0x00, /* Supported device level features: LPM support, BESL supported,
             Baseline BESL=400 us, Deep BESL=1000 us. */

    /* SuperSpeed device capability */
    0x0A,		/* Descriptor size */
    0x10,		/* Device capability type descriptor */
    0x03,		/* SuperSpeed device capability type */
    0x00,		/* Supported device level features: Not LTM capable.  */
    0x0E, 0x00, /* Speeds supported by the device : SS Gen1, HS and FS */
    0x03,		/* Functionality support */
    0x0A,		/* U1 Device Exit latency */
    0xFF, 0x07, /* U2 Device Exit latency */

    /* SuperSpeedPlus USB device capability */
    0x14,					/* Descriptor size */
    0x10,					/* Device capability type descriptor */
    0x0A,					/* SuperSpeedPlus Device capability */
    0x00,					/* Reserved */
    0x01, 0x00, 0x00, 0x00, /* SSAC=1, SSIC=0 */
    0x00, 0x11,				/* SSID=0, Min. RX Lane = 1, Min. Tx Lane = 1 */
    0x00, 0x00,				/* Reserved */
    0x30, 0x40, 0x0A,
    0x00, /* SSID=0, LSE=3(Gb/s), ST=0(Symmetric Rx), LP=1(SSPlus), LSM=10 */
    0xB0, 0x40, 0x0A,
    0x00 /* SSID=0, LSE=3(Gb/s), ST=0(Symmetric Tx), LP=1(SSPlus), LSM=10 */
};

/* Standard device qualifier descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBDeviceQualDscr[]
__attribute__((aligned(32))) = {
    0x0A,		/* Descriptor size */
    0x06,		/* Device qualifier descriptor type */
    0x00, 0x02, /* USB 2.0 */
    0x02,		/* Device class */
    0x00,		/* Device sub-class */
    0x00,		/* Device protocol */
    0x08,		/* Maxpacket size for EP0 */
    0x01,		/* Number of configurations */
    0x00		/* Reserved */
};

/* Standard super speed configuration descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBSSConfigDscr[] __attribute__((
            aligned(32))) = {
    /* Configuration descriptor */
    0x09,		/* Descriptor size */
    0x02,		/* Configuration descriptor type */
    0x12, 0x00, /* Length of this descriptor and all sub descriptors */
    0x01,		/* Number of interfaces */
    0x01,		/* Configuration number */
    0x00,		/* Configuration string index */
#if REMOTEWAKE_EN
    0xA0, /* Config characteristics - bus powered with remote wake support. */
#else
    0x80, /* Config characteristics - bus powered without remote wake support*/
#endif	  /* REMOTEWAKE_EN */
    0x32, /* Max power consumption of device (in 8mA unit) : 400mA */

    /* Interface descriptor */
    0x09,				  /* Descriptor size */
    0x04,				  /* Interface Descriptor type */
    VENDOR_INTERFACE_NUM, /* Interface number */
    0x00,				  /* Alternate setting number */
    0x00,				  /* Number of end points */
    0xFF,				  /* Interface class */
    0x00,				  /* Interface sub class */
    0x00,				  /* Interface protocol code */
    0x00,				  /* Interface descriptor string index */
};

/* Standard high speed configuration descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBHSConfigDscr[]
__attribute__((aligned(32))) = {
    /* Configuration descriptor */
    0x09,		/* Descriptor size */
    0x02,		/* Configuration descriptor type */
    0x12, 0x00, /* Length of this descriptor and all sub descriptors */
    0x01,		/* Number of interfaces */
    0x01,		/* Configuration number */
    0x00,		/* Configuration string index */
    0x80,		/* Config characteristics - bus powered without remote wake
                   support. */
    0x32,		/* Max power consumption of device (in 2mA unit) : 100mA */

    /* Interface descriptor */
    0x09, /* Descriptor size */
    0x04, /* Interface descriptor type */
    0x00, /* Interface number */
    0x00, /* Alternate setting number */
    0x00, /* Number of endpoints */
    0xFF, /* Interface class */
    0x00, /* Interface sub class */
    0x00, /* Interface protocol code */
    0x00, /* Interface descriptor string index */
};

/* Standard full speed configuration descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBFSConfigDscr[]
__attribute__((aligned(32))) = {
    /* Configuration descriptor */
    0x09,		/* Descriptor size */
    0x02,		/* Configuration descriptor type */
    0x12, 0x00, /* Length of this descriptor and all sub descriptors. */
    0x01,		/* Number of interfaces */
    0x01,		/* Configuration number */
    0x00,		/* COnfiguration string index */
    0x80,		/* Config characteristics - bus powered without remote wake
                   support. */
    0x32,		/* Max power consumption of device (in 2mA unit) : 100mA */

    /* Interface descriptor */
    0x09, /* Descriptor size */
    0x04, /* Interface descriptor type */
    0x00, /* Interface number */
    0x00, /* Alternate setting number */
    0x00, /* Number of endpoints */
    0xFF, /* Interface class */
    0x00, /* Interface sub class */
    0x00, /* Interface protocol code */
    0x00, /* Interface descriptor string index */
};

/* Standard language ID string descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBStringLangIDDscr[]
__attribute__((aligned(32))) = {
    0x04,	   /* Descriptor size */
    0x03,	   /* Device descriptor type */
    0x09, 0x04 /* Language ID supported */
};

/* Standard manufacturer string descriptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBManufactureDscr[32]
__attribute__((aligned(32))) = {0x08, 0x03, 'I', 0x00,
    'F',  0x00, 'X', 0x00};

/* Standard Product String desciptor */
USB3_DESC_ATTRIBUTES uint8_t CyFxUSBProductDscr[32]
__attribute__((aligned(32))) = {
    0x18, 0x03, 'F', 0x00, 'X', 0x00, '-', 0x00, 'S', 0x00, 'M', 0x00, 
    'I', 0x00, 'F', 0x00, '-', 0x00, 'X', 0x00, 'I', 0x00, 'P', 0x00
};

/* MS OS String Descriptor */
USB3_DESC_ATTRIBUTES uint8_t glOsString[] = {
    0x12, /* Length. */
    0x03, /* Type - string. */
    'M',
    0x00,
    'S',
    0x00,
    'F',
    0x00,
    'T',
    0x00,
    '1',
    0x00,
    '0',
    0x00,
    '0',
    0x00,			/* Signature. */
    MS_VENDOR_CODE, /* MS vendor code. */
    0x00			/* Padding. */
};

USB3_DESC_ATTRIBUTES uint8_t glOsCompatibilityId[] = {
    /* Header */
    0x28, 0x00, 0x00,
    0x00,		/* length Need to be updated based on number of interfaces. */
    0x00, 0x01, /* BCD version */
    0x04, 0x00, /* Index: 4 - compatibility ID */
    0x01,		/* count. Need to be updated based on number of interfaces. */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* reserved. */
    /* First Interface */
    0x01, /* Interface number */
    0x01, /* reserved: Need to be 1. */
    0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00, /* comp ID to bind the
                                                       device with WinUSB.*/
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,								/* sub-compatibility ID - NONE. */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* reserved - needs to be zero. */
};

USB3_DESC_ATTRIBUTES uint8_t glOsFeature[] = {
    /* Header */
    0x8E, 0x00, 0x00, 0x00, /* Length. */
    0x00, 0x01,				/* BCD version. 1.0 as per MS */
    0x05, 0x00,				/* Index */
    0x01, 0x00,				/* count. */
    /* Property section. */
    0x84, 0x00, 0x00, 0x00, /* length */
    0x01, 0x00, 0x00, 0x00, /* dwPropertyDataType: REG_DWORD_LITTLE_ENDIAN */
    0x28, 0x00,				/* wPropertyNameLength: 0x30 */

    0x44, 0x00, 0x65, 0x00, 0x76, 0x00, 0x69, 0x00, 0x63, 0x00, 0x65, 0x00,
    0x49, 0x00, 0x6E, 0x00, 0x74, 0x00, 0x65, 0x00, 0x72, 0x00, 0x66, 0x00,
    0x61, 0x00, 0x63, 0x00, 0x65, 0x00, 0x47, 0x00, 0x55, 0x00, 0x49, 0x00,
    0x44, 0x00, 0x00, 0x00, /* bPropertyName: DeviceInterfaceGUID */
    0x4E, 0x00, 0x00, 0x00, /* dwPropertyDataLength: 4E */

    '{', 0x00, '0', 0x00, '1', 0x00, '2', 0x00, '3', 0x00, '4', 0x00, '5', 0x00,
    '6', 0x00, '7', 0x00, '-', 0x00, '2', 0x00, 'A', 0x00, '4', 0x00, 'F', 0x00,
    '-', 0x00, '4', 0x00, '9', 0x00, 'E', 0x00, 'E', 0x00, '-', 0x00, '8', 0x00,
    'D', 0x00, 'D', 0x00, '3', 0x00, '-', 0x00, 'F', 0x00, 'A', 0x00, 'D', 0x00,
    'E', 0x00, 'A', 0x00, '3', 0x00, '7', 0x00, '7', 0x00, '2', 0x00, '3', 0x00,
    '4', 0x00, 'A', 0x00, '}', 0x00, 0x00, 0x00
        /* bPropertyData: {01234567-2A4F-49EE-8DD3-FADEA377234A} */
};

void Cy_USB_RegisterDescriptors(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_en_usb_speed_t usbSpeed) {
    Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_SS_DEVICE_DSCR, 0,
            (uint8_t *)CyFxUSB30DeviceDscr);
    Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0,
            (uint8_t *)CyFxUSB20DeviceDscr);
    if (usbSpeed >= CY_USBD_USB_DEV_SS_GEN2) {
        Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_SS_BOS_DSCR, 0,
                (uint8_t *)CyFxUSBBOSDscr_SSP);
    } else {
        Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_SS_BOS_DSCR, 0,
                (uint8_t *)CyFxUSBBOSDscr_SS);
    }
    Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_HS_BOS_DSCR, 0,
            (uint8_t *)CyFxUSBBOSDscr_HS);
    Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_DEVICE_QUAL_DSCR, 0,
            (uint8_t *)CyFxUSBDeviceQualDscr);
    Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_SS_CONFIG_DSCR, 0,
            (uint8_t *)CyFxUSBSSConfigDscr);
    Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_HS_CONFIG_DSCR, 0,
            (uint8_t *)CyFxUSBHSConfigDscr);
    Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_FS_CONFIG_DSCR, 0,
            (uint8_t *)CyFxUSBFSConfigDscr);
    Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_STRING_DSCR, 0,
            (uint8_t *)CyFxUSBStringLangIDDscr);
    Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_STRING_DSCR, 1,
            (uint8_t *)CyFxUSBManufactureDscr);
    Cy_USBD_SetDscr(pUsbdCtxt, CY_USB_SET_STRING_DSCR, 2,
            (uint8_t *)CyFxUSBProductDscr);
}

/* [ ] */
