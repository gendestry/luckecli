{
  description = "luckecli — ESP DMX/lighting fixture CLI";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAll = nixpkgs.lib.genAttrs systems;
    in {
      # Dev shell: build tools plus the libMVRgdtf deps so the GDTF exporter can
      # build/link. We build libMVRgdtf with its bundled tinyXML (not xerces) and
      # without MVR-xchange (which needs boost), so the only extra dep is libuuid
      # (the lib links -luuid on Linux); pthread comes from glibc.
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
              gdb # debugging (VS Code launch config)

              gcc14 # compiler (matches build; provides clangd's system headers)
              clang-tools # clangd language server for the editor

              util-linux # provides libuuid (-luuid) for libMVRgdtf
            ];
          };
        });
    };
}
