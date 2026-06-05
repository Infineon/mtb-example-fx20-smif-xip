/***************************************************************************//**
 * \file cy_usb_app.c
 * \version 1.0
 *
 * Implements the application code for SMIF XIP code example
 *
 *******************************************************************************
 * \copyright
 * (c) (2021-2023), Cypress Semiconductor Corporation (an Infineon company) or
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

#include "cy_usb_app.h"
#include "FreeRTOS.h"
#include "cy_app_utils.h"
#include "cy_debug.h"
#include "cy_device.h"
#include "cy_fx_common.h"
#include "cy_hbdma.h"
#include "cy_hbdma_mgr.h"
#include "cy_pdl.h"
#include "cy_usb_common.h"
#include "cy_usb_descriptors.h"
#include "cy_usb_usbd.h"
#include "cy_usbhs_cal_drv.h"
#include "cy_usbhs_dw_wrapper.h"
#include "cy_usbss_cal_drv.h"
#include "queue.h"
#include "task.h"
#include "timers.h"
#include "cy_usb_qspi.h"

HBDMA_BUF_ATTRIBUTES uint32_t SetSelDataBuffer[8];
/*
 * Function: Cy_USB_VbusDebounceTimerCallback()
 * Description: Timer used to do debounce on VBus changed interrupt
 * notification.
 *
 * Parameter:
 *      xTimer: RTOS timer handle.
 * return: void
 */
void Cy_USB_VbusDebounceTimerCallback(TimerHandle_t xTimer) {
    cy_stc_usb_app_ctxt_t *pAppCtxt =
        (cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;

    if (pAppCtxt->vbusChangeIntr) {
        /* Notify the task that VBus debounce is complete. */
        xMsg.type = CY_USB_VBUS_CHANGE_DEBOUNCED;
        xQueueSendFromISR(pAppCtxt->DeviceQueue, &(xMsg),
                &(xHigherPriorityTaskWoken));

        /* Clear and re-enable the interrupt. */
        pAppCtxt->vbusChangeIntr = false;
        Cy_GPIO_ClearInterrupt(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN);
        Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN,
                1);
    }
} /* end of function  */

/*
 * Function: Cy_USB_AppInit()
 * Description: This function Initializes application related data structures,
 *              register callback and creates queue and task for device
 *              function.
 * Parameter: cy_stc_usb_app_ctxt_t, cy_stc_usb_usbd_ctxt_t, DMAC_Type
 *            DW_Type, DW_Type, cy_stc_hbdma_mgr_context_t*
 * return: None.
 * Note: This function should be called after USBD_Init()
 */
void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, DMAC_Type *pCpuDmacBase,
        DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base,
        cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt)
{
    uint32_t index;
    BaseType_t status = pdFALSE;
    cy_stc_app_endp_dma_set_t *pEndpInDma;
    cy_stc_app_endp_dma_set_t *pEndpOutDma;
    cy_en_smif_status_t smifStatus = CY_SMIF_SUCCESS;

    pAppCtxt->devState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->devSpeed = CY_USBD_USB_DEV_FS;
    pAppCtxt->devAddr = 0x00;
    pAppCtxt->activeCfgNum = 0x00;
    pAppCtxt->prevAltSetting = 0x00;
    pAppCtxt->pHbDmaMgrCtxt = pHbDmaMgrCtxt;

    for (index = 0x00; index < CY_USB_MAX_ENDP_NUMBER; index++) {
        pEndpInDma = &(pAppCtxt->endpInDma[index]);
        memset((void *)pEndpInDma, 0, sizeof(cy_stc_app_endp_dma_set_t));

        pEndpOutDma = &(pAppCtxt->endpOutDma[index]);
        memset((void *)pEndpOutDma, 0, sizeof(cy_stc_app_endp_dma_set_t));
    }

    pAppCtxt->pCpuDmacBase = pCpuDmacBase;
    pAppCtxt->pCpuDw0Base = pCpuDw0Base;
    pAppCtxt->pCpuDw1Base = pCpuDw1Base;
    pAppCtxt->pUsbdCtxt = pUsbdCtxt;
    pAppCtxt->dataXferIntrEnabled = false;

    /*
     * Callbacks registered with USBD layer. These callbacks will be called
     * based on appropriate event.
     */
    Cy_USB_AppRegisterCallback(pAppCtxt);

    if (!(pAppCtxt->firstInitDone)) {
        pAppCtxt->vbusChangeIntr = false;
        pAppCtxt->vbusPresent = false;
        pAppCtxt->usbConnected = false;

        /* create queue and register it to kernel. */
        pAppCtxt->DeviceQueue = xQueueCreate(
                CY_USB_DEVICE_MSG_QUEUE_SIZE, CY_USB_DEVICE_MSG_SIZE);

        if (pAppCtxt->DeviceQueue == NULL) {
            DBG_APP_ERR("QueuecreateFail\r\n");
            return;
        }
        DBG_APP_INFO("createdQueue\r\n");
        vQueueAddToRegistry(pAppCtxt->DeviceQueue, "DeviceMsgQueue");

        /* Create task and check status to confirm task created properly. */
        status =
            xTaskCreate(Cy_USB_DeviceTaskHandler, "DeviceTask", 2048,
                    (void *)pAppCtxt, 5, &(pAppCtxt->DevicetaskHandle));

        if (status != pdPASS) {
            DBG_APP_ERR("TaskcreateFail\r\n");
            return;
        }

        pAppCtxt->vbusDebounceTimer =
            xTimerCreate("VbusDebounceTimer", 200, pdFALSE, (void *)pAppCtxt,
                    Cy_USB_VbusDebounceTimerCallback);

        if (pAppCtxt->vbusDebounceTimer == NULL) {
            DBG_APP_ERR("TimerCreateFail\r\n");
            return;
        }

        DBG_APP_INFO("xTimer Created\r\n");

		smifStatus = Cy_SMIF_Start(pAppCtxt);
		ASSERT_NON_BLOCK(CY_SMIF_SUCCESS == smifStatus, smifStatus);

        pAppCtxt->firstInitDone = 0x01;
    }
    return;
} /* end of function. */

/*
 * Function: Cy_USB_AppSetAddressCallback()
 * Description: This Function will be called by USBD layer when
 *              a USB address has been assigned to the device.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t, cy_stc_usb_cal_msg_t
 * return: void
 */
void Cy_USB_AppSetAddressCallback(void *pUsbApp,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    /* Update the state variables. */
    pAppCtxt->devState = CY_USB_DEVICE_STATE_ADDRESS;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DEFAULT;
    pAppCtxt->devAddr = pUsbdCtxt->devAddr;
    pAppCtxt->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);

    /* Check the type of USB connection and register appropriate descriptors. */
    Cy_USB_RegisterDescriptors(pAppCtxt->pUsbdCtxt, pAppCtxt->devSpeed);

#if (!CY_CPU_CORTEX_M4)
    /*
     * Map NvicMux2 and NvicMux3 to High BandWidth DMA adapter interrupts by
     * default. If we are about to start transfers in HS mode, it will be
     * switched to DW interrupts.
     */
    Cy_SysInt_SetVector(NvicMux2_IRQn, Cy_USB_EgressDma_ISR);
    Cy_SysInt_SetVector(NvicMux3_IRQn, Cy_USB_IngressDma_ISR);
#endif /* (!CY_CPU_CORTEX_M4) */
}

/*
 * Function: Cy_USB_AppRegisterCallback()
 * Description: This function will register all calback with USBD layer.
 * Parameter: cy_stc_usb_app_ctxt_t.
 * return: None.
 */
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt) {
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;

    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET,
            Cy_USB_AppBusResetCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET_DONE,
            Cy_USB_AppBusResetDoneCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_BUS_SPEED,
            Cy_USB_AppBusSpeedCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETUP,
            Cy_USB_AppSetupCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SUSPEND,
            Cy_USB_AppSuspendCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESUME,
            Cy_USB_AppResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_CONFIG,
            Cy_USB_AppSetCfgCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETADDR,
            Cy_USB_AppSetAddressCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_INTF,
            Cy_USB_AppSetIntfCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_SLEEP,
            Cy_USB_AppL1SleepCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_RESUME,
            Cy_USB_AppL1ResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_ZLP,
            Cy_USB_AppZlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SLP,
            Cy_USB_AppSlpCallback);
    return;
} /* end of function. */

/*
 * Function: Cy_USB_AppConfigureEndp()
 * Description: This Function is used by application to configure endpoints
 *              after set configuration.  This function should be used for
 *              all endpoints except endp0.
 * Parameter: cy_stc_usb_usbd_ctxt_t, pEndpDscr
 * return: void
 */
void Cy_USB_AppConfigureEndp(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        uint8_t *pEndpDscr) {
    cy_stc_usb_endp_config_t endpConfig;
    cy_en_usb_endp_dir_t endpDirection;
    bool valid;
    uint32_t endpType;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;
    uint8_t isoPkts = 0x00;
    uint8_t burstSize = 0x00;
    uint8_t maxStream = 0x00;
    uint8_t interval = 0x00;
    uint8_t *pCompDscr = NULL;
    cy_en_usbd_ret_code_t usbdRetCode;

    /* If it is not endpoint descriptor then return */
    if (!Cy_USBD_EndpDscrValid(pEndpDscr)) {
        return;
    }
    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);

    if (dir) {
        endpDirection = CY_USB_ENDP_DIR_IN;
    } else {
        endpDirection = CY_USB_ENDP_DIR_OUT;
    }
    Cy_USBD_GetEndpType(pEndpDscr, &endpType);

    if ((CY_USB_ENDP_TYPE_ISO == endpType) ||
            (CY_USB_ENDP_TYPE_INTR == endpType)) {
        /* The ISOINPKS setting in the USBHS register is the actual packets per
         * microframe value. */
        isoPkts =
            ((*((uint8_t *)(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_MAX_PKT + 1)) &
              CY_USB_ENDP_ADDL_XN_MASK) >>
             CY_USB_ENDP_ADDL_XN_POS) +
            1;
    }

    valid = 0x01;
    if (pUsbdCtxt->devSpeed > CY_USBD_USB_DEV_HS) {
        /* Get companion descriptor and from there get burstSize. */
        pCompDscr = Cy_USBD_GetSsEndpCompDscr(pUsbdCtxt, pEndpDscr);
        Cy_USBD_GetEndpCompnMaxburst(pCompDscr, &burstSize);
        Cy_USBD_GetEndpCompnMaxStream(pCompDscr, &maxStream);
        if (CY_USB_ENDP_TYPE_ISO == endpType) {
            Cy_USBD_GetEndpCompnAttribute(pCompDscr, &isoPkts);
            isoPkts = (isoPkts & 0x03U) + 0x01U;
            isoPkts *= burstSize;

            /* Fetch the endpoint service interval. */
            Cy_USBD_GetEndpInterval(pEndpDscr, &interval);
        }
    }

    /* Prepare endpointConfig parameter. */
    endpConfig.endpType = (cy_en_usb_endp_type_t)endpType;
    endpConfig.endpDirection = endpDirection;
    endpConfig.valid = valid;
    endpConfig.endpNumber = endpNumber;
    endpConfig.maxPktSize = (uint32_t)maxPktSize;
    endpConfig.isoPkts = isoPkts;
    endpConfig.burstSize = burstSize;
    endpConfig.streamID = maxStream;
    endpConfig.interval = interval;

    /*
       Since we will mostly be receiving short packets in the
       application, it is preferred to block the reception
       of a new packet until the previous short packet has been
       read out from the EPM.
       */
    endpConfig.allowNakTillDmaRdy = true;
    usbdRetCode = Cy_USB_USBD_EndpConfig(pUsbdCtxt, endpConfig);

    /* Print status of the endpoint configuration to help debug. */
    DBG_APP_INFO("Endpoint:%d, Dir:%d, Config Status:0x%x\r\n", endpNumber, endpDirection, usbdRetCode);

    return;
} /* end of function */


/*
 * Function: Cy_USB_AppSetCfgCallback()
 * Description: This Function will be called by USBD  layer when
 *              set configuration command successful. This function
 *              does sanity check and prepare device for function
 *              to take over.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t
 * return: void
 */
void Cy_USB_AppSetCfgCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {

    cy_stc_usb_app_ctxt_t *pUsbApp;
    uint8_t *pActiveCfg, *pIntfDscr, *pEndpDscr;
    uint8_t index, numOfIntf, numOfEndp;
    cy_en_usb_speed_t devSpeed;

    DBG_APP_INFO("Set Config Callback\r\n");

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);
    devSpeed = pUsbApp->devSpeed;


    /*
     * Based on type of application as well as how data flows,
     * data wire can be used so initialize datawire.
     */
    Cy_DMA_Enable(pUsbApp->pCpuDw0Base);
    Cy_DMA_Enable(pUsbApp->pCpuDw1Base);

    pActiveCfg = Cy_USB_USBD_GetActiveCfgDscr(pUsbdCtxt);
    if (!pActiveCfg) {
        /* Set config should be called when active config value > 0x00. */
        return;
    }
    numOfIntf = Cy_USBD_FindNumOfIntf(pActiveCfg);
    if (numOfIntf == 0x00) {
        return;
    }

    for (index = 0x00; index < numOfIntf; index++) {
        /* During Set Config command always altSetting 0 will be active. */
        pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, index, 0x00);
        if (pIntfDscr == NULL) {
            DBG_APP_INFO("pIntfDscrNull\r\n");
            return;
        }

        numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
        if (numOfEndp == 0x00) {
            DBG_APP_INFO("Num of Endpoints = 0\r\n");
            continue;
        }

        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00) {
            Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr);
            numOfEndp--;
            if (devSpeed > CY_USBD_USB_DEV_HS) {
                pEndpDscr =
                    (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)) + 6);
            } else {
                pEndpDscr =
                    (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
            }
        }
    }

    pUsbApp->prevDevState = CY_USB_DEVICE_STATE_CONFIGURED;
    pUsbApp->devState = CY_USB_DEVICE_STATE_CONFIGURED;

    DBG_APP_INFO("AppSetCfgCbEnd Done\r\n");
    return;
} /* end of function */

/*
 * Function: Cy_USB_AppBusResetCallback()
 * Description: This Function will be called by USBD when bus detects RESET.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t
 * return: void
 */
void Cy_USB_AppBusResetCallback(void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    cy_stc_usb_app_ctxt_t *pUsbApp;
    uint8_t i;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;

    DBG_APP_INFO("AppBusResetCallback\r\n");
    for (i = 0; i < CY_USB_MAX_ENDP_NUMBER; i++) {
        if (pUsbApp->endpInDma[i].valid) {
            DBG_APP_INFO("HBDMA destroy EP%d-In\r\n", i);
            Cy_HBDma_Channel_Disable(&(pUsbApp->endpInDma[i].hbDmaChannel));
            Cy_HBDma_Channel_Destroy(&(pUsbApp->endpInDma[i].hbDmaChannel));
            pUsbApp->endpInDma[i].valid = false;
        }

        if (pUsbApp->endpOutDma[i].valid) {
            DBG_APP_INFO("HBDMA destroy EP%d-Out\r\n", i);
            Cy_HBDma_Channel_Disable(&(pUsbApp->endpOutDma[i].hbDmaChannel));
            Cy_HBDma_Channel_Destroy(&(pUsbApp->endpOutDma[i].hbDmaChannel));
            pUsbApp->endpOutDma[i].valid = false;
        }
    }

    /*
     * USBD layer takes care of reseting its own data structure as well as
     * takes care of calling CAL reset APIs. Application needs to take care
     * of reseting its own data structure as well as "device function".
     */
    Cy_USB_AppInit(pUsbApp, pUsbdCtxt, pUsbApp->pCpuDmacBase,
            pUsbApp->pCpuDw0Base, pUsbApp->pCpuDw1Base,
            pUsbApp->pHbDmaMgrCtxt);
    pUsbApp->devState = CY_USB_DEVICE_STATE_RESET;
    pUsbApp->prevDevState = CY_USB_DEVICE_STATE_RESET;

    if (pUsbdCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
        Cy_USBSS_Cal_LPMDisable(pUsbdCtxt->pSsCalCtxt);
    }

#if (!CY_CPU_CORTEX_M4)
    /*
     * Map NvicMux2 and NvicMux3 to High BandWidth DMA adapter interrupts by
     * default. If we are about to start transfers in HS mode, it will be
     * switched to DW interrupts.
     */
    Cy_SysInt_SetVector(NvicMux2_IRQn, Cy_USB_EgressDma_ISR);
    Cy_SysInt_SetVector(NvicMux3_IRQn, Cy_USB_IngressDma_ISR);
#endif /* (!CY_CPU_CORTEX_M4) */

    return;
} /* end of function. */

/*
 * Function: Cy_USB_AppBusResetDoneCallback()
 * Description: This Function will be called by USBD  layer when
 *              set configuration command successful. This function
 *              does sanity check and prepare device for function
 *              to take over.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t
 * return: void
 */
void Cy_USB_AppBusResetDoneCallback(void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    cy_stc_usb_app_ctxt_t *pUsbApp;

    DBG_APP_INFO("AppBusResetDoneCallback\r\n");

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pUsbApp->prevDevState = pUsbApp->devState;
    return;
} /* end of function. */

/*
 * Function: Cy_USB_AppBusSpeedCallback()
 * Description: This Function will be called by USBD  layer when
 *              speed is identified or speed change is detected.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t
 * return: void
 */
void Cy_USB_AppBusSpeedCallback(void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pUsbApp->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);
    return;
} /* end of function. */

/*
 * Function: Cy_USB_AppSetupCallback()
 * Description: This Function will be called by USBD  layer when
 *              set configuration command successful. This function
 *              does sanity check and prepare device for function
 *              to take over.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t
 * return: void
 */
void Cy_USB_AppSetupCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    cy_stc_usb_app_ctxt_t *pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    uint8_t bRequest, bReqType;
    uint8_t bType, bTarget;
    uint16_t wValue, wLength, wIndex;
    bool isReqHandled = false;
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_SUCCESS;
    cy_en_usb_endp_dir_t epDir = CY_USB_ENDP_DIR_INVALID;

    DBG_APP_INFO("USB Setup Callback\r\n");
    uint8_t endpNumber;
    uint8_t emptyBuffer[8] = {0};

    /* Fast enumeration is used. Only requests addressed to the interface,
     * class, vendor and unknown control requests are received by this function.
     */

    /* Decode the fields from the setup request. */
    bReqType = pUsbdCtxt->setupReq.bmRequest;
    bType =
        ((bReqType & CY_USB_CTRL_REQ_TYPE_MASK) >> CY_USB_CTRL_REQ_TYPE_POS);
    bTarget = (bReqType & CY_USB_CTRL_REQ_RECIPENT_OTHERS);
    bRequest = pUsbdCtxt->setupReq.bRequest;
    wValue = pUsbdCtxt->setupReq.wValue;
    wIndex = pUsbdCtxt->setupReq.wIndex;
    wLength = pUsbdCtxt->setupReq.wLength;

    if (bType == CY_USB_CTRL_REQ_STD) {
        DBG_APP_INFO("Standard Request\r\n");

        /* Handle Microsoft OS String Descriptor request. */
        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
                (bRequest == CY_USB_SC_GET_DESCRIPTOR) &&
                (wValue == ((CY_USB_STRING_DSCR << 8) | 0xEE))) {

            /* Make sure we do not send more data than requested. */
            if (wLength > glOsString[0]) {
                wLength = glOsString[0];
            }

            DBG_APP_INFO("WinUSB:OSString\r\n");
            retStatus = Cy_USB_USBD_SendEndp0Data(
                    pUsbdCtxt, (uint8_t *)glOsString, wLength);
            if (retStatus == CY_USBD_STATUS_SUCCESS) {
                isReqHandled = true;
            }
        }

        /* Handle SET_FEATURE(FUNCTION_SUSPEND) and
         * CLEAR_FEATURE(FUNCTION_SUSPEND) requests here. It should be allowed
         * to pass if the device is in configured state and failed otherwise. */
        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) &&
                ((bRequest == CY_USB_SC_SET_FEATURE) ||
                 (bRequest == CY_USB_SC_CLEAR_FEATURE)) &&
                (wValue == 0)) {
            if (pUsbApp->devState == CY_USB_DEVICE_STATE_CONFIGURED) {
                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
            } else {
                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00,
                        CY_USB_ENDP_DIR_IN, TRUE);
            }
            isReqHandled = true;
        }

        if ((bRequest == CY_USB_SC_SET_FEATURE) &&
                (bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) &&
                (wValue == CY_USB_FEATURE_ENDP_HALT)) {
            DBG_APP_INFO("Set Feature: ENDPOINT_HALT\r\n");
            epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN)
                    : (CY_USB_ENDP_DIR_OUT));
            Cy_USB_USBD_EndpSetClearStall(
                    pUsbdCtxt, ((uint32_t)wIndex & 0x7FUL), epDir, true);
            Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
            isReqHandled = true;
        }

        if ((bRequest == CY_USB_SC_CLEAR_FEATURE) &&
                (bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) &&
                (wValue == CY_USB_FEATURE_ENDP_HALT)) {
            DBG_APP_INFO("Clear Feature: ENDPOINT_HALT\r\n");
            epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN)
                    : (CY_USB_ENDP_DIR_OUT));
            endpNumber = (uint8_t)(wIndex & 0x7FUL);

            Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, endpNumber, epDir,
                    false);
            Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
            isReqHandled = true;
        }
    } else if (bType == CY_USB_CTRL_REQ_VENDOR) {
        DBG_APP_INFO("Vendor request: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\r\n", bRequest,
                bReqType, wValue, wIndex, wLength);

        if ((bRequest == VENDOR_CMD_CTRL_CENTER_BL_CHECK) &&
                (wValue == 0x3C0) && (wIndex == 0x800))
            /* Added to signal to the control center that its not a boot device.
               The other EPs were receiving a EP halt without this addition */
        {
            DBG_APP_INFO("BL Check\r\n");
            retStatus = Cy_USB_USBD_SendEndp0Data(
                    pUsbdCtxt, (uint8_t *)emptyBuffer, wLength);
            if (retStatus == CY_USBD_STATUS_SUCCESS) {
                isReqHandled = true;
            }
        }
        /* Handle OS Compatibility and OS Feature requests */
        if (bRequest == MS_VENDOR_CODE) {
            if (wIndex == 0x04) {

                if (wLength > *((uint16_t *)glOsCompatibilityId)) {
                    wLength = *((uint16_t *)glOsCompatibilityId);
                }

                DBG_APP_INFO("WinUSB:OSCompat\r\n");
                retStatus = Cy_USB_USBD_SendEndp0Data(
                        pUsbdCtxt, (uint8_t *)glOsCompatibilityId, wLength);
                if (retStatus == CY_USBD_STATUS_SUCCESS) {
                    isReqHandled = true;
                }
            } else if (wIndex == 0x05) {

                if (wLength > *((uint16_t *)glOsFeature)) {
                    wLength = *((uint16_t *)glOsFeature);
                }

                DBG_APP_INFO("WinUSB:OSFeature\r\n");
                retStatus = Cy_USB_USBD_SendEndp0Data(
                        pUsbdCtxt, (uint8_t *)glOsFeature, wLength);
                if (retStatus == CY_USBD_STATUS_SUCCESS) {
                    isReqHandled = true;
                }
            }
        }
    }

    /* SET_SEL request is supposed to have an OUT data phase of 6 bytes. */
    if ((bRequest == CY_USB_SC_SET_SEL) && (wLength == 6)) {
        /* SET_SEL request is only received in USBSS case and the
         * Cy_USB_USBD_RecvEndp0Data is blocking. */
        retStatus = Cy_USB_USBD_RecvEndp0Data(
                pUsbdCtxt, (uint8_t *)SetSelDataBuffer, wLength);
        DBG_APP_INFO("SET_SEL: EP0 recv stat = %d, Data=%x:%x\r\n", retStatus,
                SetSelDataBuffer[0], SetSelDataBuffer[1]);
        isReqHandled = true;
    }

    /* If Request is not handled by the callback, Stall the command. */
    if (!isReqHandled) {

        DBG_APP_ERR("SetupRequest is not "
                "handled.bType=0x%x,bRequest:0x%x,wLength:0x%x, wIndex:0x%x,wValue:0x%x\r\n",
                bType, bRequest, wLength, wIndex, wValue);
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN,
                TRUE);
    }

} /* end of function. */

/*
 * Function: Cy_USB_AppSuspendCallback()
 * Description: This Function will be called by USBD  layer when
 *              Suspend signal/message is detected.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t, cy_stc_usb_cal_msg_t
 * return: void
 */
void Cy_USB_AppSuspendCallback(void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->prevDevState = pUsbApp->devState;
    pUsbApp->devState = CY_USB_DEVICE_STATE_SUSPEND;
} /* end of function. */

/*
 * Function: Cy_USB_AppResumeCallback()
 * Description: This Function will be called by USBD  layer when
 *              Resume signal/message is detected.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t, cy_stc_usb_cal_msg_t
 * return: void
 */
void Cy_USB_AppResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    cy_stc_usb_app_ctxt_t *pUsbApp;
    cy_en_usb_device_state_t tempState;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;

    tempState = pUsbApp->devState;
    pUsbApp->devState = pUsbApp->prevDevState;
    pUsbApp->prevDevState = tempState;
    return;
} /* end of function. */

/*
 * Function: Cy_USB_AppSetIntfCallback()
 * Description: This Function will be called by USBD  layer when
 *              set interface is called.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t, cy_stc_usb_cal_msg_t
 * return: void
 */
void Cy_USB_AppSetIntfCallback(void *pAppCtxt,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    DBG_APP_INFO("AppSetIntfCallback\r\n");

    /* We can stall the SET_INTERFACE request as only one alternate
     * setting is supported per interface by this application.
     */
    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
} /* end of function. */

/*
 * Function: Cy_USB_AppZlpCallback()
 * Description: This Function will be called by USBD layer when
 *              ZLP message comes.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t, cy_stc_usb_cal_msg_t
 * return: void
 */
void Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    /* OUT-ZLP is not expected in application use case. Even if we do receive it, we
     * can simply discard it and move on. */
    if (pMsg->type == CY_USB_CAL_MSG_OUT_ZLP) {
        Cy_USBD_ClearZlpSlpIntrEnableMask(pUsbdCtxt, (uint8_t)pMsg->data[0],
                CY_USB_ENDP_DIR_OUT, true);
    }

    return;
} /* end of function. */

/*
 * Function: Cy_USB_AppL1SleepCallback()
 * Description: This Function will be called by USBD layer when
 *              L1 Sleep message comes.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t, cy_stc_usb_cal_msg_t
 * return: void
 */
void Cy_USB_AppL1SleepCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    DBG_APP_INFO("AppL1SleepCb\r\n");
    return;
} /* end of function. */

/*
 * Function: Cy_USB_AppL1ResumeCallback()
 * Description: This Function will be called by USBD layer when
 *              L1 Resume message comes.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t, cy_stc_usb_cal_msg_t
 * return: void
 */
void Cy_USB_AppL1ResumeCallback(void *pUsbApp,
        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    DBG_APP_INFO("AppL1ResumeCb\r\n");
    return;
} /* end of function. */

/*
 * Function: Cy_USB_AppSlpCallback()
 * Description: This Function will be called by USBD layer when
 *              SLP message comes.
 * Parameter: pAppCtxt, cy_stc_usb_usbd_ctxt_t, cy_stc_usb_cal_msg_t
 * return: void
 */
void Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
        cy_stc_usb_cal_msg_t *pMsg) {
    return;
} /* end of function. */


XIP_ATTRIBUTE void Cy_HandleBootSwitchEvent(void)
{
    Cy_GPIO_Inv(USER_LED_PORT, USER_LED_PIN);
}


void Cy_USB_DeviceTaskHandler(void *pTaskParam)
{
    cy_stc_usbd_app_msg_t queueMsg;
    BaseType_t xStatus;
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pTaskParam;

    /* If VBus is present, enable the USB connection. */
    pAppCtxt->vbusPresent =
        (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) ==
         VBUS_DETECT_STATE);
    if (pAppCtxt->vbusPresent) {
        (void)Cy_USB_SSConnectionEnable(pAppCtxt);
    }

    do {
        /*
         * Wait until some data is received from the queue.
         * Timeout after 100 ms.
         */
#if ENABLE_USB_EVENT_LOGS
        Cy_App_PrintUSBEventLogs(pAppCtxt->pUsbdCtxt);
#endif
        Cy_App_PrintEqualizationResults(pAppCtxt->pUsbdCtxt->pSsCalCtxt);
        xStatus = xQueueReceive(pAppCtxt->DeviceQueue, &queueMsg, 100);
        if (xStatus != pdPASS) {
            continue;
        }

        switch (queueMsg.type) {
            case CY_USB_VBUS_CHANGE_INTR:
                /* Start the debounce timer. */
                xTimerStart(pAppCtxt->vbusDebounceTimer, 0);
                break;

            case CY_USB_VBUS_CHANGE_DEBOUNCED:
                /* Check whether VBus state has changed. */
                pAppCtxt->vbusPresent =
                    (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) ==
                     VBUS_DETECT_STATE);

                if (pAppCtxt->vbusPresent) {
                    if (!pAppCtxt->usbConnected) {
                        DBG_APP_INFO(
                                "Enabling USB connection due to VBus detect\r\n");
                        (void)Cy_USB_SSConnectionEnable(pAppCtxt);
                    }
                } else {
                    if (pAppCtxt->usbConnected) {
                        DBG_APP_INFO(
                                "Disabling USB connection due to VBus removal\r\n");
                        Cy_USB_SSConnectionDisable(pAppCtxt);
                    }
                }
                break;
            case CY_USB_BOOT_SWITCH_INTR:
                DBG_APP_INFO("Boot switch interrupt detected\r\n");
                Cy_HandleBootSwitchEvent();
                break;

            default:
                DBG_APP_INFO("MsgDefault\r\n");
                break;
        } /* end of switch() */
    } while (1);
}

/*[EOF]*/
