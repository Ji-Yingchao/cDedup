folder_path="/home/jyc/ssd/dataset/LLVM/"
files=$(ls $folder_path | sort -V)
for new_file_path in $files
do
    full_path=$folder_path$new_file_path
    jq --arg full_path "$full_path" '.InputFile = $full_path' ../conf/writeExample.json > ../conf/temp.json
    mv ../conf/temp.json ../conf/writeExample.json
    # ../cDedup ../conf/writeExample.json | grep "throughput(MB/s)" | awk '{print $2}'
    ../cDedup ../conf/writeExample.json | grep "Dedup Ratio" | awk '{print $3}'
done

# 多值筛选输出
# ../cDedup ../conf/writeExample.json | grep -E "metadata table load|New added item|total item" | awk '
    # /metadata table load/ {load_item = $4}
    # /New added item/ {new_added = $4}
    # /total item/ {total_item = $3}
    # END {print load_item "\t" new_added "\t" total_item}'