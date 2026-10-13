#!/usr/bin/env bash
# Build the vendored libfprint (synatlsmoc + Kensington 047d:00f2).
# Fully offline except apt deps: no git clone, no network fetch.
# Re-runnable. No hardware needed (runs the driver's offline test).
set -euo pipefail

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
src_dir="${project_root}/vendor/libfprint"
build_dir="${src_dir}/.build-ubuntu"

for cmd in sha256sum meson ninja apt-get; do
  command -v "${cmd}" >/dev/null 2>&1 || { echo "missing: ${cmd}" >&2; exit 1; }
done

echo "-> installing build dependencies"
sudo apt-get update -qq
sudo apt-get install -y -qq meson ninja-build gcc pkg-config \
  libglib2.0-dev libgusb-dev libssl-dev libpixman-1-dev libnss3-dev \
  libgudev-1.0-dev libudev-dev libcairo2-dev

echo "-> verifying vendored source integrity"
(cd "${project_root}/vendor" && sha256sum -c SHA256SUMS 2>&1 | grep -v ': OK$' || true) |
  grep . && { echo "vendored source verification FAILED" >&2; exit 1; }
[[ -f ${src_dir}/libfprint/drivers/synatlsmoc/kensington_transport.c ]] ||
  { echo "vendored Kensington transport missing" >&2; exit 1; }
echo "vendored source OK"

echo "-> configuring"
# NOTE: modern Ubuntu splits udev.pc into libudev; pass install dirs
# explicitly instead of relying on pkgconfig autodetection.
if [[ -f ${build_dir}/build.ninja ]]; then
  meson setup --reconfigure "${build_dir}" "${src_dir}" \
    -Ddrivers=synatlsmoc -Dgtk-examples=false -Ddoc=false \
    -Dintrospection=false -Dinstalled-tests=false \
    -Dudev_rules_dir=/etc/udev/rules.d -Dudev_hwdb=disabled
else
  meson setup "${build_dir}" "${src_dir}" \
    -Ddrivers=synatlsmoc -Dgtk-examples=false -Ddoc=false \
    -Dintrospection=false -Dinstalled-tests=false \
    -Dudev_rules_dir=/etc/udev/rules.d -Dudev_hwdb=disabled
fi

echo "-> building"
ninja -C "${build_dir}"

echo "-> offline driver test"
meson test -C "${build_dir}" synatlsmoc-kensington --print-errorlogs

lib="${build_dir}/libfprint/libfprint-2.so.2.0.0"
[[ -f ${lib} ]] || { echo "expected library missing: ${lib}" >&2; exit 1; }
echo "built: ${lib}"
echo "next: pair the reader (README step 2), then sudo ./scripts/install-driver.sh"
