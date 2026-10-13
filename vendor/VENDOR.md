# Vendored driver source

`libfprint/` is the complete, build-ready source tree for the patched
libfprint (`synatlsmoc` driver with Kensington `047d:00f2` support).
It is vendored so the driver builds even if the original upstreams
disappear. Nothing outside this directory is needed to build.

## Origin (all verified at vendor time)

- Base: `https://github.com/vojtapl/synaTudorMiS`
  commit `5452637c1f67fb35b859571ffd5b594f9225d08f`,
  subtree `libfprint/libfprint`
- Patch: `Uriziel01/kensington-verimark-desktop-linux`
  `patches/0001-Add-Kensington-VeriMark-Match-in-Sensor-support.patch`
  (commit `108870f04d6f6275fce395900b5a5d13a8506444`,
  SHA-256 `dddce7ea…beffaaab1` — see `../patches/PROVENANCE.md`)
- Resulting tree `f7161290161bc002c06020ea2acbdf9bf06befdd`
  (matches the upstream project's own pinned tree hash)

## Integrity

`SHA256SUMS` covers every vendored file. `scripts/build-driver.sh`
runs `sha256sum -c` on it before configuring the build.

## Regenerating (maintainers)

```sh
git clone https://github.com/vojtapl/synaTudorMiS.git
git -C synaTudorMiS checkout 5452637c1f67fb35b859571ffd5b594f9225d08f
git -C synaTudorMiS am /path/to/0001-Add-Kensington-VeriMark-Match-in-Sensor-support.patch
git -C synaTudorMiS archive HEAD libfprint/libfprint \
  | tar -x --strip-components=2 -C vendor/libfprint
cd vendor/libfprint && find . -type f | sort | xargs sha256sum > ../SHA256SUMS
```

License: LGPL-2.1-or-later (upstream libfprint + patch).
