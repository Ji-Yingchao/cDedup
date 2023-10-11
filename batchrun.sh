folder_path="/home/cyf/dataHDD/ubuntuImage/"
files=$(ls $folder_path)
for new_file_path in $files
do
    full_path=$folder_path$new_file_path
    jq --arg full_path "$full_path" '.InputFile = $full_path' ./conf/writeExample.json > ./conf/temp.json
    mv ./conf/temp.json ./conf/writeExample.json
    ./cDedup ./conf/writeExample.json
done

