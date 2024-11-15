# cDedup重复数据删除原型系统  
支持FSC，CDC，全文件重删；  
写入加速支持merkle tree；  
恢复缓存支持contianer-based，chunk-based，FAA-fixed；

## 测试写吞吐量
./cDedup conf/writeExample.json | grep "Dedup Ratio" | awk '{print $3}'  
stdbuf -oL ./cDedup conf/writeExample.json | grep -oP 'Actual DR \K[\d.]+'  
stdbuf -oL ./cDedup conf/writeExample.json | grep -oP 'Dedup Ratio \K[\d.]+'  

## 测试恢复吞吐量
./cDedup conf/readExample.json

## 改变文件所有者
sudo chown $USER:$USER conf/readExample.json  
sudo chown $USER:$USER conf/writeExample.json  

## 清除缓存
sudo echo 3 > /proc/sys/vm/drop_caches
