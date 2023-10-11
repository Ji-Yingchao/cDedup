origin_path=/home/cyf/dataHDD/ubuntuImage
gzip_container_path=/home/cyf/TEST/metadata/ContainersGZIP

origin_size=$(du -sb $origin_path | awk '{print $1}')
echo "Origin files size before deduplication: $origin_size bytes"

after_deduplication_size=$(ls -l ../TEST/metadata/Containers | grep -v '.r$' | awk '{total+=$5} END {print total}')
echo "Total container size after deduplication: $after_deduplication_size bytes"

after_compression_size=$(ls -l ../TEST/metadata/ContainersGZIP | awk '{total+=$5} END {print total}')
echo "Total container size after deduplication and compression: $after_compression_size bytes"

DR=$(echo "scale=2; a=($origin_size-$after_deduplication_size)/$origin_size; if (a < 1) print 0; a" | bc)
CR=$(echo "scale=2; b=($after_deduplication_size-$after_compression_size)/$after_deduplication_size; if (b < 1) print 0; b" | bc)
TOTAL_DRR=$(echo "scale=2; c=($origin_size-$after_compression_size)/$origin_size; if (c < 1) print 0; c" | bc)

echo "Deduplication Ratio $DR"
echo "Compression Ratio $CR"
echo "Total Data Reduction Ratio $TOTAL_DRR"