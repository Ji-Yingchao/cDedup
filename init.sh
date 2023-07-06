#!/bin/bash
# 这里的配置需要和参数json一致

# CDC
rm -fr TEST/metadata/FileRecipes
rm -fr TEST/metadata/Containers
rm -f TEST/metadata/fingerprints.meta

# FULL FILE
rm -fr TEST/metadata/FULL_FILE_STORAGE
rm -f TEST/metadata/FFFP.meta

# MERKLE TREE
rm -f TEST/metadata/L1.meta
rm -f TEST/metadata/L2.meta
rm -f TEST/metadata/L3.meta
rm -f TEST/metadata/L4.meta
rm -f TEST/metadata/L5.meta
rm -f TEST/metadata/L6.meta



mkdir TEST/metadata/FileRecipes
mkdir TEST/metadata/Containers
touch TEST/metadata/fingerprints.meta
mkdir TEST/metadata/FULL_FILE_STORAGE
touch TEST/metadata/FFFP.meta
touch TEST/metadata/L1.meta
touch TEST/metadata/L2.meta
touch TEST/metadata/L3.meta
touch TEST/metadata/L4.meta
touch TEST/metadata/L5.meta
touch TEST/metadata/L6.meta
