

# Voxel

**A fast, lightweight, local-first version control system — with AI built in.**

*Think Git, rebuilt from scratch in C++ for speed, simplicity, and a friendlier vocabulary.*

</div>

---

## What is Voxel?

Voxel is a standalone version control system (VCS), similar in spirit to Git, that you run from your terminal to track changes to a project over time. You `track` files, `commit` snapshots of your work, create `branch`es to work on different ideas in parallel, and `restore` your workspace to any point in its history.

Voxel was built as a from-scratch engine (not a Git wrapper) with a few goals:

- **Speed and a light footprint.** Independent benchmarking against Git on the same machine showed Voxel completing core operations (`init`, `track`, `commit`, `status`, `log`, `graph`, snapshotting, and `restore`) several times faster, using roughly a third of the peak memory and a fraction of the CPU instructions. See [`Benchmarks.md`](Benchmarks.md) for the full numbers and explanation.
- **A simpler mental model.** Plain-English commands (`track`, `freeze`, `where`, `snapshot`/`snapback`, `bin`/`revive`) instead of Git's sometimes-cryptic terminology.
- **Built-in AI assistance.** Voxel can generate commit messages, review your code, and explain diffs in plain English using an AI provider you connect.
- **Smart binary handling.** Voxel automatically detects binary/media file types (images, video, audio, 3D models, archives) and stores them as raw bytes instead of wasting time trying to compress or diff them.
- **Local peer-to-peer sharing.** No server required — you can pack a repository into a single file, or beam it directly to a teammate on the same network.

Voxel is not a drop-in Git replacement and does not use Git's on-disk format — it's an independent implementation with its own repository layout (`.voxel/`), its own object model, and its own history graph.

---

## How Voxel thinks about your project

Every Voxel repository lives inside a hidden `.voxel/` folder at the root of your project (created by `voxel init`), alongside a `.voxelignore` file where you can list folders, paths, or file extensions you never want tracked.

The core concepts:

| Concept | What it means in Voxel |
| --- | --- |
| **Workspace** | The actual files on disk that you're editing. |
| **Index** | The list of files you've explicitly `track`ed and are ready to be included in the next commit. |
| **Commit** | A permanent, named snapshot of your tracked files at a point in time, linked to the commit before it (its "parent"). |
| **Branch** | A named, moving pointer to a commit — lets you develop different lines of work side by side. |
| **HEAD** | A pointer to whichever branch (and therefore commit) you currently have checked out. |
| **Snapshot** | A *temporary*, single-slot scratchpad of your current workspace — separate from the permanent commit history. Think "undo point," not "save file." |
| **Bin** | A soft-delete area for commits and branches you've removed but might want back, instead of destroying them outright. |

Commits form a tree of history that you can view as a simple list (`log`) or as a full branching graph (`graph`), and you can export that graph as a PDF for documentation or presentations (`export`).

---

## Installation

Voxel is written in C++17 and builds with CMake. It bundles its own dependencies (Zstandard for compression, mbedTLS for SHA-256 hashing, `dtl` for diffing, and a JSON parser for AI/network payloads), so there's nothing extra to install beyond a C++ compiler and CMake itself.

### Prerequisites

- A C++17-capable compiler (Clang or GCC)
- CMake 3.15+
- Git (only needed to clone the source)

### Build from source

```bash
git clone https://github.com/Naman1177/Voxel.git
cd Voxel
cmake -B build -S .
cmake --build build
```

This produces a `voxel` executable inside the `build/` directory. Optionally install it system-wide so you can run `voxel` from anywhere:

```bash
cmake --install build
```

### Homebrew (macOS/Linux, if the tap is available to you)

```bash
brew tap Naman1177/voxel
brew install voxel
```

### Verify it worked

```bash
voxel
```

Running `voxel` with no arguments prints the built-in command list — if you see that, you're ready to go.

---

## Quick start

```bash
# 1. Create a new project folder and step into it
mkdir my-project && cd my-project

# 2. Turn it into a Voxel repository
voxel init

# 3. Add some files, then tell Voxel to start tracking everything
echo "hello world" > notes.txt
voxel track

# 4. Take your first permanent snapshot (a "commit")
voxel commit -m "Initial commit"

# 5. Check what's changed since your last commit
voxel status

# 6. See your history
voxel log
```

From here you can branch off to try something new, snapshot your in-progress work before doing something risky, or hand the whole repository to a teammate over Wi-Fi. All of that — and every other command — is documented in detail, with every accepted argument form, in **[`Commands.md`](Commands.md)**.

---

## Feature tour

### 📁 Tracking & committing
`track` stages every file in your workspace (respecting `.voxelignore`); `commit` (or its alias `freeze`) permanently saves that state to history, either with a message you type or one an AI writes for you (`commit ai`). `status` shows you, file by file, what's untracked, modified, or already tracked and unchanged.

### 🌿 Branching
`branch` creates a new line of development from your current position; `switch` moves between branches; `where` tells you which one you're on right now. `diverge` is a power-move version of branching: it lets you spin up a brand-new branch starting from *any* historical commit, not just your current one.

### 🕰 Time travel
`restore` rewinds (or fast-forwards) your entire workspace to any commit — addressed by its hash, a shortened hash prefix, a branch name, `head`, or the current position — and even supports "N commits back from here" shorthand like `main-3` or `a1b2c3d4-2`. Every `restore` automatically takes a safety snapshot first.

### 💾 Snapshots (a single-slot stash)
`snapshot` captures your current, uncommitted workspace state into a private scratch area; `snapback` restores it; `snapclear` wipes it. Unlike commits, there's only ever one snapshot slot — it's meant for "let me quickly try something and be able to undo it," not long-term storage.

### 🗑 Bin & revive (soft delete)
`bin` moves a commit or branch out of your active history without destroying it — child commits are automatically re-linked to the parent above the removed commit, so your history stays connected. `revive` brings a binned commit or branch back.

### 🔍 Diffing & visual history
`diff` shows you a structural, block-aware comparison between any two commits, branches, or your live workspace — not just raw line noise. `log` prints a linear history; `graph` renders the full branching commit graph; `export` turns that graph into a shareable PDF.

### 🔀 Merging
`merge` brings the changes from one branch into another using a three-way merge (comparing both branches against their common ancestor), and drops you into an interactive prompt to resolve any conflicting sections by hand when needed.

### 🤖 AI assistance
`login` connects Voxel to an AI provider using your API key. Once connected: `commit ai` writes your commit message for you by looking at what actually changed; `review` has the AI read through a file (or your whole project) and suggest fixes, with the option to apply them directly; `diff -ai` explains a diff to you in plain language instead of (or alongside) the raw structural output.

### ☁️ Sharing without a server
`pack` bundles an entire repository — history, branches, ignore rules, everything — into a single portable `.vxlpack` file; `unpack` rebuilds a repository from one. For live sharing, `host` broadcasts your repository to your local network with a pairing token, `client` connects to a host and pulls the repository down, and `meshoff` shuts a hosting session down.

### 🪪 Identity
`who` shows your local Voxel identity (name, email, and a hardware-derived node ID used for peer-to-peer pairing) — set up automatically the first time you run `init`, and editable through `login`.

---

## Full command reference

This README is the map — for the exhaustive, example-by-example breakdown of **every command, every flag, and every accepted argument style** (yes, including every valid way to write a `restore` target), see:

➡️ **[`Commands.md`](Commands.md)**

---

## The `.voxelignore` file

`voxel init` automatically creates a `.voxelignore` file in your project root the first time you initialize a repository (it won't overwrite one you already have). Add one entry per line — a folder name, an exact path, or a file extension — for anything you never want Voxel to track. Lines starting with `#` are comments.

---

## Project layout (for the curious)

```
Voxel-main/
├── src/            # Implementation (Commands, repository, hashing, diffing, AI, cloud/mesh, etc.)
├── inc/            # Public headers for each module
├── third_party_lib/  # Bundled Zstandard, mbedTLS, dtl (diff library), and a JSON parser
├── CMakeLists.txt  # Build configuration
├── Benchmarks.md   # Head-to-head performance comparison against Git
└── Commands.md      # Full command reference (this is the file to read next)
```

---

## Notes & caveats

- Voxel stores repository data in `.voxel/` at the project root — don't hand-edit files inside it.
- Networking features (`host`/`client`/`meshoff`) are designed for trusted local networks (e.g., the same Wi-Fi/LAN) and are meant for quick, informal sharing rather than production hosting.
- AI features require you to run `voxel login` and supply your own API key; nothing is sent anywhere until you do.
- Voxel is under active development — command behavior may evolve between versions. Always check `Commands.md` in your installed version for the current behavior.