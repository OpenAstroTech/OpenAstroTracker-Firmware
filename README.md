# OpenAstroTracker-Firmware
Official firmware for the OpenAstroTracker.

## Change log
See the [Changelog](Changelog.md) for details about what versions made what changes.

## Coding guidelines

See `.clang-format` file. A GitHub action is run on every PR to make sure that the code complies with the formatting guidelines. The action will automatically commit any changes, so you may have to `git pull` after opening a PR.

## Contribution

This is an open source project and everyone is welcome to contribute. We will be following these rules while reviewing your pull request:
- The pull request consists **only** of the **changes related to its particular feature or bugfix**. If there are multiple unrelated changes which should be merged into this repository, you have to create a separate pull request for each of them. 
- The pull request **builds correctly**. If it doesn't, please fix the issues and push them to the source branch. You can use the matrix_build.py script to build all the important configurations locally (works similar to our CI).
- The pull request can only be merged **after** all comments were resolved BY **OAT DEVELOPERS**. Please don't resolve the comments yourself since this can lead to missed issues.
- If the pull request is not maintained by its author in a reasonably prompt manner after a review, the developers can decide to close it without merging since the accumulated merge conflicts and original code changes could lead to massive efforts. You can then still recreate your pull request after applying all the required changes on your fork branch.

## Development

Even if Arduino IDE is supported, we highly recommend using VSCode with [PlatformIO](https://platformio.org/) for development. It allows automatic dependency management, powerful IDE, debugging, automatic build flags definition and more.

### Debugging

#### ATmega2560

> :warning: **Debugging is only supported on mega2560 platforms at the moment!**

For this example we will be using the `mega2560` environment, but you can use any derived environment as well

> You may need to set `debug_port` in your `platformio.ini`, platformio says it will auto-detect the port but it doesn't seem to be working at the moment

Start a gdb shell debugging the current firmware:
```shell
pio run -e mega2560 -t clean  # Clean the environment
piodebuggdb -e mega2560  # Initialize a debug session.
# This will build the firmware in debug mode, and then initialize a remote gdb session
# You may have visual debug capabilities in your IDE if it has platformio integration as well
```

When using `avr-stub` as a debug interface, it requires 2 things:
1. Serial link 0 must not be used in the firmware
    - As such, all external interfaces using `Serial` are disabled in a debug build (`Serial1`, `Serial2` etc are ok)
2. Exclusive access to an interrupt vector
    - This requires hot-patching the arduino framework (specifically `WInterrupts.c`) to disable the ISR registration. The implementation of this is in `pre_script_patch_debug.py`, which should happen automagically

> Note that while avr-stub is in RAM mode, the firmware will run very slowly and timing-related functions might not work correctly

Debugging is still a bit flakey, so you may need to try multiple times in order to get a solid debugging session.

More information is available in the [avr-stub documentation](https://github.com/jdolinay/avr_debug/tree/master/doc)
