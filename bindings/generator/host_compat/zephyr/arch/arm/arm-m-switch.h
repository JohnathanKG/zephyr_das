#pragma once

/*
 * Host stub of Zephyr's Cortex-M switch header. The real file casts pointers
 * to uint32_t and uses ARM register asm ("r4"), which is a hard error on a
 * 64-bit host compiler.
 */

#include <stdint.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/kernel/thread.h>

#ifdef CONFIG_ARM_GCC_FP_WORKAROUND
#define _R7_CLOBBER_OPT(expr) expr
#else
#define _R7_CLOBBER_OPT(expr) /**/
#endif

#if defined(CONFIG_CPU_CORTEX_M4) || defined(CONFIG_CPU_CORTEX_M7) || defined(CONFIG_ARMV8_M_DSP)
#define _ARM_M_SWITCH_HAVE_DSP
#endif

void *arm_m_new_stack(char *base, uint32_t sz, void *entry, void *arg0, void *arg1, void *arg2,
		      void *arg3);
bool arm_m_must_switch(void);
void arm_m_exc_exit(void);
bool arm_m_iciit_check(uint32_t msp, uint32_t psp, uint32_t lr);
void arm_m_iciit_stub(void);

extern uint32_t *arm_m_exc_lr_ptr;
void z_arm_configure_dynamic_mpu_regions(struct k_thread *thread);
extern uintptr_t z_arm_tls_ptr;
extern uint32_t arm_m_switch_stack_buffer;

struct arm_m_cs_ptrs {
	void *out, *in, *lr_save, *lr_fixup;
};

extern struct arm_m_cs_ptrs arm_m_cs_ptrs;

static inline void arm_m_exc_tail(void)
{
}

static inline void arm_m_switch(void *switch_to, void **switched_from)
{
	(void)switch_to;
	(void)switched_from;
}

#ifdef CONFIG_USE_SWITCH
static inline void arch_switch(void *switch_to, void **switched_from)
{
	arm_m_switch(switch_to, switched_from);
}
#endif
