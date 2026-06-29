{
  description = "luckecli — ESP DMX/lighting fixture CLI";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAll = nixpkgs.lib.genAttrs systems;
    in {
      # Dev shell: build tools plus the libMVRgdtf deps (boost + xerces-c) so the
      # GDTF exporter can build/link.
      #   nix develop          # enter the shell
      #   cmake -B build -G Ninja && cmake --build build
      devShells = forAll (system:
        let pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
            name = "luckecli-dev";
            packages = with pkgs; [
              cmake
              ninja
              pkg-config
              git # submodules

              # libMVRgdtf dependencies
              boost
              xerces-c
            ];
          };
        });
    };
}
