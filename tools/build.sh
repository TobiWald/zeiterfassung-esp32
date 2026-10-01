#!/usr/bin/env bash
# Baut die Firmware mit arduino-cli und erzeugt eine kombinierte .bin
# (Bootloader + Partitionstabelle + App) zum Flashen an Adresse 0x0.
#
# Benötigt: arduino-cli mit installiertem Core "esp32:esp32" (3.0.x)
#           sowie die Bibliothek "Adafruit GFX Library".
# Umgebungsvariablen:
#   ARDUINO_CLI   Pfad zu arduino-cli (Standard: arduino-cli)
#   CLI_CONFIG    optionale arduino-cli Konfigurationsdatei
#   FQBN_BASE     Board-Kennung (Standard: esp32:esp32:esp32s3)
#   EXTRA_ARGS    zusätzliche Argumente für "compile"
#   ESP32_CORE_DIR  Ordner, in dem der ESP32-Core liegt (Standard: ~/.arduino15)
# esptool: pip install esptool
set -euo pipefail
cd "$(dirname "$0")/.."
CLI=${ARDUINO_CLI:-arduino-cli}
CFG=()
[ -n "${CLI_CONFIG:-}" ] && CFG=(--config-file "$CLI_CONFIG")
FQBN="${FQBN_BASE:-esp32:esp32:esp32s3}:CDCOnBoot=cdc,FlashSize=4M,FlashMode=qio,PartitionScheme=huge_app,PSRAM=disabled"

"$CLI" "${CFG[@]}" compile -b "$FQBN" --output-dir build ${EXTRA_ARGS:-} Zeiterfassung

BOOT_APP0=$(find "${ESP32_CORE_DIR:-$HOME/.arduino15}" -path '*tools/partitions/boot_app0.bin' 2>/dev/null | head -1)
[ -f "$BOOT_APP0" ] || { echo "boot_app0.bin nicht gefunden – ESP32_CORE_DIR setzen"; exit 1; }
mkdir -p release
python3 -m esptool --chip esp32s3 merge_bin -o release/zeiterfassung_combined_0x0.bin \
  --flash_mode dio --flash_freq 80m --flash_size 4MB \
  0x0 build/Zeiterfassung.ino.bootloader.bin \
  0x8000 build/Zeiterfassung.ino.partitions.bin \
  0xe000 "$BOOT_APP0" \
  0x10000 build/Zeiterfassung.ino.bin
ls -l release/zeiterfassung_combined_0x0.bin
