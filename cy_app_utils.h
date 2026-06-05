/***************************************************************************//**
 * \file cy_app_utils.h
 * \version 1.0
 *
 * Defines macros and functions for error handling etc.
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
#ifndef _APP_UTILS_H_
#define _APP_UTILS_H_

#include "cy_debug.h"
#include "cy_pdl.h"
#include "cy_usb_usbd.h"


#if defined(__cplusplus)
extern "C" {
#endif

#define ASSERT(condition, value)                                               \
    Cy_CheckStatus(__func__, __LINE__, condition, value, true);
#define ASSERT_NON_BLOCK(condition, value)                                     \
    Cy_CheckStatus(__func__, __LINE__, condition, value, false);

#define RED "\033[0;31m"
#define CYAN "\033[0;36m"
#define COLOR_RESET "\033[0m"

#define LOG_COLOR(...)                                                         \
    Cy_Debug_AddToLog(1, CYAN);                                                \
    Cy_Debug_AddToLog(1, __VA_ARGS__);                                         \
    Cy_Debug_AddToLog(1, COLOR_RESET);

#define LOG_ERROR(...)                                                         \
    Cy_Debug_AddToLog(1, RED);                                                 \
    Cy_Debug_AddToLog(1, __VA_ARGS__);                                         \
    Cy_Debug_AddToLog(1, COLOR_RESET);

#define LOG_CLR(CLR, ...)                                                      \
    Cy_Debug_AddToLog(1, CLR);                                                 \
    Cy_Debug_AddToLog(1, __VA_ARGS__);                                         \
    Cy_Debug_AddToLog(1, COLOR_RESET);

#define LOG_TRACE() LOG_COLOR("-->[%s]:%d\r\n", __func__, __LINE__);

#define DELAY_MICRO(us) Cy_SysLib_DelayUs(us)
#define DELAY_MILLI(ms) Cy_SysLib_Delay(ms)

#define CY_APP_USB_LOG_BUFFSIZE (512)

    /*******************************************************************************
     * Function name: Cy_CheckStatus(const char *function, uint32_t line,
     * 								uint8_t condition, uint32_t value,
     * 								uint8_t isBlocking)
     ****************************************************************************//**
     *
     * Description: Function that handles prints error log
     *
     * \param function
     * Pointer to function
     *
     * \param line
     * Line number where error is seen
     *
     * \param condition
     *  condition of failure
     *
     * \param value
     *  error code
     *
     * \param isBlocking
     *  blocking function
     *
     * \return
     * None
     *
     ********************************************************************************/
    void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition,
            uint32_t value, uint8_t isBlocking);
    /*****************************************************************************
     * Function Name: Cy_App_PrintUSBEventLogs
     *****************************************************************************   
     * Summary:
     *  Function to print USB event logs
     *
     * Parameters:
     *  \param pUsbdCtxt
     *  Pointer to USB device context
     *
     * Return:
     *  void
     ****************************************************************************/
    void Cy_App_PrintUSBEventLogs(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt);

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
    void Cy_App_InitUsbEventLogs(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt);


    /*****************************************************************************
     * Function Name: Cy_App_PrintEqualizationResults
     *****************************************************************************
     * Summary:
     *  Function to print USB equalization results
     *
     * Parameters:
     *  \param pCalCtxt
     *  Pointer to USBSS calibration context
     *
     * Return:
     *  void
     ****************************************************************************/

    void Cy_App_PrintEqualizationResults(cy_stc_usbss_cal_ctxt_t *pCalCtxt);

#if defined(__cplusplus)
}
#endif
#endif /*_APP_UTILS_H*/
