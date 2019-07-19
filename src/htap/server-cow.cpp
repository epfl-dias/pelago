// /*
//                   ..

//                               Copyright (c) 2019-2019
//            Data Intensive Applications and Systems Laboratory (DIAS)
//                    Ecole Polytechnique Federale de Lausanne

//                               All Rights Reserved.

//       Permission to use, copy, modify and distribute this software and its
//     documentation is hereby granted, provided that both the copyright notice
//   and this permission notice appear in all copies of the software, derivative
//   works or modified versions, and any portions thereof, and that both notices
//                       appear in supporting documentation.

//   This code is distributed in the hope that it will be useful, but WITHOUT
//   ANY
//  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
//  FOR A PARTICULAR PURPOSE. THE AUTHORS AND ECOLE POLYTECHNIQUE FEDERALE DE
//  LAUSANNE
// DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM
// THE
//                              USE OF THIS SOFTWARE.
// */

// //#define OLTP_PATH "/scratch/raza/htap/opt/aeolus/aeolus-server"
// #define OLAP_PATH "./../pelago/proteusmain-server"

// #define LAUNCH_OLTP false
// #define LAUNCH_OLAP false
// #define RUNTIME 60

// #include "topology/topology.hpp"
// #include <chrono>
// #include <cstdlib>
// #include <iostream>
// #include <limits>
// #include <signal.h>
// #include <sys/types.h>
// #include <thread>
// #include <unistd.h>
// #include <vector>

// // OLTP STUFF
// #include "benchmarks/bench.hpp"
// #include "benchmarks/tpcc.hpp"
// #include "benchmarks/ycsb.hpp"
// #include "glo.hpp"
// #include "scheduler/comm_manager.hpp"
// #include "scheduler/topology.hpp"
// #include "scheduler/worker.hpp"
// #include "storage/memory_manager.hpp"
// #include "storage/table.hpp"

// pid_t init_oltp_process();
// pid_t init_olap_process();
// void kill_orphans_and_widows(int s);
// void register_handler();

// std::vector<pid_t> children;

// // std::osyncstream ss_cout(std::cout);
// using sout = std::osyncstream(std::cout);

// int main(int argc, char **argv) {

//   // const auto &vec = topology::getInstance().getCpuNumaNodes();
//   // std::cout << scheduler::Topology::getInstance() << std::endl;

//   // std::cout << vec[0].local_cpu_set << std::endl;

//   std::cout << "------- HTAP  ------" << std::endl;

//   std::cout << "[HTAP] HTAP Process ID: " << getppid() << std::endl;

//   // Register signal handler
//   register_handler();

//   // start OLTP
//   sout << "[OLTP] ------- AELOUS ------" << std::endl;
//   sout << "[OLTP] # Workers: " << num_workers << std::endl;
//   storage::MemoryManager::init();
//   // scheduler::CommManager::getInstance().init();

//   // init benchmark
//   bench::Benchmark *bench = nullptr;

//   if (bechnmark == 1) {
//     bench = new bench::TPCC("TPCC", num_workers);

//   } else if (bechnmark == 2) {
//     bench = new bench::TPCC("TPCC", 100, 0,
//                             "/home/raza/local/chBenchmark_1_0/w100/raw");
//   } else { // Defult YCSB

//     std::cout << "Write Threshold: " << write_threshold << std::endl;
//     std::cout << "Theta: " << theta << std::endl;
//     bench = new bench::YCSB("YCSB", num_fields, num_records, theta,
//                             num_iterations_per_worker, num_ops_per_txn,
//                             write_threshold, num_workers, num_workers);
//   }

//   bench->load_data(num_workers);

//   scheduler::AffinityManager::getInstance().set(
//       &scheduler::Topology::getInstance().get_worker_cores()->front());
//   scheduler::WorkerPool::getInstance().init(bench);

//   scheduler::WorkerPool::getInstance().start_workers(num_workers);

//   /* Report stats every 1 sec */
//   // timed_func::interval_runner(
//   //    [] { scheduler::WorkerPool::getInstance().print_worker_stats(); },
//   //    1000);

//   usleep(RUNTIME * 1000000);

//   while (true) {
//     std::this_thread::sleep_for(std::chrono::seconds(60));
//   }
// }

// void shutdown_oltp() {

//   // // shutdown workers
//   // scheduler::WorkerPool::getInstance().shutdown(true);

//   // // shutdown comm manager
//   // // scheduler::CommManager::getInstance().shutdown();

//   // // teardown relational schema
//   // storage::Schema::getInstance().teardown();
// }

// pid_t init_oltp_process() {}

// pid_t init_olap_process() {

//   // #if LAUNCH_OLAP
//   //   pid_t pid_olap = init_olap_process();
//   //   assert(pid_olap != 0);
//   //   children.push_back(pid_olap);
//   //   std::cout << "[HTAP] OLAP Process ID: " << pid_olap << std::endl;
//   // #endif
//   assert(false && "Unimplemented");
// }

// void kill_orphans_and_widows(int s) {
//   std::cout << "[SIGNAL] Recieved signal: " << s << std::endl;
//   for (auto &pid : children) {
//     kill(pid, SIGTERM);
//   }
//   exit(1);
// }

// void register_handler() {
//   // struct sigaction sigIntHandler;
//   // sigIntHandler.sa_handler = kill_orphans_and_widows;
//   // sigemptyset(&sigIntHandler.sa_mask);
//   // sigIntHandler.sa_flags = 0;
//   // sigaction(SIGINT, &sigIntHandler, NULL);
//   signal(SIGINT, kill_orphans_and_widows);
// }
