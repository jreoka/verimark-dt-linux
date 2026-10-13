#!/usr/bin/env bash
# Remove the fprintd override and restore stock libfprint. Run with sudo.
set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "run with sudo" >&2
  exit 1
fi

rm -f /etc/systemd/system/fprintd.service.d/50-verimark-libfprint.conf
rm -f /etc/udev/rules.d/70-verimark-dt.rules
rm -rf /opt/verimark-libfprint

systemctl daemon-reload
udevadm control --reload-rules || true
systemctl restart fprintd.service

echo "removed: fprintd is back on the stock Ubuntu libfprint."
echo "pairing + enrollments under /var/lib/fprint were left untouched."
