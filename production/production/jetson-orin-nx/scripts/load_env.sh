#!/bin/sh

load_env_defaults() {
  env_file="$1"

  if [ ! -f "$env_file" ]; then
    return 0
  fi

  while IFS= read -r raw_line || [ -n "$raw_line" ]; do
    line="$(printf '%s' "$raw_line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"

    case "$line" in
      ""|\#*)
        continue
        ;;
      *=*)
        key="${line%%=*}"
        value="${line#*=}"
        key="$(printf '%s' "$key" | sed 's/[[:space:]]*$//')"
        value="$(printf '%s' "$value" | sed 's/^["'\"'"]//;s/["'\"'"]$//')"

        if printf '%s' "$key" | grep -Eq '^[A-Za-z_][A-Za-z0-9_]*$'; then
          if ! eval '[ "${'"$key"'+x}" = x ]'; then
            export "$key=$value"
          fi
        fi
        ;;
    esac
  done < "$env_file"
}
