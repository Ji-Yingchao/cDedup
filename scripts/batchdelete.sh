for i in {0..19};
do
    jq ".RestoreVersion = ${i}" ../conf/deleteExample.json > ../conf/temp.json 
    mv ../conf/temp.json ../conf/deleteExample.json
    ../cDedup ../conf/deleteExample.json | grep "Delete time" | awk '{print $3}'
done