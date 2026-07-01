{
  lib,
  stdenv,
  cmake,
  nix-update-script,
  util-linux,
  version ? "git",
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "luckecli";
  inherit version;
  __structuredAttrs = true;
  strictDeps = true;

  src = ./.;

  nativeBuildInputs = [
    cmake
  ];
  buildInputs = [
    util-linux
  ];
  installPhase = "install -D luckecli $out/bin/luckecli";

  passthru.updateScript = nix-update-script { };

  meta = {
    description = "CLI for Lucke configuration";
    homepage = "https://github.com/gendestry/luckecli";
    maintainers = with lib.maintainers; [ ];
    mainProgram = "luckecli";
    platforms = lib.platforms.all;
  };
})
