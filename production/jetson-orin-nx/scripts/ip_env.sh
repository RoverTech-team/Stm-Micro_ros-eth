#!/bin/sh

require_ipv4() {
  label="$1"
  value="$2"

  if ! printf '%s\n' "$value" | awk -F. '
    NF != 4 { exit 1 }
    {
      for(i = 1; i <= 4; ++i) {
        if($i !~ /^[0-9]+$/ || $i < 0 || $i > 255) {
          exit 1
        }
      }
    }
  '; then
    echo "$label must be a valid IPv4 address, got: $value"
    exit 1
  fi
}

split_ipv4() {
  value="$1"
  old_ifs="${IFS}"
  IFS=.
  set -- $value
  IFS="${old_ifs}"
  printf '%s %s %s %s\n' "$1" "$2" "$3" "$4"
}
