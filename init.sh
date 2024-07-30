#!/bin/bash
# 这里的配置需要和参数json一致


rm -rf /home/jyc/hdd/linux/working/
mkdir /home/jyc/hdd/linux/working/
# mkdir /home/jyc/hdd/working/restoreFolder
mkdir /home/jyc/hdd/linux/working/Containers
# mkdir /home/jyc/hdd/working/FULL_FILE_STORAGE
mkdir /home/jyc/hdd/linux/working/metadata
mkdir /home/jyc/hdd/linux/working/metadata/FileRecipes
mkdir /home/jyc/hdd/linux/working/metadata/fingerprintsDeltaDedup

# touch /home/jyc/hdd/working/metadata/fingerprints.meta
# touch /home/jyc/hdd/working/metadata/FFFP.meta
# touch /home/jyc/hdd/working/metadata/L1.meta
# touch /home/jyc/hdd/working/metadata/L2.meta
# touch /home/jyc/hdd/working/metadata/L3.meta
# touch /home/jyc/hdd/working/metadata/L4.meta
# touch /home/jyc/hdd/working/metadata/L5.meta
# touch /home/jyc/hdd/working/metadata/L6.meta

source ./scripts/clear_global_stat.sh

