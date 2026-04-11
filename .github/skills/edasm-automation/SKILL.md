---
name: edasm-automation
description: 'Automate ProDOS EDASM (Apple II assembler) jobs via emulator. Use for: bootstrapping EDASM inputs from GitHub, running assembly jobs with source files, converting output listings to Linux text format. Trigger phrases: "run EDASM", "bootstrap EDASM", "assemble with EDASM", "EDASM job".'
---

# EDASM Automation Skill

Automate Apple II ProDOS EDASM (assembler) execution through the ProDOS8Emu emulator, with built-in support for fetching dependencies and converting outputs.

## When to Use

- **Bootstrap inputs**: Set up a fresh EDASM environment with disk image, ROM, and config
- **Run assembly jobs**: Assemble source code via the emulator and collect outputs
- **Convert outputs**: Export ProDOS text files (listings) to Linux LF format for host tools

## Complete Workflow

### Phase 1: Bootstrap EDASM Inputs (One-time Setup)

Use `tools/bootstrap_edasm_inputs.py` to download the disk image and ROM:

```bash
./tools/bootstrap_edasm_inputs.py <output_dir>
```

**What it does:**
- Downloads `EDASM_SRC.2mg` from `markpmlim/EdAsm` repository
- Downloads `apple_II.rom` from `mirrors.apple2.org.za`
- Generates `edasm_src.json` (file rearrangement config) from embedded template

**Example:**
```bash
./tools/bootstrap_edasm_inputs.py ./fresh_inputs
# Creates:
#   fresh_inputs/EDASM_SRC.2mg
#   fresh_inputs/apple_II.rom
#   fresh_inputs/edasm_src.json
```

**Custom URLs** (if mirrors are unavailable):
```bash
./tools/bootstrap_edasm_inputs.py <output_dir> \
  --disk-url <custom_2mg_url> \
  --rom-url <custom_rom_url>
```

### Phase 2: Run EDASM Job (Assemble Code)

Use `tools/run_edasm_job.py` to execute an assembly job:

```bash
./tools/run_edasm_job.py \
  --input <source.asm> \
  --listing <listing-filename> \
  --output <output-filename>
```

**Arguments:**
- `--input` (repeatable): One or more ProDOS text files to import. **First input** is used in the `ASM` command.
- `--listing`: Name of the listing file in `/OUT` (used in `PR#1,/OUT/...` command)
- `--output`: Name of the output/object file in `/OUT` (used in `ASM ...,/OUT/...` command)
- `--work-dir` (optional): Work directory to reset and use (default: `work`)

**What it does:**
1. Generates `inputs/EdAsm.AutoST` (ProDOS script) with:
   - `PR#1,/OUT/<listing>` (redirect printer to listing file)
   - `ASM <first_input>,/OUT/<output>` (assemble and write output)
   - `END` (exit EDASM)
2. Resets `work/volumes/OUT` directory
3. Executes `edasm_setup.py` with:
   - Fixed cadius, disk image, ROM paths
   - Rearrangement config
   - EdAsm.AutoST and all input files
4. Converts output text files from ProDOS CR to Linux LF

**Example:**
```bash
./tools/run_edasm_job.py \
  --input inputs/my-program.asm \
  --input inputs/common.inc \
  --listing LISTING.TXT \
  --output OUTPUT.BIN
```

**Output:**
- Listing in `work/volumes/OUT/LISTING.TXT` (ProDOS CR → Linux LF)
- Object code in `work/volumes/OUT/OUTPUT.BIN` (unchanged)

## Typical Session

```bash
# 1. One-time: Bootstrap fresh EDASM inputs (if not already present)
./tools/bootstrap_edasm_inputs.py inputs

# 2. Run an assembly job
./tools/run_edasm_job.py \
  --input inputs/my-asm.asm \
  --listing my.lst \
  --output my.obj

# 3. Check outputs
cat work/volumes/OUT/my.lst    # Linux text format (LF)
hexdump -C work/volumes/OUT/my.obj | head
```

## Files Generated

**During bootstrap:**
- `edasm_src.json` — ProDOS file rearrangement configuration

**During job run:**
- `inputs/EdAsm.AutoST` — ProDOS command script for EDASM
- `work/volumes/OUT/<listing>` — Text listing (converted to LF)
- `work/volumes/OUT/<output>` — Object/output file (unchanged)

## Key Points

- **First input is the ASM source**: The basename of the first `--input` file is used in the `ASM` command inside EdAsm.AutoST.
- **Additional inputs are imported**: Extra `--input` files are available in ProDOS but not automatically assembled.
- **Output text conversion**: Files with ProDOS text type (xattr `user.prodos8.file_type=04`) are automatically converted to Linux LF.
- **Listing requires PR#1 redirect**: Without `PR#1,/OUT/...`, output goes to the console and is not captured in a file.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| "ROM not found" or "2mg not found" | Run bootstrap first: `./tools/bootstrap_edasm_inputs.py inputs` |
| Listing not generated | Ensure `--listing` argument is set; EdAsm.AutoST must start with `PR#1,/OUT/...` |
| Output file empty | Check that the ASM command completed inside EDASM (look for error messages in emulator output) |
| Text not converted to Linux format | Verify file has xattr `user.prodos8.file_type=04` (ProDOS text type) |

## Related Tools

- `tools/edasm_setup.py` — Low-level emulator launcher (used internally by `run_edasm_job.py`)
- `tools/prodos_text_to_linux.py` — Convert ProDOS CR to Linux LF (used internally by `run_edasm_job.py`)
- `tools/bootstrap_edasm_inputs.py` — Download and stage EDASM artifacts
