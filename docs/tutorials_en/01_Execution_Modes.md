# Execution Modes

This page explains four common ways to run Prajna: run Main, script mode (.prajnascript), REPL, and Jupyter, including scenarios, steps, and common issues.

## 1. Run a file with Main

- When: single-file or project entry, like C/C++ main
- Requirement: file contains `func Main(){ ... }`
- Command:
```bash
prajna exe examples/hello_world.prajna
```
- Success: expected output and normal exit
- Troubleshooting:
  - Command not found: add Prajna’s `bin` to PATH, or run from `build_release/install/bin`
  - File not found: check working directory; run from repo root

## 2. Script mode (.prajnascript)

- When: run statements in file order like a script, for quick demos/batch tasks
- Example: `examples/hello_world.prajnascript`
```prajna
"Hello World\n".PrintLine();
```
- Command:
```bash
prajna exe examples/hello_world.prajnascript
```
- Notes: no Main required; executes top-to-bottom

## 3. REPL

- When: interactive experiments, debugging expressions, learning the language
- Command:
```bash
prajna repl
```
- Use:
```text
"Hello from REPL".PrintLine();
```
- Tips:
  - Define variables/functions incrementally and try immediately
  - For larger code, put in a file and `prajna exe` it

## 4. Jupyter

- When: literate examples, teaching, interactive exploration (.ipynb)
- Install the kernel:
```bash
prajna jupyter --install
```
- Then open the provided notebook: `docs/notebooks/hello_world.ipynb`
- Tips:
  - Ensure Jupyter (jupyterlab/notebook) is installed
  - Containerized use: see `docs/notebooks/Dockerfile`

## 5. Which mode to choose?
- Programs/examples: run Main
- Quick serial demos/batch: script mode
- Interactive trials/debugging: REPL
- Teaching/visual docs: Jupyter

## 6. Platform and path tips
- Windows: with VS toolchain builds, enter the configured shell before building/running
- Linux/macOS: add `build_release/install/bin` to PATH when running local builds
- Paths are relative to current working directory; run from repo root for examples
