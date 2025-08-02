rec {
  nixcrpkgs = import <nixcrpkgs> {};

  src = nixcrpkgs.filter ./.;

  build = (import ./nix/build.nix) src;

  win32 = build "win" nixcrpkgs.win32;
  linux-x86 = build "linux-x86" nixcrpkgs.linux-x86;
  linux-rpi = build "linux-rpi" nixcrpkgs.linux-rpi;
  macos = build "macos" nixcrpkgs.macos;

  installers = nixcrpkgs.bundle {
    win32 = win32.installer;
    linux-x86 = linux-x86.installer;
    linux-rpi = linux-rpi.installer;
    macos = macos.installer;
  };
}
