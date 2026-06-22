# Hash Table Benchmarks

This is a copy of https://github.com/JacksonAllan/c_cpp_hash_tables_benchmark with additional features and benchmarks.

This repository contains a comparative, extendible benchmarking suite for C and C++ hash-table libraries.

The benchmarks measure the speed of inserting keys, replacing keys, looking up existing keys, looking up nonexisting keys, deleting existing keys, deleting nonexisting keys, and iteration.

## 🆕 New Features in This Fork

- **🔧 New hash tables added**: [ihtab and ixhtab](https://github.com/vnmakarov/ihtab) - high-performance hash tables optimized for modern CPUs
- **📊 Memory heat table generation**: Visual heatmaps showing memory usage patterns and performance characteristics
- **📈 Geomean results**: Geometric mean calculations across all benchmarks for comprehensive performance comparison
- **🎨 Improved visualization**: Enhanced styling and presentation of heat tables and charts
- **⚡ Selective benchmarking**: Run specific hash tables via command-line arguments

> **Note**: These benchmark suite enhancements were developed with [Claude Code](https://claude.ai/code) assistance. The ihtab/ixhtab hash table implementations themselves are original non-AI work.

## Quick Start

```bash
# Build the benchmark
g++ -I. -std=c++20 -static -O3 -DNDEBUG -Wall -Wpedantic main.cpp

# Run all benchmarks
./a.out

# Run specific tables only
./a.out ihtab_cpp ixhtab_cpp std_unordered_map
```

## Running the Benchmarks

### Performance Setup (Recommended)

1. **Close background processes** to minimize interference
2. **Lock CPU frequency** to ensure consistent results:
   ```bash
   # Linux example - set to performance governor
   sudo cpupower frequency-set -g performance
   ```
3. **Use taskset for hybrid CPUs** (Intel 12th gen+, Apple M-series):
   ```bash
   # Run on performance cores only (Linux)
   taskset -c 0-7 ./a.out

   # Check core topology first
   lscpu -e
   ```

### Basic Usage

```bash
# Run all configured tables
./a.out

# Run specific tables
./a.out ihtab_cpp ixhtab_cpp

# Mix C and C++ tables
./a.out ihtab_c ixhtab_cpp std_unordered_map ankerl_unordered_dense
```

### Output Files

The benchmark generates two files with GMT timestamps:

- **`result_YYYY-MM-DDTHH_MM_SS.html`**: Interactive graphs and heatmaps
- **`result_YYYY-MM-DDTHH_MM_SS.csv`**: Raw performance data

The HTML file contains:
- Interactive performance graphs (hover/click to highlight/toggle)
- Memory usage heatmaps
- Geometric mean summary tables
- Auto-scaling charts based on visible data

## Results

Benchmark results across different architectures:

- **[AMD 9900X](https://htmlpreview.github.io/?https://github.com/vnmakarov/c_cpp_hash_tables_benchmark/blob/main/AMD.html)** - Zen 5 architecture results
- **[Intel 270K+](https://htmlpreview.github.io/?https://github.com/vnmakarov/c_cpp_hash_tables_benchmark/blob/main/270K.html)** - Arrow Lake hybrid architecture
- **[Apple M4](https://htmlpreview.github.io/?https://github.com/vnmakarov/c_cpp_hash_tables_benchmark/blob/main/M4.html)** - Apple Silicon results

## Configuration

### Quick Configuration
Edit `config.h` to modify:
- Total key count and measurement intervals
- Maximum load factor
- Which hash tables and blueprints to test
- Benchmark types to run

### Available Hash Tables
To see all available tables, check the `SHIM_*` definitions in `config.h` or run:
```bash
grep "^#define SHIM_" config.h
```

### Adding New Tables and Blueprints
For detailed instructions on extending the benchmark suite, see the original repository: https://github.com/JacksonAllan/c_cpp_hash_tables_benchmark.

## Built-in Hash Tables

### C++ Tables

* **[ihtab](https://github.com/vnmakarov/ihtab)** - New high-performance hash table (C++ interface)
* **[ixhtab](https://github.com/vnmakarov/ihtab)** - New extensible hash table (C++ interface)
* [absl::flat_hash_map](https://github.com/abseil/abseil-cpp) v20240116.2
* [ankerl::unordered_dense](https://github.com/martinus/unordered_dense) v4.1.2
* [boost::unordered_flat_map](https://www.boost.org/doc/libs/1_85_0/libs/unordered/doc/html/unordered.html) v1.85.0
* [emilib2::HashMap](https://github.com/ktprime/emhash/tree/master/thirdparty/emilib)
* [ska::bytell_hash_map](https://github.com/skarupke/flat_hash_map/blob/master/bytell_hash_map.hpp)
* [std::unordered_map](https://en.cppreference.com/w/cpp/container/unordered_map)
* [tsl::robin_map](https://github.com/Tessil/robin-map) v1.3.0

### C Tables

* **[ihtab](https://github.com/vnmakarov/ihtab)** - New high-performance hash table (C interface)
* **[ixhtab](https://github.com/vnmakarov/ihtab)** - New extensible hash table (C interface)
* cc_map from [CC](https://github.com/JacksonAllan/CC) v1.1.1
* khash from [klib](https://github.com/attractivechaos/klib) v0.2.8
* DICT from [M\*LIB](https://github.com/P-p-H-d/mlib) v0.7.3
* DICT_OA from [M\*LIB](https://github.com/P-p-H-d/mlib) v0.7.3
* hm and sh from [stb_ds](https://github.com/nothings/stb/blob/master/stb_ds.h) v0.67
* hmap from [STC](https://github.com/stclib/STC) v5.0 beta 4
* [uthash](https://troydhanson.github.io/uthash) v2.3.0
* [Verstable](https://github.com/JacksonAllan/Verstable) v2.1.0
