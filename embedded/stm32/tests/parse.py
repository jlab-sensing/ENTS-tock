#!/usr/bin/env python

import argparse
from pathlib import Path
import junit_xml
import unity_test_parser
import serial

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the test from a serial port or an input file."
    )

    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument(
        "-p",
        "--port",
        help="Serial port (e.g. /dev/ttyUSB0 or COM3).",
    )
    source.add_argument(
        "-i",
        "--input",
        type=Path,
        help="Read input from a file instead of a serial port.",
    )

    parser.add_argument(
        "--save-input",
        type=Path,
        metavar="FILE",
        help="Save received input to a file.",
    )

    parser.add_argument(
        "--xml",
        type=Path,
        metavar="FILE",
        help="Write JUnit XML results to the specified file.",
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action="count",
        default=0,
        help="Increase logging verbosity (repeat for more verbosity).",
    )

    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.port:
        print(f"Reading from {args.port}")

        # TODO Read from pyserial


        contents = b""

        with serial.Serial(args.port, 115200) as ser:
            # Read test output
            while True:
                line = ser.readline()

                contents += line

                # look for summary
                if line == "-----------------------":
                    break

            # Read remaining two lines
            for _ in range(2):
                line = ser.readline()

                contents += line

        # decode data
        contents = contents.decode("utf-8", errors="replace")

        # Save serial output
        if args.save_input:
            print(f"Saving input to: {args.save_input}")
            with open(args.save_input, "w") as results_file:
                results_file.write(contents)

        print()
        print(contents)
        print()

        results = unity_test_parser.TestResults(contents)

    else:
        print(f"Reading from file: {args.input}")

        with open(args.input, "r") as results_file:
            contents = results_file.read()

            print()
            print(contents)
            print()

            results = unity_test_parser.TestResults(contents)


    # save junit xml
    if args.xml:
        print(f"Writing JUnit XML to: {args.xml}")
        with open(args.xml, "w") as out_file:
            junit_xml.TestSuite.to_file(out_file, [results.to_junit()])

    # return error code if failure
    if results.num_failed() > 0:
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
