# Gamepad Emulation Plugin for Praydog UEVR

This plugin facilitates gamepad emulation from Praydog UEVR injector to software emulation, allowing Steam to recognize gamepad input and integrate it with Keyboard/Mouse mappings.

## Overview

- The plugin does not intend to remap gamepad mappings but seamlessly collaborates with UEVR injector and Steam.
- Its primary purpose is to provide gamepad-to-keyboard/mouse mapping.

### Note
Ensure no physical gamepads are active during the session.

## Getting Started

1. **Download VigEm Bus**
   - Obtain the latest release of VigEm bus from [GitHub](https://github.com/nefarius/ViGEmBus/releases) and install the driver.

2. **Plugin Installation**
   - Place the plugin DLL from either `x64/debug` or `release` into the (UEVR) game configuration folder under `plugins`.
   - Alternatively, compile the plugin using Visual Studio within the community.
