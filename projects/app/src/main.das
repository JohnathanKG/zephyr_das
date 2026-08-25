options gen2
options stack = 2048
options heap_size_hint = 4096
options string_heap_size_hint = 2048
require zephyr
require ../../../aot_hooks/zephyr_hooks.das
require minmaxheap/minmaxheap

struct BooleanItem {
    sort_by: int

    value: bool
}

struct BoolMinMaxHeap: TMinMaxheap {
    typedef StoredType = BooleanItem
}
var bool_item_arr = [BooleanItem(sort_by = 5,  value = true),
    BooleanItem(sort_by = 65, value = true),
    BooleanItem(sort_by = 80, value = true),
    BooleanItem(sort_by = 25, value = true),
    BooleanItem(sort_by = 37, value = true),
    BooleanItem(sort_by = 8,  value = true),
    BooleanItem(sort_by = 15, value = true),
    BooleanItem(sort_by = 57, value = true),
    BooleanItem(sort_by = 36, value = true),
    BooleanItem(sort_by = 45, value = true),
    BooleanItem(sort_by = 59, value = true),
    BooleanItem(sort_by = 20, value = true),
    BooleanItem(sort_by = 14, value = true),
    BooleanItem(sort_by = 32, value = true),
    BooleanItem(sort_by = 18, value = true)]

var bool_heap_test_1 = new default<BoolMinMaxHeap>.build_from_arr(bool_item_arr,
    @(var typevalue: BooleanItem) {
    return typevalue.sort_by
})

var led = gpio_led0_spec()

[export]
def main() {
    if (!gpio_is_ready_dt(unsafe(addr(led)))) {
        return 0;
    }
    var ret = gpio_pin_configure_dt(unsafe(addr(led)), GPIO_OUTPUT_INACTIVE);
    if (ret <   0) {
        return 0;
    }
    return 0;
}

[on_input]
def button_input_callback(evt : input_event; arg : void?) {
    if (evt.value == 0) {
        return;
    }
    
    bool_heap_test_1.display()
}