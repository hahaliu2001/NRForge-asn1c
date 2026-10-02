#!/bin/sh
set -eu
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
asn1c_bin=${1:-$repo_dir/asn1c/asn1c}
out=$(cd "$script_dir" && "$asn1c_bin" -ftyped-poc=NGSetupRequest NGSetupRequest.asn1 2>&1)
printf '%s\n' "$out" | grep -F 'struct NgSetupRequest'
printf '%s\n' "$out" | grep -F 'GlobalRanNodeId global_ran_node_id'
printf '%s\n' "$out" | grep -F 'std::optional<RanNodeName> ran_node_name'
printf '%s\n' "$out" | grep -F 'std::vector<SupportedTaItem> supported_ta_list'
printf '%s\n' "$out" | grep -F 'PagingDrx default_paging_drx'
printf '%s\n' "$out" | grep -F 'std::optional<UeRetentionInformation> ue_retention_information'
printf '%s\n' "$out" | grep -F 'class NgSetupRequest:'
printf '%s\n' "$out" | grep -F 'global_ran_node_id: GlobalRanNodeId'
printf '%s\n' "$out" | grep -F 'ran_node_name: RanNodeName | None'
printf '%s\n' "$out" | grep -F 'supported_ta_list: list[SupportedTaItem]'
printf '%s\n' "$out" | grep -F 'default_paging_drx: PagingDrx'
printf '%s\n' "$out" | grep -F 'ue_retention_information: UeRetentionInformation | None'
if printf '%s\n' "$out" | grep -F 'default_ue_retention_information'; then exit 1; fi

pyfile=$(mktemp)
trap 'rm -f "$pyfile"' EXIT HUP INT TERM
{
    printf '%s\n' 'class GlobalRanNodeId: pass' 'class RanNodeName: pass' 'class SupportedTaItem: pass' 'class PagingDrx: pass' 'class UeRetentionInformation: pass'
    printf '%s\n' "$out" | sed -n '/^# --- Python ---$/,$p' | sed '1d'
} > "$pyfile"
python3 "$pyfile"

if grep -E 'GlobalRANNodeID|RANNodeName|SupportedTAList|SupportedTAItem|PagingDRX|DefaultPagingDRX|UERetentionInformation|NGSetupRequest' "$repo_dir/asn1c/typed_poc.c" "$repo_dir/asn1c/typed_poc.h"; then exit 1; fi
