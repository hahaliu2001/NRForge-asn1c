"""Finite ordinary RRC roots using the installed C++ UPER codec."""

import copy
from functools import lru_cache
from importlib.resources import files
import json


class CodecError(ValueError):
    """First C++ codec error, with code and bit_offset preserved."""

    def __init__(self, code, bit_offset):
        self.code = code
        self.bit_offset = bit_offset
        super().__init__(f"{code}@{bit_offset}")


from ._native import decode, encode, identity, roots


@lru_cache(maxsize=1)
def _catalog():
    return json.loads(files(__package__).joinpath("conversion-schema.json").read_text())


def manifest():
    """Return a fresh copy of the full supported/blocked installed ledger."""
    return copy.deepcopy(_catalog()["manifest"])


def schema(module, root):
    """Describe the Python value model for a supported ordinary identity."""
    if type(module) is not str or type(root) is not str:
        raise TypeError("module and root must be str")
    key = module + "." + root
    if key not in _catalog()["schemas"]:
        raise ValueError("unsupported RRC root: " + key)
    return copy.deepcopy(_catalog()["schemas"][key])


__version__ = "0.1.0"
__all__ = ["CodecError", "decode", "encode", "identity", "roots", "manifest", "schema"]
