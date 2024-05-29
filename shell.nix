{ pkgs ? import <nixpkgs> {} }:
let unstable = import <nixpkgs-unstable> {};
in with pkgs; mkShell {
  nativeBuildInputs = [
    unstable.stdenv.cc
    cmake
    unstable.clang-tools
  ];

  hardeningDisable = [ "fortify" ];
}
