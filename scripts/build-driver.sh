#!/usr/bin/env bash
# Build the patched libfprint (synatlsmoc + Kensington 047d:00f2) on Ubuntu.
# Pinned sources, checksum-verified patch. Re-runnable. No hardware needed.
set -euo pipefail

BASE_REPO=https://github.com/vojtapl/synaTudorMiS.git
BASE_COMMIT=5452637c1f67fb35b859571ffd5b594f9225d08f
PATCH_SHA256=dddce7ea4e647cfd7585db822082fcf156f243e8fb69bef30fbd71dbeffaaab1

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
patch_file="${project_root}/patches/0001-Add-Kensington-VeriMark-Match-in-Sensor-support.patch"
src_dir="${project_root}/worktrees/synaTudorMiS-verimark/libfprint/libfprint"
build_dir="${src_dir}/.build-ubuntu"

for cmd in git sha256sum meson ninja apt-get; do
  command -v "${cmd}" >/dev/null 2>&1 || { echo "missing: ${cmd}" >&2; exit 1; }
done

echo "-> installing build dependencies"
sudo apt-get update -qq
sudo apt-get install -y -qq meson ninja-build gcc pkg-config \
  libglib2.0-dev libgusb-dev libssl-dev libpixman-1-dev libnss3-dev \
  libgudev-1.0-dev libudev-dev libcairo2-dev

echo "-> verifying patch checksum"
actual=$(sha256sum "${patch_file}" | awk '{print $1}')
[[ ${actual} == "${PATCH_SHA256}" ]] || { echo "patch checksum mismatch" >&2; exit 1; }

echo "-> fetching pinned base"
if [[ ! -d ${project_root}/upstream/synaTudorMiS/.git ]]; then
  git clone --filter=blob:none "${BASE_REPO}" "${project_root}/upstream/synaTudorMiS"
fi
git -C "${project_root}/upstream/synaTudorMiS" fetch origin "${BASE_COMMIT}"
git -C "${project_root}/upstream/synaTudorMiS" checkout --detach "${BASE_COMMIT}"

echo "-> preparing patched worktree"
PATCHED_TREE=f7161290161bc002c06020ea2acbdf9bf06befdd
wt="${project_root}/worktrees/synaTudorMiS-verimark"
mkdir -p "${project_root}/worktrees"
if [[ ! -e ${wt} ]]; then
  git -C "${project_root}/upstream/synaTudorMiS" worktree add --detach \
    "${wt}" "${BASE_COMMIT}"
fi
if ! git -C "${wt}" diff --quiet || ! git -C "${wt}" diff --cached --quiet; then
  echo "worktree has local changes; refusing to touch it" >&2
  exit 1
fi
if [[ $(git -C "${wt}" rev-parse 'HEAD^{tree}') == "${PATCHED_TREE}" ]]; then
  echo "patched source already prepared"
elif [[ $(git -C "${wt}" rev-parse HEAD) == "${BASE_COMMIT}" ]]; then
  git -C "${wt}" am --committer-date-is-author-date "${patch_file}"
  [[ $(git -C "${wt}" rev-parse 'HEAD^{tree}') == "${PATCHED_TREE}" ]] ||
    { echo "patched tree mismatch" >&2; exit 1; }
else
  echo "worktree is neither base nor patched tree; clean it up first" >&2
  exit 1
fi

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
