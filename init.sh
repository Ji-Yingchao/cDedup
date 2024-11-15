#!/bin/bash
# 这里的配置需要和参数json一致

BACK_DIR=/home/jyc/hdd/linux/working/
rm -rf ${BACK_DIR}
mkdir ${BACK_DIR}
mkdir ${BACK_DIR}/Containers
mkdir ${BACK_DIR}/metadata
mkdir ${BACK_DIR}/metadata/FileRecipes
mkdir ${BACK_DIR}/metadata/fingerprintsDeltaDedup

# mkdir /home/jyc/hdd/working/FULL_FILE_STORAGE
# mkdir /home/jyc/hdd/working/restoreFolder

# touch /home/jyc/hdd/working/metadata/fingerprints.meta
# touch /home/jyc/hdd/working/metadata/FFFP.meta
# touch /home/jyc/hdd/working/metadata/L1.meta
# touch /home/jyc/hdd/working/metadata/L2.meta
# touch /home/jyc/hdd/working/metadata/L3.meta
# touch /home/jyc/hdd/working/metadata/L4.meta
# touch /home/jyc/hdd/working/metadata/L5.meta
# touch /home/jyc/hdd/working/metadata/L6.meta

source ./scripts/clear_global_stat.sh
RESTORE_DIR=/home/jyc/ssd/restore/
rm -f ${RESTORE_DIR}/*
