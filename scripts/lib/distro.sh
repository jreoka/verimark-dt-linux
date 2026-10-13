# Shared distro helper. Source it, don't run it:
#   source "$(dirname "${BASH_SOURCE[0]}")/lib/distro.sh"
#
# Provides:
#   DISTRO_ID / DISTRO_FAMILY  (debian | arch | fedora | unknown)
#   pkg_install <pkgs...>      install via the native manager
#   libfprint_version          print installed stock libfprint version
#
# For tests: point OS_RELEASE_FILE at a fixture os-release file.

detect_distro() {
  local file="${OS_RELEASE_FILE:-/etc/os-release}"
  local id="" like=""
  if [[ -f ${file} ]]; then
    id=$(grep -E '^ID=' "${file}" | head -1 | cut -d= -f2 | tr -d '"' | tr '[:upper:]' '[:lower:]')
    like=$(grep -E '^ID_LIKE=' "${file}" | head -1 | cut -d= -f2 | tr -d '"' | tr '[:upper:]' '[:lower:]' || true)
  fi
  DISTRO_ID="${id:-unknown}"
  local hay=" ${id} ${like} "
  if [[ ${like} == *debian* || ${like} == *ubuntu* ]] ||
     [[ ${hay} == *" debian "* || ${hay} == *" ubuntu "* || ${hay} == *" linuxmint "* ||
        ${hay} == *" pop "* || ${hay} == *" zorin "* || ${hay} == *" kali "* ||
        ${hay} == *" raspbian "* || ${hay} == *" elementary "* ]]; then
    DISTRO_FAMILY=debian
  elif [[ ${like} == *arch* ]] ||
       [[ ${hay} == *" arch "* || ${hay} == *" cachyos "* || ${hay} == *" endeavouros "* ||
          ${hay} == *" manjaro "* || ${hay} == *" garuda "* ]]; then
    DISTRO_FAMILY=arch
  elif [[ ${like} == *fedora* || ${like} == *rhel* || ${like} == *centos* ]] ||
       [[ ${hay} == *" fedora "* || ${hay} == *" rhel "* || ${hay} == *" centos "* ||
          ${hay} == *" rocky "* || ${hay} == *" alma "* || ${hay} == *" ol "* ||
          ${hay} == *" nobara "* || ${hay} == *" bazzite "* ]]; then
    DISTRO_FAMILY=fedora
  else
    DISTRO_FAMILY=unknown
  fi
}

# Install packages with the native manager. Needs root/sudo.
pkg_install() {
  case "${DISTRO_FAMILY:-unknown}" in
    debian)
      sudo apt-get update -qq
      sudo apt-get install -y -qq "$@"
      ;;
    arch)
      sudo pacman -S --needed --noconfirm "$@"
      ;;
    fedora)
      sudo dnf install -y "$@"
      ;;
    *)
      echo "unsupported distro '${DISTRO_ID}': install these manually: $*" >&2
      return 1
      ;;
  esac
}

# Print the stock libfprint version, or nothing if not installed.
libfprint_version() {
  case "${DISTRO_FAMILY:-unknown}" in
    debian)
      dpkg-query -W -f='${Version}' libfprint-2-2 2>/dev/null
      ;;
    arch)
      pacman -Q libfprint 2>/dev/null | awk '{print $2}'
      ;;
    fedora)
      rpm -q --queryformat '%{VERSION}' libfprint 2>/dev/null
      ;;
  esac
}

detect_distro
