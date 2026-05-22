# Examples

This `examples/` folder contains small applications that demonstrate how to use various aspects of tock and `libents`. See individual app directories for more information on the functionality of each example.

## Template Example

The `template/` directory contains a simple example that can be used as a starting point for writing new tests. It demonstrates how to print out to the console and add delays between prints.

## Creating a new example

1. Copy the `./template` directory and rename it to match the source files that are being tested.
2. Edit the `PACKAGE_NAME` in the `Makefile` to match the new directory name.
