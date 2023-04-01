/* Derivation file for the ovrly project
   callPackage style is used for better integration with nixpkgs.
*/

{ pkgs ? import <nixpkgs> {} }:

pkgs.callPackage ./derivation.nix {}
