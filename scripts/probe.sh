#!/usr/bin/env bash
# Probe for the Kensington VeriMark DT (047d:00f2). Read-only.
set -uo pipefail

VID=047d
PID=00f2
fail=0

echo "== USB =="
if command -v lsusb >/dev/null 2>&1; then
  if lsusb -d "${VID}:${PID}"; then
    echo "reader present"
  else
    echo "reader NOT found on USB (plug it in directly, no hub)"
    fail=1
  fi
else
  echo "lsusb not installed (apt install usbutils)"
  fail=1
fi

echo
echo "== interfaces (expect: if0 -> usbhid for FIDO2, if1 unbound) =="
found=0
for dev in /sys/bus/usb/devices/*; do
  if [[ -f ${dev}/idVendor && -f ${dev}/idProduct ]] &&
     [[ $(cat "${dev}/idVendor") == "${VID}" && $(cat "${dev}/idProduct") == "${PID}" ]]; then
    found=1
    echo "device: $(basename "${dev}")"
    for iface in "${dev}:"*/; do
      [[ -d ${iface} ]] || continue
      num=$(cat "${iface}/bInterfaceNumber" 2>/dev/null || echo "?")
      drv=$(basename "$(readlink "${iface}/driver" 2>/dev/null)" 2>/dev/null || echo "(unbound)")
      echo "  interface ${num}: driver=${drv}"
    done
  fi
done
[[ ${found} == 1 ]] || { echo "no ${VID}:${PID} sysfs entry"; fail=1; }

echo
echo "== software stack =="
# shellcheck source=lib/distro.sh
source "$(dirname -- "${BASH_SOURCE[0]}")/lib/distro.sh"
echo "distro: ${DISTRO_ID} (family: ${DISTRO_FAMILY})"
for bin in fprintd fprintd-enroll fprintd-verify fprintd-list; do
  if command -v "${bin}" >/dev/null 2>&1; then
    echo "  ${bin}: present"
  else
    echo "  ${bin}: MISSING (install fprintd via your package manager)"
  fi
done
if ver=$(libfprint_version); [[ -n ${ver} ]]; then
  echo "  stock libfprint: ${ver} (does NOT support 047d:00f2; this repo builds one that does)"
else
  echo "  stock libfprint: not installed"
fi
if systemctl is-active --quiet fprintd 2>/dev/null; then
  echo "  fprintd service: active"
  if busctl call net.reactivated.Fprint /net/reactivated/Fprint/Manager \
      net.reactivated.Fprint.Manager GetDefaultDevice 2>/dev/null | grep -q .; then
    echo "  fprintd default device: claimed"
  else
    echo "  fprintd default device: none (expected before driver install)"
  fi
else
  echo "  fprintd service: not active"
fi

echo
if [[ ${fail} == 0 ]]; then
  echo "probe OK"
else
  echo "probe found problems (see above)"
  exit 1
fi
