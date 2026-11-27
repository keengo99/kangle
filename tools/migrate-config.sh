#!/bin/sh
set -eu

# Convert legacy Kangle configuration files to the current XML schema.
# The PHP implementation is kept beside this wrapper so the migration logic
# can use a real XML parser instead of fragile regular-expression rewrites.

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
KANGLE_DIR=/vhs/kangle
EXPECT_KANGLE_DIR=0

for ARG in "$@"; do
	if [ "$EXPECT_KANGLE_DIR" -eq 1 ]; then
		KANGLE_DIR=$ARG
		EXPECT_KANGLE_DIR=0
		continue
	fi
	case "$ARG" in
		--kangle-dir)
			EXPECT_KANGLE_DIR=1
			;;
		--kangle-dir=*)
			KANGLE_DIR=${ARG#*=}
			;;
	esac
done

if [ -n "${PHP_BIN:-}" ]; then
	PHP_COMMAND=$PHP_BIN
else
	PHP_COMMAND=
	# Prefer the newest bundled EasyPanel PHP, while still supporting older
	# installations that do not have php83.
	for CANDIDATE in "$KANGLE_DIR"/ext/php*/bin/php; do
		if [ -x "$CANDIDATE" ]; then
			PHP_COMMAND=$CANDIDATE
		fi
	done
	if [ -z "$PHP_COMMAND" ] && command -v php >/dev/null 2>&1; then
		PHP_COMMAND=$(command -v php)
	fi
	if [ -z "$PHP_COMMAND" ]; then
		echo "error: PHP CLI was not found; set PHP_BIN or install PHP with DOM support" >&2
		exit 1
	fi
fi

if ! "$PHP_COMMAND" -r 'exit(class_exists("DOMDocument") ? 0 : 1);'; then
	echo "error: PHP DOM extension is required" >&2
	exit 1
fi

exec "$PHP_COMMAND" "$SCRIPT_DIR/migrate-config.php" "$@"
