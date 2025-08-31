#!/bin/sh
#Finder Script with basic checks
# Author: Sriram Rajkumar

# Check if both arguments are provided
if [ $# -ne 2 ]; then
    echo "Error: Exactly 2 arguments required." >&2
    echo "Usage: $0 <filesdir> and/or <searchstr> is missing" >&2
    exit 1
fi

# Assign the 2 arguements to respective variables
filesdir="$1"
searchstr="$2"

# Check if arguments are not empty string
if [ -z "$filesdir" ]; then
    echo "Error: File Directory (First Args) cannot be empty." >&2
    exit 1
fi

if [ -z "$searchstr" ]; then
    echo "Search String (Second Args) cannot be empty." >&2
    exit 1
fi

# Check if Directory is Valid within the filesystem
if [ ! -d "$filesdir" ]; then
    echo "Error: '$filesdir' is not a valid directory" >&2
    exit 1
fi

# Computing Total Files in Directory
total_files=$(find "$filesdir" -type f | wc -l)


# Starting String Search recursively
match_sum=$(grep -r -c "$searchstr" "$filesdir" | awk -F: '{sum += $2} END {print sum + 0}')

echo "The number of files are $total_files and the number of matching lines are $match_sum"
