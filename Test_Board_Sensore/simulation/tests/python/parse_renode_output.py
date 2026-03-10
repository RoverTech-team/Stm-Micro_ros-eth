#!/usr/bin/env python3
"""
Parse Renode output and extract RESULT key=value pairs.
Handles ANSI escape sequences and color codes.
"""

import sys
import re


def strip_ansi_codes(text):
    """Remove ANSI escape sequences from text.

    Handles common ANSI codes including:
    - CSI sequences: ESC[...m (color codes)
    - Single character escapes
    """
    # Pattern matches ESC[...m sequences (color codes like [0m, [32m, [31;1m, etc.)
    ansi_pattern = re.compile(r"\x1b\[[0-9;]*m")
    text = ansi_pattern.sub("", text)

    # Also handle literal bracket patterns that appear in Robot output
    # These appear as literal [0m, [32m etc. in the string
    literal_pattern = re.compile(r"\[0-9;]+m")
    text = literal_pattern.sub("", text)

    return text


def parse_results(output):
    """Extract all RESULT key=value pairs from output.

    Args:
        output: Raw Renode output string (may contain ANSI codes)

    Returns:
        Dictionary of key-value pairs
    """
    # Strip ANSI codes first
    clean_output = strip_ansi_codes(output)

    results = {}
    for line in clean_output.split("\n"):
        # Match RESULT key=value pattern
        match = re.match(r"RESULT\s+(\w+)=([^\r\n]+)", line.strip())
        if match:
            key, value = match.groups()
            results[key] = value.strip()

    return results


def extract_single_result(output, key):
    """Extract a single RESULT value by key.

    Args:
        output: Raw Renode output string
        key: The key to extract (e.g., 'initialized', 'default_distance')

    Returns:
        The value string, or None if not found
    """
    results = parse_results(output)
    return results.get(key)


def main():
    """Command-line interface for extracting a single result."""
    if len(sys.argv) < 2:
        print("Usage: parse_renode_output.py <key> [output_file]", file=sys.stderr)
        print("  key:         The RESULT key to extract", file=sys.stderr)
        print(
            "  output_file: Optional file containing Renode output (default: stdin)",
            file=sys.stderr,
        )
        sys.exit(1)

    key = sys.argv[1]

    # Read from file or stdin
    if len(sys.argv) >= 3:
        try:
            with open(sys.argv[2], "r") as f:
                output = f.read()
        except IOError as e:
            print(f"Error reading file: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        output = sys.stdin.read()

    value = extract_single_result(output, key)

    if value is not None:
        print(value)
        sys.exit(0)
    else:
        available = list(parse_results(output).keys())
        print(f"Key '{key}' not found. Available keys: {available}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
