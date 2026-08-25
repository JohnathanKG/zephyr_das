#include <zephyr/kernel.h>
#include "main.das.h"

int main(void)
{
	das::main::Standalone ctx;
	das::main::g_ctx = &ctx;
	ctx.main();
	k_sleep(K_FOREVER);
	return 0;
}
