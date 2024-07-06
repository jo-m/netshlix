{
  description = "Nix flake for esp32-idf";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    # FIXME: Temporary https://github.com/mirrexagon/nixpkgs-esp-dev/pull/56.
    # Reset to "github:mirrexagon/nixpkgs-esp-dev" when merged.
    esp-dev.url = "github:Lindboard/nixpkgs-esp-dev";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    esp-dev,
    ...
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs =
        import nixpkgs
        {
          inherit system;
        }
        // esp-dev.packages.${system};
    in {
      formatter = pkgs.alejandra;

      devShells.default = pkgs.mkShell {
        buildInputs = with pkgs; [
          esp-idf-esp32
        ];
      };
    });
}
