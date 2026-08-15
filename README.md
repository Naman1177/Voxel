# Voxel

**A fast, local-first version control system — with AI built in.**

Voxel is Git-like in spirit, but rebuilt from scratch in C++ with its own storage format, its own diff/merge engine, and a friendlier vocabulary. This document explains **how Voxel actually works and how to think about it** — not the exact commands to type. For that, see [`Commands.md`](commands.md).

| I want to... | Go to |
| --- | --- |
| 🚀 Install Voxel | [`installation.md`](installation.md) |
| 📖 See every command & flag | [`Commands.md`](commands.md) |
| 📊 See it benchmarked against Git | [`Benchmarks.md`](Benchmarks.md) |

---

## The big idea

Every project you put under Voxel gets a hidden `.voxel/` folder at its root. That folder *is* the repository — your entire history, every branch, every saved version of every file, lives inside it. The rest of your project, the files you actually see and edit, is your **workspace**.

Voxel's whole job is managing the relationship between those two things.

| Term | What it means in Voxel |
| --- | --- |
| **Workspace** | The live files on disk, exactly as you're editing them. |
| **Index** | The list of files you've `track`ed and are ready to be included in the next commit. |
| **Commit** | A permanent, hashed snapshot of your tracked files, linked to the commit before it. |
| **Branch** | A named, moving pointer to a commit. |
| **HEAD** | A pointer to whichever branch (and commit) you currently have checked out. |
| **Snapshot** | A *temporary*, single-slot scratchpad of your workspace — not history. |
| **Bin** | A soft-delete area for commits/branches you removed but might want back. |

---

## How your work moves through Voxel

```
 WORKSPACE  →  TRACKED (staged)  →  COMMIT (permanent history)
 your files      "ready to save"      a saved point in time
```

| Stage | What's happening | How safe is it? |
| --- | --- | --- |
| **Workspace** | Files as you're editing them right now | Not safe — nothing is recorded yet |
| **Tracked / staged** | Voxel fingerprints each file and marks it "ready" | Still not permanent — a holding area, like a cart before checkout |
| **Commit** | Everything staged is sealed into a permanent, hash-linked point in history | Permanent — kept forever unless you deliberately bin it |

Every commit stores a link to its parent commit, so your whole history forms a connected chain, like frames in a film reel. You can write your own commit message, or let Voxel's AI read what actually changed and write one for you.

---

## Branches

A **branch** is a name pointing at a specific commit — a bookmark in history. Commit while "on" a branch, and the bookmark moves forward with you automatically.

- Keep separate lines of work going side by side (`main` for your real progress, a second branch for an experiment) without either one touching the other.
- Jump between branches any time — your workspace files update to match wherever you land.
- Want to branch off from somewhere *back* in history instead of from where you are right now? Voxel lets you pick any past commit as the starting point for a brand-new branch — no need to disturb your current work to explore an old fork in the road.

---

## Snapshot vs. Commit — the distinction that matters most

| | **Snapshot** | **Commit** |
| --- | --- | --- |
| What it is | A single, temporary scratch copy of your workspace | A permanent, hashed entry in your project's history |
| How many can exist | Only **one** at a time — a new one overwrites the old | Unlimited — every commit is kept |
| Where it lives | A private, throwaway holding area | The permanent commit chain (`log` / `graph`) |
| What it's for | "Remember exactly what this looked like right now, just in case" | "This is a real, named point in my project's story" |
| Gets cleared | Automatically, right after a successful commit | Never, unless you deliberately `bin` it |

Voxel leans on snapshots as an internal safety net too — see below.

---

## Restore: Voxel's time travel, and why it isn't just "checkout"

`restore` moves your **entire workspace** to match any point in history. It's the most powerful command in Voxel, and it behaves a little differently from Git's `checkout`/`switch` on purpose:

| | Git `checkout` / `switch` | Voxel `restore` |
| --- | --- | --- |
| What moves | Usually just HEAD (and files, if you ask) | Your entire workspace, always, in one motion |
| Detached state | Can leave you in a confusing "detached HEAD" | No detached state — you always land on a resolvable target |
| `head`/`HEAD` meaning | Points at "here, right now" | Always means the repository's **very first commit** — the beginning of your timeline, not your current position |
| Safety net | None built in — you can lose uncommitted work | Automatically takes a **snapshot before and after**, so the exact state you left is always one `snapback` away |
| Relative addressing | `HEAD~2`, `HEAD^` | The same idea, spelled out: `main-3`, `a1b2c3d4-2` — "N commits back from this resolved target" |
| Going too far back | Errors out | Safely stops at the root commit instead of failing |

You can target a `restore` at: the tip of your current branch, the very first commit, a full or shortened commit hash, a branch name, or any of those stepped back N commits. Because `restore` overwrites files to match the target, Voxel treats every restore as a small, blast-radius-limited operation — snapshot first, move second, snapshot again after — so a restore is always reversible with a single `snapback`, even though it's technically destructive to your working files.

---

## Removing history without destroying it

Sometimes a commit or branch is a dead end. Instead of forcing permanent deletion, Voxel has a **bin** — a soft-delete area.

| Action | What happens |
| --- | --- |
| `bin` a commit | Removed from active history; its child is automatically re-linked to its parent, so the chain stays unbroken |
| `bin` a branch | Removed from your branch list, data untouched |
| `revive` | Brings a binned commit or branch straight back |
| Root commit | Can never be binned — it's the foundation everything else is built on |

---

## The scope-aware diff engine

Most VCS diffs compare files **line by line**, which is why a single reformatted brace can make a diff look like the whole function changed. Voxel's `diff` engine works differently: it first breaks each file into **logical scope blocks** — functions, classes, structs, whatever a scope boundary looks like in that file — and then compares scope-to-scope, not line-to-line.

| Step | What Voxel does |
| --- | --- |
| 1. Segment | Splits old and new file versions into scope blocks (e.g. `function start_engine() { ... }`) |
| 2. Exact match | Matches blocks that share the same scope name between versions |
| 3. Fuzzy match | Catches **renamed** scopes by comparing content similarity, even when the header changed |
| 4. Classify | Labels each remaining block `MODIFIED`, `ADDED`, or `DELETED` |
| 5. Present | Shows you the diff grouped by scope, not by raw line number |

The upshot: `diff` reads more like *"this function changed, this one was added, this one was deleted"* than a flood of `+`/`-` lines. Add `-ai` to any `diff` and Voxel's AI provider turns that scope-grouped structural diff into a plain-English explanation.

---

## Merging: three-way comparison inside a sandbox

`merge` doesn't touch your real files until it's sure it's safe to. Here's the actual flow:

| Step | What happens |
| --- | --- |
| 1. Common ancestor | Voxel finds the shared commit both branches grew out of |
| 2. Three-way compare | Each side's changes are compared against that shared ancestor, not against each other directly |
| 3. Sandbox | All merge output — auto-merged files, conflicting sections, everything — is written into a **temporary sandbox area**, never straight into your workspace |
| 4. Conflict resolution | If both sides touched the same section, Voxel pauses and asks you to resolve it interactively |
| 5. Apply | Only once the sandbox merge is fully resolved does Voxel copy the result into your actual workspace |

This sandbox step is the safety net: if a merge goes badly or you back out midway, your real workspace was never touched in the first place — nothing to undo, because nothing landed yet.

When a conflict comes up, you choose per section:

| Choice | Result |
| --- | --- |
| **Ours** | Keep your current branch's version of that section |
| **Theirs** | Keep the incoming branch's version |
| **Both** | Keep yours, then theirs, back to back |

Voxel refuses to merge a branch into itself, and refuses to run without a resolvable current branch.

---

## How Voxel treats different kinds of files

Voxel inspects every file it tracks and decides how to store it — you never have to tell it which is which:

| File type | How Voxel stores it | Why |
| --- | --- | --- |
| Text (code, config, docs, anything readable) | Compressed, and diffed scope-by-scope | Meaningful diffs, small footprint |
| Binary/media (images, video, audio, 3D models, archives, PDFs) | Stored raw, byte-for-byte | Line diffing a `.png` or `.mp4` is meaningless — Voxel doesn't waste time trying |

---

## `.voxelignore`

Created automatically the first time you initialize a repository (never overwritten if one already exists). Plain text, one rule per line:

| You can list | Effect |
| --- | --- |
| A folder name | Skips everything inside it |
| An exact file path | Skips just that file |
| A file extension | Skips every file of that type, anywhere in the project |
| A line starting with `#` | Treated as a comment, ignored |

Every workspace-scanning operation — `status`, `track`, `snapshot`, and more — respects this file automatically, every time.

---

## Your Voxel identity (hardware ID)

The first time you ever set up a repository on a machine, Voxel silently generates a permanent identity for that computer, derived from characteristics of the hardware itself. It stays the same across every project on that machine and can't be spoofed just by copying files around. `who` shows it to you any time.

Its one job: when you share a repository directly with another computer (below), both machines can recognize and trust each other without any central server or account system in the middle. You never have to manage it — it just works quietly in the background.

---

## Sharing a project without a server

| Method | How it works | Best for |
| --- | --- | --- |
| **Pack / unpack** | Bundles your whole repository — full history, branches, ignore rules — into one portable file | Emailing, archiving, dropping in shared storage |
| **Host / client** | One machine broadcasts the repository live on the local network with a short-lived pairing token; the other connects and pulls it directly | Quick, trusted handoffs on the same Wi-Fi/LAN |

No internet upload, no third-party server, no account required, either way.

---

## AI features

Voxel can optionally connect to an AI provider:

| Feature | What it does |
| --- | --- |
| `commit ai` | Writes your commit message by reading what actually changed |
| `review` | Reads your code (one file or the whole project) and suggests fixes, with the option to apply them |
| `diff -ai` | Explains a scope-aware diff in plain English |

**By default, Voxel expects a Google Gemini API key.** You bring your own key — Voxel doesn't ship with one, and nothing leaves your machine until you connect a provider and run an AI command yourself. A handful of other providers are supported too if you'd rather use something else (full list in [`Commands.md`](Commands.md)). Every non-AI feature works exactly the same with no provider connected at all — AI is entirely optional on top of a fully functional VCS.

---

## What's keeping this fast and small

| Component | Role |
| --- | --- |
| **Zstandard (zstd)** | Compresses your text files quickly and losslessly — how history holds many versions without `.voxel/` ballooning |
| **mbedTLS** | Generates the SHA-256 fingerprint for every file and commit — instant "has this changed?" checks and unique, permanent commit IDs |

Both are bundled inside Voxel — nothing extra to install.

---

## A couple of things worth knowing

- Nothing inside `.voxel/` is meant to be edited by hand.
- `host`/`client`/`meshoff` are built for trusted local networks and quick handoffs, not production hosting.
- Voxel is under active development — [`Commands.md`](Commands.md) in your installed version is always the source of truth for current behavior.

---

## Credits

**Voxel was designed and written by Naman Malhotra.**

If Voxel's fast, or the AI commit messages saved you from writing "fix stuff" for the hundredth time, consider dropping a ⭐ on the repo — it genuinely helps.
