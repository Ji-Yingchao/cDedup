#!/bin/bash

# CDC
rm -fr FileRecipes
rm -fr Containers
rm fingerprints.meta

# FULL FILE
rm -fr FULL_FILE_STORAGE
rm FFFP.meta

# MERKLE TREE
rm /home/cyf/cDedup/L1.meta
rm /home/cyf/cDedup/L2.meta
rm /home/cyf/cDedup/L3.meta
rm /home/cyf/cDedup/L4.meta
rm /home/cyf/cDedup/L5.meta
rm /home/cyf/cDedup/L6.meta

mkdir FileRecipes
mkdir Containers
touch fingerprints.meta
mkdir FULL_FILE_STORAGE
touch FFFP.meta
touch /home/cyf/cDedup/L1.meta
touch /home/cyf/cDedup/L2.meta
touch /home/cyf/cDedup/L3.meta
touch /home/cyf/cDedup/L4.meta
touch /home/cyf/cDedup/L5.meta
touch /home/cyf/cDedup/L6.meta
