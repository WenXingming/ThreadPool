#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <dedup-binary>" >&2
    exit 1
fi

dedup_bin="$1"
tmp_dir="$(mktemp -d /tmp/dedup_cli_test_XXXXXX)"

cleanup() {
    rm -rf "$tmp_dir"
}
trap cleanup EXIT

assert_contains() {
    local text="$1"
    local expected="$2"

    if [[ "$text" != *"$expected"* ]]; then
        echo "Expected output to contain: $expected" >&2
        echo "Actual output:" >&2
        echo "$text" >&2
        exit 1
    fi
}

assert_command_fails() {
    local output_file="$1"
    shift

    if "$@" > "$output_file" 2>&1; then
        echo "Expected command to fail: $*" >&2
        exit 1
    fi
}

printf "same-content" > "$tmp_dir/a.txt"
printf "same-content" > "$tmp_dir/b.txt"
printf "unique-value" > "$tmp_dir/c.txt"

default_output="$("$dedup_bin" "$tmp_dir")"
assert_contains "$default_output" "Duplicate group #1"
assert_contains "$default_output" "$tmp_dir/a.txt"
assert_contains "$default_output" "$tmp_dir/b.txt"
assert_contains "$default_output" "scanned files: 3"
assert_contains "$default_output" "hashed files: 3"
assert_contains "$default_output" "duplicate groups: 1"
assert_contains "$default_output" "errors: 0"

thread_output="$("$dedup_bin" "$tmp_dir" --threads 1)"
assert_contains "$thread_output" "Duplicate group #1"
assert_contains "$thread_output" "threads: 1"
assert_contains "$thread_output" "duplicate groups: 1"

missing_arg_output="$tmp_dir/missing_arg.out"
assert_command_fails "$missing_arg_output" "$dedup_bin"
assert_contains "$(cat "$missing_arg_output")" "Usage:"

invalid_threads_output="$tmp_dir/invalid_threads.out"
assert_command_fails "$invalid_threads_output" "$dedup_bin" "$tmp_dir" --threads 0
assert_contains "$(cat "$invalid_threads_output")" "Usage:"
