#!/bin/bash
# Author: Sriram Rajkumar

# Check if both arguments are provided
if [ $# -ne 2 ]; then
    echo "Error: Exactly 2 arguments required." >&2
    echo "Usage: $0 <writefile> <writestr>" >&2
    exit 1
fi

# Assign the 2 arguements to respective variables
writefile="$1"
writestr="$2"

# Check if arguments are not empty string
if [ -z "$writefile" ]; then
    echo "Error: First argument (writefile) cannot be empty." >&2
    exit 1
fi

if [ -z "$writestr" ]; then
    echo "Error: Second argument (writestr) cannot be empty." >&2
    exit 1
fi

# Extract directory path from the full file path excludes the filename
dir_path=$(dirname "$writefile")

# Create directory path if it doesn't exist -p creates the directory even if it doesn't exist
if ! mkdir -p "$dir_path" 2>/dev/null; then
    echo "Error: Could not create directory path: $dir_path" >&2
    exit 1
fi

# Writing the string to the file, overwriting if it exists
if ! echo "$writestr" > "$writefile" 2>/dev/null; then
    echo "Error: Could not create or write to file: $writefile" >&2
    exit 1
fi

echo "File created successfully: $writefile"