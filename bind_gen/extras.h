#pragma once

/*
 * Board/app helpers that are not in public Zephyr headers.
 * Parsed and bound automatically via the generated umbrella.
 */
#include <zephyr/drivers/gpio.h>

static inline gpio_dt_spec gpio_led0_spec(void)
{
#ifdef DAS_ZEPHYR_HOST
	return {};
#else
	return GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#endif
}
