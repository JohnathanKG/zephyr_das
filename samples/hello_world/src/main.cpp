#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "daScript/nano_print.h"
#include "main.das.h"

static void board_print(const char *text) {
	printk("%s", text ? text : "");
}

int main(void) {
	das::das_nano_set_print(&board_print);
	das::main::Standalone ctx;
	das::main::g_ctx = &ctx;  // aot_driver always emits this
	ctx.main();
	k_sleep(K_FOREVER);
	return 0;
}
