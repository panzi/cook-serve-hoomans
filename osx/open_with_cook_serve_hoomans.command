#!/bin/bash
archive="$(osascript -e 'tell application "Terminal" to return POSIX path of (choose file with prompt "Choose “game.ios” from the Cook, Serve, Delicious! installation directory." invisibles true showing package contents true)')"
if [ $? -ne 0 ]; then
	exit 1
fi
"$(dirname "$BASH_SOURCE")/bin/cook_serve_hoomans" "$archive"
echo "Press ENTER to continue..."
read
