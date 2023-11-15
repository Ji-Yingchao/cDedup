#!/bin/bash
# 这里的配置需要和参数json一致

rm -fr TEST/metadata

mkdir TEST/metadata
mkdir TEST/metadata/FileRecipes
mkdir TEST/metadata/Containers
mkdir TEST/metadata/FULL_FILE_STORAGE

touch TEST/metadata/fingerprints.meta
touch TEST/metadata/FFFP.meta
touch TEST/metadata/L1.meta
touch TEST/metadata/L2.meta
touch TEST/metadata/L3.meta
touch TEST/metadata/L4.meta
touch TEST/metadata/L5.meta
touch TEST/metadata/L6.meta

source ./scripts/cleaar_global_stat.sh

