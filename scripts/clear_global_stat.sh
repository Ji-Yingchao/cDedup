#!/bin/bash

json_file="/home/jyc/cDedup/global_stat.json"

# Check if jq is installed
if ! command -v jq > /dev/null; then
  echo "Error: jq is not installed. Please install jq before running this script."
  exit 1
fi

# Set all numeric values to zero using jq
jq 'walk(if type == "number" then 0 else . end)' "$json_file" > "${json_file%.json}_zeroed.json"
mv "${json_file%.json}_zeroed.json" "$json_file"

jq 'walk(if type == "string" then "0" else . end)' "$json_file" > "${json_file%.json}_zeroed.json"
mv "${json_file%.json}_zeroed.json" "$json_file"
