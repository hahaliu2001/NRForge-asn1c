# M6.003 qualification receive ledger

Owner froze this policy on 2026-10-17 America/Los_Angeles. It is shared by AMF
response reception and Simulator request/command reception. It is implemented
here only as a qualification policy, not a production AMF/Simulator adapter.

| Location | Presence and R1 projection | Refusal |
| --- | --- | --- |
| SetupRequest protocolIEs | ASN.1 permits an empty container; R1 requires IDs 130,139,134,136 exactly once; local canonical order is 130,139,134,136; receive reorder is accepted and preserved | Missing/duplicate: invalid_transfer |
| ID 130 | reject; mandatory for R1 non-GBR; both rates 1000..4000000000000 integer bit/s | Outside profile: unsupported_profile |
| ID 139 | reject; primary gTPTunnel, 32 address bits, four TEID octets, nonzero TEID; loopback allowed, unspecified/multicast/broadcast rejected | unsupported_profile |
| ID 134 | reject; ipv4 | unsupported_profile |
| ID 136 | reject; one flow, QFI=9; nonDynamic5QI branch with fiveQI=9; ARP 1..15 and frozen pre-emption labels | Duplicate QFI: invalid_transfer; other unsupported values: unsupported_profile |
| QoS descriptor optional fields | No priorityLevelQos, averagingWindow, maximumDataBurstVolume, GBR, reflective QoS, additional flow information or E-RAB projection | Present: unsupported_profile |
| SetupRequest other known IEs | No R1 projection, including NetworkInstance and TLContainer | unsupported_profile |
| SetupResponse dLQosFlowPerTNLInformation | Mandatory; same primary tunnel constraints; exactly one associated QFI=9 | Duplicate QFI: invalid_transfer; extra/mismatched flow: unsupported_profile |
| AssociatedQosFlowItem qosFlowMappingIndication | Not projected, including ul/dl | unsupported_profile |
| SetupResponse additional DL TNL, securityResult, failed-flow list | Present values are retained by codec but not projected | unsupported_profile |
| ReleaseCommand cause | Mandatory; nas / normal-release only | Other valid causes: unsupported_profile |
| ReleaseResponse | Empty root is valid, but absent/truncated bytes are not; extensions handled before success | No fabricated success from missing bytes |
| All ProtocolIE/ProtocolExtension containers | Preserve every identity, received criticality, order, duplicate and raw unknown payload; known rows checked against original schema registry | Duplicate identity or known criticality mismatch: invalid_transfer |
| Known extension values | Retain and decode; none are projected in R1 | unsupported_profile |
| Unknown reject IE/extension | Retain raw data in general codec; reject projection | invalid_transfer |
| Unknown ignore IE/extension | Successful structural native decode required; retain raw bytes and original metadata; safe to ignore at this protocol layer without deriving any additional semantics | No conversion into a known field |
| Unknown notify IE/extension | No approved Error Indication action | unsupported_profile |
| Unknown SEQUENCE addition | No per-entry criticality; retain bitmap/index/payload; no additions currently safe-listed | unsupported_profile |

The runner also exports every reachable schema field and registry entry to
`receive-ledger.json`. This table and `policy.py` determine policy; the generated
inventory is evidence, not automatic permission to accept optional fields.
General codec tests may accept ASN.1 values which the R1 projection rejects.
Unknown raw content remains decode-only if the current codec refuses encoding;
the runner tests that refusal explicitly rather than inventing an encoder.

## Error mapping and resources

- Decode truncated_input/nonzero_padding/trailing_data/constraint_violation:
  malformed_aper, with original native code/bit offset retained.
- resource_limit/allocation_failure: resource_limit; allocation failure does
  not assert peer input invalidity.
- Abstract syntax, missing/duplicate singleton and criticality errors:
  invalid_transfer. ASN.1-valid non-R1 data: unsupported_profile.
- Local invalid_argument/encode constraint errors: local argument errors.
  Internal invalid_state/unexpected exceptions: internal errors, never success.
- Native input and output caps are each 4096 octets, wire budget 32768 bits,
  total collection budget 65536, known-open depth 16, known staging budget
  1048576 octets, unknown retention 4096 octets/1024 records, extension bitmap
  1024 bits. These explicitly stated finite campaign budgets do not introduce
  new production SDK defaults. Their reviewed handoff remains later work.

No result emits an SBI event, finishes a release, deletes N4 state, retries,
or asserts correlation. Unsupported replies retain the accepted clean-run
consequence and never bypass N1/N2 completion gates.
