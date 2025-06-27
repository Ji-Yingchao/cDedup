#!/bin/bash

# 恢复，备份一个恢复一个
folder_path="/home/jyc/ssd/dataset/LLVM/"
restore_path="/home/jyc/ssd/restore/restore"
files=$(ls $folder_path | sort -V)
i=0
for new_file_path in $files
do
    # 写一个备份版本
    full_path=$folder_path$new_file_path
    jq --arg full_path "$full_path" '.InputFile = $full_path' ../conf/writeExample.json > ../conf/temp.json
    mv ../conf/temp.json ../conf/writeExample.json
    ../cDedup ../conf/writeExample.json > /dev/null 2>&1 
    # ../cDedup ../conf/writeExample.json | grep "Dedup Ratio" | awk '{print $3}' >> results.txt
    # ../cDedup ../conf/writeExample.json | awk '
    # /backup throughput/ {throughput = $NF}
    # /Actual DR/ {ratio = $NF}
    # /Arrange Time/ {time = $NF}
    # END {
    #     print ratio"\t" throughput "\t" time >> "results.txt"
    # }'
    # 恢复一个备份版本
    sync && echo 3 > /proc/sys/vm/drop_caches
    full_path=$restore_path$i
    jq --arg full_path "$full_path" '.RestorePath = $full_path' ../conf/readExample.json > ../conf/temp.json
    mv ../conf/temp.json ../conf/readExample.json
    jq ".RestoreVersion = ${i}" ../conf/readExample.json > ../conf/temp.json 
    mv ../conf/temp.json ../conf/readExample.json
    ../cDedup ../conf/readExample.json | grep -E "Read Container Count|Restore Throughput|Read Amplification" | awk '
    /Read Container Count/ {read_counter = $4}
    /Restore Throughput/ {speed = $3}
    /Read Amplification/ {amplification = $3}
    END {print speed "\t" read_counter "\t" amplification}' >> results.txt
    ((i++))
done
