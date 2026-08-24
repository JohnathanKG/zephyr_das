## zephyr_das

Install west in a virtual environment located here, or optionally globally. Via `pip3 install -U west` or (linux) `pip3 install --user -U west`

Proceed to run `west init` and `west update`.  It might take some time.
Install the `arm-zephyr-eabi`toolchain with `west sdk install --toolchain=arm-zephyr-eabi`
Enter `bind-gen`, `mkdir build && cd build` 

    The following will be only for `stm32f4xx` series. I have no ability to test with further microprocessors.
    You will have to tune `bind_gen/include_dirs.txt` to your processor otherwise, but the steps are otherwise the same.
    
Run `cmake -G Ninja ../ && cmake --build .  -t regen-binds`
Go to the `zephyr_das` directory and run `west build -p -b nucleo_f446re projects/app`
Flash your processor with `west flash` and that's it! You will have a reactive button. 

TODO: Renode simulation environment instructions

Renode guide:
Install `renode` from https://github.com/renode/renode
Run `cd renode` then `renode main.resc` (if you build from source, use `renode --ui main.resc` and you can see it react in `Sensors` view in realtime!)
Run `sysbus.gpioc.bluebutton Toggle` once or twice (it works fine on physical MCU. I am not sure what requires once or twice button presses to enable)
It will print `toggle led` when toggling the led! 