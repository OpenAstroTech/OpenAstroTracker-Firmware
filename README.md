# OpenAstroTracker-Firmware
Official firmware for the OpenAstroTracker.

## Coding guidelines

TBD

## Contribution

This is an open source project and everyone is welcome to contribute. We will be following these rules while reviewing your pull request:
- The pull request consists only of the changes related to its particular feature or bugfix. If there are multiple unrelated changes which should be merged into this repository, you have to create a separate pull request for each of them. 
- The pull request builds correctly. If it doesn't, please fix the issues and push them to the source branch. You can use the matrix_build.py script to build all the important configurations locally (works similar to our CI).
- The pull request can only be merged after all comments were resolved BY **OAT DEVELOPERS**. Please don't resolve the comments yourself since this can lead to missed issues.
- If the pull request is not maintained by its author in a reasonably prompt manner after a review, the developers can decide to close it without merging since the accumulated merge conflicts and original code changes could lead to massive efforts. You can then still recreate your pull request after applying all the required changes on your fork branch.

## Development

Even if Arduino IDE is supported, we highly recommend using VSCode with [PlatformIO](https://platformio.org/) for development. It allows automatic dependency management, powerful IDE, debugging, automatic build flags definition and more.
