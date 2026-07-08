# Voxel vs Git Performance Benchmarks

Automated performance comparison executed via `hyperfine` profiling on local repository sandboxes. 

## 📊 Summary Performance Table

| Command / Workflow | Git Performance (Mean) | Voxel Performance (Mean) | Voxel Speed Factor |
| :--- | :--- | :--- | :--- |
| **Repository Initialization** | 11.9 ms ± 1.3 ms | 1.9 ms ± 0.5 ms | **6.28x faster** 🚀 |
| **File Staging / Tracking** | 20.6 ms ± 0.6 ms | 2.8 ms ± 0.3 ms | **7.35x faster** 🚀 |
| **Commit Engine** | 15.7 ms ± 1.0 ms | 2.7 ms ± 0.5 ms | **5.84x faster** 🚀 |
| **Workspace Status Scan** | 8.9 ms ± 1.8 ms | 1.9 ms ± 0.2 ms | **4.58x faster** 🚀 |
| **Timeline Log Reader** | 8.2 ms ± 0.6 ms | 1.2 ms ± 0.1 ms | **6.92x faster** 🚀 |
| **Lineage Graph Rendering** | 8.8 ms ± 0.4 ms | 2.3 ms ± 1.1 ms | **3.87x faster** 🚀 |
| **Volatile Save (Stash/Snap)**| 22.0 ms ± 0.9 ms | 2.4 ms ± 0.2 ms | **9.14x faster** 🚀 |
| **Workspace Restoration** | 9.3 ms ± 0.5 ms | 3.0 ms ± 0.3 ms | **3.12x faster** 🚀 |

---

## 🔍 Deep Architectural Analysis

### 1. `init` (6.28x Faster)
* **What it means:** When you run `git init`, Git builds a dense network of files, including hidden sample hooks, description structures, configuration templates, and multiple sub-shards inside `.git/objects/`.
* **The Voxel Edge:** Voxel acts like a lightweight engine. It skips the sample bloat and creates only the exact administrative folders required to manage the active branch graph.

### 2. `track` vs `git add` (7.35x Faster)
* **What it means:** Git treats staging as a heavy lifting zone. When you run `git add`, Git compresses file copies into temporary objects right away, updating a complex binary index file.
* **The Voxel Edge:** Voxel separates its concerns cleanly. Your tracking logic maps paths directly to hashes inside a flat index file, deferring file actions to the actual storage phase.

### 3. `commit` (5.84x Faster)
* **What it means:** Git runs complex tree constructions, hashes individual directory levels, and handles global graph validations before appending a new node.
* **The Voxel Edge:** Your dual-engine design uses a smart traffic router. By looking at extensions, Voxel copies heavy pre-compressed media as raw bytes to maximize SSD performance, while applying Zstd compression to code files—all within a tiny **2.7 ms** window.

### 4. `status` (4.58x Faster)
* **What it means:** Git maps filesystem stats (`lstat` cache entries) across modification times to detect working tree changes, which incurs parsing overhead.
* **The Voxel Edge:** Voxel loops through your clean, flat index file layout in a lightning-fast loop, matching current file states directly against registered hash signatures.

### 5. `log` & `graph` (Up to 6.92x Faster)
* **What it means:** Git reads historical layers by recursively opening and parsing individual cryptographic commit objects, extracting data graphs on the fly.
* **The Voxel Edge:** Voxel treats its history ledger as a simple, append-only linear log file. Reading the timeline is as fast as streaming a flat text block off your Mac's NVMe drive.

### 6. `snapshot` vs `git stash` (9.14x Faster)
* **What it means:** This is your biggest performance win. `git stash` is slow because it creates up to three completely independent internal commit objects to back up your working directory and staging index.
* **The Voxel Edge:** Voxel implements a true volatile scratchpad. Because it only allows one snapshot state to live at a time, it wipes the previous folder and dumps raw bytes straight to disk with no encryption or compression overhead.




# Voxel vs Git Performance & Hardware Telemetry

Automated low-level profiling executed using `/usr/bin/time -l` on macOS (Apple Silicon).

## 🎛️ Low-Level Hardware Metrics Comparison

| Command / Workflow | Metric Type | Git Ecosystem | Voxel Engine | Hardware Efficiency Multiplier |
| :--- | :--- | :--- | :--- | :--- |
| **Repository Init** | Peak RAM (Max RSS)<br>CPU Instructions | 4.60 MB<br>13.32 M | 1.54 MB<br>2.07 M | **2.98× Less RAM**<br>**6.43× Fewer Cycles** 🧠 |
| **File Staging (Track)** | Peak RAM (Max RSS)<br>CPU Instructions | 6.19 MB<br>250.30 M | 2.27 MB<br>29.10 M | **2.72× Less RAM**<br>**8.60× Fewer Cycles** ⚡ |
| **Commit Storage** | Peak RAM (Max RSS)<br>CPU Instructions | 5.66 MB<br>124.48 M | 1.75 MB<br>19.62 M | **3.23× Less RAM**<br>**6.34× Fewer Cycles** 🔒 |
| **Workspace Status** | Peak RAM (Max RSS)<br>CPU Instructions | 5.55 MB<br>92.52 M | 2.09 MB<br>25.17 M | **2.65× Less RAM**<br>**3.67× Fewer Cycles** 🔍 |
| **Timeline Log** | Peak RAM (Max RSS)<br>CPU Instructions | 5.50 MB<br>91.69 M | 1.58 MB<br>17.96 M | **3.48× Less RAM**<br>**5.10× Fewer Cycles** 📜 |
| **Lineage Graph** | Peak RAM (Max RSS)<br>CPU Instructions | 5.35 MB<br>89.14 M | 1.60 MB<br>17.98 M | **3.34× Less RAM**<br>**4.95× Fewer Cycles** 🗺️ |
| **Volatile Snapshot** | Peak RAM (Max RSS)<br>CPU Instructions | 5.88 MB<br>128.27 M | 1.62 MB<br>20.48 M | **3.62× Less RAM**<br>**6.26× Fewer Cycles** ⚡ |
| **Workspace Restore** | Peak RAM (Max RSS)<br>CPU Instructions | 5.24 MB<br>89.99 M | 1.86 MB<br>26.84 M | **2.81× Less RAM**<br>**3.35× Fewer Cycles** 🌲 |

---

## 🧠 Telemetry Insights

### 1. Staging Throughput (`git add` vs `track`)
Git retired **250,308,414 instructions** to parse and index your workspace. Voxel required only **29,105,728 instructions**. Git suffers from heavy internal text-encoding translations and index state-matching loops, while Voxel tracks direct filesystem structures using lean, flat vector buffers.

### 2. High-Speed Snapshot Architecture
When stashing local state, Git forces **128 Million instructions** to balance a multi-commit tracking safety mechanism. Voxel captures the workspace in a compact **20 Million instructions** using a direct byte-stream dump, reducing cache pollution on the CPU.

### 3. Idle Footprint Suppression
Git carries a standard baseline memory penalty, hovering at a minimum `maximum resident set size` of **~5.2 MB** even for instant execution paths like `log` or `status`. Voxel allocates memory strictly on demand, frequently completing historical sweeps inside a tiny **1.5 MB** hardware profile.