// Copyright (c) 2018 Baidu.com, Inc. All Rights Reserved
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gflags/gflags.h>
#include <bthread/bthread.h>
#include <brpc/channel.h>
#include <brpc/controller.h>
#include <braft/raft.h>
#include <braft/util.h>
#include <braft/route_table.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <numeric>
#include <ostream>
#include <stdio.h>
#include <vector>

#include "bld/block.pb.h"

DEFINE_bool(log_each_request, false, "Print log for each request");
DEFINE_bool(latency_show, false, "Print latency");
DEFINE_bool(use_bthread, false, "Use bthread to send requests");
DEFINE_int32(block_size, 64 * 1024 * 1024u, "Size of block");
DEFINE_int32(request_size, 1024, "Size of each requst");
DEFINE_int32(thread_num, 1, "Number of threads sending requests");
DEFINE_int32(timeout_ms, 500, "Timeout for each request");
DEFINE_int32(send_time, 30, "request time test");
DEFINE_int32(write_percentage, 100, "Percentage of fetch_add");
DEFINE_int32(tm, 10, "show qps msg");
DEFINE_string(conf, "", "Configuration of the raft group");
DEFINE_string(group, "Block", "Id of the replication group");
DEFINE_bool(only_res, false, "only show the result");


bvar::LatencyRecorder g_latency_recorder("block_client");

std::atomic_bool interrupt{};

#define ULL unsigned long long

std::vector<double> iops_records;
std::vector<ULL> count_records;
std::vector<ULL> lat_records;
std::vector<ULL> qps_records;

std::atomic<int> send_msg;

brpc::Channel channel;
// braft::PeerId leader;
braft::PeerId leader("127.0.1.1:8200");

auto now()->decltype(std::chrono::high_resolution_clock::now()){
	return std::chrono::high_resolution_clock::now();
}

void writer(){

    while (!brpc::IsAskedToQuit()&&!interrupt.load(std::memory_order_relaxed)) {    // ??????????????????

        example::BlockService_Stub stub(&channel);  //??????protobuf????????????,????????????
        brpc::Controller cntl;
        cntl.set_timeout_ms(FLAGS_timeout_ms);      //????????????????????????
        // Randomly select which request we want send;
        example::BlockRequest request;
        example::BlockResponse response;
        request.set_offset(butil::fast_rand_less_than(
                            FLAGS_block_size - FLAGS_request_size));
        const char* op = NULL;
        if (butil::fast_rand_less_than(100) < (size_t)FLAGS_write_percentage) {
            op = "write";
            cntl.request_attachment().resize(FLAGS_request_size, 'a');
            stub.write(&cntl, &request, &response, NULL);
        } else {
            op = "read";
            request.set_size(FLAGS_request_size);
            stub.read(&cntl, &request, &response, NULL);
        }

        if (cntl.Failed()) {
            continue;
        }
        if (response.success()){
            send_msg.fetch_add(1,std::memory_order_relaxed);    //???????????????????????????
        }else{
            continue;
        }       

        g_latency_recorder << cntl.latency_us();
        if (FLAGS_log_each_request) {
            LOG(INFO) << "Received response from " << leader
                      << " op=" << op
                      << " offset=" << request.offset()
                      << " request_attachment="
                      << cntl.request_attachment().size()
                      << " response_attachment="
                      << cntl.response_attachment().size()
                      << " latency=" << cntl.latency_us();
            bthread_usleep(1000L * 1000L);
        }
    }
}

void output(){
    printf("%10s\t%10s\t%10s\t%10s\t%10s\t%10s\n","thread_num","req_size","count","send(MB)","qps","lat(usec)");
    fflush(stdout);
    auto outTime = now();   //????????????
    bool flag = false;      //?????? 40 ???????????????????????????
	while(!interrupt.load(std::memory_order_relaxed)){
		sleep(FLAGS_tm);
        if(!flag&&(std::chrono::duration_cast<std::chrono::seconds>(now() - outTime).count() > 40)){    // ????????????????????????40??????????????????????????????????????????       
            flag=true;
        }
        auto counter = send_msg.load(std::memory_order_relaxed);    //?????? FlAGS_tm ???????????????????????????
        send_msg.store(0);      // ???????????????
        unsigned long long send_all = counter * FLAGS_request_size;     //?????? ?????? ???????????????size ???????????????

        auto count = counter/FLAGS_thread_num;  //???????????? ?????? ????????? ??????????????????????????????

        if(!FLAGS_only_res){        //??????????????????????????????shell??????????????????
            printf("%10d\t%10d\t%10d\t%10.3lf\t%10ld\t%10ld\n",FLAGS_thread_num,FLAGS_request_size,count,double(send_all)/1024.0/1024.0/double(FLAGS_tm),g_latency_recorder.qps(1),g_latency_recorder.latency(1));
            fflush(stdout);
        }

        if(flag){       //??????????????????????????????????????????????????????
            iops_records.push_back(double(send_all)/1024.0/1024.0);
            count_records.push_back(count);
            lat_records.push_back(g_latency_recorder.latency(1));
            qps_records.push_back(g_latency_recorder.qps(1));
        }
	}
}

void running_channel(){

    std::vector<std::thread> tids;
    tids.resize(FLAGS_thread_num);
    std::vector<unsigned long long> request_sizes(FLAGS_thread_num);
    auto start = now();
    
    for (int i = 0; i < FLAGS_thread_num; ++i) {
        // tids[i] = std::thread(sender,std::ref(request_sizes[i]));
        tids[i] = std::thread(writer);
    }

    std::thread th1(output);
	using namespace std::chrono;
	while(duration_cast<seconds>(now() - start).count() < FLAGS_send_time){
		sleep(1);
	}
    interrupt.store(true,std::memory_order_relaxed);

    while (FLAGS_latency_show&&!brpc::IsAskedToQuit()&&!interrupt.load(std::memory_order_relaxed)) {
        sleep(1);
        LOG_IF(INFO, !FLAGS_log_each_request)
                << "Sending request size"<< FLAGS_request_size
                << "Sending Request to " << FLAGS_group
                << " (" << FLAGS_conf << ')'
                << " at qps=" << g_latency_recorder.qps(1)
                << " latency=" << g_latency_recorder.latency(1);
    }

    LOG(INFO) << "Block client is going to quit";
    for (int i = 0; i < FLAGS_thread_num; ++i) {        
        tids[i].join();
    }

    auto avg_iops = double(std::accumulate(iops_records.begin(),iops_records.end(),0.0))/double(iops_records.size())/double(FLAGS_tm);
    auto avg_count = std::accumulate(count_records.begin(),count_records.end(),0)/count_records.size();
    auto avg_lat = std::accumulate(lat_records.begin(),lat_records.end(),0)/lat_records.size();
    auto avg_qps = std::accumulate(qps_records.begin(),qps_records.end(),0)/qps_records.size();

    printf("RES%7d\t%10d\t%10lu\t%10.3lf\t%10ld\t%10ld\n",FLAGS_thread_num,FLAGS_request_size,avg_count,avg_iops,avg_qps,avg_lat);  //??????????????????
    fflush(stdout);
    th1.join();
}

int main(int argc, char* argv[]) {
    GFLAGS_NAMESPACE::ParseCommandLineFlags(&argc, &argv, true);
    butil::AtExitManager exit_manager;
    LOG(INFO) << "request send thread nums " << FLAGS_thread_num
            << " request size " << FLAGS_request_size;

    if (channel.Init(leader.addr, NULL) != 0) {
        LOG(ERROR) << "Fail to init channel to " << leader;
        bthread_usleep(FLAGS_timeout_ms * 1000L);
    }
    
    running_channel();
    return 0;
}
