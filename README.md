# verimark-dt-linux

Use the **Kensington VeriMark DT fingerprint reader (USB `047d:00f2`)**
on Linux with `fprintd` — enroll, verify, login, unlock, `sudo`.

Works on **Ubuntu 24.04+ / Debian, Arch (+ derivatives), Fedora (+
derivatives)** — every script detects your distro and uses the native
package manager (`apt`, `pacman`, `dnf`) and the native PAM tooling
(`pam-auth-update`, direct PAM edits, `authselect`).

Stock distros' `libfprint` does **not** support this reader, so this repo
builds a patched `libfprint` (pinned sources, checksum-verified) natively
on your distro, installs it as a reversible `fprintd` override, and wires up
udev + PAM. The full driver build was compile-verified on Ubuntu 26.04 and
the driver's offline hardware test passes (`synatlsmoc-kensington OK`).
Arch/Fedora use the same flow with native package names (not run here —
report back).

## Status / honesty notes

- The fingerprint driver itself is community work, not mine: a native
  Match-in-Sensor `libfprint` driver for `047d:00f2` (see Credits). It is
  reported working with real enroll/verify/login/`sudo` on hardware.
- What this repo adds: Ubuntu-native build + install + udev + PAM
  automation, and a hardware profile of this exact reader reverse
  engineered from the Windows driver package (`docs/rea-notes.md`).
- I verified everything that can be verified without the physical reader:
  patched-source reproduction (tree hash match), full compile, offline
  driver test. The last mile — pairing, enroll, verify — needs the reader
  plugged into your machine (steps 3–5 below).

## Requirements

- Ubuntu 24.04 LTS+, Debian 12+, Arch (+ CachyOS/EndeavourOS/Manjaro),
  or Fedora 40+ (+ Nobara/Bazzite*) — x86_64
- The reader plugged **directly** into the PC (no hub) for pairing/enroll
- `sudo` access, internet access for packages + the one-time source check

\* Immutable/atomic variants (Bazzite, Silverblue): layer the build deps
with `rpm-ostree install` and reboot first; the rest is unchanged.

## Install

### 0. Probe (read-only, safe to run anytime)

```sh
./scripts/probe.sh
```

Confirms the reader is visible (`047d:00f2`), which USB interface the
kernel claimed, and your `fprintd`/`libfprint` versions.

### 1. Build the driver

```sh
./scripts/build-driver.sh
```

Installs build deps via your native package manager, integrity-checks
the vendored source (`vendor/`, works offline), then builds `libfprint` with the
`synatlsmoc` driver and runs its offline test.
Takes a few minutes. Re-runnable.

### 2. Pair the reader (one time)

Pairing replaces the reader's current host pairing — Windows Hello on
that reader will need re-setup afterwards. The pairing flow needs live
USB captures, so it uses the battle-tested tools from the upstream
project (linked in Credits) — do that step there, then come back with:

```sh
/var/lib/fprint/verimark/047d-00f2/libfprint-persistent.bin
```

owned by `root:root`, mode `0600`. The installer refuses to continue
without it. **Back this file up** (encrypted, offline, never to GitHub):
losing it means pairing + enrolling again.

### 3. Install

```sh
sudo ./scripts/install-driver.sh
```

Installs the library to `/opt/verimark-libfprint/lib64`, adds an
`fprintd` systemd drop-in pointing at it, restarts `fprintd`, and
verifies the service loaded the new library and sees the reader.
Fully reversible with `sudo ./scripts/uninstall-driver.sh`.

### 4. Enroll and verify

```sh
fprintd-enroll -f right-index-finger
fprintd-verify -f right-index-finger
```

Lift the finger fully between scans. `verify-match` on the enrolled
finger = working.

### Resetting fingerprints (wipe enrollments)

```sh
./scripts/reset-fingerprints.sh [username]   # defaults to you
```

Lists the user's enrollments, asks for a typed `YES`, then deletes them
all through `fprintd` and confirms the list is empty. This is the
supported Linux-side reset — but know its limit: it removes enrollments
through the stack, it is **not** a sensor-flash format (no such operation
exists on Linux for this reader). For a full on-chip wipe, use the
sibling Windows tool `kensington-reset` (vendor erase command `0x442028`).

### 5. Enable login / unlock / sudo (optional)

```sh
sudo ./scripts/enable-pam.sh
```

Uses your distro's native PAM wiring (`pam-auth-update` on Debian/Ubuntu,
`authselect` on Fedora, direct `system-local-login` + `sudo` edits with
backups on Arch). Keep a terminal open
and confirm password fallback still works before logging out.

## Troubleshooting

- `probe.sh` should show `047d:00f2` with interface 0 on `usbhid`
  (FIDO2 keeps working through it) and interface 1 unbound for libfprint.
- `fprintd` stuck after plug/unplug: close Settings, unplug, replug,
  `sudo systemctl restart fprintd`.
- Pairing/marriage problems, LED matrix, packet captures: see the
  upstream project's docs (Credits) — that level of debugging needs the
  hardware in hand.
- Never "fix" things by formatting sensor storage or poking raw templates
  at the driver level; use `./scripts/reset-fingerprints.sh` for enrollment
  removal, and the Windows reset tool (`kensington-reset`, sibling repo)
  for a full on-chip wipe.

## Files

```sh
scripts/probe.sh            # read-only device + stack detection
scripts/build-driver.sh     # integrity-checked, multi-distro offline-capable build
scripts/install-driver.sh   # /opt install + fprintd drop-in (reversible)
scripts/uninstall-driver.sh # removes drop-in + /opt copy, restarts fprintd
scripts/enable-pam.sh       # distro-native PAM wiring (pam-auth-update/authselect/PAM edits)
scripts/test-device.sh      # fprintd discovery + enroll/verify smoke
scripts/reset-fingerprints.sh # delete all of a user's enrollments (typed YES)
udev/70-verimark-dt.rules   # uaccess for 047d:00f2
packaging/fprintd/50-verimark-libfprint.conf
vendor/libfprint/           # complete build-ready driver source (see VENDOR.md)
vendor/SHA256SUMS           # integrity manifest checked before every build
patches/0001-...patch       # the driver delta, human-readable (see PROVENANCE.md)
docs/rea-notes.md           # hardware profile from the Windows driver
```

## Credits

- Driver core: `vojtapl/synaTudorMiS` (base) + Kensington
  Match-in-Sensor patch by Uriziel01
  (`Uriziel01/kensington-verimark-desktop-linux`, LGPL-2.1-or-later —
  the vendored patch keeps its headers; see `patches/PROVENANCE.md`).
  Pairing tooling and deep protocol docs live there too.
- Related: `visorcraft/Kensington_VeriMark_06cb-0088` (older Gen-1
  dongle), `Popax21/synaTudor` (ThinkPad relinking approach),
  upstream `libfprint` / `fprintd`.
- No affiliation with Kensington, ACCO Brands, or Synaptics.
