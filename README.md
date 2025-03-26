# cDedup重复数据删除原型系统  
支持FSC，CDC，全文件重删；  
写入加速支持merkle tree；  
恢复缓存支持contianer-based，chunk-based，FAA-fixed；

## 测试写吞吐量
./cDedup conf/writeExample.json | grep "Dedup Ratio" | awk '{print $3}'  
stdbuf -oL ./cDedup conf/writeExample.json | grep -oP 'Actual DR \K[\d.]+'  
stdbuf -oL ./cDedup conf/writeExample.json | grep -oP 'Dedup Ratio \K[\d.]+'  
./batchrun.sh > results.txt

## 测试恢复吞吐量
./cDedup conf/readExample.json
./batchread.sh > results.txt

## 数据集
folder_path="/home/cyf/ssd0/SFD_TAR/"
folder_path="/home/jyc/ssd/dataset/CHM/"

folder_path="/home/jyc/ssd/dataset/LLVM/"
files=$(ls $folder_path | sort -V)

folder_path="/home/jyc/ssd/dataset/GCC/"
folder_path="/home/jyc/ssd/dataset/MySQL/"
folder_path="/home/jyc/ssd/dataset/linuxVersion/"

## 改变文件所有者
sudo chown $USER:$USER conf/readExample.json  
sudo chown $USER:$USER conf/writeExample.json  

# sort 对文件中的数字从小到大进行排序
sort -n input.txt -o output.txt

## 清除缓存
sudo echo 3 > /proc/sys/vm/drop_caches
