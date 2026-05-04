# ifd-sscp

**ifd-sscp** is an open-source PC/SC-Lite IFD Handler (reader driver) for Linux systems, enabling support for NFC Readers that implement the **SPAC SSCPv2** protocol in **transparent (coupler) mode**.

It provides a bridge between the PC/SC daemon (`pcscd`) and SSCPv2-compatible NFC readers, making smart cards accessible via standard PC/SC APIs on Linux platforms.

## Features

- Implements a PC/SC IFD Handler compatible with `pcsc-lite`
- Supports transparent mode communication over **SSCPv2**
- Clean and modular codebase, easy to integrate and extend
- Works on Linux systems using standard pcsc-lite infrastructure
- Tested on Linux X64, Linux ARM64 (Raspberry)
- MIT License — free to use, modify, and distribute

## Architecture

This handler sits between `pcscd` and an NFC reader using the SSCPv2 protocol in transparent mode. It communicates over serial or USB interfaces to perform smart card operations.

```
+-------------+      +--------------------------+      +---------------------------+
| Application | <--> | PC/SC Lite (libpcsclite) | <--> | pcscd + ifd-sscp (driver) |
+-------------+      +--------------------------+      +---------------------------+
                                                                     |
                                                           +---------------------+
                                                           | NFC Reader (SSCPv2) |
                                                           +---------------------+
```

## About PC/SC-Lite

[PC/SC-Lite](https://pcsclite.apdu.fr/) is an open-source implementation of the PC/SC (Personal Computer/Smart Card) standard, enabling communication with smart cards using standardized APIs on Unix-like systems.

It provides a daemon (`pcscd`), client libraries, and a plugin interface for reader drivers (IFD Handlers), such as `ifd-sscp`.

More information and source code can be found on the official project website:  
[https://pcsclite.apdu.fr/](https://pcsclite.apdu.fr/)

## Installation

### Prerequisites

- Linux with `pcsc-lite` and development headers
- Reader device supporting **SPAC SSCPv2** in transparent mode
- C compiler, cmake and make

On Debian & Ubuntu, use this command line to install `pcsc-lite` and development headers

```bash
sudo apt update
sudo apt install pcscd pcsc-tools libpcsclite-dev
```

### Build and install

```bash
git clone https://github.com/springcard/ifd-sscp.git
cd ifd-sscp
mkdir build
cd build
cmake ..
make
sudo make install
```

This will compile and install the handler into the appropriate `pcsc-lite` directory (typically `/usr/lib/pcsc/drivers/`).

### Configure

As root or a *sudoer*, create a file named `/etc/reader.conf.d/ifd-sscp`

Edit this file and enter the following content

```
FRIENDLYNAME "IFD SSCP"
DEVICENAME /dev/ttyUSB0
LIBPATH /usr/lib/pcsc/drivers/ifd-sscp.bundle/Contents/Linux/libifd-sscp.so
```

- On line `DEVICENAME`, replace `/dev/ttyUSB0` by the Serial communication device your Reader is connected to.
- On line `LIBPATH`, verify that the path matches the location where `make install` has deployed the binary.

**NB:** Ensure the user has permissions to access the serial or USB device.

### Test

Stop the PC/SC daemon

```bash
sudo service pcscd stop
```

Launch the PC/SC daemon manually

```bash
sudo pcscd -f -d
```

If everything is OK, you must ear your Reader beep, and see positive messages in the console.
Otherwise, observe the log and debug!

## License

This project is licensed under the [MIT License](LICENSE).

This open-source project is provided as-is, without support or maintenance.
Developers and implementers are encouraged to use it responsibly and at their own risk.

---

**SPAC** is a trademark of the Secure Protocol Alliance for Couplers.  
**SpringCard** is a member of SPAC, but this project is not endorsed not promoted by SPAC.

