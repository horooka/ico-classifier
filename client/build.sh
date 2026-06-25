#!/usr/bin/env bash
set -euo pipefail

GTK_CFLAGS="$(pkg-config --cflags gtkmm-3.0)"
GTK_LIBS="$(pkg-config --libs gtkmm-3.0)"
JSON_CFLAGS="$(pkg-config --cflags json-glib-1.0)"
JSON_LIBS="$(pkg-config --libs json-glib-1.0)"
CURL_CFLAGS="$(pkg-config --cflags libcurl)"
CURL_LIBS="$(pkg-config --libs libcurl)"

g++ -std=c++17 -Wall -Wextra -Wpedantic \
  -Iinclude \
  $GTK_CFLAGS $JSON_CFLAGS $CURL_CFLAGS \
  src/main.cpp \
  src/runpod_ico_classifier.cpp \
  src/config_io.cpp \
  src/icon_dir_io.cpp \
  src/name_match.cpp \
  src/infer_client.cpp \
  src/pick_dialogs.cpp \
  $GTK_LIBS $JSON_LIBS $CURL_LIBS \
  -o ico-classifier
