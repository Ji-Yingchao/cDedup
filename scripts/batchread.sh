folder_path="/home/cyf/hikvision/ssd2/restoreFolder-jyc/restore"
for i in {0..90};
do
    full_path=$folder_path$i
    jq --arg full_path "$full_path" '.RestorePath = $full_path' ../conf/readExample.json > ../conf/temp.json
    mv ../conf/temp.json ../conf/readExample.json
    jq ".RestoreVersion = ${i}" ../conf/readExample.json > ../conf/temp.json 
    mv ../conf/temp.json ../conf/readExample.json
    ../cDedup ../conf/readExample.json | grep "Restore Throughput" | awk '{print $3}'
done