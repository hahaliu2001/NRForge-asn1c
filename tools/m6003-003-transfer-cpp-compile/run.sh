#!/usr/bin/env bash
set -Eeuo pipefail

readonly REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
readonly TOOL_DIR="$REPO_ROOT/tools/m6003-003-transfer-cpp-compile"
readonly RUNS_ROOT="${M6003_RUNS_ROOT:-/home/jie/m6003-b1-rerun/runs}"
readonly RUNTIME_ROOT="${M6003_RUNTIME_ROOT:-/home/jie/m6003-b1-rerun/tree}"
readonly OUTPUT_PARENT="${M6003_OUTPUT_PARENT:-$REPO_ROOT/verification}"
readonly CXX_BIN="${CXX:-c++}"
readonly CASES=(setup_request setup_response release_command release_response)

timestamp="$(date -u +%Y%m%dT%H%M%S.%NZ)"
out="$OUTPUT_PARENT/m6003-003-transfer-cpp-compile-$timestamp"
mkdir -p "$OUTPUT_PARENT"
mkdir "$out" || { echo "Refusing to reuse output directory: $out" >&2; exit 2; }

completed=0
failed_case=""
final_status="RUNNING"
finish_summary() {
    local status=$?
    if (( status != 0 )); then
        final_status="FAIL"
    elif (( completed == ${#CASES[@]} )); then
        final_status="PREPARED"
    else
        final_status="INCOMPLETE"
    fi
    {
        printf 'status=%s\n' "$final_status"
        printf 'execution_mode=compile_and_link_only\n'
        printf 'executables_run=no\n'
        printf 'completed_cases=%s/%s\n' "$completed" "${#CASES[@]}"
        printf 'failed_case=%s\n' "${failed_case:-none}"
        printf 'source_head=%s\n' "$(git -C "$REPO_ROOT" rev-parse HEAD 2>&1 || true)"
        printf 'source_branch=%s\n' "$(git -C "$REPO_ROOT" branch --show-current 2>&1 || true)"
        printf 'runtime_root=%s\n' "$RUNTIME_ROOT"
        printf 'runtime_source_sha256:\n%s\n' "$(sha256sum "$RUNTIME_ROOT/libaper/runtime.hpp" "$RUNTIME_ROOT/libaper/runtime.cpp" "$RUNTIME_ROOT/libaper/sequence_extensions.hpp" 2>&1 || true)"
        printf 'runtime_archive_sha256:\n%s\n' "$(sha256sum "$RUNTIME_ROOT/libaper/.libs/libaper.a" 2>&1 || true)"
        printf 'consumer_sha256:\n%s\n' "$(sha256sum "$TOOL_DIR/consumer.cpp" 2>&1 || true)"
        printf 'compiler=%s\n' "$CXX_BIN"
        printf 'output_dir=%s\n' "$out"
    } > "$out/SUMMARY.txt"
    printf '\n=== SUMMARY ===\n'
    cat "$out/SUMMARY.txt"
    printf 'summary_path=%s/SUMMARY.txt\n' "$out"
}
trap finish_summary EXIT

if ! command -v "$CXX_BIN" >/dev/null 2>&1; then
    echo "C++ compiler not found: $CXX_BIN" >&2
    exit 2
fi
for required in \
    "$RUNTIME_ROOT/libaper/runtime.hpp" \
    "$RUNTIME_ROOT/libaper/runtime.cpp" \
    "$RUNTIME_ROOT/libaper/sequence_extensions.hpp" \
    "$RUNTIME_ROOT/libaper/.libs/libaper.a" \
    "$TOOL_DIR/consumer.cpp"; do
    if [[ ! -f "$required" ]]; then
        echo "Required input is missing: $required" >&2
        exit 2
    fi
done

{
    printf 'repo_root=%s\n' "$REPO_ROOT"
    printf 'source_head=%s\n' "$(git -C "$REPO_ROOT" rev-parse HEAD)"
    printf 'source_branch=%s\n' "$(git -C "$REPO_ROOT" branch --show-current)"
    printf 'source_status:\n'
    git -C "$REPO_ROOT" status --short
    printf '\ninputs_sha256:\n'
    sha256sum "$TOOL_DIR/consumer.cpp" \
        "$REPO_ROOT/.gitignore" \
        "$REPO_ROOT/libasn1typed/Makefile.am" \
        "$REPO_ROOT/libasn1typed/asn1typed_extract.c" \
        "$REPO_ROOT/libasn1typed/asn1typed_extract.h" \
        "$REPO_ROOT/libasn1typed/check_asn1typed_transfer_roots.c" \
        "$REPO_ROOT/libasn1typed/fixtures/ngap-transfer-roots.asn1" \
        "$RUNTIME_ROOT/libaper/runtime.hpp" \
        "$RUNTIME_ROOT/libaper/runtime.cpp" \
        "$RUNTIME_ROOT/libaper/sequence_extensions.hpp" \
        "$RUNTIME_ROOT/libaper/.libs/libaper.a"
    for name in "${CASES[@]}"; do
        printf '\ncase=%s\n' "$name"
        sha256sum "$RUNS_ROOT/$name/out/root_types.hpp" \
            "$RUNS_ROOT/$name/out/root_mapping.hpp" \
            "$RUNS_ROOT/$name/out/root_codec.hpp"
    done
} > "$out/IDENTITY.txt" 2>&1

for name in "${CASES[@]}"; do
    case "$name" in
        setup_request)
            root_type='probe_setup_request::NgapIEsPduSessionResourceSetupRequestTransfer'
            encode='probe_setup_request::encode_ngap_i_es_pdu_session_resource_setup_request_transfer'
            decode='probe_setup_request::decode_ngap_i_es_pdu_session_resource_setup_request_transfer'
            ;;
        setup_response)
            root_type='probe_setup_response::NgapIEsPduSessionResourceSetupResponseTransfer'
            encode='probe_setup_response::encode_ngap_i_es_pdu_session_resource_setup_response_transfer'
            decode='probe_setup_response::decode_ngap_i_es_pdu_session_resource_setup_response_transfer'
            ;;
        release_command)
            root_type='probe_release_command::NgapIEsPduSessionResourceReleaseCommandTransfer'
            encode='probe_release_command::encode_ngap_i_es_pdu_session_resource_release_command_transfer'
            decode='probe_release_command::decode_ngap_i_es_pdu_session_resource_release_command_transfer'
            ;;
        release_response)
            root_type='probe_release_response::NgapIEsPduSessionResourceReleaseResponseTransfer'
            encode='probe_release_response::encode_ngap_i_es_pdu_session_resource_release_response_transfer'
            decode='probe_release_response::decode_ngap_i_es_pdu_session_resource_release_response_transfer'
            ;;
    esac

    case_dir="$out/$name"
    mkdir "$case_dir"
    for header in root_types.hpp root_mapping.hpp root_codec.hpp; do
        if [[ ! -f "$RUNS_ROOT/$name/out/$header" ]]; then
            failed_case="$name (missing $header)"
            echo "Required generated input is missing: $RUNS_ROOT/$name/out/$header" >&2
            exit 2
        fi
    done

    cmdfile="$case_dir/command.txt"
    {
        printf '%q ' "$CXX_BIN" -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion \
            -I"$RUNTIME_ROOT/libaper" -I"$RUNS_ROOT/$name/out" \
            -DM6003_ROOT_TYPE="$root_type" -DM6003_ENCODE="$encode" -DM6003_DECODE="$decode" \
            "$TOOL_DIR/consumer.cpp" "$RUNTIME_ROOT/libaper/.libs/libaper.a" \
            -o "$case_dir/consumer"
        printf '\n'
    } > "$cmdfile"
    echo "[$((completed + 1))/${#CASES[@]}] Compiling and linking $name"
    set +e
    "$CXX_BIN" -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion \
        -I"$RUNTIME_ROOT/libaper" -I"$RUNS_ROOT/$name/out" \
        -DM6003_ROOT_TYPE="$root_type" -DM6003_ENCODE="$encode" -DM6003_DECODE="$decode" \
        "$TOOL_DIR/consumer.cpp" "$RUNTIME_ROOT/libaper/.libs/libaper.a" \
        -o "$case_dir/consumer" >"$case_dir/stdout.txt" 2>"$case_dir/stderr.txt"
    rc=$?
    set -e
    printf '%s\n' "$rc" > "$case_dir/exit-code.txt"
    if (( rc != 0 )); then
        failed_case="$name (compiler exit $rc)"
        echo "FAIL $name (exit $rc); see $case_dir/{stdout.txt,stderr.txt,exit-code.txt}" >&2
        exit "$rc"
    fi
    completed=$((completed + 1))
    echo "PASS $name compiled and linked"
done

echo "All four roots compiled and linked. Executables were not run."
