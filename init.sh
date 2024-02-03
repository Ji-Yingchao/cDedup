#!/bin/bash
# 这里的配置需要和参数json一致
rm -fr /home/cyf/dataSSD/working/

mkdir /home/cyf/dataSSD/working/
mkdir /home/cyf/dataSSD/working/restoreFolder
mkdir /home/cyf/dataSSD/working/Containers
mkdir /home/cyf/dataSSD/working/FULL_FILE_STORAGE
mkdir /home/cyf/dataSSD/working/metadata
mkdir /home/cyf/dataSSD/working/metadata/FileRecipes

touch /home/cyf/dataSSD/working/metadata/fingerprints.meta
touch /home/cyf/dataSSD/working/metadata/FFFP.meta
touch /home/cyf/dataSSD/working/metadata/L1.meta
touch /home/cyf/dataSSD/working/metadata/L2.meta
touch /home/cyf/dataSSD/working/metadata/L3.meta
touch /home/cyf/dataSSD/working/metadata/L4.meta
touch /home/cyf/dataSSD/working/metadata/L5.meta
touch /home/cyf/dataSSD/working/metadata/L6.meta

source ./scripts/clear_global_stat.sh

