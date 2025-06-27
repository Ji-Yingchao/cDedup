# ============== Backup ===================
# 多值筛选输出
# ../cDedup ../conf/writeExample.json | grep -E "metadata table load|New added item|total item" | awk '
    # /metadata table load/ {load_item = $4}
    # /New added item/ {new_added = $4}
    # /total item/ {total_item = $3}
    # END {print load_item "\t" new_added "\t" total_item}'

    # ./cDedup ./conf/writeExample.json | grep -E "Actual DR|throughput" | awk '
    # /Actual DR/ {dr = $3}
    # /throughput/ {speed = $2}
    # END {print dr "\t" speed}' > results.txt

# ../cDedup ../conf/writeExample.json | grep "throughput(MB/s)" | awk '{print $2}'
# ../cDedup ../conf/writeExample.json | grep "Dedup Ratio" | awk '{print $3}'
# ../cDedup ../conf/writeExample.json | grep "Actual DR" | awk '{print $3}' >> results.txt


# ============== Restore ===================
# 恢复，一次测
# folder_path="/home/jyc/ssd/restore/restore"
# for i in {0..4};
# do
#     full_path=$folder_path$i
#     jq --arg full_path "$full_path" '.RestorePath = $full_path' ../conf/readExample.json > ../conf/temp.json
#     mv ../conf/temp.json ../conf/readExample.json
#     jq ".RestoreVersion = ${i}" ../conf/readExample.json > ../conf/temp.json 
#     mv ../conf/temp.json ../conf/readExample.json
#     # ../cDedup ../conf/readExample.json | grep "Restore Throughput" | awk '{print $3}'
#     ../cDedup ../conf/readExample.json | grep "Read Container Count" | awk '{print $4}'
#     # ../cDedup ../conf/readExample.json | grep "Base Container Max Value" | awk '{print $5}'
# done

# 恢复，数读容器和引用容器次数(base和delta)
# ../cDedup ../conf/readExample.json | grep -E "Read Container Count|Read Base Container Count|Read Delta Container Count| 
#     Reference Container Count|Reference Base Container Count|Reference Delta Container Count" | awk '
#     /Read Container Count/ {read_counter = $4}
#     /Read Base Container Count/ {read_base = $5}
#     /Read Delta Container Count/ {read_delta = $5}
#     /Reference Container Count/ {ref_counter = $4}
#     /Reference Base Container Count/ {ref_base = $5}
#     /Reference Delta Container Count/ {ref_delta = $5}
#     END {print read_counter "\t" read_base "\t" read_delta "\t" ref_counter "\t" ref_base "\t" ref_delta}'

# ../cDedup ../conf/readExample.json | grep -E "Read Container Count|Reference Container Count|Restore Throughput" | awk '
#     /Read Container Count/ {read_counter = $4}
#     /Reference Container Count/ {ref_counter = $4}
#     /Restore Throughput/ {speed = $3}
#     END {print read_counter "\t" ref_counter "\t" speed}' >> results_read.txt
