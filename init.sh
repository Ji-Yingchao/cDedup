#!/bin/bash
# 这里的配置需要和参数json一致

rm -fr ./metadata

mkdir ./metadata
mkdir ./metadata/FileRecipes
mkdir ./metadata/Containers
mkdir ./metadata/FULL_FILE_STORAGE

touch ./metadata/fingerprints.meta
touch ./metadata/FFFP.meta
touch ./metadata/L1.meta
touch ./metadata/L2.meta
touch ./metadata/L3.meta
touch ./metadata/L4.meta
touch ./metadata/L5.meta
touch ./metadata/L6.meta

source ./scripts/cleaar_global_stat.sh

