#!/usr/bin/env bash
# Wire fingerprint auth into Ubuntu login/unlock/sudo via pam-auth-update.
# Run with sudo AFTER enroll + verify succeed. Keep a root terminal open
# until you have confirmed password fallback still works.
set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "run with sudo" >&2
  exit 1
fi

apt-get update -qq
apt-get install -y -qq fprintd libpam-fprintd

if ! fprintd-list "${SUDO_USER:-$USER}" 2>/dev/null | grep -q .; then
  echo "no enrollments found; run fprintd-enroll first" >&2
  exit 1
fi

pam-auth-update --enable fprintd

echo "enabled: fingerprint offered for login, unlock and sudo, password still works."
echo "verify in a NEW terminal: 'sudo -k; sudo true' should offer your finger."
