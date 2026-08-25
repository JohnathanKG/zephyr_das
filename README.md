# zephyr_das

## Setup

Install west in a virtual environment in this directory, or optionally globally:

```sh
pip3 install -U west
```

On Linux you can instead use:

```sh
pip3 install --user -U west
```

Then initialize and update west (this may take a while):

```sh
west init
west update
```

Install the ARM Zephyr toolchain:

```sh
west sdk install --toolchain=arm-zephyr-eabi
```

## Generate bindings

> The following is only for the `stm32f4xx` series. I have no ability to test with further microprocessors.
> You will have to tune `bind_gen/include_dirs.txt` to your processor otherwise, but the steps are otherwise the same.

```sh
cd bind_gen
mkdir build && cd build
cmake -G Ninja ../
cmake --build . -t regen-binds
```

## Build and flash

From the `zephyr_das` directory:

```sh
west build -p -b nucleo_f446re projects/app
west flash
```

That's it — you will have a reactive button.

## Renode simulation

TODO: Renode simulation environment instructions

1. Install [Renode](https://github.com/renode/renode).
2. Run:
  ```sh
   cd renode
   renode main.resc
  ```
   If you built from source, use `renode --ui main.resc` so you can see it react in the **Sensors** view in realtime.
3. Toggle the button:
  ```
   sysbus.gpioc.bluebutton Toggle
  ```
   Run this once or twice (it works fine on a physical MCU; I am not sure what requires one vs two button presses to enable).

It will print a minmaxheap when toggling the button.