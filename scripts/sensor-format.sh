#!/usr/bin/env bash
# FULL SENSOR FLASH FORMAT for the VeriMark DT (DB2_FORMAT over the
# synatlsmoc driver). Wipes every fingerprint template on the sensor chip.
#
# DANGER: this is irreversible on-device. It may also invalidate the
# host pairing, meaning you would re-pair + re-enroll afterwards.
# Back up /var/lib/fprint (encrypted, offline) BEFORE running this.
# Prefer ./scripts/reset-fingerprints.sh unless you need the whole chip clean.
set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "run with sudo" >&2
  exit 1
fi

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
tool_src="${project_root}/tools/sensor-format.c"
tool_bin="${project_root}/tools/sensor-format"
build_lib_dir="${project_root}/vendor/libfprint/.build/libfprint"
pairing_file=/var/lib/fprint/verimark/047d-00f2/libfprint-persistent.bin

for cmd in gcc pkg-config; do
  command -v "${cmd}" >/dev/null 2>&1 || { echo "missing: ${cmd} (run scripts/build-driver.sh first)" >&2; exit 1; }
done
[[ -f ${build_lib_dir}/libfprint-2.so.2.0.0 ]] ||
  { echo "built driver missing (run scripts/build-driver.sh first)" >&2; exit 1; }
[[ -f ${pairing_file} ]] ||
  { echo "missing pairing file: ${pairing_file} (README step 2)" >&2; exit 1; }

echo "YOU ARE ABOUT TO FORMAT THE SENSOR FLASH ON THE VERIMARK DT."
echo "- every fingerprint template on the reader will be destroyed"
echo "- the host pairing may stop working -> re-pair + re-enroll"
echo "- back up /var/lib/fprint first (encrypted, offline, never to git)"
read -r -p "type FORMAT-CHIP to continue: " confirm
[[ ${confirm} == FORMAT-CHIP ]] || { echo "aborted"; exit 2; }

if [[ ${tool_bin} -ot ${tool_src} || ! -x ${tool_bin} ]]; then
  echo "-> compiling sensor-format tool"
  gcc -O2 -o "${tool_bin}" "${tool_src}" \
    "$(pkg-config --cflags glib-2.0 gobject-2.0 gio-2.0)" \
    -I "${project_root}/vendor/libfprint" \
    -I "${build_lib_dir}" \
    -L "${build_lib_dir}" -lfprint-2 \
    "$(pkg-config --libs glib-2.0 gobject-2.0 gio-2.0)"
fi

export LD_LIBRARY_PATH="${build_lib_dir}"
export SYNA_TLSMOC_PERSISTENT_DATA_FILE="${pairing_file}"
exec "${tool_bin}"
