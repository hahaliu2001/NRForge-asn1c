"""NGAP 0.2.0 outer PDU and independent Transfer APER operations."""
class CodecError(ValueError):
    """Native first error, preserving code and bit offset."""
    def __init__(self, code, bit_offset):
        self.code = code
        self.bit_offset = bit_offset
        super().__init__(f"{code}@{bit_offset}")

from ._native import (decode, encode, identity, messages, schema,
                      transfers, encode_transfer, decode_transfer, transfer_schema)
__version__ = "0.2.0"
__all__ = ["CodecError", "decode", "encode", "identity", "messages", "schema",
           "transfers", "encode_transfer", "decode_transfer", "transfer_schema"]
