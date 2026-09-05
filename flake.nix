{
  description = "musikcube: a cross-platform, terminal-based audio engine, library, player and server";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        linuxOnly = pkgs.lib.optionals pkgs.stdenv.isLinux [
          pkgs.alsa-lib
          pkgs.libev
        ];

        # shared with devShells.default below so the package and the dev
        # shell can never drift apart on what's needed to build this.
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.pkg-config
          pkgs.git
        ];

        buildInputs = [
          pkgs.curl
          pkgs.openssl
          pkgs.zlib
          pkgs.ncurses
          pkgs.taglib
          pkgs.ffmpeg
          pkgs.nlohmann_json
          pkgs.sqlite
          pkgs.libmicrohttpd
          pkgs.lame
          pkgs.asio
        ] ++ linuxOnly;
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "musikcube";
          version = "3.0.5";

          src = pkgs.lib.cleanSource ./.;

          nativeBuildInputs = nativeBuildInputs ++ [ pkgs.patchelf ];
          inherit buildInputs;

          # the project's own `postbuild` cmake target runs
          # script/patch-rpath.sh unconditionally to rewrite bin/{musikcube,
          # plugins/*.so} to find their sibling libraries via $ORIGIN (the
          # layout the upstream release tarballs use, see
          # script/archive-standalone-nix.sh). It relies on relative paths
          # that don't resolve from the cmake build directory, so under Nix
          # it just fails to find anything and no-ops -- harmless, since the
          # Nix ld wrapper already burns correct absolute /nix/store rpaths
          # into every binary at link time regardless. patchelf is still
          # needed on PATH for the script to run at all (silently failing to
          # find its target is fine; a missing binary would abort it).

          installPhase = ''
            runHook preInstall

            # nixpkgs' cmake setup-hook leaves us inside the out-of-tree
            # cmake build directory (a "build/" subdir of $sourceRoot) --
            # but the project's own build writes bin/ to $sourceRoot itself
            # (see script/post-build.sh), one level up from here.
            cd ..

            mkdir -p $out/opt/musikcube
            cp -r bin/* $out/opt/musikcube/

            mkdir -p $out/bin
            ln -s $out/opt/musikcube/musikcube $out/bin/musikcube
            ln -s $out/opt/musikcube/musikcubed $out/bin/musikcubed

            # Noctalia (https://github.com/noctalia-dev/noctalia-shell) theme
            # template: generates a musikcube color theme from the current
            # wallpaper palette. See contrib/noctalia/README.md.
            mkdir -p $out/share/noctalia-templates/musikcube
            cp ${./contrib/noctalia}/* $out/share/noctalia-templates/musikcube/

            runHook postInstall
          '';

          meta = with pkgs.lib; {
            description = "A cross-platform, terminal-based audio engine, library, player and server";
            homepage = "https://github.com/clangen/musikcube";
            license = licenses.bsd3;
            platforms = platforms.linux ++ platforms.darwin;
            mainProgram = "musikcube";
          };
        };

        devShells.default = pkgs.mkShell {
          name = "musikcube-dev";

          inherit nativeBuildInputs buildInputs;

          # matches the build's own detection of a Nix environment (see
          # src/musikcube/CMakeLists.txt's `NIX_CC` check for static ncurses
          # linking on Darwin -- irrelevant here on Linux, but harmless).
          shellHook = ''
            echo "musikcube dev shell: cmake $(cmake --version | head -n1 | awk '{print $3}'), $(cc --version | head -n1)"
          '';
        };

        # convenience: a real Subsonic-API server for manually exercising the
        # subsonicsource plugin against, e.g.:
        #   nix run .#navidrome
        apps.navidrome = flake-utils.lib.mkApp { drv = pkgs.navidrome; };
      });
}
