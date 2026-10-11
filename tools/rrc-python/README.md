# RRC-P9 installed Python delivery

Separate `nrforge-rrc` package over the sealed P8 installed C++ SDK. The finite
profile exposes 64 roots from the 85-root ledger; 21 blocked roots remain blocked.
See `python/rrc/README.md` and the contract/closeout for public representations,
completion, limits, ownership and platform scope. No benchmark is involved.

1. Build/install P8 with `tools/rrc-sdk/build.py` from the frozen schema.
2. Run `prepare.py --output <new-stage>`; build its sdist with `python -m build
   --sdist --no-isolation`. Extract it to a new directory and build a wheel there
   with `CC=gcc CXX=g++ python -m build --wheel --no-isolation
   -Ccmake.define.NRFORGE_PYTHON_SDK_PREFIX=<absolute-sdk-prefix>`.
3. `prepare_vectors.py --help` describes independent native oracle preparation.
   pycrate is a qualification-only dependency, absent from installed consumers.
4. Install the wheel and the accepted NGAP/F1AP/E1AP wheels into a new venv.
   `run_installed.py --python <absolute-venv-python> --output <outside-repo-json>
   --hide <schema> --hide <sdk-prefix> --hide <sdk-work> --hide <stage>
   --hide <extracted-sdist> --hide <wheel-build>` copies tests outside the checkout,
   temporarily hides inputs, runs all four isolated suites and restores inputs
   in `finally`. Hide any additional original checkout/build inputs explicitly.
5. Run the five generator tests with `NRFORGE_RRC_TEST_SDK=<sdk-prefix>` and
   `python -m unittest discover -s python/rrc/tests -p test_generator.py -v`.
   The real CMake build/install seal gates also reject altered generated C++.
6. Run libuper/libaper/libngap/libasn1typed/libasn1parser `make check`.
   `record.py --work <outside-repo-work> --output verification-summary.json`
   binds source, sdist/wheel contents, installed files, public SDK/archive,
   accepted APER wheels, logs and unchanged baseline blobs. Its work layout uses
   the `p9-*` names recorded in the accepted receipt.

Independent approval binds canonical sorted compact JSON of `source_sha256`
and `verification_sha256`; the review receipt is excluded to avoid recursion.
