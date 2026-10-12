# P18 expanded RRC Python SDK

Read contract.md and ../../docs/rrc-python-expanded-p18-closeout.md.

1. Build/install the sealed P17 SDK; use `prepare.py --output <new-stage>`.
2. Build an sdist, extract into a clean directory, and build its wheel using
   `-Ccmake.define.NRFORGE_PYTHON_SDK_PREFIX=<absolute-sdk-prefix>`.
3. `prepare_vectors.py --help` projects freshly compiled pycrate 0.7.11 native
   semantics through public model metadata; native Contents carrier projection
   uses the independent P16 oracle. It is absent from installed consumers.
4. Install the RRC wheel and previously accepted NGAP/F1AP/E1AP wheels into a
   fresh venv. `run_installed.py --python <venv-python> --output <outside-json>
   --hide <schema> --hide <SDK> --hide <stage> --hide <build>` copies tests out,
   hides the repository and supplied inputs, and restores them in finally.
5. Run the five generator tests with `NRFORGE_RRC_TEST_SDK=<SDK>`.
6. `record.py --help` binds recovered package sources, generated bindings, SDK,
   sdist, wheel, installed members and retained execution evidence. It validates
   every unchanged baseline blob and exact distribution entries. After scratch
   recovery the original APER wheels were restored from saved deliveries and
   their archive hashes and installed members are checked against acceptance.

The original P18 workflow files disappeared when the previous scratch workspace
was removed. Package sources and artifacts were restored byte-for-byte. These
workflow scripts were reconstructed from P9 and require fresh independent
review; the old snapshot is not presented as approval of this new snapshot.
