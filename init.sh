#!/bin/bash
# 这里的配置需要和参数json一致
rm -fr /home/cyf/hikvision/hdd1/working/

mkdir /home/cyf/hikvision/hdd1/working/
mkdir /home/cyf/hikvision/hdd1/working/restoreFolder
mkdir /home/cyf/hikvision/hdd1/working/Containers
mkdir /home/cyf/hikvision/hdd1/working/FULL_FILE_STORAGE
mkdir /home/cyf/hikvision/hdd1/working/metadata
mkdir /home/cyf/hikvision/hdd1/working/metadata/FileRecipes
mkdir /home/cyf/hikvision/hdd1/working/metadata/fingerprintsDeltaDedup

touch /home/cyf/hikvision/hdd1/working/metadata/fingerprints.meta
touch /home/cyf/hikvision/hdd1/working/metadata/FFFP.meta
touch /home/cyf/hikvision/hdd1/working/metadata/L1.meta
touch /home/cyf/hikvision/hdd1/working/metadata/L2.meta
touch /home/cyf/hikvision/hdd1/working/metadata/L3.meta
touch /home/cyf/hikvision/hdd1/working/metadata/L4.meta
touch /home/cyf/hikvision/hdd1/working/metadata/L5.meta
touch /home/cyf/hikvision/hdd1/working/metadata/L6.meta

source ./scripts/clear_global_stat.sh

