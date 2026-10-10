"""Typed NGAP APER operations backed by the complete installed C++ SDK."""
class CodecError(ValueError):
    """C++ codec failure; code and bit_offset preserve the runtime first error."""
    def __init__(self, code, bit_offset):
        self.code = code
        self.bit_offset = bit_offset
        super().__init__(f"{code}@{bit_offset}")

from ._native import decode, encode, identity, messages, schema
__version__ = "0.1.0"
__all__ = ["CodecError", "decode", "encode", "identity", "messages", "schema"]
