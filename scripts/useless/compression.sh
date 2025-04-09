# 压缩正常的容器，结尾没有r的；
Container_path="/home/cyf/cDedup/TEST/metadata/Containers/"
mkdir /home/cyf/cDedup/TEST/metadata/ContainersGZIP/
files=$(ls $Container_path | grep -v '.r$')
for file in $files
do
    echo "gzip $full_path"
    full_path=$Container_path$file
    gzip -c $full_path > /home/cyf/cDedup/TEST/metadata/ContainersGZIP/$file.gz
done