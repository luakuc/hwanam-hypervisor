#include <stdio.h>
#include <stdlib.h>
#include <asm/asm.h>
#include <arch/armv7.h>

#include <debug.h>

#include <vdev.h>

#include "hvc_trap.h"

static void _trap_dump_bregs(void)
{
    uint32_t spsr, lr, sp;

    printf(" - banked regs\n");
    asm volatile(" mrs     %0, sp_usr\n\t" : "=r"(sp) : : "memory", "cc");
    asm volatile(" mrs     %0, lr_usr\n\t" : "=r"(lr) : : "memory", "cc");
    printf(" - usr: sp:%x lr:%x\n", sp, lr);
    asm volatile(" mrs     %0, spsr_svc\n\t" : "=r"(spsr) : : "memory", "cc");
    asm volatile(" mrs     %0, sp_svc\n\t" : "=r"(sp) : : "memory", "cc");
    asm volatile(" mrs     %0, lr_svc\n\t" : "=r"(lr) : : "memory", "cc");
    printf(" - svc: spsr:%x sp:%x lr:%x\n", spsr, sp, lr);
    asm volatile(" mrs     %0, spsr_irq\n\t" : "=r"(spsr) : : "memory", "cc");
    asm volatile(" mrs     %0, sp_irq\n\t" : "=r"(sp) : : "memory", "cc");
    asm volatile(" mrs     %0, lr_irq\n\t" : "=r"(lr) : : "memory", "cc");
    printf(" - irq: spsr:%x sp:%x lr:%x\n", spsr, sp, lr);
}

int do_hvc_trap(struct core_regs *regs)
{
    int32_t vdev_num = -1;

    uint32_t hsr = read_hsr();
    uint32_t ec = (hsr & HSR_EC_BIT) >> EXTRACT_EC;
    uint32_t iss = hsr & HSR_ISS_BIT;
    uint32_t far = read_hdfar();
    uint32_t fipa;
    uint32_t srt;

    struct arch_vdev_trigger_info info;
    int level = VDEV_LEVEL_LOW;

    fipa = (read_hpfar() & HPFAR_FIPA_MASK) >> HPFAR_FIPA_SHIFT;
    fipa = fipa << HPFAR_FIPA_PAGE_SHIFT;
    fipa = fipa | (far & HPFAR_FIPA_PAGE_MASK);

    info.ec = ec;
    info.iss = iss;
    info.fipa = fipa;
    info.sas = (iss & ISS_SAS_MASK) >> ISS_SAS_SHIFT;

    srt = (iss & ISS_SRT_MASK) >> ISS_SRT_SHIFT;
    info.value = &(regs->gpr[srt]);
    info.raw = &(regs->gpr[srt]);

    switch (ec) {
    case TRAP_EC_ZERO_UNKNOWN:
    case TRAP_EC_ZERO_WFI_WFE:
    case TRAP_EC_ZERO_MCR_MRC_CP15:
    case TRAP_EC_ZERO_MCRR_MRRC_CP15:
    case TRAP_EC_ZERO_MCR_MRC_CP14:
    case TRAP_EC_ZERO_LDC_STC_CP14:
    case TRAP_EC_ZERO_HCRTR_CP0_CP13:
    case TRAP_EC_ZERO_MRC_VMRS_CP10:
    case TRAP_EC_ZERO_BXJ:
    case TRAP_EC_ZERO_MRRC_CP14:
    case TRAP_EC_NON_ZERO_SVC:
    case TRAP_EC_NON_ZERO_SMC:
    case TRAP_EC_NON_ZERO_PREFETCH_ABORT_FROM_OTHER_MODE:
    case TRAP_EC_NON_ZERO_PREFETCH_ABORT_FROM_HYP_MODE:
    case TRAP_EC_NON_ZERO_DATA_ABORT_FROM_HYP_MODE:
        level = VDEV_LEVEL_HIGH;
        break;
    case TRAP_EC_NON_ZERO_HVC:
        level = VDEV_LEVEL_MIDDLE;
        break;
    case TRAP_EC_NON_ZERO_DATA_ABORT_FROM_OTHER_MODE:
        level = VDEV_LEVEL_LOW;
        break;
    default:
        debug_print("[hyp] do_hvc_trap:unknown hsr.iss= %x\n", iss);
        debug_print("[hyp] hsr.ec= %x\n", ec);
        debug_print("[hyp] hsr= %x\n", hsr);
        goto trap_error;
    }

    vdev_num = vdev_find(level, &info);
    if (vdev_num < 0) {
        debug_print("[hvc] cann't search vdev number\n\r");
        goto trap_error;
    }

    if (iss & ISS_WNR) {
        if (vdev_write(level, vdev_num, &info) < 0) {
            goto trap_error;
        }
    } else {
        if (vdev_read(level, vdev_num, &info) < 0) {
            goto trap_error;
        }
    }
    vdev_post(level, vdev_num, &info, regs);

    return 0;

trap_error:
    _trap_dump_bregs();
    printf("fipa is %x guest pc is %x\n", fipa, regs->pc);
    while(1)
        ;
    //abort();
    return -1;
}
