# Code B+-Trees

An implementation of a B+-Tree, which can directly generate Machine Code realizing a lookup operation to increase performance. The repository contains both the tree implementation as well as scripts used for benchmarking the B+-Tree with YCSB workloads.

## Repository layout

| Path | Contents |
| --- | --- |
| `augmented_btree_olc.hpp` | B+-tree with OLC and the machine code generator |
| `x86-instructions.h` | Structs realizing x86 instructions |
| `experiment-data/main-ycsb.cpp` | Main Benchmark File |
| `experiment-data/index.h` | Tree configuration |
| `experiments.py` | Builds the benchmark and runs experiments with different parameters |
| `script/generate_ycsb.py` | Generates workload files with YCSB |
| `workload_specification/` | YCSB workload specifications |
| `.devcontainer/` | Development container (Ubuntu 22.04 with all dependencies) |
| `code-b+tree-results-synchronization.csv` | Results shown in the evaluation using synchronization |
| `code-b+tree-results-no-synchronization.csv` | Results shown in the evaluation with synchronization logic removed |

## Starting the dev container

The dev container comes with everything needed to build the code and generate workloads: GCC, CMake, Boost, Python 3, Java 17 (for YCSB), gdb and perf.

**Requirements:** Docker on an x86-64 Linux host. The container runs with `--privileged` so that threads can be pinned to cores and memory can be marked both for writing and executing.

### With VS Code

1. Install the [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension.
2. Open the repository folder in VS Code.
3. Run **Dev Containers: Reopen in Container** from the command palette (`Ctrl+Shift+P`), or click **Reopen in Container** in the notification.
4. The first build of the image takes a few minutes. The repository is then mounted at `/workspaces/code-trees`.

### Without VS Code

With the [Dev Container CLI](https://github.com/devcontainers/cli):

```bash
npm install -g @devcontainers/cli
devcontainer up --workspace-folder .
devcontainer exec --workspace-folder . bash
```

Or with plain Docker:

```bash
docker build -t code-trees .devcontainer
docker run -it --privileged -v "$PWD":/workspaces/code-trees -w /workspaces/code-trees code-trees bash
```

## Running experiments

Run the script from the repository root inside the container:

```bash
python3 experiments.py
```

All paths in the script are relative, so it must be started from the repository root.

The first run needs internet access: `generate_ycsb.py` downloads YCSB 0.16.0 into `YCSB/`.

### What the script does

The `experiments.py` script copies the relevant files to `temp-experiments/`. It then iterates over all set configurations, building the experiment code and generating YCSB data as necessary. The results are appended to `results.csv`.


### Configuring an experiment

All experiment parameters can be set in `experiments.py`. Every field except `repetitions` takes a list, and the script runs every combination of the values:

```python
experiment_ranges = ExperimentRanges(
    initial_tree_size = [10000000],     # number of insert operations in the fill phase
    workload_size     = [10000000],     # number of insert and lookup operations in the mixed phase
    lookup_ratio      = [0.5,0.6,0.7,0.8,0.9,0.95,0.98,0.99,1.0],      # ratio of lookup operations to insert operations in the mixed phase
    key_type          = ['randint', 'rand32'],  # 'randint' (random 64-bit), 'rand32' (random 32-bit)
    node_size         = [256, 512, 1024, 2048, 4096],   # node size in bytes
    linear            = [True, False],  # True: linear search, False: binary search
    max_code_depth    = [-1, 1, 2, 4, 8],   # number of tree levels compiled to code (-1 compiles the whole tree)
    threads           = [1,2,4,8,16,32,64], # number of threads
    placement         = ['compact'],    # how threads are pinned to cores
    repetitions       = 5,  # repetitions per configuration
)
```

For example, this sweeps the thread count and the read ratio on a tree with 10 million keys:

```python
experiment_ranges = ExperimentRanges(initial_tree_size = [10000000], workload_size = [10000000],
    lookup_ratio = [1.0, 0.9, 0.5], key_type = ['randint'], node_size = [512], linear = [True],
    max_code_depth = [-1], threads = [1, 2, 4, 8, 16, 32], placement = ['compact'], repetitions = 5)
```

### Output

The results are appended to `results.csv` in the repository root. The script writes the header line at the start of every run, so after several runs the file contains several header lines. Delete or rename the file to start fresh.

| Column | Meaning |
| --- | --- |
| `architecture` | Currently only intel |
| `experiment_type` | Currently only `mixed`|
| `fill_path`, `mixed_path` | Workload files used |
| `num_threads` | Number of threads |
| `linear` | if `true`, linear search is used, otherwise binary search |
| `max_code_depth` | Value of `max_code_depth` |
| `node_size` | Node size in bytes |
| `initial_tree_size` | Number of operations in the fill workload |
| `workload_size` | Number of operations in the mixed workload |
| `lookup_ratio` | Value of `lookup_ratio` |
| `run` | Repetition index |
| `type` | `mixed_native` (native B+-Tree) or `mixed_code` (tree with Code B+-Tree) |
| `time` | Duration of the mixed phase in milliseconds |
| `throughput` | Mixed phase throughput in million operations per second |
| `placement` | Core spread |

