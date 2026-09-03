# Contributing Guidelines

Thank you for your interest in contributing to our project!

## Getting Started

Before you start, please make sure to have a look at our [Code of
Conduct](./CODE_OF_CONDUCT.md), which outlines our expectations for participants
within our community. We use the
[Contributor Covenant](https://www.contributor-covenant.org) with a copy
provided within the repository.

This repository is primarily based on [libtock-c](https://github.com/tock/libtock-c) and the TockOS project. We recommend following the getting started guide at https://book.tockos.org/ to familiarize yourself with the build system. The website itself is a good reference for understanding how the system works.

## Remove Development Instance

We host a remote development environment so that contributors don't need physical access to the hardware. The development instance has the necessary packages installed for development. Previously we've had issues getting hardware to contributors outside of the United States.

### Getting Access

1. Contact @jmadden173 (jtmadden@ucsc.edu) to request access. Provide public SSH key and public email.
2. SSH in with provided login name and ip.
3. Run `./check_tools.sh` located in your home directory. *If packages are not installing, try `pipx ensurepath`.*
4. Run `git config --global user.name "[YOUR NAME]"`.
5. Run `git config --global user.email "[YOUR EMAIL]"`.
6. Close and re-open the SSH connection to refresh environment variables.

From here you can develop directly from the SSH connector. Alternatively you can use VSCode locally or through an RDP session.

### VSCode (local)

> See https://code.visualstudio.com/docs/remote/ssh for more information on how to use this functionality.

1. Download 'Remote - SSH' extension on VSCode extensions.
2. Go to 'Remote - SSH' extension page on VSCode.
3. Press '+' icon and add 'username@ip' to list of ssh hosts.
4. Click '->' to 'Connect to Current Window'.

### VSCode (remote)

A development desktop is accessible through RDP. We recommend [Remina](https://remmina.org/) as the RDP client. GNOME was chosen as the desktop with VSCode installed.

## FAQ

### Why can't I build apps with source files in subdirectory?

This is a limitation of `libtock-c` that was found when creating the repository.

### I updated `libents` library, but I'm not seeing the changes in the app.

This is another limitation of `libtock-c`. See [this](https://github.com/tock/libtock-c/issues/582) issue for more information.

### Why printing with `%f` formatting show nothing?

Floating point printing support is not enabled in `newlib` due to the amount to flash space is consumes. As a workaround you can print the value in hex with `%x`. [Online tools](https://www.h-schmidt.net/FloatConverter/IEEE754.html) are available to convert from hex back to a float.

### Local submodule commits do not match GitHub commits

Run the following. `sync` ensures the paths match whats listed in `.gitmodules`. `update` forces it to update to the listed commit.

> Warning: This is destructive if you were making changes to submodules with the intention of pushing them upstream. If you are not modifying any submodules then it can be safely ran.

```
git submodule sync --recursive
git submodule update --init --recursive --force
```

### Why are logs not being saved to SD card

Check esp32 logs for errors relating to the SD card. Ensure the SD card is **FAT32** formatted. The disk partition can be reformatted with the following command.

```
mkfs.fat -F 32 /dev/[PARTITION]
```

### Why are SD cards being truncated?

Only a max of **240 bytes** can be logged at once.
