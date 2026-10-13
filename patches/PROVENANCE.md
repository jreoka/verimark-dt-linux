# Patch provenance

`0001-Add-Kensington-VeriMark-Match-in-Sensor-support.patch` is vendored
from **Uriziel01/kensington-verimark-desktop-linux** (`patches/` there),
commit `108870f04d6f6275fce395900b5a5d13a8506444` ("Add Kensington
VeriMark Match-in-Sensor support").

- Upstream base it applies to: `vojtapl/synaTudorMiS`
  commit `5452637c1f67fb35b859571ffd5b594f9225d08f`
- Expected file SHA-256: `dddce7ea4e647cfd7585db822082fcf156f243e8fb69bef30fbd71dbeffaaab1`
  (verified by `scripts/build-driver.sh` before applying)
- Expected patched tree: `f7161290161bc002c06020ea2acbdf9bf06befdd`
  (verified after applying)
- License: **LGPL-2.1-or-later** (same as libfprint; patch headers
  retained). This repo's own scripts/docs are MIT (see LICENSE);
  the built libfprint remains LGPL as upstream.

What the patch adds (from its diffstat): a `kensington_transport.c`
 Match-in-Sensor transport for USB `047d:00f2` (endpoint-zero requests
 `0x16`/`0x17`), `047d:00f2` registration in the `synatlsmoc` driver,
 a `verimark-smoke` example, and an offline `synatlsmoc-kensington` test.
