{
  description = "Development environment for rofi power";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs, ... }: {
    devShells.x86_64-linux.default =
      let
        pkgs = nixpkgs.legacyPackages.x86_64-linux;
      in
      pkgs.mkShell { 
        buildInputs = with pkgs; [
          cmake
          gnumake
          gcc
          pkg-config
          autoconf
          bear
          automake
          libtool

          glib
          rofi
          cairo.dev
          gtk3
        ];
      };
  };
}
