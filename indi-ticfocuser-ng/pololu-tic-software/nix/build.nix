src: config_name: env:

let
  payload = env.make_derivation {
    builder = ./builder.sh;
    inherit src;
    cross_inputs = [ env.libusbp env.qt ];
    dejavu = (if env.os == "linux" then env.dejavu-fonts else null);
  };

  license_set =
    (if env.os == "linux" then env.dejavu-fonts.license_set else {}) //
    env.libusbp.license_set //
    env.qt.license_set //
    env.global_license_set;

  license = env.make_derivation {
    name = "license";
    builder.ruby = ./license_builder.rb;
    inherit src;
    commit = builtins.getEnv "commit";
    nixcrpkgs_commit = builtins.getEnv "nixcrpkgs_commit";
    nixpkgs_commit = builtins.getEnv "nixpkgs_commit";
    license_names = builtins.attrNames license_set;
    licenses = builtins.attrValues license_set;
  };

  installer = env.make_derivation {
    name = "${config_name}-installer";
    builder.ruby =
      if env.os == "windows" then ./windows_installer_builder.rb
      else if env.os == "linux" then ./linux_installer_builder.rb
      else if env.os == "macos" then ./macos_installer_builder.rb
      else throw "?";
    inherit src config_name payload license;
    libusbp = env.libusbp;
  };

in
  payload // { inherit env license installer; }
