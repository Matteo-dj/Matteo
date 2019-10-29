/*----------------------------------------------------------------------------
 * Copyright (c) <2013-2015>, <Huawei Technologies Co., Ltd>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *---------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * Notice of Export Control Law
 * ===============================================
 * Huawei LiteOS may be subject to applicable export control laws and regulations, which might
 * include those applicable to Huawei LiteOS of U.S. and the country in which you are located.
 * Import, export and usage of Huawei LiteOS in any manner by you shall be in compliance with such
 * applicable export control laws and regulations.
 *---------------------------------------------------------------------------*/

#include "los_hwi.h"
#include "los_memory.h"
#ifdef LOSCFG_LIB_LIBC
#include "string.h"
#endif
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

LITE_OS_SEC_DATA_MINOR UINT32  g_vuwIntCount = 0;
LITE_OS_SEC_BSS_MINOR HWI_HANDLE_FORM_S m_astHwiForm[OS_HWI_MAX_NUM];
#ifdef LOSCFG_SHELL
LITE_OS_SEC_BSS_MINOR UINT32 g_vuwHwiFormCnt[OS_HWI_MAX_NUM];
#endif


/*****************************************************************************
 Function    : osHwiInit
 Description : initialization of the hardware interrupt
 Input       : None
 Output      : None
 Return      : None
 *****************************************************************************/
LITE_OS_SEC_TEXT_INIT VOID osHwiInit(void)
{
    UINT32 uwHwiNum;

    for (uwHwiNum = 0; uwHwiNum < OS_HWI_MAX_NUM; uwHwiNum++)
    {
        m_astHwiForm[uwHwiNum].pfnHook = (HWI_PROC_FUNC)NULL; /*lint !e611*/
        m_astHwiForm[uwHwiNum].uwParam = 0;
        m_astHwiForm[uwHwiNum].pstNext = (struct tagHwiHandleForm *)NULL;
    }
    hal_interrupt_init();

    return ;
}

/*****************************************************************************
 Function    : LOS_HwiCreate
 Description : create hardware interrupt
 Input       : uwHwiNum   --- hwi num to create
               pfnHandler --- hwi handler
               usHwiPrio  --- priority of the hwi
               uwArg      --- param of the hwi handler
 Output      : None
 Return      : OS_SUCCESS on success or error code on failure
 *****************************************************************************/
LITE_OS_SEC_TEXT_INIT UINT32 LOS_HwiCreate( HWI_HANDLE_T  uwHwiNum,
                                       HWI_PRIOR_T   usHwiPrio,
                                       HWI_MODE_T    usMode,
                                       HWI_PROC_FUNC pfnHandler,
                                       HWI_ARG_T     uwArg
                                      )
{
    UINTPTR uvIntSave;
    HWI_HANDLE_FORM_S *pstHwiForm;
    HWI_HANDLE_FORM_S *pstHwiFormNode = (HWI_HANDLE_FORM_S*)NULL;

    if (NULL == pfnHandler)
    {
        return OS_ERRNO_HWI_PROC_FUNC_NULL;
    }

    if (uwHwiNum > OS_USER_HWI_MAX || uwHwiNum < OS_USER_HWI_MIN) /*lint !e568 !e685*/
    {
        return OS_ERRNO_HWI_NUM_INVALID;
    }

    uvIntSave = LOS_IntLock();

    pstHwiForm = &m_astHwiForm[uwHwiNum];
    
#if OS_HWI_SHARE_ENABLE
    while(NULL != pstHwiForm->pstNext)
    {
        pstHwiForm = pstHwiForm->pstNext;
        if(pstHwiForm->uwParam == uwArg)    /*lint !e413*/
            goto HANDLERONLIST;
    }

    pstHwiFormNode = (HWI_HANDLE_FORM_S *)LOS_MemAlloc( m_aucSysMem0, sizeof(HWI_HANDLE_FORM_S ));
    if(NULL == pstHwiFormNode)
    {
        LOS_IntRestore(uvIntSave);
        return OS_ERRNO_HWI_NO_MEMORY;
    }
    pstHwiFormNode->pfnHook = pfnHandler;
    pstHwiFormNode->uwParam = uwArg;
    pstHwiFormNode->pstNext = (struct tagHwiHandleForm *)NULL;
    pstHwiForm->pstNext = pstHwiFormNode;
#else
    (void)pstHwiFormNode;
    if (pstHwiForm->pfnHook != NULL &&
        (uwHwiNum == NUM_HAL_INTERRUPT_TIMER))
    {
        goto HANDLERONLIST;
    }
    
    pstHwiForm->pfnHook = pfnHandler;
    pstHwiForm->uwParam = uwArg;
#endif
   

    LOS_IntRestore(uvIntSave);
    return LOS_OK;

HANDLERONLIST:
    LOS_IntRestore(uvIntSave);
    return OS_ERRNO_HWI_ALREADY_CREATED;
}

/*****************************************************************************
 Function    : LOS_HwiDelete
 Description : Must mask the interrupt before delete hardware interrupt
 Input       : irq   --- hwi num to create
               uwArg      --- param of the hwi handler
 Output      : None
 Return      : NONE
 *****************************************************************************/
void LOS_HwiDelete(unsigned int irq, HWI_ARG_T uwArg)
{
    HWI_HANDLE_FORM_S *pstHwiForm = (HWI_HANDLE_FORM_S*)NULL;
    HWI_HANDLE_FORM_S *pstHwiFormtmp;
    UINTPTR uvIntSave;

    if(OS_INT_ACTIVE)
        return ;

    if (irq > OS_USER_HWI_MAX || irq < OS_USER_HWI_MIN)/*lint !e685 !e568*/
        return;

    uvIntSave = LOS_IntLock();
    pstHwiFormtmp = &m_astHwiForm[irq];
#if OS_HWI_SHARE_ENABLE
    pstHwiForm = pstHwiFormtmp->pstNext;

    while (pstHwiForm)
    {
        if(uwArg == pstHwiForm->uwParam)
        {
            pstHwiFormtmp->pstNext =  pstHwiForm->pstNext;
            LOS_MemFree(m_aucSysMem0, (void *)pstHwiForm);/*lint !e424*/
            break;
        }
        else
        {
            pstHwiFormtmp = pstHwiForm;
            pstHwiForm = pstHwiForm->pstNext;  
        }
    }
    
    pstHwiFormtmp = &m_astHwiForm[irq];
    if (pstHwiFormtmp->pstNext == NULL)
    {
        hal_interrupt_mask(irq);
    }
#else
    (void)pstHwiForm;
    pstHwiFormtmp->pfnHook = NULL;/*lint !e64*/
    pstHwiFormtmp->uwParam = 0;
    hal_interrupt_mask(irq);
#endif
    LOS_IntRestore(uvIntSave);
}

#ifdef LOSCFG_INTERWORK_THUMB
//LITE_OS_SEC_TEXT_INIT  VOID LOS_IntRestore(UINTPTR uvIntSave)
VOID LOS_IntRestore(UINTPTR uvIntSave)
{
    __asm__ __volatile__(
        "MSR     cpsr_c, %0"
        ::"r"(uvIntSave) );
}
UINTPTR LOS_IntLock(VOID)
{
      UINT32 uwRet = 0,temp;
    __asm__ __volatile__(
        "MRS %0,cpsr\n"
        "ORR %1,%0,#0xc0\n"
        "MSR cpsr_c,%1"
        :"=r"(uwRet),  "=r"(temp)
        :
        :"memory");
    return uwRet;
} /*lint !e529*/
UINTPTR LOS_IntUnLock(VOID)
{
    UINTPTR uwCpsSave = 0;
    __asm__ __volatile__(
        "MRS   %0, cpsr\n"
        "BIC   %0, %0, #0xc0\n"
        "MSR   cpsr_c, %0"
        :"=r"(uwCpsSave) ::);
    return uwCpsSave;

}
#endif
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

