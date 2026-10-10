# Unified typed NGAP-PDU dispatch closeout

Completed 2026-10-10 on `feature/ngap-cpp-aper-first-message`, based on `2122667859367e3a0482f31373a0ae9846c7f58c`. Owner-authorized autonomous implementation, independent review and fix closure are complete. No schema, existing BODY/envelope renderer or APER runtime source was changed.

## Delivered API

`libngap/pdu.hpp` provides owned typed construction, unified complete encode/decode, safe concrete BODY access, checked received-criticality changes and immutable message identity. Generated lightweight `ngap.hpp`, message-specific public headers, private adapters and one immutable complete registry provide all 131 identities. The generator derives the full slot set from owned evidence, destroys Parser/Fixer trees before rendering, and verifies repeated outputs. Missing adapters fail at link time.

PDU copy deeply owns data; noexcept move invalidates the source. Immutable metadata survives Registry destruction. Known BODY decode and model allocation run within one existing known-open transaction; no malformed known message falls back to opaque. One outer complete operation retains existing budgets, sticky errors, padding and trailing-data policy. Unknown root slots and outer extensions remain owned receive-only values.

## Accepted evidence

One linked executable replayed all **3432** historical native cases for **131 messages** (81 initiating, 32 successful, 18 unsuccessful; 81 procedures). Full encoded bytes, full selected decoded semantics and every historical wire/native-semantic hash are unchanged. **7315** physical/resource checks passed. Independent reconciliation checked all responses, 49 source fingerprints, 659 generated files, 131 translation units, executable and output hashes.

The independent supplemental public-policy executable passed unknown code 81 in all three roles, every **112** absent outcome slot (49 successful, 63 unsuccessful), unknown outer extension 0, owned input lifetime, exact header preservation, receive-only refusal and retained-byte resource failure at bit 18.

Typed checks passed 40/40, runtime checks 10/10, core API checks 1/1 with assertions active under NDEBUG and four real allocation failures. Old N11 exact-profile replay passed all 2652 checks; historical accepted profiles were preserved. Negative missing-adapter linking refused all 131 unresolved registration symbols. New source/tool/evidence files were checked in the distribution directory.

Core unit ASan/UBSan passed. A supplemental complete3432 run instrumented runtime, PDU core and adapter000; its stdout exactly matches the accepted normal output. The other130 adapters used normal objects: this is not full adapter sanitizer coverage. Actual LSan attempts failed with ptrace fatal errors; no leak PASS is claimed. CMake output received static review; configuration/build was not executed because CMake is unavailable in this environment. The actual compiler/linker qualification and Autotools core checks did execute.

## Independent findings closed

A mismatched adapter model/value type could initially publish an incorrect identity. Actual model and actual value type are now checked inside the known-open transaction, and again before publication/encode. Independent mismatch, null model and ignored-error probes confirm refusal, transaction rollback, first physical error and no opaque fallback.

Qualification originally checked manifest schema presence without proving all native procedure/default-criticality/extension facts. It now independently checks the native descriptor and the actual compiled registry declarations. Five forged-evidence mutations were independently rejected. Final core, generator/tool, full case/fingerprint and actual public-policy reviews all passed without blockers.

## Reproduction and limits

See `tools/ngap-dispatch/README.md`, `tools/unified-ngap-qualification/README.md`, the accepted profile and qualification/compatibility summaries. The profile retains the original candidate run status separately and records acceptance after independent review; the historical profile is unchanged.

This is a finite frozen-schema public API integration and interoperability profile. It is not exhaustive ASN.1 value-space coverage, live commercial equipment interoperability, NGAP application/procedure policy, contained NAS/RRC qualification or a benchmark. No RAN application integration, Python binding, merge, branch deletion or force push was performed. This milestone is complete; subsequent application work remains separate.
