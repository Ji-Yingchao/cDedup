#!/bin/bash
# 这里的配置需要和参数json一致

BACK_DIR=/home/jyc/hdd/deltaworking/
rm -rf ${BACK_DIR}/Containers
rm -rf ${BACK_DIR}/metadata
rm -rf ${BACK_DIR}/HotContainers

# mkdir ${BACK_DIR}
mkdir ${BACK_DIR}/Containers
mkdir ${BACK_DIR}/metadata
mkdir ${BACK_DIR}/metadata/FileRecipes
mkdir ${BACK_DIR}/metadata/fingerprintsDeltaDedup

mkdir ${BACK_DIR}/metadata/containerIndex
mkdir ${BACK_DIR}/HotContainers

# mkdir /home/jyc/hdd/working/restoreFolder
# touch /home/jyc/hdd/working/metadata/fingerprints.meta

source ./scripts/clear_global_stat.sh
RESTORE_DIR=/home/jyc/ssd/restore/
rm -f ${RESTORE_DIR}/*
