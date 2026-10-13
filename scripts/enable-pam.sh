#!/usr/bin/env bash
# Wire fingerprint auth into login/unlock/sudo using the distro-native tool:
#   debian/ubuntu: pam-auth-update / Fedora: authselect / Arch: direct PAM edits.
# Run with sudo AFTER enroll + verify succeed. Keep a root terminal open
# until you have confirmed password fallback still works.
set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
  echo "run with sudo" >&2
  exit 1
fi

# shellcheck source=lib/distro.sh
source "$(dirname -- "${BASH_SOURCE[0]}")/lib/distro.sh"

case "${DISTRO_FAMILY}" in
  debian) pkg_install fprintd libpam-fprintd ;;
  arch) pkg_install fprintd ;;
  fedora) pkg_install fprintd fprintd-pam authselect ;;
  *)
    echo "unsupported distro '${DISTRO_ID}': wire PAM manually (see README)" >&2
    exit 1
    ;;
esac

if ! fprintd-list "${SUDO_USER:-$USER}" 2>/dev/null | grep -q .; then
  echo "no enrollments found; run fprintd-enroll first" >&2
  exit 1
fi

case "${DISTRO_FAMILY}" in
  debian)
    pam-auth-update --enable fprintd
    ;;
  fedora)
    authselect enable-feature with-fingerprint
    authselect apply-changes
    authselect current
    ;;
  arch)
    # ArchWiki "fprint": fingerprint-first for local login + sudo.
    # NOTE (CVE-2024-37408): fingerprint-sufficient sudo lets background
    # processes auth without prompting. Drop the sudo file if that worries
    # you; login keeps working.
    for pam_file in /etc/pam.d/system-local-login /etc/pam.d/sudo; do
      [[ -f ${pam_file} ]] || { echo "missing ${pam_file}, skipping" >&2; continue; }
      [[ -f ${pam_file}.bak ]] || cp "${pam_file}" "${pam_file}.bak"
      if ! grep -q 'pam_fprintd\.so' "${pam_file}"; then
        { echo 'auth      sufficient pam_fprintd.so'; cat "${pam_file}"; } > "${pam_file}.new"
        mv "${pam_file}.new" "${pam_file}"
        echo "prepended pam_fprintd.so to ${pam_file} (backup at ${pam_file}.bak)"
      else
        echo "pam_fprintd.so already present in ${pam_file}"
      fi
    done
    ;;
esac

echo "enabled: fingerprint offered for login, unlock and sudo, password still works."
echo "verify in a NEW terminal: 'sudo -k; sudo true' should offer your finger."
