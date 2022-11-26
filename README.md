# braft_block

**测试braft在本地性能**

## 介绍

1. 在 start_all.sh 中设定

    - 盘符
    - 线程数目
    - 客户端每次发送的request size
    - braft中LogManage中的appendBuffer参数设定
 
 2. 在 stop_all.sh 中为单次测试清理运行过程中生成的log文件
 

## 运行

```
   //创建bld目录
   mkdir bld
   
   cmake ..
   
   make
   
   cd ..
   
   //运行测试脚本
   bash start_all.sh
```
