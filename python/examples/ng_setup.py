"""Construct a schema-valid minimal NG Setup Request with the installed SDK.

Zero-valued identifiers are a test fixture, not deployment configuration.
The dict keys are generated C++ spelling; use schema() to inspect other messages.
"""
import nrforge_ngap as ngap


def variant(name, payload):
    return {"type": name, "value": {"value": payload}}


def request_body():
    prefix = "NgapContainersProtocolIeFieldNgapPduContentsNgSetupRequestIEs_"
    zero3 = b"\0\0\0"
    node = variant("NgapIEsGlobalRanNodeId_global_gnb_id", {
        "p_lmn_identity": zero3,
        "g_nb_id": variant("NgapIEsGnbId_g_nb_id", {
            "octets": zero3, "bit_count": 22,
        }),
        "i_e_extensions": None,
        "sequence_extensions": {"received_bitmap_bit_count": 0, "unknown_additions": []},
    })
    def extensions(**fields):
        return dict(fields, i_e_extensions=None,
                    sequence_extensions={"received_bitmap_bit_count": 0, "unknown_additions": []})
    slice_item = extensions(s_nssai=extensions(s_st=b"\0", s_d=None))
    broadcast = extensions(p_lmn_identity=zero3,
                           t_ai_slice_support_list={"elements": [slice_item]})
    supported = extensions(t_ac=zero3, broadcast_plmn_list={"elements": [broadcast]})
    entries = [
        {"id": 27, "criticality": "reject",
         "value": variant(prefix + "global_ran_node_id", node)},
        {"id": 102, "criticality": "reject",
         "value": variant(prefix + "supported_ta_list", {"elements": [supported]})},
        {"id": 21, "criticality": "ignore",
         "value": variant(prefix + "default_paging_drx", "v_32")},
    ]
    return {"protocol_i_es": {"elements": entries},
            "sequence_extensions": {"received_bitmap_bit_count": 0, "unknown_additions": []}}


def main():
    wire = ngap.encode("NGSetupRequest", request_body(), criticality="reject")
    expected = bytes.fromhex("00150025000003001b000800000000000000000066000d000000000000000000000000000015400100")
    if wire != expected:
        raise RuntimeError("Encoding differs from the pinned native fixture")
    decoded = ngap.decode(wire)
    print("SDK:", ngap.identity())
    print("NGSetupRequest:", wire.hex())
    print("Decoded message:", decoded["message"])


if __name__ == "__main__":
    main()
