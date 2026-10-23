#include <bits/stdc++.h>

#include <iostream>
#include <fstream>
#include <thread>
#include <unordered_map>
#include <utility>
#include <bitset>
#include <thread>
#include <barrier>

#include "../cmdparser.hpp"
#include "augmented_btree_olc.hpp"

// #include "tree.cpp"

uint64_t insert_seed = 73487829;

std::optional<Value> get_code(const Key k);

struct ExperimentSetup {
    uint16_t threads;
    bool linear;
    std::string fill_workload_path;
    std::string mixed_workload_path;
    float ratio;
    int32_t max_code_depth;
    uint16_t reps;
    std::vector<int> cores;
};

struct KeyValue{
    Key key;
    Value value;
};

struct ExperimentElement {
    KeyValue kv;
    char type;
};


void pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << "Failed to pin thread to core " << core_id << std::endl;
        exit(1);
    }
}

std::vector<int> parse_cores(const std::string& spec, const uint16_t threads){
    std::vector<int> cores;
    std::stringstream ss(spec);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (tok.empty()) continue;
        auto dash = tok.find('-');
        if (dash == std::string::npos) {
            cores.push_back(std::stoi(tok));
        } else {
            for (int c = std::stoi(tok.substr(0, dash)); c <= std::stoi(tok.substr(dash + 1)); ++c) {
                cores.push_back(c);
            }
        }
    }
    if (cores.empty()) {
        for (int i = 0; i < threads; ++i) cores.push_back(i);   // old behaviour: 0..T-1
    }
    if (cores.size() < threads) {
        std::cerr << "Core list has " << cores.size() << " entries but " << threads << " threads requested" << std::endl;
        exit(1);
    }
    return cores;
}



template <typename BTree>
void mixed_workload(BTree *tree, const std::vector<ExperimentElement>& elements, ExperimentSetup es, const bool dynamic, const uint32_t start){
    for (auto na = start; na < elements.size(); na+=es.threads) {
        // ExperimentElement el = elements.at(na);
        if (elements.at(na).type == 'w'){
            tree->put(elements.at(na).kv.key, elements.at(na).kv.value, es.linear, dynamic);
        } else {
            std::optional<Value> v = tree->get(elements.at(na).kv.key, es.linear);
            if (!v.has_value()) {
                std::cout << "Key " << elements.at(na).kv.key << " can not be found in the tree." << std::endl;
                std::optional<Value> v = tree->get(elements.at(na).kv.key, es.linear);
                exit(0);
            } else if (v != elements.at(na).kv.value) {
                std::cout << "Key " << elements.at(na).kv.key << " returned wrong value " << v.value() << "." << std::endl;
            }
        }
    }
}

template <typename BTree>
double run_phase(BTree *tree, const std::vector<ExperimentElement>& elements, const ExperimentSetup& es, const bool dynamic){
    std::barrier start_barrier(es.threads + 1);   // workers + main thread
    std::vector<std::thread> threads;
    threads.reserve(es.threads);

    for (auto i = 0U; i < es.threads; ++i) {
        threads.emplace_back([&, i]() {
            pin_thread_to_core(es.cores[i]);
            start_barrier.arrive_and_wait();
            mixed_workload(tree, elements, es, dynamic, i);
        });
    }

    start_barrier.arrive_and_wait();              // all workers pinned and ready
    auto start = std::chrono::steady_clock::now();
    for (auto &thread : threads) {
        thread.join();
    }
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

template <typename BTree>
void mixed_workload_full(BTree *tree, std::vector<ExperimentElement>& fill_elements, std::vector<ExperimentElement>& mixed_elements, ExperimentSetup es, uint16_t rep, const char code_tree){
    run_phase(tree, fill_elements, es, code_tree);

    if(code_tree){
        tree->gen_machine_code(es.linear);
    }

    double total_time = run_phase(tree, mixed_elements, es, code_tree);

    std::cout << "intel,mixed," << es.fill_workload_path << "," << es.mixed_workload_path << "," << es.threads << "," << (es.linear ? "linear" : "binary") << "," << es.max_code_depth << "," << Config::Nodesize << "," << fill_elements.size() << "," << mixed_elements.size() << "," << es.ratio << "," << rep << "," << (code_tree == 0 ? "mixed_native" : (code_tree == 1 ? "mixed_code" : "mixed_full_code")) << "," << total_time << "," << (mixed_elements.size() / (total_time/1000)) / 1000000 << "\n";
}


void load_workload(std::vector<ExperimentElement>& elements, std::string path){
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not open file: " << path << "\n";
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string op;
        uint64_t num;

        if (!(ss >> op >> num)) {
            std::cerr << "Malformed line, skipping: " << line << "\n";
            continue;
        }

        char type = 'w';
        if (op == "READ") {
            type = 'r';
        } else if (op == "INSERT") {
            type = 'w';
        } else {
            std::cerr << "Unknown op '" << op << "', skipping: " << line << "\n";
            continue;
        }

        KeyValue kv = {num, num};

        elements.push_back({kv, type});
    }

}


void configure_parser(cli::Parser &parser) {
    parser.set_optional<std::string>("c", "cores", "", "Comma separated core list / ranges for pinning, e.g. 0-7 or 0,8,16 (default 0..T-1)");
    parser.set_optional<std::string>("f", "fill-workload", "../workloads/fill_randint_workloada", "The fill workload to use");
    parser.set_optional<std::string>("g", "mixed-workload", "../workloads/mixed_randint_workloada", "The mixed workload to use");
    parser.set_optional<float>("i", "ratio", 1.0, "The ratio of reads to writes");
    parser.set_optional<bool>("l", "linear", false, "If set to true the a linear search instead of a binary search will be used");
    parser.set_optional<int32_t>("m", "max-code-depth", -1, "The maximal depth of the code tree before jumping to native tree.");
    parser.set_optional<uint16_t>("r", "repetitions", 1, "Sets the number of repetitions of the experiments");
    parser.set_optional<uint16_t>("t", "threads", 1, "Sets the number of threads used by the experiments");
}

int main(int argc, char **argv) {
    cli::Parser parser(argc, argv);
    configure_parser(parser);
    parser.run_and_exit_if_error();

    ExperimentSetup es;

    es.cores = parse_cores(parser.get<std::string>("c"), es.threads);
    es.linear = parser.get<bool>("l");
    es.fill_workload_path = parser.get<std::string>("f");
    es.mixed_workload_path = parser.get<std::string>("g");
    es.ratio = parser.get<float>("i");
    es.max_code_depth = parser.get<int32_t>("m");
    es.reps = parser.get<uint16_t>("r");
    es.threads = parser.get<uint16_t>("t");

    std::vector<ExperimentElement> fill_workload;
    std::vector<ExperimentElement> mixed_workload;

    load_workload(fill_workload, es.fill_workload_path);
    load_workload(mixed_workload, es.mixed_workload_path);
    
    for(auto i = 0; i<es.reps; ++i){
        BTreeOLC<Key, Value>* tree2 = new BTreeOLC<Key, Value>();
        tree2->max_code_depth = es.max_code_depth;
        mixed_workload_full(tree2, fill_workload, mixed_workload, es, i, 0);
        delete tree2;
    }

    for(auto i = 0; i<es.reps; ++i){
        BTreeOLC<Key, Value>* tree2 = new BTreeOLC<Key, Value>();
        tree2->max_code_depth = es.max_code_depth;
        mixed_workload_full(tree2, fill_workload, mixed_workload, es, i, 1);
        delete tree2;
    }
  
    return 0;
}
