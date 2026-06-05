/***************************************************************************//**
 * \file cy_usb_qspi.c
 * \version 1.0
 *
 *  Implements functions to configure the SMIF peripheral for SPI/QSPI operation
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
 *******************************************************************************/

#include "cy_usb_qspi.h"
#include "cy_smif.h"
#include "cycfg_qspi_memslot.h"

cy_stc_smif_context_t qspiContext;

/* QSPI/ SMIF Config*/
static const cy_stc_smif_config_t qspiConfig =
{
    .mode = (uint32_t)CY_SMIF_NORMAL,
    .deselectDelay = 7u,
    .rxClockSel = (uint32_t)CY_SMIF_SEL_INV_INTERNAL_CLK,
    .blockEvent = (uint32_t)CY_SMIF_BUS_ERROR,
};



/*
Function     : AddressToArray ()
Description  : Change address value to byte-array
Parameters  :  uint32_t value, uint8_t *byteArray, uint8_t size
Return      :  cy_en_smif_status_t
*/
static cy_en_smif_status_t AddressToArray(uint32_t value, uint8_t *byteArray, uint8_t size)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    if(byteArray == NULL)
    {
        return CY_SMIF_BAD_PARAM;
    }

    do
    {
        size--;
        byteArray[size] = (uint8_t)(value & 0x000000FF);
        value >>= 8U; /* Shift to get the next byte */
    } while (size > 0U);

    return status;
}


/*
 * Function     :  Cy_SPI_FlashReset ()
 * Description  :  Issue soft reset to SPI Flash
 * Parameters   :  SMIF_Type *base, cy_stc_smif_mem_config_t const *memDevice
 * Return       :  cy_en_smif_status_t
 * */
static cy_en_smif_status_t Cy_SPI_FlashReset(SMIF_Type *base, cy_stc_smif_mem_config_t const *memDevice)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;

    status = Cy_SMIF_TransmitCommand(base,
            CY_APP_SPI_RESET_ENABLE_CMD,
            CY_SMIF_WIDTH_QUAD,
            NULL,
            CY_SMIF_CMD_WITHOUT_PARAM,
            CY_SMIF_WIDTH_NA,
            memDevice->slaveSelect,
            CY_SMIF_TX_LAST_BYTE,
            &qspiContext);
    ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

    if(CY_SMIF_SUCCESS == status)
    {
        status = Cy_SMIF_TransmitCommand(base,
                CY_APP_SPI_SW_RESET_CMD,
                CY_SMIF_WIDTH_QUAD,
                NULL,
                CY_SMIF_CMD_WITHOUT_PARAM,
                CY_SMIF_WIDTH_NA,
                memDevice->slaveSelect,
                CY_SMIF_TX_LAST_BYTE,
                &qspiContext);
        ASSERT_NON_BLOCK(status == CY_SMIF_SUCCESS, status);

        /* Wait for tRPH (35us) for reset to complete. Sufficient margin added */
        Cy_SysLib_DelayUs(1000);
    }
    return status;
}



/*
 * Function     :  Cy_SPI_WriteAnyRegister ()
 * Description  :  Issue Write Any Register (WRAR) command to SPI Flash
 * Parameters   :  SMIF_Type *base, cy_stc_smif_mem_config_t const *memDevice, uint32_t regAddress, uint8_t value
 * Return       :  cy_en_smif_status_t
 * */

static cy_en_smif_status_t  Cy_SPI_WriteAnyRegister(SMIF_Type *base, cy_stc_smif_mem_config_t const *memDevice,
                                                    uint32_t regAddress, uint8_t value)
{
    cy_en_smif_status_t status = CY_SMIF_BAD_PARAM;
    uint8_t addrArray[CY_SPI_MAX_NUM_ADDR_BYTES] = {0};
    if(NULL != memDevice)
    {
        uint8_t numAddrBytes = memDevice->deviceCfg->numOfAddrBytes;

        DBG_APP_INFO("Write 0x%x to Register: 0x%x\r\n", value, regAddress);

        AddressToArray(regAddress, addrArray,numAddrBytes);

        status = Cy_SMIF_MemCmdWriteEnable(base, memDevice, &qspiContext);
        ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);

        if(CY_SMIF_SUCCESS == status)
        {
            status = Cy_SMIF_TransmitCommand(base,
                    CY_APP_SPI_WRITE_ANY_REGISTER_CMD,
                    CY_SMIF_WIDTH_SINGLE,
                    addrArray,
                    numAddrBytes,
                    CY_SMIF_WIDTH_SINGLE,
                    memDevice->slaveSelect,
                    CY_SMIF_TX_NOT_LAST_BYTE,
                    &qspiContext);
            ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);

            if(CY_SMIF_SUCCESS == status)
            {
                status = Cy_SMIF_TransmitDataBlocking(base, &value, CY_SMIF_WRITE_ONE_BYTE, CY_SMIF_WIDTH_SINGLE, &qspiContext);
                ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);
            }
        }
    }
    return status;
}

/*
 * Function     :  Cy_SPI_ReadAnyRegister
 * Description  :  Issue Read Any register (RDAR) command to SPI Flash
 * Parameters   :  SMIF_Type *base, cy_stc_smif_mem_config_t const *memDevice, uint8_t *readValue, uint32_t regAddress
 * Return       :  uint8_t
 * */

static cy_en_smif_status_t Cy_SPI_ReadAnyRegister(SMIF_Type *base, cy_stc_smif_mem_config_t const *memDevice,
                                                  uint8_t *readValue, uint32_t regAddress)
{
    cy_en_smif_status_t status = CY_SMIF_BAD_PARAM;
    uint8_t addrArray[CY_SPI_MAX_NUM_ADDR_BYTES] = {0};
    uint8_t numAddrBytes = memDevice->deviceCfg->numOfAddrBytes;
    cy_stc_smif_mem_device_cfg_t *device = memDevice->deviceCfg;

    if((NULL != device) && (NULL != readValue))
    {
        cy_stc_smif_mem_cmd_t *cmdRead = device->readCmd;

        if(NULL != cmdRead)
        {
            AddressToArray(regAddress, addrArray,numAddrBytes);

            status = Cy_SMIF_MemCmdWriteEnable(base, memDevice, &qspiContext);
            ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);

            if(CY_SMIF_SUCCESS == status)
            {
                status = Cy_SMIF_TransmitCommand(base,
                        CY_APP_SPI_READ_ANY_REGISTER_CMD,
                        CY_SMIF_WIDTH_SINGLE,
                        addrArray,
                        numAddrBytes,
                        CY_SMIF_WIDTH_SINGLE,
                        memDevice->slaveSelect,
                        CY_SMIF_TX_NOT_LAST_BYTE,
                        &qspiContext);
                ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);

                if (CY_SMIF_SUCCESS == status)
                {
                    if(cmdRead->dummyCycles > 0)
                    {
                        Cy_SMIF_SendDummyCycles(SMIF_HW,cmdRead->dummyCycles);
                    }
                    status = Cy_SMIF_ReceiveDataBlocking(SMIF0, readValue, CY_SMIF_READ_ONE_BYTE, CY_SMIF_WIDTH_SINGLE, &qspiContext);
                    ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);
                    DBG_APP_INFO("Read Register: 0x%x Read value = 0x%x\r\n",regAddress, *readValue);
                }
            }
        }
    }
    return status;
}

/*
 * Function     :  Cy_SPI_QuadEnableVolatile
 * Description  :  Enable Quad mode by writing to the volatile Quad Enable bit
 * Parameters   :  SMIF_Type *base, cy_stc_smif_mem_config_t const *memDevice, cy_stc_smif_context_t const *context
 * Return       :  cy_en_smif_status_t
 * */
cy_en_smif_status_t Cy_SPI_QuadEnableVolatile(SMIF_Type *base, cy_stc_smif_mem_config_t const *memDevice,
                                              cy_stc_smif_context_t const *context)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;
    uint8_t regValue = 0;

    /*Reset to clear the volatile SPI flash settings*/
    Cy_SPI_FlashReset(base, memDevice);
    status = Cy_SPI_ReadAnyRegister(base, memDevice, &regValue, CY_APP_CR1V_REG_ADDR);
    if(CY_SMIF_SUCCESS == status)
    {
        status = Cy_SPI_WriteAnyRegister(base, memDevice, CY_APP_CR1V_REG_ADDR, regValue | CY_APP_QE_BITMASK);
        ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);
        if(CY_SMIF_SUCCESS == status)
        {
            DBG_APP_INFO("Quad mode enabled (volatile)\r\n");
        }
    }
    return status;
}

/*****************************************************************************
 * Function Name: Cy_SMIF_Start
 *****************************************************************************
 * Summary:
 *  Initialize and start SMIF interface for flash operations
 *
 * Parameters:
 *  \param pAppCtxt
 *  Application context pointer
 *
 * Return:
 *  cy_en_smif_status_t - SMIF operation status
 ****************************************************************************/
cy_en_smif_status_t Cy_SMIF_Start(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_en_smif_status_t status = CY_SMIF_SUCCESS;

    status = Cy_SMIF_Init(SMIF_HW, &qspiConfig, SMIF_BLOCKING_API_TIMEOUT_US, &qspiContext);
    ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);
    
	Cy_SMIF_Enable(SMIF_HW, &qspiContext);
   
    /* Initialize the SPI flash using the SFDP table */
    status = Cy_SMIF_MemInit(SMIF_HW, &smifBlockConfig, &qspiContext);
    ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);
    
    /* Cy_SMIF_MemEnableQuadMode is not used as it sets the Non-Volatile version of 
     * Quad Enable bit in the S25FS256 Flash using the WRR command.
     */
    status = Cy_SPI_QuadEnableVolatile(SMIF_HW, smifBlockConfig.memConfig[0], &qspiContext);
    ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == status, status);
    Cy_SMIF_Disable(SMIF0);

    /* Change to XIP mode */
    Cy_SMIF_SetMode(SMIF0, CY_SMIF_MEMORY);
    Cy_SMIF_Enable(SMIF0, &qspiContext);

    if(status == CY_SMIF_SUCCESS)
    {
        DBG_APP_INFO("SMIF Initialized in XIP mode\r\n");
    }
	return status;
}


