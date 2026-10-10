# Signed extensible INTEGER generated-code qualification

Build configured dependencies in order (`libasn1common`, parser, fixer, print,
compiler, typed). Install `requirements.txt` in a separate Python environment.
Then run `python3 tools/shared-integer-qualification/qualify.py --output
 tools/shared-integer-qualification/summary.json`.

The tool generates actual types/mapping/codec output after deleting Parser/Fixer
ownership, compiles it with the unchanged runtime integration path, and compares
all complete bytes, INTEGER end positions (with following BOOLEAN), and decoded
values against both an independent bit model and pinned native PER.

The native parser cannot serve as an oracle for UNION roots: it drops root tail
intervals. Those cases have independent min/max hull-offset tests plus a legacy
compiler PER-constraint cross-check. This limitation is recorded rather than
turning first-interval encoding into project behavior. Malformed input, OOM,
atomicity and sticky state are covered separately by focused C++/C checks.
