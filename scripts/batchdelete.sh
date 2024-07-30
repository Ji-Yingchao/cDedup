for i in {30..39};
do
    jq ".DeleteVersion = ${i}" ../conf/deleteExample.json > ../conf/temp.json 
    mv ../conf/temp.json ../conf/deleteExample.json
    ../cDedup ../conf/deleteExample.json | grep "Delete time" | awk '{print $3}'
done