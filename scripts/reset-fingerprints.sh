#!/usr/bin/env bash
# Delete ALL enrolled fingerprints for a user through the proper fprintd
# stack. This is the supported Linux-side reset.
#
# What it does NOT do: a sensor-flash format. No such operation exists on
# Linux for this reader (upstream deliberately excludes storage-format).
# For a full on-chip wipe, use the sibling Windows tool `kensington-reset`.
set -uo pipefail

TARGET="${1:-${SUDO_USER:-$USER}}"

if ! systemctl is-active --quiet fprintd 2>/dev/null; then
  echo "fprintd is not running" >&2
  exit 1
fi

echo "enrollments for ${TARGET}:"
if ! fprintd-list "${TARGET}"; then
  echo "nothing enrolled (or unknown user)"
  exit 0
fi

echo
echo "This deletes EVERY fingerprint enrolled for '${TARGET}'."
read -r -p "type YES to continue: " confirm
[[ ${confirm} == YES ]] || { echo "aborted"; exit 2; }

fprintd-delete "${TARGET}" || { echo "delete failed" >&2; exit 1; }

echo
echo "remaining enrollments (expect none):"
remaining=$(fprintd-list "${TARGET}" 2>/dev/null || true)
if [[ -n ${remaining} ]]; then
  echo "${remaining}"
  echo "still enrolled — delete did not take" >&2
  exit 1
fi
echo "cleared."
