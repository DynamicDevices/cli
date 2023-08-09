{
  nixConfig = {
    extra-substituters = [ "https://dynamicdevices.cachix.org" ];
    extra-trusted-public-keys = [ "dynamicdevices.cachix.org-1:XRHcFz9U+B7UbqEUiBlgjKaUe5N87xCZVK0vgCOZQt4=" ];
  };
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-23.05";
    nrf-nix.url = "github:matthewcroughan/nrf-nix";
    openthread-mqttsn = {
      url = "git+https://github.com/DynamicDevices/openthread-upstream?submodules=1&ref=nrf-connect-with-mqtt-sn";
      flake = false;
    };
  };
  outputs = { self, nixpkgs, nrf-nix, openthread-mqttsn, ... }:
  let
    pkgs = import nixpkgs { system = "x86_64-linux"; overlays = [ nrf-nix.overlays.default ]; config = { allowUnfree = true; segger-jlink.acceptLicense = true; }; };
  in
  rec {
    packages.x86_64-linux.flash = pkgs.writeShellScriptBin "flash-script" ''
      TMPDIR=$(${pkgs.mktemp}/bin/mktemp -d)
      trap 'rm -rf -- "TMPDIR"' EXIT
      if [ -z "$1" ]; then
          echo "Error: Specify the port to flash to, e.g: /dev/ttyACM0"
          exit 1
      fi
      ${pkgs.nrfutil}/bin/nrfutil pkg generate --hw-version 52 --sd-req 0x00 --debug-mode --application ${packages.x86_64-linux.default}/zephyr.hex "$TMPDIR/dfu.zip"
      ${pkgs.nrfutil}/bin/nrfutil dfu usb-serial -pkg "$TMPDIR/dfu.zip" -p "$1" -b 115200
    '';
    packages.x86_64-linux.default = pkgs.mkZephyrProject {
      name = "cli";
      app = ".";  # I have to handle apps that are at the root better in nrf-nix
      preBuild = ''
        rm -rf modules/lib/openthread
        cp -r --no-preserve=mode ${openthread-mqttsn} modules/lib/openthread
      '';
      cmakeFlags = [
        ''-DEXTRA_CONF_FILE="overlay-usb.conf overlay-multiprotocol.conf"''
      ];
      board = "nrf52840dongle_nrf52840";
      westWorkspace = pkgs.fetchWestWorkspace {
        url = "https://github.com/nrfconnect/sdk-nrf";
        rev = "v2.4.1";
        sha256 = "sha256-y91uYSsdk0vMSAubhQbGwTarnZDfBZlJrIopR0MA9MA=";
      };
      src = self;
      filesToInstall = [ "*.hex" ];
    };
  };
}

