# AGENTS

This repository uses agent-assisted workflows. This file provides the repo-specific defaults that an agent should follow.

## Plan directory

- Write all plan documents into: `plans/`

## Primary instructions

- Follow: `.github/copilot-instructions.md`

## Build & test

- Configure: `cmake -S . -B build`
- Build: `cmake --build build`
- Run tests: `ctest --test-dir build`

## Notes

- `third_party_docs/` is a local documentation cache; only commit redistributable content and keep `third_party_docs/index.txt` up to date.
