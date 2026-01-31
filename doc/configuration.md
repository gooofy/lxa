# lxa Configuration Guide

This document explains how to configure lxa for your environment.

## Overview

lxa uses a flexible configuration system that supports:
- Configuration files (INI format)
- Command-line options
- Environment variables
- Sensible defaults

## Configuration File

### Location

Default location: `~/.lxa/config.ini`

Override with `-c` option:
```bash
lxa -c /path/to/custom.ini myprogram
```

### File Format

The configuration file uses standard INI format with sections and key=value pairs:

```ini
# lxa configuration file

[system]
# ROM and memory settings
rom_path = /usr/share/lxa/lxa.rom
ram_size = 10485760  # 10 MB in bytes

[drives]
# Map Amiga drives to Linux directories
SYS = ~/.lxa/System
Work = ~/amiga_projects
DH1 = /mnt/data/amiga
DH2 = /mnt/storage/amiga_archive

[floppies]
# Map floppy drives to directories
DF0 = ~/.lxa/floppy0
DF1 = ~/downloads/amiga_disk

[debug]
# Debug settings
log_level = 1
trace_cpu = 0
break_on_exception = 0
```

### Configuration Sections

#### [system]

**rom_path**
- Path to lxa ROM file
- Default: Auto-detected (searches current dir, ~/.lxa/, /usr/share/lxa/)
- Example: `rom_path = /opt/lxa/lxa.rom`

**ram_size**
- Amount of RAM in bytes
- Default: 10485760 (10 MB)
- Range: 524288 (512 KB) to 134217728 (128 MB)
- Example: `ram_size = 20971520  # 20 MB`

#### [drives]

Maps Amiga logical drives to Linux directories.

**Format**: `<DRIVE> = <linux_path>`

**Examples**:
```ini
SYS = ~/.lxa/System
Work = ~/Documents/amiga
DH0 = /mnt/data/amiga_hd
DH1 = /media/storage/amiga
TEMP = /tmp/lxa_temp
```

**Notes**:
- Drive names are case-insensitive (SYS = sys = Sys)
- Paths support tilde expansion (~)
- Multiple drives can point to the same directory
- Relative paths are relative to lxa working directory

#### [floppies]

Maps floppy drives to directories (treated like hard drives in lxa).

**Format**: `<FLOPPY> = <linux_path>`

**Examples**:
```ini
DF0 = ~/.lxa/floppy0
DF1 = ~/amiga_floppies/workbench
DF2 = /mnt/nfs/amiga/floppies
```

**Notes**:
- Floppy drives (DF0-DF3) are just logical drives
- No actual floppy hardware emulation
- Directories work the same as hard drive mappings

#### [debug]

Debug and diagnostic options.

**log_level**
- Controls logging verbosity
- Values: 0 (ERROR), 1 (WARNING), 2 (INFO), 3 (DEBUG)
- Default: 1
- Example: `log_level = 3`

**trace_cpu**
- Enable CPU instruction tracing
- Values: 0 (off), 1 (on)
- Default: 0
- Warning: Very verbose, dramatically slows execution
- Example: `trace_cpu = 1`

**break_on_exception**
- Break to debugger on 68k exceptions
- Values: 0 (off), 1 (on)
- Default: 0
- Example: `break_on_exception = 1`

## Command-Line Options

Command-line options override configuration file settings.

### Usage

```bash
lxa [options] <program>
```

### Options Reference

**-c <config>**
- Specify configuration file
- Default: `~/.lxa/config.ini`
- Example: `lxa -c /etc/lxa/server.ini myapp`

**-r <rom>**
- Specify ROM file path
- Overrides `rom_path` in config
- Example: `lxa -r /opt/lxa/lxa.rom myapp`

**-s <sysroot>**
- Set system root directory (legacy mode)
- Maps to `SYS:` drive
- Example: `lxa -s /mnt/amiga myapp`

**-d**
- Enable debug output
- Shows detailed execution traces
- Example: `lxa -d myapp`

**-v**
- Verbose mode
- More detailed output than normal
- Example: `lxa -v myapp`

**-t**
- Enable CPU instruction tracing
- Shows every m68k instruction executed
- Warning: Extremely verbose
- Example: `lxa -t myapp`

**-b <addr|symbol>**
- Set breakpoint at address or symbol
- Can be specified multiple times
- Examples:
  - `lxa -b 0x20000 myapp`
  - `lxa -b _start myapp`
  - `lxa -b main myapp`

### Option Precedence

Configuration priority (highest to lowest):
1. Command-line options
2. Configuration file
3. Environment variables
4. Built-in defaults

Example:
```bash
# config.ini has: rom_path = /usr/share/lxa/lxa.rom
# Command line overrides it:
lxa -r /tmp/test.rom myapp
# Uses /tmp/test.rom
```

## Environment Variables

### LXA_PREFIX

Override the default lxa user environment location.

**Default**: `~/.lxa`

**Usage**:
```bash
export LXA_PREFIX=/opt/lxa_env
lxa myprogram
# Uses /opt/lxa_env instead of ~/.lxa
```

**Effect**:
- Config file becomes `$LXA_PREFIX/config.ini`
- Default SYS: drive becomes `$LXA_PREFIX/System`

### LXA_DATA_DIR

Override system data directory location.

**Default**: Auto-detected (searches /usr/share/lxa/, ../share/lxa/, etc.)

**Usage**:
```bash
export LXA_DATA_DIR=/opt/lxa/share
lxa myprogram
```

**Effect**:
- ROM searches in `$LXA_DATA_DIR/lxa.rom`
- System templates copied from `$LXA_DATA_DIR/System/`

### PATH

Add lxa to PATH for easy invocation:

```bash
export PATH=/opt/lxa/bin:$PATH
lxa myprogram  # No need for full path
```

## Virtual File System (VFS)

### Drive Mapping

lxa maps Amiga logical drives to Linux directories transparently.

#### Case-Insensitive Resolution

Amiga uses case-insensitive filenames, Linux is case-sensitive. lxa resolves this:

**Amiga Path**: `SYS:s/startup-sequence`

**Resolution Process**:
1. Look for exact match: `SYS/s/startup-sequence`
2. If not found, scan directory case-insensitively
3. Find `SYS/S/Startup-Sequence` (actual file)
4. Return match

**Performance**: First match is cached to avoid repeated directory scans.

#### Example Mappings

With configuration:
```ini
[drives]
SYS = /home/user/.lxa/System
Work = /home/user/Projects/amiga
```

| Amiga Path | Linux Path |
|------------|------------|
| `SYS:` | `/home/user/.lxa/System` |
| `SYS:C/Dir` | `/home/user/.lxa/System/C/Dir` |
| `sys:c/dir` | `/home/user/.lxa/System/C/Dir` (case-insensitive) |
| `Work:` | `/home/user/Projects/amiga` |
| `Work:MyApp/main.c` | `/home/user/Projects/amiga/MyApp/main.c` |

### Special Devices

Certain Amiga device names have special handling:

**NIL:**
- Null device (like /dev/null)
- Reads return EOF immediately
- Writes are discarded
- Example: `Copy file NIL:` (silent copy)

**PIPE:**
- Future: Inter-process communication
- Not yet implemented

**CONSOLE:**
- Console I/O (terminal)
- Reads from stdin, writes to stdout
- Used by Shell and interactive programs

### Legacy Sysroot Mode

For backward compatibility, lxa supports legacy mode without configuration:

```bash
lxa -s /path/to/amiga myprogram
```

In this mode:
- `/path/to/amiga` maps to `SYS:`
- No other drives are mapped
- Configuration file is ignored

## First-Run Setup

On first execution, lxa automatically creates a default environment.

### Automatic Initialization

```bash
$ lxa
lxa: First run detected - creating default environment at /home/user/.lxa
lxa: Copying system files...
lxa: Creating directory structure...
lxa: Environment created successfully.
```

### Created Structure

```
~/.lxa/
├── config.ini              # Configuration file
├── System/                 # SYS: drive
│   ├── C/                  # Commands
│   │   ├── dir
│   │   ├── type
│   │   ├── delete
│   │   └── ...
│   ├── S/                  # Scripts
│   │   └── Startup-Sequence
│   ├── Libs/               # Libraries
│   ├── Devs/               # Devices
│   └── T/                  # Temporary files
└── lxa.rom                # ROM file (if not in /usr/share)
```

### Default Configuration

Generated `~/.lxa/config.ini`:
```ini
[system]
rom_path = ~/.lxa/lxa.rom

[drives]
SYS = ~/.lxa/System
T = ~/.lxa/System/T
```

### Customizing After First Run

Edit `~/.lxa/config.ini` to add drives, change settings, etc.

```bash
nano ~/.lxa/config.ini
```

Add additional drives:
```ini
[drives]
SYS = ~/.lxa/System
Work = ~/Documents/amiga_projects
DH1 = /mnt/storage/amiga_files
```

## Configuration Examples

### Minimal Configuration

```ini
[drives]
SYS = ~/.lxa/System
```

### Developer Setup

```ini
[system]
rom_path = ~/projects/lxa/build/target/rom/lxa.rom

[drives]
SYS = ~/projects/lxa/build/target/sys
Work = ~/amiga_dev
Source = ~/projects/amiga_software
T = /tmp/lxa_temp

[debug]
log_level = 2
```

### Multi-User Setup

```ini
[system]
rom_path = /opt/lxa/share/lxa.rom

[drives]
SYS = /opt/lxa/share/System
Work = ~/amiga_work
Home = ~
T = /tmp/lxa_$USER
```

### Server Configuration

```ini
[system]
rom_path = /srv/lxa/lxa.rom
ram_size = 20971520  # 20 MB

[drives]
SYS = /srv/lxa/system
Work = /srv/lxa/work
Data = /srv/data/amiga
Backup = /srv/backup/amiga

[debug]
log_level = 1
```

## Troubleshooting

### "Failed to open config file"

Not an error - lxa falls back to defaults.

To create a config file:
```bash
mkdir -p ~/.lxa
cat > ~/.lxa/config.ini << 'EOF'
[drives]
SYS = ~/.lxa/System
EOF
```

### "Drive not found: Work:"

**Problem**: Drive not mapped in configuration.

**Solution**: Add drive mapping to config.ini:
```ini
[drives]
Work = ~/amiga_projects
```

### Case-Sensitivity Issues

**Problem**: `SYS:s/startup` not found, but file exists as `SYS/S/Startup-Sequence`.

**Solution**: lxa should handle this automatically. If not:
1. Check drive is properly mapped in config
2. Verify file exists: `ls ~/.lxa/System/S/`
3. Enable debug mode: `lxa -d myprogram`

### ROM Not Found

**Problem**: "Failed to open lxa.rom"

**Solutions**:
1. Install system-wide: Copy to `/usr/share/lxa/lxa.rom`
2. User install: Copy to `~/.lxa/lxa.rom`
3. Specify in config:
   ```ini
   [system]
   rom_path = /path/to/lxa.rom
   ```
4. Command-line: `lxa -r /path/to/lxa.rom myprogram`

### Permission Denied

**Problem**: Can't access mapped drive directory.

**Solution**: Check permissions:
```bash
ls -ld ~/.lxa/System
# Should show: drwxr-xr-x
```

Fix permissions:
```bash
chmod 755 ~/.lxa/System
```
