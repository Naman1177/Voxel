# Voxel — Full Command Reference

This document explains **every Voxel command** — what it actually does under the hood, in plain language, along with **every accepted way of calling it**. It's written for everyday use, not as a technical spec — you don't need to know how Voxel works internally to use anything below. For the conceptual, "how does this all fit together" explanation, see [`README.md`](README.md).

General usage pattern:

```bash
voxel <command> [arguments]
```

Run `voxel` with no arguments at any time to see the quick built-in command list.

---

## Table of contents

- [init](#voxel-init)
- [status](#voxel-status)
- [track](#voxel-track)
- [commit](#voxel-commit)
- [freeze](#voxel-freeze)
- [branch](#voxel-branch)
- [switch](#voxel-switch)
- [where](#voxel-where)
- [diverge](#voxel-diverge)
- [log](#voxel-log)
- [graph](#voxel-graph)
- [export](#voxel-export)
- [restore](#voxel-restore)
- [snapshot](#voxel-snapshot)
- [snapback](#voxel-snapback)
- [snapclear](#voxel-snapclear)
- [diff](#voxel-diff)
- [merge](#voxel-merge)
- [bin](#voxel-bin)
- [revive](#voxel-revive)
- [login](#voxel-login)
- [change_model](#voxel-change_model)
- [review](#voxel-review)
- [who](#voxel-who)
- [pack](#voxel-pack)
- [unpack](#voxel-unpack)
- [host](#voxel-host)
- [client](#voxel-client)
- [pull](#voxel-pull)
- [meshoff](#voxel-meshoff)
- [The `.voxelignore` file](#the-voxelignore-file)
- [How Voxel resolves a "target"](#how-voxel-resolves-a-target-hashesbranchesheadn-back)

---

## `voxel init`

**What it does:** Turns the current folder into a Voxel repository — this is the command that makes everything else possible.

Concretely, it creates a hidden `.voxel/` folder (holding object storage for compressed/raw file data, branch references, and the snapshot slot), sets your starting branch to `main`, and generates a `.voxelignore` template file in your project root if one doesn't already exist. The very first time you ever run `init` on a machine, Voxel also silently generates a permanent, hardware-derived node ID for that computer — stored once in your home folder and reused by every future repository on that machine. This is what powers `who` and the peer-to-peer mesh features (`host`/`client`/`pull`).

**Usage:**
```bash
voxel init
```

| Form | Result |
| --- | --- |
| `voxel init` | Creates `.voxel/`, the `main` branch, and a default `.voxelignore` |

- Takes no arguments.
- Fails safely with an error if a Voxel repository already exists in the current folder — it will never overwrite or reset an existing one.

---

## `voxel status`

**What it does:** Scans your workspace and shows you the state of every file, compared against what's currently tracked (staged). This is your "what's changed" check-in, meant to be run often and freely — it never modifies anything.

Each file is labeled:

| Label | Meaning |
| --- | --- |
| **`[Untracked]`** | Voxel doesn't know about this file yet — run `track` to start tracking it |
| **`[Modified]`** | The file is tracked, but its content has changed since it was last tracked |
| **`[Tracked]`** | The file is tracked and matches exactly what's staged |

Each entry also prints the file's current content hash, which you can use as a quick "did this really change" fingerprint.

**Usage:**
```bash
voxel status
```

- Takes no arguments.
- If the workspace has no trackable files at all, it tells you the workspace is empty instead of printing an empty list.

---

## `voxel track`

**What it does:** Stages files so they'll be included the next time you `commit`. This is Voxel's equivalent of Git's `git add`, except it always tracks **everything** in the workspace in one go (any file or folder listed in `.voxelignore` is skipped automatically).

For each file, Voxel decides how to store it: recognized binary/media formats (video, audio, images, 3D models, archives, PDFs — see the full list below) are copied as raw bytes, while everything else (source code, text, config files, etc.) is compressed with zstd.

**Usage:**
```bash
voxel track
```

| Form | Result |
| --- | --- |
| `voxel track` | Tracks every file in the workspace not excluded by `.voxelignore` |

- Takes no arguments — it always tracks the whole workspace (minus ignored paths) at once.
- Safe to run repeatedly; re-tracking just refreshes the staged hash for anything that changed.

**File types stored raw instead of compressed:**
`.mp4 .mov .avi .mkv .flv .m4v .webm .mp3 .wav .flac .ogg .aac .m4a .jpg .jpeg .png .gif .tiff .bmp .heic .ico .webp .blend .blender .fbx .obj .stl .dae .3ds .glb .gltf .psd .zip .rar .7z .pdf`

---

## `voxel commit`

**What it does:** Permanently saves the currently tracked (staged) state of your project as a new point in history, linked to the commit that came before it. This is the core "save" operation in Voxel — everything before this point (workspace edits, staging) was provisional; a commit is not.

After a successful commit, Voxel automatically clears any active snapshot (see [`snapshot`](#voxel-snapshot)), since your work is now safely committed and the temporary safety net is no longer needed.

**Usage — every accepted form:**

| Form | Result |
| --- | --- |
| `voxel commit` | Interactive — Voxel prompts you to type a message |
| `voxel commit "your message"` | Message supplied directly as an argument |
| `voxel commit -m "your message"` | Message supplied directly as an argument |
| `voxel commit ai` | AI writes the commit message for you (requires `voxel login` first) |

Notes:
- If you run `voxel commit` with no message and just press Enter without typing anything, the commit is aborted (empty messages aren't allowed).
- `voxel commit ai` looks at what actually changed in your tracked files and generates a descriptive message automatically, then commits with it.
- You must have run `voxel track` at least once (with something actually staged) before you can commit — Voxel will tell you if there's nothing tracked yet.

---

## `voxel freeze`

**What it does:** Identical to `commit`, except it automatically runs `track` for you first. It's a convenience shortcut for "track everything and commit it in one step" — useful when you know you want to save absolutely everything currently in your workspace without checking `status` first.

**Usage — every accepted form:**

| Form | Result |
| --- | --- |
| `voxel freeze` | Interactive prompt for a message |
| `voxel freeze "your message"` | Message supplied directly |
| `voxel freeze -m "your message"` | Message supplied directly |
| `voxel freeze ai` | AI writes the commit message for you |

All the same rules and message forms as `commit` apply — the only difference is `freeze` stages every file first, so you don't need to run `track` separately.

---

## `voxel branch`

**What it does:** Creates a new branch — a new named line of development — starting from wherever you currently are in history. It does **not** switch you onto the new branch; you stay on your current one until you explicitly `switch`.

**Usage — every accepted form:**

| Form | Result |
| --- | --- |
| `voxel branch` | Interactive — Voxel prompts you for a branch name |
| `voxel branch feature-login` | Name supplied directly |

Notes:
- Branch names cannot contain spaces.
- Branch names cannot be empty.
- If you want to branch off from a specific *past* commit instead of your current position, use [`diverge`](#voxel-diverge) instead.

---

## `voxel switch`

**What it does:** Moves you onto a different branch, updating your workspace files to match that branch's latest commit. This is a direct jump — it doesn't take a safety snapshot first the way `restore` does, so make sure your current changes are committed or snapshotted if you want to keep them.

**Usage:**
```bash
voxel switch feature-login
```

| Form | Result |
| --- | --- |
| `voxel switch <branch-name>` | Moves HEAD and your workspace files onto `<branch-name>`'s latest commit |

- Requires exactly one argument: the target branch name.
- Errors out clearly if you don't provide a branch name.

---

## `voxel where`

**What it does:** Tells you which branch you currently have checked out. Handy as a quick "am I where I think I am" sanity check before you commit or restore something.

**Usage:**
```bash
voxel where
```

- Takes no arguments.

---

## `voxel diverge`

**What it does:** A more powerful form of branching — instead of always branching from your current position (like `branch` does), `diverge` lets you create a brand-new branch starting from **any specific commit in history**, identified by its full hash. This is useful for exploring "what if I'd taken a different direction from this old point" without disturbing your current work. After creating the branch, Voxel automatically switches you onto it and restores your workspace to that commit's state.

**Usage — every accepted form:**

| Form | Result |
| --- | --- |
| `voxel diverge <full-64-char-commit-hash>` | Interactive — Voxel prompts: "Target verified. Enter new branch name:" |
| `voxel diverge <full-64-char-commit-hash> <new-branch-name>` | Branch name supplied directly |

Examples:
```bash
voxel diverge a1b2c3d4e5f6...64chars experiment-branch
voxel diverge a1b2c3d4e5f6...64chars
```

Notes:
- The commit hash **must be the full 64-character hash** (not a shortened prefix) — Voxel will reject anything else as invalid.
- The commit must actually exist in the repository.
- The new branch name must not already be in use.

---

## `voxel log`

**What it does:** Prints a simple, linear view of your commit history — one entry per commit, in order. Use this when you want a quick scroll through what's happened, without the visual branching structure `graph` shows.

**Usage:**
```bash
voxel log
```

- Takes no arguments.

---

## `voxel graph`

**What it does:** Renders the *full* commit history as a branching graph in your terminal, showing how branches split off from and merge back into each other — not just a flat list like `log`. Use this when you want to see the actual shape of your project's history: where experiments forked off, where they merged back, and where the bin removed something.

**Usage:**
```bash
voxel graph
```

- Takes no arguments.

---

## `voxel export`

**What it does:** Exports the same commit graph shown by `voxel graph` to a PDF file, so you can share, print, or archive your project's history outside the terminal — useful for documentation, code reviews, or presenting project progress to people who don't have Voxel installed.

**Usage:**
```bash
voxel export
```

- Takes no arguments.

---

## `voxel restore`

**What it does:** The time-travel command. Rewinds (or moves) your entire workspace to match the state of any point in your commit history. This is a *destructive* operation on your working files (they get overwritten to match the target), which is why Voxel automatically takes a fresh [`snapshot`](#voxel-snapshot) of your current state immediately before and after restoring — so you always have one level of "undo" available via `snapback` if you change your mind.

**Usage — every accepted form:**

```bash
voxel restore                 # defaults to the tip of your CURRENT branch (effectively "reset to latest")
voxel restore head            # restore to the very first commit ever made (the repository's root/genesis commit)
voxel restore HEAD            # same as above — case-insensitive
voxel restore <hash>          # restore to an exact commit, by full hash
voxel restore <short-hash>    # restore to a commit matched by a unique hash PREFIX (e.g. "a1b2c3d4")
voxel restore <branch-name>   # restore to the latest commit ON that branch
voxel restore <target>-<N>    # restore to N commits BEFORE the resolved target (see below)
```

**Every accepted "target expression" in detail:**

| You type | Voxel restores to |
| --- | --- |
| *(nothing)* | The latest commit on your **current branch** |
| `head` or `HEAD` | The repository's very first commit (its permanent root/genesis point) — **note:** in Voxel this means the *beginning* of history, not "your current position" the way Git's `HEAD` does |
| A full 64-character commit hash | That exact commit |
| A short, unique hash prefix (e.g. `a1b2c3d4`) | The first commit whose hash starts with that prefix |
| A branch name (e.g. `feature-login`) | The latest commit on that branch |
| `<anything>-<number>`, e.g. `main-2`, `a1b2c3d4-3`, `head-1` | Resolves `<anything>` normally first (using the rules above), then walks **backward `<number>` parent commits** from there. If you try to walk back further than history goes, it safely stops at the root commit. |

Examples:
```bash
voxel restore                 # back to where your current branch's history currently sits
voxel restore main            # jump to the tip of the "main" branch
voxel restore main-1          # jump to ONE commit before the tip of "main" (its parent)
voxel restore a1b2c3d4        # jump to the commit whose hash starts with a1b2c3d4
voxel restore a1b2c3d4-2      # jump to TWO commits before that one
voxel restore head            # jump all the way back to the project's very first commit
```

Notes:
- If the target expression can't be resolved to any known commit or branch, Voxel prints a clear error and changes nothing.
- Every `restore` refreshes the safety snapshot, so `snapback` always reflects the state right before your most recent restore.

---

## `voxel snapshot`

**What it does:** Takes a quick, temporary backup of your current, *uncommitted* workspace files — a single-slot scratchpad, separate entirely from your permanent commit history. Think of it as "let me remember exactly what this looked like right now, in case I want it back in a minute" — not a replacement for committing.

**Usage:**
```bash
voxel snapshot
```

| Form | Result |
| --- | --- |
| `voxel snapshot` | Overwrites the single snapshot slot with your current workspace state |

- Takes no arguments.
- There is only ever **one** snapshot slot at a time — taking a new snapshot silently overwrites whatever was there before.
- Automatically taken for you before/after every `restore`, so you always have a way back to where you just were.

---

## `voxel snapback`

**What it does:** Restores your workspace files from the current snapshot slot — undoing back to whatever state `snapshot` last captured. This is your one-level "undo" for anything that isn't a commit.

**Usage:**
```bash
voxel snapback
```

- Takes no arguments.
- If no snapshot has been taken yet (or it's already been cleared), Voxel tells you there's nothing to roll back to.

---

## `voxel snapclear`

**What it does:** Empties the current snapshot slot, freeing it up so a new one can be taken. Voxel also does this automatically after a successful `commit`/`freeze`, since a committed change no longer needs a temporary safety net.

**Usage:**
```bash
voxel snapclear
```

- Takes no arguments.

---

## `voxel diff`

**What it does:** Compares two versions of your project (or a single pair of files) and shows you a **structural, scope-aware** diff — grouped by function/block rather than raw line-by-line noise — so you can actually read what functionally changed instead of scanning a wall of `+`/`-` lines.

**Usage — every accepted form:**
```bash
voxel diff                          # compare your live WORKSPACE against the last commit on your current branch
voxel diff head                     # compare your live WORKSPACE against the repository's very first (root) commit
voxel diff <branch-name>            # compare your live WORKSPACE against that branch's root/base commit
voxel diff <target-A> <target-B>    # compare two arbitrary targets against each other (branches, hashes, etc.)
voxel diff <fileA-path> <fileB-path>  # compare two literal FILES on disk directly, by path (bypasses commit history entirely)

# AI-explained diffs — add -ai / --ai to any of the forms above:
voxel diff -ai
voxel diff -ai <branch-name>
voxel diff -ai <target-A> <target-B>
voxel diff -ai <fileA-path> <fileB-path>
```

| Form | Compares |
| --- | --- |
| `voxel diff` | Workspace vs. last commit on current branch |
| `voxel diff head` | Workspace vs. the root (very first) commit |
| `voxel diff <branch>` | Workspace vs. that branch's base commit |
| `voxel diff <A> <B>` | Any two targets against each other (branches, hashes, `head`, etc.) |
| `voxel diff <fileA> <fileB>` | Two literal files on disk, ignoring commit history entirely |
| `voxel diff -ai [...]` | Any of the above, plus a plain-English AI explanation of the change |

Notes:
- If both arguments you give are real, existing file paths on disk, Voxel treats it as a direct file-to-file comparison instead of a history lookup.
- `-ai`/`--ai` routes the same comparison through your connected AI provider (see `voxel login`) to get a plain-English explanation of the change instead of (or alongside) the raw structural diff.
- Target resolution for branches/hashes follows the same general rules described in [How Voxel resolves a "target"](#how-voxel-resolves-a-target-hashesbranchesheadn-back).

---

## `voxel merge`

**What it does:** Combines the changes from one branch into another. Voxel finds the common ancestor commit both branches share, compares each branch's changes against that shared point (a three-way comparison), and assembles the result inside a temporary sandbox area first — your real workspace files are only overwritten once the merge is fully resolved, so a merge that goes wrong never leaves your working files half-changed. If Voxel finds overlapping changes to the same part of a file, it pauses and asks you, interactively, how to resolve each conflict before anything is applied.

**Usage — every accepted form:**
```bash
voxel merge                     # merge "main" INTO your current branch
voxel merge feature-login       # merge "feature-login" INTO your current branch
voxel merge feature-login main  # merge "feature-login" INTO "main", regardless of what branch you're currently on
```

| Form | Result |
| --- | --- |
| `voxel merge` | Merges `main` into whatever branch you're currently standing on |
| `voxel merge <source>` | Merges `<source>` into your current branch |
| `voxel merge <source> <target>` | Merges `<source>` into `<target>` explicitly, regardless of your active branch |

**When a conflict is found**, Voxel shows you the conflicting file and asks you to choose, per conflicting section:

| Choice | Result |
| --- | --- |
| **[1] Keep OURS** | Keep your current branch's version of that section |
| **[2] Keep THEIRS** | Keep the incoming branch's version of that section |
| **[3] Keep BOTH** | Keep yours first, then theirs, both included |

Notes:
- Voxel refuses to merge a branch into itself ("Already up to date").
- Voxel refuses to run if it can't determine a valid current branch (i.e., you're not in a proper repository).

---

## `voxel bin`

**What it does:** "Soft deletes" a commit or a branch — removes it from your active history/branch list without destroying the underlying data, so it can be brought back later with [`revive`](#voxel-revive). Binning a commit automatically re-connects the history around it (its child commit is re-linked to point at its parent), so your commit graph stays intact instead of getting a broken link.

**Usage:**
```bash
voxel bin <commit-hash>     # bin a specific commit, by its full hash
voxel bin <branch-name>     # bin an entire branch
```

| Form | Result |
| --- | --- |
| `voxel bin <64-char-hash>` | Bins that specific commit and re-links its child to its parent |
| `voxel bin <branch-name>` | Bins the entire branch |

- Takes exactly one argument, which Voxel auto-detects as either a commit hash (if it's 64 characters long) or a branch name (anything else).
- You **cannot** bin the repository's root/genesis commit — it's the foundation everything else is built on.
- You **cannot** bin the `main` branch.
- If you bin the branch you're currently standing on, Voxel warns you to switch to another branch before continuing to work.
- If any branch's tip currently points at a commit you bin, that branch is automatically updated to point at the commit's parent instead, so it keeps working.

---

## `voxel revive`

**What it does:** The reverse of `bin` — restores a previously binned commit or branch back into the active repository, exactly as it was when it was removed.

**Usage:**
```bash
voxel revive <commit-hash>   # revive a specific binned commit, by its full hash
voxel revive <branch-name>   # revive a binned branch
```

| Form | Result |
| --- | --- |
| `voxel revive <64-char-hash>` | Restores that commit back into active history |
| `voxel revive <branch-name>` | Restores that branch back into your active branch list |

- Takes exactly one argument, auto-detected the same way as `bin` (64 characters = commit hash, otherwise a branch name).
- Fails clearly if the target isn't actually in the bin.
- Fails clearly if reviving a branch would collide with a branch name that already exists in the active repository.

---

## `voxel login`

**What it does:** Connects Voxel's AI-powered features (`commit ai`, `review`, `diff -ai`) to an AI provider using your own API key. You'll be prompted for your username, email, and API key; Voxel saves this configuration locally inside your repository so future AI commands can use it without asking again. **By default, Voxel expects a Google Gemini API key** — a handful of other providers (Claude, OpenAI, Ollama, Groq, DeepSeek) are also supported if you'd rather use one of those.

**Usage:**
```bash
voxel login
```

- Takes no arguments — it's fully interactive and will prompt you for the needed details.
- Requires that you've already run `voxel init` in this project (it upgrades an existing local configuration rather than creating one from nothing).
- Nothing is sent to any AI provider until you actually use an AI-powered command (`commit ai`, `review`, `diff -ai`).

---

## `voxel change_model`

**What it does:** Reconfigures your AI setup after the fact — switch to a different provider, model, or API key without re-running the full `login` flow. Useful if you started with one provider and want to switch (e.g. from Gemini to a local Ollama model), or you just want to bump the model version you're using.

**Usage:**
```bash
voxel change_model
```

| Step | What you're asked |
| --- | --- |
| 1 | Pick a provider: Gemini, Claude, OpenAI, Ollama, Groq, or DeepSeek |
| 2 | Enter the exact model name (e.g. `gemini-2.5-flash`, `claude-3-5-sonnet`, `gpt-4o`) |
| 3 | Enter your API key for that provider — skipped automatically if you picked Ollama, since it runs locally |

- Takes no arguments — fully interactive.
- Requires an existing Voxel repository (`voxel init` first).
- Overwrites your previous AI configuration with whatever you enter here.

---

## `voxel review`

**What it does:** Has your connected AI provider read through your code and give feedback — bugs, improvements, style notes — and can optionally apply suggested fixes for you. Requires `voxel login` to have been run first.

**Usage — every accepted form:**
```bash
voxel review                          # review whichever file you're currently "active" in
voxel review myfile.py                # review a specific file, by path
voxel review all                      # review every file currently in the workspace
voxel review "focus on error handling"   # review your active file, with an extra note/instruction for the AI
voxel review myfile.py "check for security issues"   # review a specific file, WITH a note
voxel review all "check for security issues"          # review everything, WITH a note
```

| Form | Result |
| --- | --- |
| `voxel review` | Reviews your current active file |
| `voxel review <file>` | Reviews a specific file by path |
| `voxel review all` | Reviews every file in the workspace |
| `voxel review "<note>"` | Reviews your active file, with your note attached as guidance for the AI |
| `voxel review <file> "<note>"` | Reviews a specific file, with your note |
| `voxel review all "<note>"` | Reviews everything, with your note |

How Voxel figures out what you meant with a single extra word after `review`: if it's the literal word `all`, it reviews the whole workspace; if it matches a real file path on disk, it reviews that file; otherwise, it's treated as a free-text note attached to your current active file's review.

---

## `voxel who`

**What it does:** Prints your local Voxel identity: the username/email currently saved in this repository's config (set via `login`), plus a permanent, hardware-derived node ID unique to your machine. That node ID is what other machines see when you `host`, `client`, or `pull` in a peer-to-peer session.

**Usage:**
```bash
voxel who
```

- Takes no arguments.
- If you haven't run `voxel init` (which generates your machine's node ID) or `voxel login` (which sets your name/email), some fields will show as not set.

---

## `voxel pack`

**What it does:** Bundles your **entire** repository — full commit history, all branches, snapshots folder structure, and your `.voxelignore` rules — into a single portable file with a `.vxlpack` extension. This is the easiest way to hand off, back up, or archive a whole project in one file. Your personal AI configuration (API keys, provider, connection status) is deliberately **not** included — the packed file ships with a fresh, disconnected default config for privacy.

**Usage:**
```bash
voxel pack                # names the output file after your current folder, e.g. "my-project.vxlpack"
voxel pack backup-name     # names the output file yourself, e.g. "backup-name.vxlpack"
```

| Form | Result |
| --- | --- |
| `voxel pack` | Output named after the current folder |
| `voxel pack <name>` | Output named `<name>.vxlpack` |

---

## `voxel unpack`

**What it does:** Rebuilds a full repository from a `.vxlpack` file previously created by `pack`.

**Usage:**
```bash
voxel unpack my-project.vxlpack
```

| Form | Result |
| --- | --- |
| `voxel unpack <file.vxlpack>` | Rebuilds the full repository (history, branches, ignore rules) from that file |

- Requires exactly one argument: the path/filename of the `.vxlpack` file to unpack.

---

## `voxel host`

**What it does:** Starts a local peer-to-peer sharing session for your current repository — packages it up in the background and broadcasts it on your local network (Wi-Fi/LAN) so someone else running `voxel client` nearby can discover and download it directly, with no server or internet upload involved. Voxel prints a short pairing token when the session starts, which is valid for 5 minutes.

**Usage:**
```bash
voxel host
```

- Takes no arguments to start; the pairing token is generated and shown automatically.
- Refuses to start a second hosting session if one is already active on this machine — run `voxel meshoff` first.
- Keep the terminal running while you want the session to stay available; end it any time with `voxel meshoff` (or by stopping the process).

---

## `voxel client`

**What it does:** Connects to someone else's active `voxel host` session on the same local network and pairs your machine with theirs. Pairing alone doesn't transfer any data yet — once paired, use [`voxel pull`](#voxel-pull) to actually bring their repository (or a specific branch of it) down to your machine.

**Usage:**
```bash
voxel client <pairing-token>         # connect using the token shown by their "voxel host" session (auto-discovers them on the network)
voxel client --ip <host-ip-address>  # connect directly to a known IP address instead of discovering by token
```

| Form | Result |
| --- | --- |
| `voxel client <token>` | Auto-discovers and pairs with the host that generated that token |
| `voxel client --ip <ip>` | Pairs directly with a known host IP, skipping token discovery |

- You must provide either a pairing token or an `--ip` address — Voxel will error if you give neither.
- Refuses to run if your own machine is currently hosting a session (you can't host and client at the same time).
- A successful pairing is what makes `voxel pull` possible afterward.

---

## `voxel pull`

**What it does:** Once you've paired with a host using `voxel client`, `pull` is the command that actually **brings their changes down to your machine**. It connects to the paired host over the network, requests the branch you asked for (or everything, by default), verifies the transfer wasn't corrupted, and then merges the incoming code into your current branch using Voxel's normal three-way, sandboxed merge engine — the same safety process used by [`merge`](#voxel-merge). Your workspace files are only updated once that merge succeeds.

Concretely, here's what happens when you run it:

| Step | What Voxel does |
| --- | --- |
| 1. Connect | Uses the pairing info saved by `voxel client` to reach the host over the local network |
| 2. Request | Asks the host for a specific branch, or `ALL` branches if you didn't specify one |
| 3. Verify | Checks the transferred payload's hash to confirm nothing was corrupted in transit |
| 4. Isolate | Extracts the incoming data into a temporary sandbox — never directly into your live repository |
| 5. Merge | Runs the incoming branch through the same three-way merge engine as `voxel merge`, prompting you to resolve any conflicts |
| 6. Apply | Only after the merge succeeds does Voxel update your actual workspace files to reflect the pulled changes |
| 7. Clean up | Removes the temporary transfer files and sandbox once done |

**Usage:**
```bash
voxel pull                # pulls EVERY branch from the paired host, merging the active one into your current branch
voxel pull feature-login  # pulls just the "feature-login" branch from the paired host
```

| Form | Result |
| --- | --- |
| `voxel pull` | Pulls all branches from the host; the host's active branch is merged into yours, other branches are synced as new local pointers |
| `voxel pull <branch-name>` | Pulls just that one branch from the host and merges it into your current branch |

Notes:
- You must have paired with a host first via `voxel client <token>` (or `--ip`) — `pull` will error out with "No host connection found" otherwise.
- Because `pull` routes through the same sandboxed merge engine as `voxel merge`, you'll be asked to resolve any conflicts the same way (ours/theirs/both) before anything lands on disk.
- The host's local AI configuration and `.voxelignore` are never pulled — only repository history and code.

---

## `voxel meshoff`

**What it does:** Cleanly shuts down your active `voxel host` session — stops broadcasting, closes the network ports, and removes the temporary hosting files.

**Usage:**
```bash
voxel meshoff
```

- Takes no arguments.
- Safe to run even if no session is currently active — it will just tell you there's nothing to shut down.

---

## The `.voxelignore` file

Created automatically the first time you run `voxel init` (and never overwritten if you already have one). It controls what `track`/`freeze`/`status`/`snapshot` skip entirely.

| You can list | Effect |
| --- | --- |
| A folder name | Skipped entirely, including everything inside it |
| An exact file path | That one file is skipped |
| A file extension | Every file of that type, anywhere in the project, is skipped |
| A line starting with `#` | Treated as a comment and ignored |

- One entry per line.

---

## How Voxel resolves a "target" (hashes/branches/head/N-back)

Several commands — `restore`, `diff`, `diverge` — accept a "target" describing a point in your history. Where supported, targets can be:

| Target form | Resolves to |
| --- | --- |
| A full commit hash | The exact 64-character identifier |
| A short hash prefix | As few characters as needed to uniquely identify one commit (e.g. `a1b2c3d4`) |
| A branch name | That branch's current latest commit |
| `head` / `HEAD` | The repository's very first (root/genesis) commit — different from Git's `HEAD`, which points at "here"; in Voxel, `head` always means the *beginning* of the timeline |
| Empty/nothing | Your current branch's latest commit (i.e., "where I already am") |
| A trailing `-N` | After resolving the part before the dash using any of the rules above, step backward `N` commits (through parent links) from that point. For example `feature-3` means "3 commits before the tip of `feature`," and `a1b2c3-1` means "the direct parent of commit `a1b2c3...`." Stepping back further than the start of history safely stops at the root commit instead of erroring. |

Not every command that takes a target supports every one of these forms identically (for example, `diverge` requires a *full* hash, not a prefix) — see each command's own section above for the exact list it accepts.

---

## Next steps

| I want to... | Go to |
| --- | --- |
| Understand how Voxel actually works | [`README.md`](README.md) |
| Install Voxel | [`installation.md`](installation.md) |
| See Voxel benchmarked against Git | [`Benchmarks.md`](Benchmarks.md) |