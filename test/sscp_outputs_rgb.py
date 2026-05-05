#!/usr/bin/env python3
"""Minimal pyscard test for ifd-sscp SSCP_OutputsRGB through SCardControl."""

import argparse
import sys
from time import sleep

from smartcard.scard import (
    SCARD_PROTOCOL_T0,
    SCARD_PROTOCOL_T1,
    SCARD_SCOPE_SYSTEM,
    SCARD_S_SUCCESS,
    SCARD_SHARE_DIRECT,
    SCardConnect,
    SCardControl,
    SCardDisconnect,
    SCardEstablishContext,
    SCardGetErrorMessage,
    SCardListReaders,
    SCardReleaseContext,
    SCARD_LEAVE_CARD,
    SCARD_CTL_CODE,
)


SCARD_PROTOCOL_UNDEFINED = 0
SSCP_CMD_OUTPUT_RGB = 0x000050

COLORS = {
    "red": 0xFF0000,
    "green": 0x00FF00,
    "blue": 0x0000FF,
    "off": 0x000000,
}


def parse_byte(value):
    number = int(value, 0)
    if not 0 <= number <= 0xFF:
        raise argparse.ArgumentTypeError("must be in range 0..255")
    return number


def check(hresult, where):
    if hresult != SCARD_S_SUCCESS:
        raise RuntimeError(f"{where}: {SCardGetErrorMessage(hresult)}")


def select_reader(reader_names, wanted):
    if not reader_names:
        raise RuntimeError("no PC/SC reader found")
    if wanted is None:
        return reader_names[0]
    if wanted.isdigit():
        index = int(wanted)
        try:
            return reader_names[index]
        except IndexError as exc:
            raise RuntimeError(f"reader index out of range: {index}") from exc
    matches = [name for name in reader_names if wanted.lower() in name.lower()]
    if len(matches) != 1:
        raise RuntimeError(f"reader selector must match exactly one reader: {wanted!r}")
    return matches[0]


def outputs_rgb(hcard, control_code, rgb, led_duration, buzzer_duration):
    payload = [
        (rgb >> 16) & 0xFF,
        (rgb >> 8) & 0xFF,
        rgb & 0xFF,
        led_duration,
        buzzer_duration,
    ]
    hresult, response = SCardControl(hcard, control_code, payload)
    check(hresult, "SCardControl")
    if response:
        print("response:", bytes(response).hex(" "))


def connect_direct(context, reader_name):
    for protocol in (SCARD_PROTOCOL_UNDEFINED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1):
        hresult, hcard, active_protocol = SCardConnect(
            context, reader_name, SCARD_SHARE_DIRECT, protocol
        )
        if hresult == SCARD_S_SUCCESS:
            return hcard
    check(hresult, "SCardConnect")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["red", "green", "blue", "off", "beep", "demo"])
    parser.add_argument("-r", "--reader", help="reader index or unique name fragment")
    parser.add_argument("-d", "--duration", type=parse_byte, default=0xFF)
    parser.add_argument("-b", "--beep-duration", type=parse_byte, default=2)
    parser.add_argument(
        "--scard-ctl-code",
        action="store_true",
        help="wrap 0x50 with SCARD_CTL_CODE() before calling SCardControl",
    )
    args = parser.parse_args()

    control_code = (
        SCARD_CTL_CODE(SSCP_CMD_OUTPUT_RGB)
        if args.scard_ctl_code
        else SSCP_CMD_OUTPUT_RGB
    )

    hresult, context = SCardEstablishContext(SCARD_SCOPE_SYSTEM)
    check(hresult, "SCardEstablishContext")

    hcard = None
    try:
        hresult, reader_names = SCardListReaders(context, [])
        check(hresult, "SCardListReaders")
        reader_name = select_reader(reader_names, args.reader)
        print(f"reader: {reader_name}")
        print(f"control code: 0x{control_code:08X}")

        hcard = connect_direct(context, reader_name)

        if args.action == "beep":
            outputs_rgb(hcard, control_code, 0x000000, 0, args.beep_duration)
        elif args.action == "demo":
            for name in ("red", "green", "blue"):
                outputs_rgb(hcard, control_code, COLORS[name], 10, 0)
                sleep(1.0)
            outputs_rgb(hcard, control_code, 0x000000, 0, args.beep_duration)
        elif args.action == "off":
            outputs_rgb(hcard, control_code, COLORS[args.action], 0, 0)
        else:
            outputs_rgb(hcard, control_code, COLORS[args.action], args.duration, 0)
    finally:
        if hcard is not None:
            SCardDisconnect(hcard, SCARD_LEAVE_CARD)
        SCardReleaseContext(context)


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
