options gen2
options stack = 2048

require zephyr

var led = gpio_led0_spec()

[export]
def main() {
    print("hello\n")
    if (!gpio_is_ready_dt(unsafe(addr(led)))) {
        return 0
    }
    let ret = gpio_pin_configure_dt(unsafe(addr(led)), GPIO_OUTPUT_INACTIVE)
    if (ret < 0) {
        return 0
    }
    return 0
}
