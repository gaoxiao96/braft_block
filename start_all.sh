#!/bin/bash

### 为测试不同盘服务，prefix_name 可设置为nvm,data1,data2,当值为空，测试根目录
prefix='/'
prefix_name=''

## 是否为单测
signle_test=true

if [ "$signle_test" = false ]; then

    ## 盘符循环，''为根目录
    driveLetter=('')
    ## 循环 设置braft 测试参数 Flush buffer to LogStorage if the buffer size reaches the limit
    ## LogMannager.cpp 文件第483行
    appendBuffer=(512 1024 2048 4096 8192)

    requestSize=(512 1024 2048 4096 8192)
    #线程数
    threadNum=(1 32 64 128 256 512 1024 2048)

    ## 盘符循环
    for prefix_name in ${driveLetter[@]}
    do
        ## 设置braft fsync保存目录
        data_path=${prefix}${prefix_name}'/gx_data'
        ## 设置braft 测试结果保存名称
        storage_path="./braft_${prefix_name}.txt"
        ## 循环 设置braft 测试参数 Flush buffer to LogStorage if the buffer size reaches the limit
        ## LogMannager.cpp 文件第483行
        for append_buffer_size in ${appendBuffer[@]}
        do
            ## 循环 设置brpc 发送request size
            for req in ${requestSize[@]}
            do
                ## 循环 设置brpc 发送线程数量
                for th in ${threadNum[@]}
                do
                    ## 使得 append_buffer_size 单位变为（Kb）
                    x=$(($append_buffer_size*1024))
                    echo $x
                    echo 'raft_max_append_buffer_size   '${x} >> ${storage_path}
                    ## 运行server服务端
                    bash ./run_server.sh --data_path=${data_path}_${th} --raft_max_append_buffer_size=${x}
                    sleep 5
                    ## 运行client客户端
                    bash ./run_client.sh --thread_num=${th} --send_time=300 --request_size=${req} --tm=5 --only_res=true >> ${storage_path}
                    ## 运行 stop.sh 脚本
                    bash ./stop_all.sh
                    ## 运行结束，删除运行时产生的临时文本数据
                    rm -rf ${data_path}_${th}
                    sleep 10
                done
            done
        done
    done

else

    data_path=${prefix}${prefix_name}'/gx_data'
    storage_path="./braft1_${prefix_name}.txt"
    # x=$((512*1024))
    x=2097152
    th=1
    req=512000
    bash ./run_server.sh --data_path=${data_path}_${th} --raft_max_append_buffer_size=${x}
    sleep 5
    bash ./run_client.sh --thread_num=${th} --send_time=100 --request_size=${req} --tm=10 --only_res=false  >> ${storage_path}
    bash ./stop_all.sh
    rm -rf ${data_path}_${th}

fi