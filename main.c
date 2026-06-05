/***************************************************************************/ /**
 * \file main.c
 * \version 1.0
 *
 * Main source file for FX20 SMIF XIP application
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

#include "FreeRTOS.h"
#include "app_version.h"
#include "cy_app_utils.h"
#include "cy_debug.h"
#include "cy_hbdma_version.h"
#include "cy_pdl.h"
#include "cy_usb_app.h"
#include "cy_fx_common.h"
#include "cy_usb_usbd.h"
#include "cy_usbd_version.h"
#include "cy_usbfs_cdc.h"
#include "cy_usbss_cal_drv.h"
#include "cybsp.h"
#include "queue.h"
#include "task.h"
#include <string.h>

cy_stc_usbss_cal_ctxt_t ssCalCtxt;
cy_stc_usb_cal_ctxt_t hsCalCtxt;

/* Global variables associated with High BandWidth DMA setup. */
cy_stc_hbdma_context_t HBW_DrvCtxt; /* High BandWidth DMA driver context. */
cy_stc_hbdma_dscr_list_t
HBW_DscrList; /* High BandWidth DMA descriptor free list. */
cy_stc_hbdma_buf_mgr_t HBW_BufMgr; /* High BandWidth DMA buffer manager. */
cy_stc_hbdma_mgr_context_t
HBW_MgrCtxt; /* High BandWidth DMA manager context. */

/* CPU DMA register pointers. */
DMAC_Type *pCpuDmacBase;
DW_Type *pCpuDw0Base, *pCpuDw1Base;

cy_stc_usb_usbd_ctxt_t usbdCtxt;	/* USBD context structure. */
cy_stc_usb_app_ctxt_t appCtxt;		/* Application context structure. */
cy_stc_scb_uart_context_t uartCtxt; /* SCB-UART context structure. */

extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void);

#if DEBUG_INFRA_EN
/* Select SCB interface used for UART based logging. */
#define LOGGING_SCB (SCB1)
#define LOGGING_SCB_IDX (1)
#define DEBUG_LEVEL (3u)

/* Debug log related initialization */
#define LOGBUF_SIZE (1024u)
uint8_t logBuff[LOGBUF_SIZE];
cy_stc_debug_config_t dbgCfg = {
    .pBuffer = logBuff,
    .traceLvl = DEBUG_LEVEL,
    .bufSize = LOGBUF_SIZE,
#if USBFS_LOGS_ENABLE
    .dbgIntfce = CY_DEBUG_INTFCE_USBFS_CDC,
#else
    .dbgIntfce = CY_DEBUG_INTFCE_UART_SCB1,
#endif /* USBFS_LOGS_ENABLE */
    .printNow = true
};

/* Task for handling Debug prints */
TaskHandle_t printLogTaskHandle;

void PrintTaskHandler(void *pTaskParam) {
    while (1) {
        /* Print any pending logs to the output console. */
        Cy_Debug_PrintLog();

        /* Put the thread to sleep for 5 ms */
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

#endif /* DEBUG_INFRA_EN */

void SysTickIntrWrapper(void) {
    Cy_USBD_TickIncrement(&usbdCtxt);
    xPortSysTickHandler();
}

void vPortSetupTimerInterrupt(void) {
    /* Register the exception vectors. */
    Cy_SysInt_SetVector(PendSV_IRQn, xPortPendSVHandler);
    Cy_SysInt_SetVector(SVCall_IRQn, vPortSVCHandler);
    Cy_SysInt_SetVector(SysTick_IRQn, SysTickIntrWrapper);

    /* Start the SysTick timer with a period of 1 ms. */
    Cy_SysTick_SetClockSource(CY_SYSTICK_CLOCK_SOURCE_CLK_CPU);
    Cy_SysTick_SetReload(Cy_SysClk_ClkFastGetFrequency() / 1000U);
    Cy_SysTick_Clear();
    Cy_SysTick_Enable();
}

/*****************************************************************************
 * Function Name: PrintVersionInfo
 ******************************************************************************
 * Summary:
 *  Function to print version information to UART console.
 *
 * Parameters:
 *  type: Type of version string.
 *  version: Version number including major, minor, patch and build number.
 *
 * Return:
 *  None
 *****************************************************************************/
void PrintVersionInfo(const char *type, uint32_t version) {
    char tString[32];
    uint16_t vBuild;
    uint8_t vMajor, vMinor, vPatch;
    uint8_t typeLen = strlen(type);

    vMajor = (version >> 28U);
    vMinor = ((version >> 24U) & 0x0FU);
    vPatch = ((version >> 16U) & 0xFFU);
    vBuild = (uint16_t)(version & 0xFFFFUL);

    memcpy(tString, type, typeLen);
    tString[typeLen++] = '0' + (vMajor / 10);
    tString[typeLen++] = '0' + (vMajor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vMinor / 10);
    tString[typeLen++] = '0' + (vMinor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vPatch / 10);
    tString[typeLen++] = '0' + (vPatch % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vBuild / 1000);
    tString[typeLen++] = '0' + ((vBuild % 1000) / 100);
    tString[typeLen++] = '0' + ((vBuild % 100) / 10);
    tString[typeLen++] = '0' + (vBuild % 10);
    tString[typeLen++] = '\r';
    tString[typeLen++] = '\n';
    tString[typeLen] = 0;

    DBG_APP_INFO("%s", tString);
}


/*******************************************************************************
 * Function name: Cy_Fx_InitPeripheralClocks
 ****************************************************************************//**
 *
 * Function used to enable clocks to different peripherals on the FX10/FX20 device.
 *
 * \param adcClkEnable
 * Whether to enable clock to the ADC in the USBSS block.
 *
 * \param usbfsClkEnable
 * Whether to enable bus reset detect clock input to the USBFS block.
 *
 *******************************************************************************/
void Cy_Fx_InitPeripheralClocks(bool adcClkEnable, bool usbfsClkEnable) {
    if (adcClkEnable) {
        /* Divide PERI clock at 75 MHz by 75 to get 1 MHz clock using 16-bit
         * divider #1. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 1, 74);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 1);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_LVDS2USB32SS_CLOCK_SAR,
                CY_SYSCLK_DIV_16_BIT, 1);
    }

    if (usbfsClkEnable) {
        /* Divide PERI clock at 75 MHz by 750 to get 100 KHz clock using 16-bit
         * divider #2. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 2, 749);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 2);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_USB_CLOCK_DEV_BRS,
                CY_SYSCLK_DIV_16_BIT, 2);
    }

    /* Use PERI 8-bit divider #0 to divide 75 MHz HFCLK by 9 to get 8.333 MHz.
    */
    Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 0, 8);
    Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_8_BIT, 0);
    Cy_SysLib_DelayUs(10);
    Cy_SysClk_PeriphAssignDivider(PCLK_SCB1_CLOCK, CY_SYSCLK_DIV_8_BIT, 0);
}

/*****************************************************************************
 * Function Name: VbusDetGpio_ISR
 *****************************************************************************
 * Summary
 *  Interrupt handler for the Vbus detect GPIO transition detection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
static void VbusDetGpio_ISR(void) {
    BaseType_t xHigherPriorityTaskWoken;
    cy_stc_usbd_app_msg_t xMsg;

    /* Send VBus changed message to the task thread. */
    xMsg.type = CY_USB_VBUS_CHANGE_INTR;
    xQueueSendFromISR(appCtxt.DeviceQueue, &(xMsg),
            &(xHigherPriorityTaskWoken));

    /* Remember that VBus change has happened and disable the interrupt. */
    appCtxt.vbusChangeIntr = true;
    Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 0);
}

/*****************************************************************************
 * Function Name: Cy_BootSwitchISR
 *****************************************************************************
 * Summary
 *  Interrupt handler for the boot (PMOD) switch detection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
static void Cy_BootSwitchISR(void) {
    BaseType_t xHigherPriorityTaskWoken;
    cy_stc_usbd_app_msg_t xMsg;
    if(1UL == Cy_GPIO_GetInterruptStatus(BOOT_GPIO_PORT, BOOT_GPIO_PIN))
    {
        /* Clear the interrupt. */
        Cy_GPIO_ClearInterrupt(BOOT_GPIO_PORT, BOOT_GPIO_PIN);
        /* Send boot switch status changed message to the task thread. */
        xMsg.type = CY_USB_BOOT_SWITCH_INTR;
        xQueueSendFromISR(appCtxt.DeviceQueue, &(xMsg),
                &(xHigherPriorityTaskWoken));
    }
}


/*****************************************************************************
 * Function Name: Cy_App_ConfigureVbusDetect
 *****************************************************************************
 * Summary
 *  Configure VBUS Detect IOs and corresponding interrupt
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_App_ConfigureVbusDetect(void) {
    cy_stc_gpio_pin_config_t pinCfg;
    cy_stc_sysint_t intrCfg;

    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Configure VBus detect GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom = HSIOM_SEL_GPIO;
    pinCfg.intEdge = CY_GPIO_INTR_BOTH;
    pinCfg.intMask = 0x01UL;
    Cy_GPIO_Pin_Init(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, &pinCfg);

    /* Register edge detect interrupt for Vbus detect GPIO. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrPriority = 7;
#else
    intrCfg.intrSrc = NvicMux4_IRQn;
    intrCfg.cm0pSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrPriority = 3;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, VbusDetGpio_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);
} /* end of function. */

/*****************************************************************************
 * Function Name: Cy_USB_HS_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB-HS Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_HS_ISR(void) {
    if (Cy_USBHS_Cal_IntrHandler(&hsCalCtxt)) {
        portYIELD_FROM_ISR(true);
    }
}

/*****************************************************************************
 * Function Name: Cy_USB_SS_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB-SS Interrupts. Re-invoke the RTOS schedule once ISR
 *  execution is complete.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_SS_ISR(void) {
    /* Call the USB32DEV interrupt handler. */
    Cy_USBSS_Cal_IntrHandler(&ssCalCtxt);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_USB_IngressDma_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USBSS ingress DMA interrupts. Calls the respective driver
 *  function and then invokes the RTOS scheduler to switch to the highest
 *  priority active task.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_IngressDma_ISR(void) {
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_USB_IN);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_USB_EgressDma_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USBSS egress DMA interrupts. Calls the respective driver
 *  function and then invokes the RTOS scheduler to switch to the highest
 *  priority active task.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_EgressDma_ISR(void) {
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_USB_EG);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_App_ConfigureInterrupts
 *****************************************************************************
 * Summary
 *  Configure interrupt handlers for USB and HBDMA.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
static void Cy_App_ConfigureInterrupts(void) {
    cy_stc_sysint_t intrCfg;

    /* Register the USBSS ISR and enable the interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_int_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.intrSrc = NvicMux1_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_SS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.intrSrc = NvicMux1_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_SS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

#if CY_CPU_CORTEX_M4
    /* Register the ISR and enable the interrupt. */
    intrCfg.intrSrc = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.intrSrc = NvicMux0_IRQn;
    intrCfg.cm0pSrc = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_USB_HS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = usbhsdev_interrupt_u2d_dpslp_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.cm0pSrc = usbhsdev_interrupt_u2d_dpslp_o_IRQn;
    intrCfg.intrSrc = NvicMux0_IRQn;
    intrCfg.intrPriority = 2;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_USB_HS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* ISR for the USB Ingress DMA adapter */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux3_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_IngressDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* ISR for the USB Egress DMA adapter */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_egrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux2_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_egrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_EgressDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);


}

/*****************************************************************************
 * Function Name: Cy_App_ConfigureInterrupts
 *****************************************************************************
 * Summary
 *  Configure interrupt handler for the PMODE GPIO switch.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
static void Cy_App_EnableSwitchISR ()
{
    cy_stc_sysint_t intrCfg;

    /* ISR for PMOD GPIO*/
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = ioss_interrupts_gpio_dpslp_13_IRQn;
    intrCfg.intrPriority = 7;
#else
    intrCfg.intrSrc = NvicMux4_IRQn;
    intrCfg.cm0pSrc = ioss_interrupts_gpio_dpslp_13_IRQn;
    intrCfg.intrPriority = 3;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_BootSwitchISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);
}

/*
 * Notes on DMA Buffer RAM Usage:
 * 1. The initial part of the buffer RAM is reserved for the descriptors used by
 * the DMA manager. The space used for this is reserved using the
 * gHbDmaDescriptorSpace array which is placed in a section named
 * ".hbDmaDescriptor". This array should have a minimum size of 8192 (8 KB) and
 * has a default size allocation of 16384 bytes (16 KB). No other data should be
 * placed in this section.
 *
 * 2. The descriptor region is followed by RW data structures which are placed
 * in the ".descSection". Only data members placed in this section will be
 * initialized during the firmware load process.
 *
 * 3. The ".descSection" is followed by the ".hbBufSection" which will hold data
 * structures which do not need to be explicitly initialized (equivalent of
 * ".bss" section).
 *
 * 4. This is followed by the ".hbDmaBufferHeap" section which will be used to
 * allocate all the DMA buffers from. The gHbDmaBufferHeap array represents the
 * memory region which will be given to the DMA buffer manager to allocate
 * buffers from and can be sized based on the available memory. No other data or
 * variables should be placed in this section.
 *
 * Any pre-initialized data which is to be placed in the High BandWidth Buffer
 * RAM should be added to the ".descSection". Any non-initialized data which is
 * to be placed in the High BandWidth Buffer RAM should be added to the
 * ".hbBufSection".
 */

/* Region of 16 KB reserved for High BandWidth DMA descriptors. */
static __attribute__((section(".hbDmaDescriptor"),
            used)) uint32_t gHbDmaDescriptorSpace[16384 / 4];

/* Region of 960 KB reserved for DMA buffer heap. */
/*
 * Note: Since this application requires a large amount of buffer RAM, it is
 * only supported on parts that support 1 MB of DMA buffer RAM.
 */
static __attribute__((section(".hbDmaBufferHeap"),
            used)) uint32_t gHbDmaBufferHeap[960 * 1024 / 4];

/*****************************************************************************
 * Function Name: Cy_App_HBDma_Init
 *****************************************************************************
 * Summary
 *  Initialize HBDMA block
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
bool Cy_App_HBDma_Init(void) {
    cy_en_hbdma_status_t drvstat;
    cy_en_hbdma_mgr_status_t mgrstat;

    /* Initialize the HBW DMA driver layer. */
    drvstat = Cy_HBDma_Init(LVDSSS_LVDS, USB32DEV, &HBW_DrvCtxt, 0, 0);
    if (drvstat != CY_HBDMA_SUCCESS) {
        return false;
    }

    /* Verify that gHbDmaDescriptorSpace is located at the base of the DMA
     * buffer SRAM. */
    if ((uint32_t)gHbDmaDescriptorSpace != CY_HBW_SRAM_BASE_ADDR) {
        DBG_APP_ERR("High BandWidth DMA descriptors not placed at the correct "
                "address\r\n");
        return false;
    }

    /* Setup a HBW DMA descriptor list using the space reserved in
     * gHbDmaDescriptorSpace. */
    mgrstat = Cy_HBDma_DscrList_Create(&HBW_DscrList,
            sizeof(gHbDmaDescriptorSpace) / 16);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS) {
        return false;
    }

    /* Initialize the DMA buffer manager to use the gHbDmaBufferHeap region. */
    mgrstat = Cy_HBDma_BufMgr_Create(&HBW_BufMgr, (uint32_t *)gHbDmaBufferHeap,
            sizeof(gHbDmaBufferHeap));
    if (mgrstat != CY_HBDMA_MGR_SUCCESS) {
        return false;
    }

    /* Initialize the HBW DMA channel manager. */
    mgrstat = Cy_HBDma_Mgr_Init(&HBW_MgrCtxt, &HBW_DrvCtxt, &HBW_DscrList,
            &HBW_BufMgr);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS) {
        return false;
    }

    /* Request DMA callback handling in ISR context. */
    Cy_HBDma_Mgr_DmaCallbackConfigure(&HBW_MgrCtxt, true);

    /* Both LVDS DMA adapters are used only for ingress transfers. */
    Cy_HBDma_Mgr_SetLvdsAdapterIngressMode(&HBW_MgrCtxt, true, true);

    /* Specify that DMA clock should be set to 240 MHz once USB 3.x connection
     * is active. */
    Cy_USBD_SetDmaClkFreq(&usbdCtxt, CY_HBDMA_CLK_240_MHZ);

    return true;
}

/*****************************************************************************
 * Function Name: Cy_USBSS_DeInit
 *****************************************************************************
 * Summary
 *  PSVP specific code to make sure that USB32 IP and PHY are disabled
 *  before a connection is attempted.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_USBSS_DeInit(cy_stc_usbss_cal_ctxt_t *pCalCtxt) {
    USB32DEV_Type *base = pCalCtxt->regBase;
    USB32DEV_MAIN_Type *USB32DEV_MAIN = &base->USB32DEV_MAIN;

    /* Disable the clock for USB3.2 function */
    USB32DEV_MAIN->CTRL &= ~USB32DEV_MAIN_CTRL_CLK_EN_Msk;

    /* Disable PHYSS */
    base->USB32DEV_PHYSS.USB40PHY[0].USB40PHY_TOP.TOP_CTRL_0 &=
        ~(USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_RX_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_PLL_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_VBUS_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PHYSS_EN_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_PCLK_EN_Msk);

    base->USB32DEV_PHYSS.USB40PHY[1].USB40PHY_TOP.TOP_CTRL_0 &=
        ~(USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_RX_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_PLL_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_VBUS_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PHYSS_EN_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_PCLK_EN_Msk);

    /* Disable the SuperSpeed Device function */
    USB32DEV_MAIN->CTRL &= ~USB32DEV_MAIN_CTRL_SSDEV_ENABLE_Msk;
}

/*****************************************************************************
 * Function Name: Cy_USB_DisableUsbBlock
 ******************************************************************************
 * Summary:
 *  Function to disable the USB32DEV IP block after terminating current
 *  connection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_DisableUsbBlock(void) {
    /* Disable the USB32DEV IP. */
    USB32DEV->USB32DEV_MAIN.CTRL &= ~USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;

    /* Disable HBDMA adapter interrupts and the adapter itself. */
    NVIC_DisableIRQ(lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn);
    NVIC_DisableIRQ(lvds2usb32ss_usb32_egrs_dma_int_o_IRQn);
    Cy_HBDma_DeInit(&HBW_DrvCtxt);

    DBG_APP_TRACE("Disabled HBWSS DMA adapters\r\n");
}

/*****************************************************************************
 * Function Name: Cy_USB_EnableUsbBlock
 ******************************************************************************
 * Summary:
 *  Function to enable the USB32DEV IP block before enabling a new USB
 *  connection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_EnableUsbBlock(void) {
    /* Enable the USB DMA adapters and respective interrupts. */
    Cy_HBDma_Init(NULL, USB32DEV, &HBW_DrvCtxt, 0, 0);

    NVIC_EnableIRQ(lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn);
    NVIC_EnableIRQ(lvds2usb32ss_usb32_egrs_dma_int_o_IRQn);

    /* Make sure to enable USB32DEV IP. */
    USB32DEV->USB32DEV_MAIN.CTRL |= USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;
}

/*****************************************************************************
 * Function Name: Cy_USB_SSConnectionEnable
 *****************************************************************************
 * Summary
 *  PSVP specific USB connect function.
 *
 * Parameters:
 *  pAppCtxt: Pointer to application context structure.
 *
 * Return:
 *  Always returns true.
 ****************************************************************************/
bool Cy_USB_SSConnectionEnable(cy_stc_usb_app_ctxt_t *pAppCtxt) {
    /* Enable USB connection with desired rate set based on make parameter. */
    Cy_USBD_ConnectDevice(pAppCtxt->pUsbdCtxt, USB_CONN_TYPE);
    pAppCtxt->usbConnected = true;

    return true;
}

/*****************************************************************************
 * Function Name: Cy_USB_SSConnectionDisable
 *****************************************************************************
 * Summary
 *  PSVP specific USB disconnect function.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_USB_SSConnectionDisable(cy_stc_usb_app_ctxt_t *pAppCtxt) {
    Cy_USBD_DisconnectDevice(pAppCtxt->pUsbdCtxt);
    Cy_USBSS_DeInit(pAppCtxt->pUsbdCtxt->pSsCalCtxt);
    pAppCtxt->usbConnected = false;
    pAppCtxt->devState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
}

/*****************************************************************************
 * Function Name: Cy_App_SystemInit(void)
 ******************************************************************************
 * Summary:
 *  Initialize Clocks, WDT and BSP related parameters
 *
 * Parameters:
 *  void

 * Return:
 *  Does not return.
 *****************************************************************************/
static void Cy_App_SystemInit(void) {
    /* Initialize the PDL driver library and set the clock variables. */
    Cy_PDL_Init(&cy_deviceIpBlockCfgFX3G2);
    cybsp_init();

    /* Configure the clock input to the SCB, USBFS and USB ADC blocks. */
    Cy_Fx_InitPeripheralClocks(true, true);

    /* Unlock and then disable the watchdog. */
    Cy_WDT_Unlock();
    Cy_WDT_Disable();

    /* Set BASEPRI value to 0 to ensure all exceptions can run and then enable
     * interrupts. */
#if (CY_CPU_CORTEX_M4)
    __set_BASEPRI(0);
#endif /* (CY_CPU_CORTEX_M4) */
    __enable_irq();
}

static void Cy_App_InitializeCtxts(void) {
    pCpuDmacBase = ((DMAC_Type *)DMAC_BASE);
    pCpuDw0Base = ((DW_Type *)DW0_BASE);
    pCpuDw1Base = ((DW_Type *)DW1_BASE);

    memset((uint8_t *)&appCtxt, 0, sizeof(appCtxt));
    memset((uint8_t *)&ssCalCtxt, 0, sizeof(ssCalCtxt));
    memset((uint8_t *)&hsCalCtxt, 0, sizeof(hsCalCtxt));
    memset((uint8_t *)&usbdCtxt, 0, sizeof(usbdCtxt));

    /* Store IP base address in CAL context. */
    ssCalCtxt.regBase = USB32DEV;
    hsCalCtxt.pCalBase = MXS40USBHSDEV_USBHSDEV;
    hsCalCtxt.pPhyBase = MXS40USBHSDEV_USBHSPHY;
}

static void Cy_App_DebugInit(void) {
#if DEBUG_INFRA_EN
#if USBFS_LOGS_ENABLE
    /* Give some time for USBFS enumeration to complete. */
    Cy_Debug_LogInit(&dbgCfg);
    Cy_SysLib_Delay(5000U);
#else
    /* Initialize the UART for logging. */
    InitUart(LOGGING_SCB_IDX);
    Cy_Debug_LogInit(&dbgCfg);
#endif /* USBFS_LOGS_ENABLE */

    DBG_APP_INFO("EZ-USB FX: SMIF XIP example\r\n");

    /* Print application, USBD stack and HBDMA version information. */
    PrintVersionInfo("APP_VERSION: ", APP_VERSION_NUM);
    PrintVersionInfo("USBD_VERSION: ", USBD_VERSION_NUM);
    PrintVersionInfo("HBDMA_VERSION: ", HBDMA_VERSION_NUM);

    /* Create task for printing logs and check status. */
    xTaskCreate(PrintTaskHandler, "PrintLogTask", 512, NULL, 5,
            &printLogTaskHandle);
#if ENABLE_USB_EVENT_LOGS
    Cy_App_InitUsbEventLogs(&usbdCtxt);
#endif
#endif /* DEBUG_INFRA_EN */
}

/*****************************************************************************
 * Function Name: main(void)
 ******************************************************************************
 * Summary:
 *  Entry to the application.
 *
 * Parameters:
 *  void

 * Return:
 *  Does not return.
 *****************************************************************************/
int main(void) {

    Cy_App_SystemInit();
    Cy_App_InitializeCtxts();

    Cy_App_ConfigureVbusDetect();
    Cy_App_ConfigureInterrupts();

    Cy_App_DebugInit();

    /* Initialize the HbDma IP and DMA Manager */
    Cy_App_HBDma_Init();

    /* Initialize the USBD layer */
    Cy_USB_USBD_Init(&appCtxt, &usbdCtxt, pCpuDmacBase, &hsCalCtxt, &ssCalCtxt,
            &HBW_MgrCtxt);

    /* Register USB descriptors with the stack. */
    Cy_USB_RegisterDescriptors(&usbdCtxt, USB_CONN_TYPE);

    /* Initialize the application and create application task to handle runtime events*/
    Cy_USB_AppInit(&appCtxt, &usbdCtxt, pCpuDmacBase, pCpuDw0Base, pCpuDw1Base,
            &HBW_MgrCtxt);
    
    /* Clear the indicator LED and enable ISR for PMODE GPIO switch. */
    Cy_GPIO_Clr(USER_LED_PORT, USER_LED_PIN);
    Cy_App_EnableSwitchISR();

    /* Invokes scheduler: Not expected to return. */
    vTaskStartScheduler();
    while (1);
    return 0;
}

/*****************************************************************************
 * Function Name: Cy_OnResetUser(void)
 ******************************************************************************
 * Summary:
 *  Init function which is executed before the load regions in RAM are updated.
 *  The High BandWidth subsystem needs to be enable here to allow variables
 *  placed in the High BandWidth SRAM to be updated.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_OnResetUser(void)
{
    Cy_UsbFx_OnResetInit();
}

/* [] END OF FILE */
