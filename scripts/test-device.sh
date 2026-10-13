#!/usr/bin/env bash
# Smoke-test the installed stack: fprintd discovery, enrollments, verify.
# Verify step is interactive (touch the reader when asked).
set -uo pipefail

echo "== fprintd default device =="
busctl call net.reactivated.Fprint /net/reactivated/Fprint/Manager \
  net.reactivated.Fprint.Manager GetDefaultDevice || exit 1

echo
echo "== enrollments for ${SUDO_USER:-$USER} =="
fprintd-list "${SUDO_USER:-$USER}" || exit 1

echo
echo "== verify (touch the enrolled finger when prompted) =="
fprintd-verify -f right-index-finger "${SUDO_USER:-$USER}"
