/***************************************************************************//**
 * \file cy_usb_descriptors.h
 * \version 1.0
 *
 * This file contains defines for USB descriptors for the application
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

#ifndef _DESCR_H_
#define _DESCR_H_

#if defined(__cplusplus)
extern "C" {
#endif

#define CY_USB_VENDOR_ID  (0x04B4)
#define CY_USB_PRODUCT_ID (0x480A)

#define VENDOR_INTERFACE_NUM (0x00)


/* Device Capability Type Codes */
#define CY_USB2_EXTN_CAPB_TYPE (0x02)
#define CY_SS_USB_CAPB_TYPE    (0x03)

#if defined(__cplusplus)
}
#endif

#endif  /*__DESCR_H__*/
