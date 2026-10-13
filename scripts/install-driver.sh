#!/usr/bin/env bash
# Install the built driver as a reversible fprintd override. Run with sudo.
set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "run with sudo" >&2
  exit 1
fi

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_lib="${project_root}/vendor/libfprint/.build/libfprint/libfprint-2.so.2.0.0"
pairing_file=/var/lib/fprint/verimark/047d-00f2/libfprint-persistent.bin
install_dir=/opt/verimark-libfprint/lib64
dropin_dir=/etc/systemd/system/fprintd.service.d
dropin_source="${project_root}/packaging/fprintd/50-verimark-libfprint.conf"

[[ -f ${build_lib} && -f ${dropin_source} ]] ||
  { echo "build the driver first: ./scripts/build-driver.sh" >&2; exit 1; }
[[ -f ${pairing_file} ]] ||
  { echo "missing pairing file: ${pairing_file} (README step 2)" >&2; exit 1; }
[[ $(stat -c %u "${pairing_file}") == 0 && $(stat -c %a "${pairing_file}") == 600 ]] ||
  { echo "pairing file must be root-owned mode 0600" >&2; exit 1; }
readelf -d "${build_lib}" | grep -q 'Library soname: \[libfprint-2.so.2\]' ||
  { echo "unexpected SONAME in built library" >&2; exit 1; }

install -d -o root -g root -m 0755 "${install_dir}" "${dropin_dir}"
install -o root -g root -m 0755 "${build_lib}" "${install_dir}/libfprint-2.so.2.0.0"
ln -sfn libfprint-2.so.2.0.0 "${install_dir}/libfprint-2.so.2"
ln -sfn libfprint-2.so.2 "${install_dir}/libfprint-2.so"
install -o root -g root -m 0644 "${dropin_source}" "${dropin_dir}/50-verimark-libfprint.conf"

# udev access for the reader (harmless to re-apply)
install -o root -g root -m 0644 \
  "${project_root}/udev/70-verimark-dt.rules" /etc/udev/rules.d/70-verimark-dt.rules
udevadm control --reload-rules
udevadm trigger --subsystem-match=usb --attr-match=idVendor=047d || true

systemctl daemon-reload
systemctl restart fprintd.service

service_pid=$(systemctl show --property MainPID --value fprintd.service)
if [[ -z ${service_pid} || ${service_pid} == 0 ]] ||
   ! grep -q '/opt/verimark-libfprint/lib64/libfprint-2.so.2.0.0' "/proc/${service_pid}/maps"; then
  echo "fprintd did not load the VeriMark libfprint build" >&2
  exit 1
fi

busctl call net.reactivated.Fprint /net/reactivated/Fprint/Manager \
  net.reactivated.Fprint.Manager GetDefaultDevice >/dev/null

echo "installed: fprintd is using the VeriMark libfprint build and sees the reader."
echo "PAM was not touched; enroll first (fprintd-enroll), then scripts/enable-pam.sh."
