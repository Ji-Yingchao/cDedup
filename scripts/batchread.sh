#!/bin/bash


# 恢复，一次测
folder_path="/home/jyc/ssd/restore/restore"
for i in {0..99};
do
    full_path=$folder_path$i
    jq --arg full_path "$full_path" '.RestorePath = $full_path' ../conf/readExample.json > ../conf/temp.json
    mv ../conf/temp.json ../conf/readExample.json
    jq ".RestoreVersion = ${i}" ../conf/readExample.json > ../conf/temp.json 
    mv ../conf/temp.json ../conf/readExample.json
    # ../cDedup ../conf/readExample.json | grep "Restore Throughput" | awk '{print $3}'
    # ../cDedup ../conf/readExample.json | grep "Reference Container Count" | awk '{print $4}'
    # ../cDedup ../conf/readExample.json | grep "Base Container Max Value" | awk '{print $5}'
    ../cDedup ../conf/readExample.json | grep -E "Read Container Count|Read Base Container Count|Read Delta Container Count| 
    Reference Container Count|Reference Base Container Count|Reference Delta Container Count" | awk '
    /Read Container Count/ {read_counter = $4}
    /Read Base Container Count/ {read_base = $5}
    /Read Delta Container Count/ {read_delta = $5}
    /Reference Container Count/ {ref_counter = $4}
    /Reference Base Container Count/ {ref_base = $5}
    /Reference Delta Container Count/ {ref_delta = $5}
    END {print read_counter "\t" read_base "\t" read_delta "\t" ref_counter "\t" ref_base "\t" ref_delta}'
done


# 恢复，备份一个恢复一个
# folder_path="/home/jyc/ssd/dataset/linuxVersion/"
# restore_path="/home/jyc/ssd/restore/restore"
# files=$(ls $folder_path)
# i=0
# for new_file_path in $files
# do
#     # 写一个备份版本
#     full_path=$folder_path$new_file_path
#     jq --arg full_path "$full_path" '.InputFile = $full_path' ../conf/writeExample.json > ../conf/temp.json
#     mv ../conf/temp.json ../conf/writeExample.json
#     ../cDedup ../conf/writeExample.json > /dev/null 2>&1
#     # 恢复一个备份版本
#     full_path=$restore_path$i
#     jq --arg full_path "$full_path" '.RestorePath = $full_path' ../conf/readExample.json > ../conf/temp.json
#     mv ../conf/temp.json ../conf/readExample.json
#     jq ".RestoreVersion = ${i}" ../conf/readExample.json > ../conf/temp.json 
#     mv ../conf/temp.json ../conf/readExample.json
#     ../cDedup ../conf/readExample.json | grep "Speed factor" | awk '{print $3}'
#     ((i++))
# done