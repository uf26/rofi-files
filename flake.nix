{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      package = self.packages.${system}.default;
      packages.${system}.default = pkgs.stdenv.mkDerivation {
        pname = "rofi-files";
        version = "1.0";
        src = ./.;
        buildInputs = with pkgs; [
          autoreconfHook
          pkg-config
          gobject-introspection
          wrapGAppsHook3
	  rofi-unwrapped
          glib
          cairo
        ];
      };
    };
}
