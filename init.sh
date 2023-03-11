#!/bin/bash

# CDC
rm -fr FileRecipes
rm -fr Containers
rm fingerprints.meta

# FULL FILE
rm -fr FULL_FILE_STORAGE
rm FFFP.meta

mkdir FileRecipes
mkdir Containers
touch fingerprints.meta
mkdir FULL_FILE_STORAGE
touch FFFP.meta
