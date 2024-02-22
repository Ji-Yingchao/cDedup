#!/bin/bash
# 这里的配置需要和参数json一致
rm -fr /home/cyf/hikvision/ssd1/working/

mkdir /home/cyf/hikvision/ssd1/working/
mkdir /home/cyf/hikvision/ssd1/working/metadata
mkdir /home/cyf/hikvision/ssd1/working/metadata/FileRecipesFolder
touch /home/cyf/hikvision/ssd1/working/metadata/FingerprintsIndex
touch /home/cyf/hikvision/ssd1/working/ContainerPool

source ./scripts/clear_global_status.sh

