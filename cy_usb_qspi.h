/***************************************************************************//**
* \file cy_usb_qspi.h
* \version 1.0
*
* Provided API declarations for the QSPI implementation.
*
*******************************************************************************
* \copyright
* (c) (2021-2026), Cypress Semiconductor Corporation (an Infineon company) or
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
***************************************************************************/



#ifndef _QSPI_H_
#define _QSPI_H_

#include "cy_pdl.h"
#include "cy_debug.h"
#include "cy_usb_app.h"
#include "cy_app_utils.h"

#if defined(__cplusplus)
extern "C" {
#endif



/*****************************************************************************
*                          SMIF macros                                       *
******************************************************************************/

#define SMIF_HW                                 SMIF0
#define SMIF_BLOCKING_API_TIMEOUT_US            (100000u) /* 100 ms */
#define CY_APP_SPI_WRITE_ANY_REGISTER_CMD       (0x71)
#define CY_APP_SPI_READ_ANY_REGISTER_CMD        (0x65)
#define CY_APP_CR1V_REG_ADDR                    (0x800002)
#define CY_APP_QE_BITMASK                       (0x02)
#define CY_APP_SPI_RESET_ENABLE_CMD             (0x66)
#define CY_APP_SPI_SW_RESET_CMD                 (0x99)
#define CY_SPI_MAX_NUM_ADDR_BYTES               (0x04)

extern cy_stc_smif_context_t qspiContext;


/*****************************************************************************
* Function Name:Cy_QSPI_ConfigureSMIFPins(bool init)
******************************************************************************
* Summary:
* Function to Configure SMIF pins
*
* Parameters:
*  \param pAppCtxt
*   User Context
*  \param init
*   Initialize
*
* Return:
*  Does not return.
*****************************************************************************/
void Cy_QSPI_ConfigureSMIFPins(bool init);


cy_en_smif_status_t Cy_SMIF_Start(cy_stc_usb_app_ctxt_t *pUsbApp);

#if defined(__cplusplus)
}
#endif
#endif /*_QSPI_H_*/

