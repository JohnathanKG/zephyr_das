options gen2
options stack = 2048
options heap_size_hint = 4096
options string_heap_size_hint = 2048
require zephyr;
require zephyr_hooks

var led = gpio_led0_spec()

[export]
def main() {
    if (!gpio_is_ready_dt(unsafe(addr(led)))) {
        return 0;
    }
    var ret = gpio_pin_configure_dt(unsafe(addr(led)), GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        return 0;
    }

    k_msleep(1000000);
    
    return 0;
}

[on_input]
def button_input_callback(evt : input_event; arg : void?) {
    if (evt.value == 0) {
        return;
    }

    var ret = gpio_pin_toggle_dt(unsafe(addr(led)));
    if (ret < 0) {
        return;
    }
    k_msleep(1000);
    gpio_pin_toggle_dt(unsafe(addr(led)));
}