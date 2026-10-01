#!/usr/bin/env bash
# Host-Test bauen und ausführen (Linux, g++). GFX=Pfad zur Adafruit GFX Library
set -e
cd "$(dirname "$0")"
GFX=${GFX:-/opt/ard/sketchbook/libraries/Adafruit-GFX-Library}
mkdir -p out
g++ -std=gnu++17 -O1 -w -DARDUINO=100 -Istub -I$GFX -I../../Zeiterfassung host_test.cpp fake_env.cpp \
  ../../Zeiterfassung/ui.cpp ../../Zeiterfassung/timeutil.cpp ../../Zeiterfassung/exporter.cpp \
  $GFX/Adafruit_GFX.cpp -x c ../../Zeiterfassung/qrcodegen.c -o out/host_test
./out/host_test
