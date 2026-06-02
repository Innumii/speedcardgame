#!/usr/bin/env bash
# setup.sh — installs all prerequisites for macOS (Homebrew) and Linux builds,
#             and downloads pre-built Windows x86_64 MinGW binaries into include/ and lib/.
#
# Windows users: run setup.ps1 from any PowerShell prompt instead of this script.
#   powershell -ExecutionPolicy Bypass -File setup.ps1
set -euo pipefail

# ── Colour helpers ────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
info()  { echo -e "${GREEN}[setup]${NC} $*"; }
step()  { echo -e "${CYAN}  →${NC} $*"; }
warn()  { echo -e "${YELLOW}[warn]${NC}  $*"; }
die()   { echo -e "${RED}[error]${NC} $*" >&2; exit 1; }

# ── Detect OS ─────────────────────────────────────────────────────────────────
OS="$(uname -s)"
case "$OS" in
  Darwin)    PLATFORM="macos"   ;;
  Linux)     PLATFORM="linux"   ;;
  MINGW*|MSYS*|CYGWIN*)
             PLATFORM="windows" ;;
  *)         die "Unsupported OS: $OS. This script supports macOS and Linux only.
For Windows, run:  powershell -ExecutionPolicy Bypass -File setup.ps1" ;;
esac

info "Detected platform: $PLATFORM"

# ── Project directories ───────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INCLUDE_DIR="$SCRIPT_DIR/include"
LIB_DIR="$SCRIPT_DIR/lib"

# ═════════════════════════════════════════════════════════════════════════════
# macOS — Homebrew installs for the native Mac build (Makefile.mac)
# ═════════════════════════════════════════════════════════════════════════════
install_macos() {
  # ── 1. Homebrew ─────────────────────────────────────────────────────────────
  if ! command -v brew &>/dev/null; then
    info "Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    if [[ -f /opt/homebrew/bin/brew ]]; then
      eval "$(/opt/homebrew/bin/brew shellenv)"
    fi
  else
    info "Homebrew already installed: $(brew --version | head -1)"
  fi

  # ── 2. Xcode Command Line Tools ─────────────────────────────────────────────
  if ! xcode-select -p &>/dev/null; then
    info "Installing Xcode Command Line Tools..."
    xcode-select --install
    read -rp "Press Enter once the Xcode CLT installation has finished..."
  else
    info "Xcode CLT already installed: $(xcode-select -p)"
  fi

  # ── 3. Build tools + SDL2 + OpenSSL ─────────────────────────────────────────
  brew_install pkg-config
  brew_install sdl2
  brew_install sdl2_ttf
  brew_install sdl2_image
  brew_install sdl2_mixer
  brew_install openssl@3

  info "macOS prerequisites installed successfully."
  echo ""
  info "Build with:  make -f Makefile.mac"
}

# Helper: brew install only if not already present
brew_install() {
  local formula="$1"
  if brew list --formula 2>/dev/null | grep -q "^${formula}$"; then
    info "Already installed: $formula"
  else
    info "Installing $formula..."
    brew install "$formula"
  fi
}

# ═════════════════════════════════════════════════════════════════════════════
# Linux — MinGW-w64 cross-compiler + Windows SDL2/OpenSSL binaries
# ═════════════════════════════════════════════════════════════════════════════
install_linux() {
  # ── Package manager detection ────────────────────────────────────────────────
  if command -v apt-get &>/dev/null; then
    info "Using apt"
    sudo apt-get update -y
    sudo apt-get install -y \
      build-essential pkg-config curl tar zstd \
      mingw-w64 \
      libsdl2-dev libsdl2-ttf-dev libsdl2-image-dev libsdl2-mixer-dev \
      libssl-dev

  elif command -v dnf &>/dev/null; then
    info "Using dnf"
    sudo dnf install -y \
      gcc-c++ make pkgconfig curl tar zstd \
      mingw64-gcc-c++ \
      SDL2-devel SDL2_ttf-devel SDL2_image-devel SDL2_mixer-devel \
      openssl-devel

  elif command -v pacman &>/dev/null; then
    info "Using pacman"
    sudo pacman -S --noconfirm \
      base-devel pkg-config curl tar zstd \
      mingw-w64-gcc \
      sdl2 sdl2_ttf sdl2_image sdl2_mixer openssl

  else
    die "No supported package manager found (apt, dnf, or pacman required)."
  fi

  info "System packages installed."
  download_windows_deps

  echo ""
  info "To cross-compile the Windows binary:"
  info "  make CXX=x86_64-w64-mingw32-g++ WINDRES=x86_64-w64-mingw32-windres"
}

# ═════════════════════════════════════════════════════════════════════════════
# Windows — redirect to setup.ps1
#
# If someone accidentally runs this script from inside an MSYS2/Git Bash
# shell on Windows, give them a clear redirect rather than a confusing error.
# ═════════════════════════════════════════════════════════════════════════════
install_windows() {
  local ps1="$SCRIPT_DIR/setup.ps1"

  echo ""
  warn "This script does not handle Windows setup directly."
  info "Please use setup.ps1 instead — it runs from any PowerShell prompt"
  info "and does not require an existing MSYS2 installation."
  echo ""

  if [[ -f "$ps1" ]]; then
    # Attempt to auto-launch setup.ps1 via PowerShell if it is available on PATH
    local pwsh=""
    if   command -v pwsh        &>/dev/null; then pwsh="pwsh"
    elif command -v powershell  &>/dev/null; then pwsh="powershell"
    fi

    if [[ -n "$pwsh" ]]; then
      info "Found PowerShell ($pwsh). Launching setup.ps1 now..."
      echo ""
      # Convert MSYS2/Cygwin-style path to Windows path for PowerShell
      local win_ps1
      win_ps1="$(cygpath -w "$ps1" 2>/dev/null || echo "$ps1")"
      exec "$pwsh" -ExecutionPolicy Bypass -File "$win_ps1"
    else
      info "Open PowerShell and run:"
      info "  powershell -ExecutionPolicy Bypass -File setup.ps1"
    fi
  else
    warn "setup.ps1 was not found next to this script."
    info "Download it from the repository and run:"
    info "  powershell -ExecutionPolicy Bypass -File setup.ps1"
  fi

  exit 0
}

# ═════════════════════════════════════════════════════════════════════════════
# Windows dependency downloader (used by Linux cross-compile path only)
#   SDL2 family  → GitHub Releases  (*-mingw.tar.gz assets, libsdl-org/* repos)
#   OpenSSL      → MSYS2 mingw64 repo  (*.pkg.tar.zst)
# ═════════════════════════════════════════════════════════════════════════════
download_windows_deps() {
  info "Downloading pre-built Windows (x86_64 MinGW) libraries..."
  mkdir -p "$INCLUDE_DIR" "$LIB_DIR"

  local TMP_DIR
  TMP_DIR="$(mktemp -d)"
  trap 'rm -rf "$TMP_DIR"' RETURN

  fetch_sdl_lib "libsdl-org/SDL"       "SDL2"
  fetch_sdl_lib "libsdl-org/SDL_ttf"   "SDL2_ttf"
  fetch_sdl_lib "libsdl-org/SDL_image" "SDL2_image"
  fetch_sdl_lib "libsdl-org/SDL_mixer" "SDL2_mixer"
  fetch_openssl

  info "All Windows dependencies installed into include/ and lib/."
  warn "Bundle all lib/*.dll files alongside Archcast.exe when distributing."
}

# ── Resolve latest GitHub release asset URL matching a pattern ────────────────
github_latest_url() {
  local repo="$1"
  local pattern="$2"
  local url
  url=$(curl -fsSL "https://api.github.com/repos/${repo}/releases/latest" \
    | grep '"browser_download_url"' \
    | grep "$pattern" \
    | head -1 \
    | sed 's/.*"browser_download_url": *"\([^"]*\)".*/\1/')
  [[ -n "$url" ]] || die "No release asset matching '$pattern' found in github.com/$repo"
  echo "$url"
}

# ── Download one SDL2 mingw dev tarball and unpack into include/ and lib/ ─────
fetch_sdl_lib() {
  local repo="$1"
  local label="$2"

  info "Fetching $label..."
  local url archive top_dir mingw_root
  url=$(github_latest_url "$repo" "mingw\.tar\.gz")
  archive="$TMP_DIR/${label}.tar.gz"

  step "Downloading: $url"
  curl -fsSL "$url" -o "$archive"

  step "Unpacking..."
  tar -xzf "$archive" -C "$TMP_DIR"

  top_dir=$(tar -tzf "$archive" | head -1 | cut -d'/' -f1)
  mingw_root="$TMP_DIR/${top_dir}/x86_64-w64-mingw32"
  [[ -d "$mingw_root" ]] || die "x86_64-w64-mingw32/ not found inside $archive"

  step "Copying headers → include/"
  cp -r "$mingw_root/include/." "$INCLUDE_DIR/"

  step "Copying import libs → lib/"
  cp -r "$mingw_root/lib/." "$LIB_DIR/"

  step "Copying DLLs → lib/"
  find "$mingw_root/bin" -name "*.dll" -exec cp -f {} "$LIB_DIR/" \; 2>/dev/null || true

  info "$label done."
}

# ── Download OpenSSL from the MSYS2 mingw64 package repo ─────────────────────
fetch_openssl() {
  info "Fetching OpenSSL (MSYS2 mingw64 repo)..."

  local repo_url="https://repo.msys2.org/mingw/mingw64"
  local pkg_index="$TMP_DIR/msys2_index.html"

  step "Querying MSYS2 package index..."
  curl -fsSL "$repo_url/" -o "$pkg_index"

  local pkg_filename
  pkg_filename=$(grep -oP 'mingw-w64-x86_64-openssl-[\d.]+-\d+-any\.pkg\.tar\.zst(?=")' "$pkg_index" \
    | sort -V | tail -1)
  [[ -n "$pkg_filename" ]] || die "Could not locate openssl package in MSYS2 index."

  local pkg_file="$TMP_DIR/$pkg_filename"
  step "Downloading: ${repo_url}/${pkg_filename}"
  curl -fsSL "${repo_url}/${pkg_filename}" -o "$pkg_file"

  step "Unpacking .pkg.tar.zst..."
  local pkg_extract="$TMP_DIR/openssl_pkg"
  mkdir -p "$pkg_extract"
  zstd -d "$pkg_file" --stdout | tar -x -C "$pkg_extract"

  local mingw64="$pkg_extract/mingw64"
  [[ -d "$mingw64" ]] || die "Unexpected MSYS2 layout — mingw64/ not found."

  step "Copying OpenSSL headers → include/"
  [[ -d "$mingw64/include/openssl" ]] \
    && cp -r "$mingw64/include/openssl" "$INCLUDE_DIR/" \
    || warn "openssl include dir not found in package"

  step "Copying OpenSSL libs → lib/"
  find "$mingw64/lib" \( -name "libssl*" -o -name "libcrypto*" \) \
    -exec cp -f {} "$LIB_DIR/" \; 2>/dev/null || true

  step "Copying OpenSSL DLLs → lib/"
  find "$mingw64/bin" \( -name "libssl*.dll" -o -name "libcrypto*.dll" \) \
    -exec cp -f {} "$LIB_DIR/" \; 2>/dev/null || true

  info "OpenSSL done."
}

# ── Main ──────────────────────────────────────────────────────────────────────
case "$PLATFORM" in
  macos)   install_macos   ;;
  linux)   install_linux   ;;
  windows) install_windows ;;
esac