#!/bin/bash


# 恢复，一次测
folder_path="/home/jyc/ssd/restore/restore"
for i in {0..90};
do
    full_path=$folder_path$i
    jq --arg full_path "$full_path" '.RestorePath = $full_path' ../conf/readExample.json > ../conf/temp.json
    mv ../conf/temp.json ../conf/readExample.json
    jq ".RestoreVersion = ${i}" ../conf/readExample.json > ../conf/temp.json 
    mv ../conf/temp.json ../conf/readExample.json
    ../cDedup ../conf/readExample.json | grep "Restore Throughput" | awk '{print $3}'
done


# 恢复，备份一个恢复一个
folder_path="/home/jyc/ssd/dataset/linuxVersion/"
restore_path="/home/jyc/ssd/restore/restore"
files=$(ls $folder_path)
i=0
for new_file_path in $files
do
    # 写一个备份版本
    full_path=$folder_path$new_file_path
    jq --arg full_path "$full_path" '.InputFile = $full_path' ../conf/writeExample.json > ../conf/temp.json
    mv ../conf/temp.json ../conf/writeExample.json
    ../cDedup ../conf/writeExample.json > /dev/null 2>&1
    # 恢复一个备份版本
    full_path=$restore_path$i
    jq --arg full_path "$full_path" '.RestorePath = $full_path' ../conf/readExample.json > ../conf/temp.json
    mv ../conf/temp.json ../conf/readExample.json
    jq ".RestoreVersion = ${i}" ../conf/readExample.json > ../conf/temp.json 
    mv ../conf/temp.json ../conf/readExample.json
    ../cDedup ../conf/readExample.json | grep "Speed factor" | awk '{print $3}'
    ((i++))
done