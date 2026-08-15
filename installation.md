# Installing Voxel

Voxel currently supports **macOS and Linux**. There's no single "right" way to install it — pick whichever method below fits how you like to manage tools.

| Method | Best for | Requires Homebrew? |
| --- | --- | --- |
| [Homebrew](#option-a--homebrew-recommended-for-mac--linuxbrew-users) | Easiest updates/uninstalls, if you already use Homebrew | Yes |
| [Install script (curl)](#option-b--the-curl-install-script) | Fastest one-liner, no Homebrew needed | No |
| [Build from source](#option-c--build-from-source-manually) | Contributors, or anyone who wants full control over the build | No |

---

## Prerequisites

What you need depends entirely on which install method you pick below.

### If you're using Homebrew (Option A)

| Requirement | Notes |
| --- | --- |
| Homebrew | Install from [brew.sh](https://brew.sh) if you don't already have it |

Homebrew handles the compiler and build for you — nothing else to prepare.

### If you're using the curl script or building manually (Option B or C)

| Requirement | Why |
| --- | --- |
| A C++17-capable compiler | `g++` on Linux, `clang++` on macOS (both auto-detected by the Makefile) |
| `make` | Drives the build |
| `git` | Used to clone the source (the curl script does this for you automatically) |
| `sudo` access | The final install step copies the `voxel` binary into `/usr/local/bin` |

You do **not** need to install Zstandard, mbedTLS, or any diff/JSON library yourself — Voxel bundles all of its dependencies inside `third_party_lib/` and builds them as part of the same `make` step.

---

## Option A — Homebrew (recommended for Mac/Linuxbrew users)

```bash
brew tap Naman1177/voxel
brew trust naman1177/voxel/voxel
brew install voxel
```

| Step | What it does |
| --- | --- |
| `brew tap` | Adds Voxel's tap so Homebrew knows where to find the formula |
| `brew trust` | Confirms you trust the tap's source before installing from it |
| `brew install` | Builds and installs `voxel`, placing it on your `PATH` automatically |

Updating later is a normal Homebrew update:

```bash
brew upgrade voxel
```

And removing it is just as clean:

```bash
brew uninstall voxel
```

---

## Option B — the curl install script

If you don't use Homebrew, this is the fastest path — one command, no manual cloning:

```bash
curl -sSL https://raw.githubusercontent.com/Naman1177/Voxel/main/install.sh | bash
```

**What this actually does, step by step**, so you're never running something opaque on your machine:

| Step | Action |
| --- | --- |
| 1 | Clones the Voxel source into a temporary folder (`voxel_temp_source`) |
| 2 | Runs `make` to compile the engine from that source |
| 3 | Runs `sudo make install`, copying the compiled binary to `/usr/local/bin/voxel` (this is the only step that needs your password) |
| 4 | Deletes the temporary source folder completely — your system is left with just the installed binary, no leftover source tree |

Because step 3 installs to `/usr/local/bin`, you'll be prompted for your password partway through — that's expected, not a sign anything went wrong.

If you'd rather read the script before running it, it's right here: [`install.sh`](install.sh).

---

## Option C — build from source manually

Best if you want to inspect or modify the code before installing, or you're setting up a contributor environment.

```bash
git clone https://github.com/Naman1177/Voxel.git
cd Voxel
make
sudo make install
```

| Command | What it does |
| --- | --- |
| `git clone ...` | Downloads the full source tree to your machine |
| `make` | Compiles Voxel and all bundled dependencies into a local `voxel` binary |
| `sudo make install` | Copies that binary to `/usr/local/bin` so `voxel` works from any directory, then cleans up the local copy |

If you'd rather keep the binary local instead of installing it system-wide, just stop after `make` — you'll find a `voxel` executable sitting in the project folder, runnable as `./voxel`.

To remove a manual install:

```bash
sudo rm /usr/local/bin/voxel
```

---

## Verify it worked

Regardless of which method you used, confirm the install the same way:

```bash
voxel
```

Running `voxel` with no arguments prints the built-in command list. If you see that, you're ready to go — head to [`Commands.md`](Commands.md) for everything Voxel can do, or [`README.md`](README.md) for how it all fits together.

---

## Troubleshooting

| Problem | Likely cause | Fix |
| --- | --- | --- |
| `command not found: voxel` after install | `/usr/local/bin` isn't on your `PATH` | Add `export PATH="/usr/local/bin:$PATH"` to your shell profile (`.zshrc`/`.bashrc`) and restart your terminal |
| `make` fails with compiler errors | No C++17-capable compiler installed | Install `g++` (Linux, e.g. `sudo apt install g++`) or Xcode Command Line Tools (macOS, `xcode-select --install`) |
| `sudo make install` asks for a password and hangs in a script/CI context | Installing to `/usr/local/bin` requires elevated privileges | Run interactively so you can enter your password, or install to a directory you own and add it to `PATH` manually |
| Homebrew says the formula isn't found | Tap wasn't added first | Run `brew tap Naman1177/voxel` before `brew install voxel` |

---

## Next steps

| I want to... | Go to |
| --- | --- |
| Understand how Voxel actually works | [`README.md`](README.md) |
| See every command and flag | [`Commands.md`](Commands.md) |
| See Voxel benchmarked against Git | [`Benchmarks.md`](Benchmarks.md) |