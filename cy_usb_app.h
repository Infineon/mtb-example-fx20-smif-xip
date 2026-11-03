/***************************************************************************//**
 * \file cy_usb_app.h
 * \version 1.0
 *
 * Defines used by the FX20 SMIF XIP application
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

#ifndef _CY_USB_APP_H_
#define _CY_USB_APP_H_

#include "cy_debug.h"
#include "cy_pdl.h"
#include "cy_usbhs_dw_wrapper.h"
#include "timers.h"


#if defined(__cplusplus)
extern "C" {
#endif

/* P4.0 is used for VBus detect functionality. */
#define VBUS_DETECT_GPIO_PORT (P4_0_PORT)
#define VBUS_DETECT_GPIO_PIN (P4_0_PIN)
#define VBUS_DETECT_GPIO_INTR (ioss_interrupts_gpio_dpslp_4_IRQn)
#define VBUS_DETECT_STATE (0u)

#define BOOT_GPIO_PORT (P13_0_PORT)
#define BOOT_GPIO_PIN  (P13_0_PIN)

#if MIPI_DEMO_KIT
#define USER_LED_PORT (P13_1_PORT)
#define USER_LED_PIN  (P13_1_PIN)
#else
#define USER_LED_PORT (P11_5_PORT)
#define USER_LED_PIN  (P11_5_PIN)
#endif

#define USB3_DESC_ATTRIBUTES                                                   \
	__attribute__((section(".descSection"), used)) __attribute__((aligned(32)))
#define HBDMA_BUF_ATTRIBUTES                                                   \
	__attribute__((section(".hbBufSection"), used)) __attribute__((aligned(32)))

#ifdef __IAR_SYSTEMS_ICC__
#define XIP_ATTRIBUTE                                                           \
    __attribute__ ((section(".cy_xip"), used))
#else
#define XIP_ATTRIBUTE                                                           \
    __attribute__ ((section(".cy_xip"), used)) __attribute__ ((aligned (8)))
#endif /* __IAR_SYSTEMS_ICC__ */


/* Get the LS byte from a 16-bit number */
#define CY_GET_LSB(w) ((uint8_t)((w) & UINT8_MAX))

/* Get the MS byte from a 16-bit number */
#define CY_GET_MSB(w) ((uint8_t)((w) >> 8))

extern uint8_t glOsString[];
extern uint8_t glOsCompatibilityId[];
extern uint8_t glOsFeature[];

#define CY_USB_VBUS_CHANGE_INTR      (0x10)
#define CY_USB_VBUS_CHANGE_DEBOUNCED (0x11)
#define CY_USB_BOOT_SWITCH_INTR      (0x12)

#define CY_USB_DEVICE_MSG_QUEUE_SIZE (24)
#define CY_USB_DEVICE_MSG_SIZE (sizeof(cy_stc_usbd_app_msg_t))
#define MS_VENDOR_CODE (0xF0)

typedef enum cy_en_vendor_cmd_codes_t {
	VENDOR_CMD_CTRL_CENTER_BL_CHECK = 0xA1,
} cy_en_vendor_cmd_codes_t;

typedef struct cy_stc_usb_app_ctxt_ cy_stc_usb_app_ctxt_t;

/*
 * USB application data structure which is bridge between USB system and device
 * functionality.
 * It maintains some USB system information which comes from USBD and it also
 * maintains info about functionality.
 */
struct cy_stc_usb_app_ctxt_ {
	uint8_t firstInitDone;				   /** Whether APP init is complete. */
	cy_en_usb_device_state_t devState;	   /** Current device state. */
	cy_en_usb_device_state_t prevDevState; /** Previous device state. */
	cy_en_usb_speed_t devSpeed;			   /** Active USB connection speed. */
	uint8_t devAddr;		  /** USB address assigned to the device. */
	uint8_t activeCfgNum;	  /** Active configuration index. */
	uint8_t prevAltSetting;	  /** Previous alternate setting number. */
	bool dataXferIntrEnabled; /** Whether DMA interrupts are initialised. */

	cy_stc_app_endp_dma_set_t
		endpInDma[CY_USB_MAX_ENDP_NUMBER]; /** Data transfer info for IN
											  endpoints. */
	cy_stc_app_endp_dma_set_t
		endpOutDma[CY_USB_MAX_ENDP_NUMBER]; /** Data transfer info for OUT
											   endpoints. */

	DMAC_Type *pCpuDmacBase; /** Pointer to DMAC IP registers. */
	DW_Type *pCpuDw0Base;	 /** Pointer to DataWire-0 IP registers. */
	DW_Type *pCpuDw1Base;	 /** Pointer to DataWire-1 IP registers. */
	cy_stc_hbdma_mgr_context_t
		*pHbDmaMgrCtxt; /** High BW DMA manager context pointer. */
	cy_stc_usb_usbd_ctxt_t *pUsbdCtxt; /** USBD stack context pointer. */

	TaskHandle_t DevicetaskHandle; /** application task handle. */
	QueueHandle_t DeviceQueue;	   /** application message queue. */
	TimerHandle_t vbusDebounceTimer;   /** VBus change debounce timer handle. */

	bool vbusChangeIntr;	 /** VBus change interrupt received flag. */
	bool vbusPresent;		 /** VBus presence indicator flag. */
	bool usbConnected;		 /** Whether USB connection is enabled. */
};

void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt,
					cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, DMAC_Type *pCpuDmacBase,
					DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base,
					cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt);

void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt);
uint32_t *Cy_USB_CalculateEpmAddr(uint32_t endpNum,
								  cy_en_usb_endp_dir_t endpDirection);

void Cy_USB_AppSetCfgCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
							  cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppBusResetCallback(void *pAppCtxt,
								cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
								cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppBusResetDoneCallback(void *pAppCtxt,
									cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
									cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppBusSpeedCallback(void *pAppCtxt,
								cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
								cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppSetupCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
							 cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppSuspendCallback(void *pAppCtxt,
							   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
							   cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
							  cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppSetIntfCallback(void *pAppCtxt,
							   cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
							   cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppL1SleepCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
							   cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppL1ResumeCallback(void *pUsbApp,
								cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
								cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
						   cy_stc_usb_cal_msg_t *pMsg);
void Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
						   cy_stc_usb_cal_msg_t *pMsg);

bool Cy_USB_SSConnectionEnable(cy_stc_usb_app_ctxt_t *pAppCtxt);
void Cy_USB_SSConnectionDisable(cy_stc_usb_app_ctxt_t *pAppCtxt);

void Cy_USB_EgressDma_ISR(void);
void Cy_USB_IngressDma_ISR(void);

void Cy_USB_RegisterDescriptors(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
								cy_en_usb_speed_t usbSpeed);

void Cy_USB_DeviceTaskHandler(void *pTaskParam);

#if defined(__cplusplus)
}
#endif

#endif /* _CY_USB_APP_H_ */

/* End of File */
