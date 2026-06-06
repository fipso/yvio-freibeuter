# Dev shell for the yvio HLE emulator.
#   nix-shell        # enter the environment
#   make run         # configure + build + run (from the emu/ dir)
{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  name = "yvio-emu";
  packages = with pkgs; [
    gcc
    gnumake
    cmake
    pkg-config
    unicorn # ARM7TDMI-S CPU emulation (libunicorn)
    raylib # window / audio / input (used from M1 onward)
  ];

  shellHook = ''
    echo "yvio-emu dev shell: gcc $(gcc -dumpversion), cmake $(cmake --version | head -1 | cut -d' ' -f3)"
    echo "unicorn: $(pkg-config --modversion unicorn 2>/dev/null || echo MISSING)  raylib: $(pkg-config --modversion raylib 2>/dev/null || echo '(no .pc, use find_package)')"
  '';
}
