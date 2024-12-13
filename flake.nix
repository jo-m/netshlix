{
  description = "Nix flake for esp32-idf";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
    esp-dev.url = "github:mirrexagon/nixpkgs-esp-dev";
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

        packages = [pkgs.clang-tools_18];
      };
    });
}
