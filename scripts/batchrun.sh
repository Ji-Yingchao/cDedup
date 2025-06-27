folder_path="/home/jyc/ssd/dataset/LLVM/"
files=$(ls $folder_path | sort -V)
count=0

for new_file_path in $files
do
    full_path=$folder_path$new_file_path
    jq --arg full_path "$full_path" '.InputFile = $full_path' ../conf/writeExample.json > ../conf/temp.json
    mv ../conf/temp.json ../conf/writeExample.json
    ../cDedup ../conf/writeExample.json | grep "Throughput" | awk '{print $2}' >> results.txt
    sleep 5

    # ((count++))       
    # if [ "$count" -eq 90 ]; then  
    #     echo "到第十次了，跳出循环"
    #     break
    # fi

done