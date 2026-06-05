/***************************************************************************//**
* \file cy_app_utils.c
* \version 1.0
*
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

#include "cy_app_utils.h"
#include "cy_usbss_cal_drv.h"

static uint32_t g_UsbEvtLogBuf[CY_APP_USB_LOG_BUFFSIZE];
static uint16_t gCurUsbEvtLogIndex = 0;

/*****************************************************************************
 * Function Name: Cy_CheckStatus
 *****************************************************************************
 * Summary:
 *  Function that handles and prints error log
 *
 * Parameters:
 *  \param function
 *  Pointer to function name
 *  \param line
 *  Line number where error is seen
 *  \param condition
 *  Condition of failure
 *  \param value
 *  Error code
 *  \param isBlocking
 *  Blocking function flag
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition,
					uint32_t value, uint8_t isBlocking) {
	if (!condition) {
		/* Application failed with the error code status */
		Cy_Debug_AddToLog(1, RED);
		Cy_Debug_AddToLog(
			1, "Function %s failed at line %d with status = 0x%x\r\n", function,
			line, value);
		Cy_Debug_AddToLog(1, COLOR_RESET);
		if (isBlocking) {
			/* Loop indefinitely */
			for (;;) {
			}
		}
	}
}

/*****************************************************************************
 * Function Name: Cy_App_InitUsbEventLogs
 *****************************************************************************
 * Summary:
 *  Initialize USB event logging buffer
 *
 * Parameters:
 *  \param pUsbdCtxt
 *  Pointer to USB device context
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_App_InitUsbEventLogs(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt) {
	cy_en_usbd_ret_code_t status = CY_USBD_STATUS_SUCCESS;

	/* Allocate a memory block and register it as USB CAL event log buffer. */
	status = Cy_USBD_InitEventLog(pUsbdCtxt, g_UsbEvtLogBuf, CY_APP_USB_LOG_BUFFSIZE);
	ASSERT_NON_BLOCK(CY_USBD_STATUS_SUCCESS == status, status);
}

/*****************************************************************************
 * Function Name: Cy_App_PrintUSBEventLogs
 *****************************************************************************
 * Summary:
 *  Print out pending USB event log data
 *
 * Parameters:
 *  \param pUsbdCtxt
 *  Pointer to USB device context
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_App_PrintUSBEventLogs(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt) {
	uint16_t prevLogIdx = gCurUsbEvtLogIndex;

	/* Print out any pending USB event log data. */
	gCurUsbEvtLogIndex = Cy_USBD_GetEvtLogIndex(pUsbdCtxt);
	while (gCurUsbEvtLogIndex != prevLogIdx) {
		DBG_APP_INFO("USBEVT: %x\r\n", g_UsbEvtLogBuf[prevLogIdx]);
		prevLogIdx++;
		if (prevLogIdx == CY_APP_USB_LOG_BUFFSIZE) {
			prevLogIdx = 0u;
		}
	}
}


/*****************************************************************************
 * Function Name: Cy_App_PrintEqualizationResults
 *****************************************************************************
 * Summary:
 *  Print out the CTLE RX adaptation results
 *
 * Parameters:
 *  \param pCalCtxt
 *  Pointer to USBSS CAL context
 *
 * Return:
 *  void
 ****************************************************************************/


void Cy_App_PrintEqualizationResults(cy_stc_usbss_cal_ctxt_t *pCalCtxt) {
	/* Print the CTLE RX adaptation results if the algorithm has been run since
	 * last check. */
	Cy_USBSS_Cal_PrintCtleResults(pCalCtxt, 0);
	Cy_USBSS_Cal_PrintCtleResults(pCalCtxt, 1);
}


