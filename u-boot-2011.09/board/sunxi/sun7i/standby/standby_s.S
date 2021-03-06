/*
;/*
;**********************************************************************************************************************
;*											        eGon
;*						                     the Embedded System
;*									       script parser sub-system
;*
;*						  Copyright(C), 2006-2010, SoftWinners Microelectronic Co., Ltd.
;*                                           All Rights Reserved
;*
;* File    : standby_s.s
;*
;* By      : Jerry
;*
;* Version : V2.00
;*
;* Date	  :
;*
;* Descript:
;**********************************************************************************************************************
*/

/*
;************************************************************************************************************
;*
;*                                             function
;*
;*    函数名称：
;*
;*    参数列表：
;*
;*    返回值  ：
;*
;*    说明    ：
;*
;*
;************************************************************************************************************
*/

	.globl boot_store_sp
boot_store_sp:
	STMFD   sp!, {r0-r3}
	mov  r1, #0x200
	str  sp, [r1]

	mov  pc, lr
/*
;************************************************************************************************************
;*
;*                                             function
;*
;*    函数名称：
;*
;*    参数列表：
;*
;*    返回值  ：
;*
;*    说明    ：
;*
;*
;************************************************************************************************************
*/
	.globl boot_restore_sp
boot_restore_sp:
	mov  r1, #0x200
	ldr  sp, [r1]
	LDMFD   sp!, {r0-r3}

	mov  pc, lr
/*
;************************************************************************************************************
;*
;*                                             function
;*
;*    函数名称：
;*
;*    参数列表：
;*
;*    返回值  ：
;*
;*    说明    ：
;*
;*
;************************************************************************************************************
*/
	.globl boot_set_sp
boot_set_sp:
	ldr  sp, =0xb400
	mov  pc, lr

	.globl boot_halt
boot_halt:
	wfi


