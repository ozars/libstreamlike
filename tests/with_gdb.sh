#!/bin/bash

if [ $# -lt 1 ]; then
  echo "No program given. Exiting..."
  exit 1
fi

PROG=$1

cleanup()
{
  if [ ! -z "$tmpfile" ]; then
    echo "Cleaning up $tmpfile:"
    echo '```'
    cat "$tmpfile"
    echo '```'
    rm -rf "$tmpfile"
    if [ $? -eq 0 ]; then
      echo "Done."
    else
      echo "Error."
    fi
  else
    echo "No file to cleanup."
  fi
}

trap cleanup 0 2 3 6 9
tmpfile=`mktemp` || exit 1

cat <<EOT > "$tmpfile"
set environment CK_VERBOSITY=verbose
set environment CK_LOG_FILE_NAME=-
set environment CK_TAP_LOG_FILE_NAME=-
EOT

for i in "${@:2}"; do
  echo "$i" >> "$tmpfile"
done

CK_INCLUDE_TAGS=NONE make check
"`dirname $0`/../libtool" execute gdb -x "$tmpfile" "$PROG"

