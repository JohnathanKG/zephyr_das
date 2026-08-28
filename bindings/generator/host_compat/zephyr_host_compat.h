#pragma once

/*
 * Host-side shims so generated dasZephyr bindings can compile as a macOS
 * daslang plugin. Cortex-M headers assume ARM builtins and BASEPRI that
 * Apple Clang (AArch64/x86_64) does not provide.
 *
 * Do not #undef __ARM_FP here — daScript/vecmath needs it for NEON.
 */

#ifndef __builtin_arm_get_fpscr
#define __builtin_arm_get_fpscr() (0U)
#endif
#ifndef __builtin_arm_set_fpscr
#define __builtin_arm_set_fpscr(fpscr) ((void)(fpscr))
#endif

#ifndef __get_BASEPRI
static inline unsigned int __get_BASEPRI(void)
{
	return 0U;
}
#endif

#ifndef __set_BASEPRI
static inline void __set_BASEPRI(unsigned int key)
{
	(void)key;
}
#endif

#ifndef __set_BASEPRI_MAX
static inline void __set_BASEPRI_MAX(unsigned int prio)
{
	(void)prio;
}
#endif

#ifndef __get_PRIMASK
static inline unsigned int __get_PRIMASK(void)
{
	return 0U;
}
#endif
