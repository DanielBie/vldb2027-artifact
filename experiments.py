#!/usr/bin/env python3

import os
from dataclasses import dataclass, field
import shutil
import subprocess
import re

workload_file = """
fieldcount=1
recordcount=   {num_write_keys}
operationcount={num_operations}
fieldlength=4

workload=com.yahoo.ycsb.workloads.CoreWorkload

readallfields=true

readproportion={read_proportion}
updateproportion=0
scanproportion=0
insertproportion={insert_proportion}

requestdistribution=zipfian
keytype={key_type}
"""

dynamic_cmake = """
cmake_minimum_required(VERSION 3.16)
project(code-tree-experiment)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g3")

add_executable(code-tree-experiment main.cpp)

target_link_libraries(code-tree-experiment boost_serialization)
"""

@dataclass
class ExperimentRanges:
    initial_tree_size: [int]
    lookup_ratio: [float]
    linear: [bool]
    max_code_depth: [int]
    node_size: [int]
    threads: [int]
    repetitions: int
    workload_size: [int]
    placement: [str]
    key_type: [str]

CORES_PER_CCD = 8
CCDS_PER_SOCKET = 12

def core_list(placement, threads):
    if threads > CORES_PER_CCD * CCDS_PER_SOCKET:
        raise ValueError(f"{threads} threads do not fit on one socket")
    if placement == 'compact':
        # fill CCD 0 first, then CCD 1, ... (old behaviour: 0..T-1)
        cores = list(range(threads))
    elif placement == 'spread':
        # round-robin over the CCDs of socket 0: 0,8,16,...,88,1,9,17,...
        cores = [(i % CCDS_PER_SOCKET) * CORES_PER_CCD + i // CCDS_PER_SOCKET for i in range(threads)]
    else:
        raise ValueError(f"unknown placement {placement}")
    return ','.join(str(c) for c in cores)

def change_line_in_file(filename, old_line, new_line):
    with open(filename, "r") as file:
        lines = file.readlines()
    with open(filename, "w") as file:
        for line in lines:
            if old_line in line:
                file.write(new_line)
            else:
                file.write(line)

def replace_value_in_file(filename, reg, new_value):
    with open(filename, "r") as file:
        lines = file.readlines()
    with open(filename, "w") as file:
        for line in lines:
            match = re.search(reg, line)
            if match:
                old_value = match.group(1)
                file.write(line.replace(str(old_value) + ',', str(new_value) + ','))
            else:
                file.write(line)

def run_build_step(cmd, cwd, log):
    result = subprocess.run(cmd, cwd=cwd, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        log.flush()
        log_path = log.name
        with open(log_path, "r") as f:
            print(f.read())
        raise RuntimeError(f"Build step {cmd} failed with exit code {result.returncode}. See {log_path} for details.")

def build_experiment_code():
    cmake_content = dynamic_cmake
    with open("temp-experiments/CMakeLists.txt", "w") as f:
        f.write(cmake_content)
    build_dir = "temp-experiments/build"
    if not os.path.exists(build_dir):
        os.makedirs(build_dir)
    print("building experiment code...")
    with open("temp-experiments/build.log", "w") as log:
        run_build_step(["cmake", "-DCMAKE_BUILD_TYPE=Release", ".."], cwd=build_dir, log=log)
        run_build_step(["make"], cwd=build_dir, log=log)
    print("build done, see temp-experiments/build.log for details")

def run_experiments_dynamic_ycsb(er):
    if not os.path.exists("temp-experiments"):
        os.mkdir("temp-experiments")
    results = open('results.csv', 'a')
    results.write('architecture,experiment_type,fill_path,mixed_path,num_threads,linear,max_code_depth,node_size,initial_tree_size,workload_size,lookup_ratio,run,type,time,throughput,placement\n')
    shutil.copy("experiment-data/main-ycsb.cpp", "temp-experiments/main.cpp")
    shutil.copy("experiment-data/index.h", "temp-experiments")
    shutil.copy("augmented_btree_olc.hpp", "temp-experiments")
    shutil.copy("x86-instructions.h", "temp-experiments")
    change_line_in_file("temp-experiments/augmented_btree_olc.hpp", '#include \"index.hpp\"', "#include \"index.h\"\n")

    for nk in er.initial_tree_size:
        for ia in er.workload_size:
            for ns in er.node_size:
                change_line_in_file("temp-experiments/index.h", 'static const uint16_t Nodesize', "    static const uint16_t Nodesize = " + str(ns) + ";\n")

                build_experiment_code()
                for pw in er.lookup_ratio:
                    for kt in er.key_type:
                        workload_format = workload_file.format(num_write_keys = nk,num_operations = ia, read_proportion = pw, insert_proportion = 1-pw, key_type = kt)
                        if os.path.getsize("workload_specification/workloada") == len(workload_format.encode()):
                            with open("workload_specification/workloada") as f:
                                same = f.read() == workload_format
                        else:
                            same = False
                        if not same:
                            with open('workload_specification/workloada', 'w') as workload_f:
                                workload_f.write(workload_file.format(num_write_keys = nk,num_operations = ia, read_proportion = pw, insert_proportion = 1-pw, key_type=kt))
                            os.system('./script/generate_ycsb.py a ' + kt)
                        for l in er.linear:
                            for t in er.threads:
                                for p in er.placement:
                                    for m in er.max_code_depth:
                                            replace_value_in_file("temp-experiments/main.cpp", r'"max-code-depth",\s*(\d+)', m)
                                            replace_value_in_file("temp-experiments/main.cpp", r'"repetitions",\s*(\d+)', er.repetitions)
                                            replace_value_in_file("temp-experiments/main.cpp", r'"threads",\s*(\d+)', t)
                                            replace_value_in_file("temp-experiments/main.cpp", r'"pre-write-key-ratio",\s*(\d+.\d+)', pw)
                                            replace_value_in_file("temp-experiments/main.cpp", r'"write-keys",\s*(\d+)', nk)

                                            x = ''
                                            try:
                                                command = ['./build/code-tree-experiment', '-f', "../workloads/fill_" + kt + "_workloada", '-g', "../workloads/mixed_" + kt + "_workloada", '-i', str(pw), '-m', str(m), '-r', str(er.repetitions), '-t', str(t), '-c', core_list(p, t)]
                                                if l:
                                                    command.append('-l')
                                                print(command)
                                                x = subprocess.check_output(command, cwd = 'temp-experiments')
                                                for line in x.decode().splitlines():
                                                    if line:
                                                        results.write(line + ',' + p + '\n')
                                                results.flush()
                                            except Exception as e:
                                                print('error ', e)
    results.close()

            

experiment_ranges = ExperimentRanges(initial_tree_size = [100000], lookup_ratio = [1.0], linear = [True], max_code_depth = [-1], node_size = [512], threads = [1], repetitions = 5, workload_size = [100000], placement = ['compact'], key_type = ['randint'])

run_experiments_dynamic_ycsb(experiment_ranges)
