{
  lib,
  stdenv,
  cmake,
  nix-update-script,
  util-linux,
  src ? ./.,
  version ? "git",
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "luckecli";
  inherit src version;
  __structuredAttrs = true;
  strictDeps = true;

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
