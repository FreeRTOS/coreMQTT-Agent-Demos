## MQTT Agent and Demo Tasks (including OTA) - Using coreMQTT

[coreMQTT](https://github.com/FreeRTOS/coreMQTT) is an MIT licensed open source C MQTT client library for microcontrollers and small microprocessor based IoT devices. Its design is intentionally simple to ensure it has no dependency on any other library or operating system, and to better enable static analysis including [memory safety proofs](https://www.freertos.org/2020/02/ensuring-the-memory-safety-of-freertos-part-1.html). That simplicity and lack of operating system dependency (coreMQTT does not require multithreading at all) means coreMQTT does not build thread safety directly into its implementation. Instead thread safety must be provided by higher level software. This labs project implements a coreMQTT extension that provides that higher level functionality in the form of an MQTT agent (or MQTT daemon). While the implementation demonstrated here is currently specific to FreeRTOS, there are not many dependencies on FreeRTOS, meaning the implementation can easily be adapted for use with other operating systems.

## Cloning this repository
This repo uses [Git Submodules](https://git-scm.com/book/en/v2/Git-Tools-Submodules) to bring in dependent components.

**Note:** If you download the ZIP file provided by the GitHub UI, you will not get the contents of the submodules. (The ZIP file is also not a valid git repository)

To clone using HTTPS:
```
git clone https://github.com/FreeRTOS/coreMQTT-Agent-Demos.git --recurse-submodules
```
Using SSH:
```
git clone git@github.com:FreeRTOS/coreMQTT-Agent-Demos.git --recurse-submodules
```

If you have downloaded the repo without using the `--recurse-submodules` argument, you need to run:
```
git submodule update --init --recursive
```

## Getting started
The [documentation page](https://freertos.org/mqtt/mqtt-agent-demo.html) for this repository contains information on the MQTT agent and the contained demo project.  There is also a [supplemental documentation page](https://freertos.org/ota/ota-mqtt-agent-demo.html) that describes how to run an Over the Air (OTA) update agent as one of the RTOS tasks that share the same MQTT connection.

## Building Demos
This repository contains build files for the FreeRTOS Windows port - able to be run with the free [Community version of Visual Studio](https://visualstudio.microsoft.com/vs/community/) - and an ARM Cortex M model in the [QEMU hardware emulator](https://www.qemu.org/).

### Visual Studio

1. From the Visual Studio IDE, open the `mqtt_multitask_demo.sln` Visual Studio solution file in the `build/VisualStudio/` directory.

1. Select **Build Solution** from the IDE's **Build** menu

### QEMU Cortex-M3

**Command Line**

1. Ensure that `arm-none-eabi-gcc` from the [GNU ARM Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) and GNU `make` are in your path.

1. From the `build/Cortex-M3_MPS2_QEMU_GCC` directory, run `make`.

**Eclipse IDE**

1. Ensure that `arm-none-eabi-gcc` from the [GNU ARM Embedded Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads) and GNU `make` are in your path.

1. Open the `build/Cortex-M3_MPS2_QEMU_GCC` directory as an Eclipse project.

1. Select `Project -> Build Project` from the Eclipse menu.

In both cases, the generated executable can be found at `build/Cortex-M3_MPS2_QEMU_GCC/output/RTOSDemo.elf`.

## Getting help
You can use your Github login to get support from both the FreeRTOS community and directly from the primary FreeRTOS developers on our [active support forum](https://forums.freertos.org).  The [FAQ](https://www.freertos.org/FAQ.html) provides another support resource.

## Security

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.

## License

This library is licensed under the MIT License. See the [LICENSE](LICENSE.md) file.

