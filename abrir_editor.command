#!/bin/bash
cd "$(dirname "$0")" || exit 1
(sleep 1; open -a "Google Chrome" "http://localhost:8781/mkxl_editor.html") &
exec python3 -m http.server 8781 --bind 127.0.0.1
