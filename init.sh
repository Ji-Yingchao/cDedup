#!/bin/bash
# 这里的配置需要和参数json一致
rm -fr ./working/

mkdir ./working/
mkdir ./working/Containers
mkdir ./working/FULL_FILE_STORAGE
mkdir ./working/metadata
mkdir ./working/metadata/FileRecipes

touch ./working/metadata/fingerprints.meta
touch ./working/metadata/FFFP.meta
touch ./working/metadata/L1.meta
touch ./working/metadata/L2.meta
touch ./working/metadata/L3.meta
touch ./working/metadata/L4.meta
touch ./working/metadata/L5.meta
touch ./working/metadata/L6.meta

source ./scripts/clear_global_stat.sh

