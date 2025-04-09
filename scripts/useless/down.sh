#!/bin/bash

# Destination directory where you want to save the source code
destination_dir="/home/cyf/dataHDD/linuxVersion"

# URL of the Linux kernel archive page
kernel_archive_url="https://mirrors.edge.kernel.org/pub/linux/kernel/v5.x/"

# Use curl to fetch the HTML content of the archive page
archive_page=$(curl -s "$kernel_archive_url")


# Use grep and awk to extract links to Linux kernel 5.x versions
versions=($(echo "$archive_page" | grep -oP 'linux-5.\d+\.\d+' | awk '!seen[$0]++'))
# 太多了，200个就够了
versions=("${versions[@]:0:200}")

# Download function
download_version() {
    local version="$1"
    local url="https://www.kernel.org/pub/linux/kernel/v5.x/$version.tar.gz"
    wget "$url" -P "$destination_dir"
}

# Number of parallel downloads to run
max_parallel_downloads=16

# Counter for parallel downloads
current_parallel_downloads=0

# Loop through the versions and download in parallel
for version in "${versions[@]}"; do
    if [ "$current_parallel_downloads" -ge "$max_parallel_downloads" ]; then
        wait
        current_parallel_downloads=0
    fi

    download_version "$version" &
    ((current_parallel_downloads++))
done

# Wait for any remaining background downloads to finish
wait

echo "All downloads are complete."
