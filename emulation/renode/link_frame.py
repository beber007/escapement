# Copyright (c) 2026 Bertrand Hurst. Part of Escapement, distributed under the terms of
# LICENSE at the root of this repository.
"""The frames of SleepU5's link (SleepU5.c), as tools/unoq_sleep.py sends them, for
escapement_u5.robot: 0x00, the bytes and their CRC-16 encoded with COBS, 0x00."""
import binascii


def _cobs(data):
    out, block = bytearray(), bytearray()
    for b in data:
        if b == 0:
            out += bytes([len(block) + 1]) + block
            block = bytearray()
        else:
            block.append(b)
            if len(block) == 254:
                out += b"\xff" + block
                block = bytearray()
    return bytes(out + bytes([len(block) + 1]) + block)


def link_frame(first, count, bad_crc=False):
    """The bytes of a frame carrying the count from FIRST, COUNT bytes of it, modulo 256;
    with BAD_CRC, its CRC off by one."""
    data = bytes((int(first) + i) & 0xFF for i in range(int(count)))
    crc = binascii.crc_hqx(data, 0xFFFF) ^ (1 if bad_crc else 0)
    return [0] + list(_cobs(data + crc.to_bytes(2, "big"))) + [0]
