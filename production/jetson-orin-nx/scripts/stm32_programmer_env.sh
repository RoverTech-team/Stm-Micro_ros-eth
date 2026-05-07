#!/bin/sh

prepend_path_if_dir() {
  dir="$1"
  if [ -d "$dir" ]; then
    case ":$PATH:" in
      *":$dir:"*) ;;
      *) PATH="$dir:$PATH" ;;
    esac
  fi
}

discover_stm32_programmer_dir() {
  for dir in \
    /Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin \
    /Volumes/Untitled/STM32CubeProgrammer.app/Contents/Resources/bin \
    /Volumes/Untitled/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin \
    /Volumes/Untitled/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin
  do
    if [ -x "$dir/STM32_Programmer_CLI" ]; then
      printf '%s\n' "$dir"
      return 0
    fi
  done

  found_dir="$(find /Volumes/Untitled -type f -name STM32_Programmer_CLI -print 2>/dev/null | head -n 1)"
  if [ -n "$found_dir" ]; then
    dirname "$found_dir"
    return 0
  fi

  return 1
}

setup_stm32_programmer_path() {
  discovered_dir="$(discover_stm32_programmer_dir || true)"

  if [ -n "$discovered_dir" ]; then
    prepend_path_if_dir "$discovered_dir"
  fi

  export PATH
}

resolve_stm32_programmer_cli() {
  if [ -n "${STM32_PROGRAMMER_CLI:-}" ] && [ -x "${STM32_PROGRAMMER_CLI}" ]; then
    printf '%s\n' "$STM32_PROGRAMMER_CLI"
    return 0
  fi

  if command -v STM32_Programmer_CLI >/dev/null 2>&1; then
    command -v STM32_Programmer_CLI
    return 0
  fi

  return 1
}
