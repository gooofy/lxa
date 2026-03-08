# lxa Documentation

Welcome to the `lxa` documentation index. This directory collects the current user, developer, and project-maintenance guides.

## User Documentation

### [Configuration Guide](configuration.md)
How to configure `lxa`, including drives, floppies, environment variables, and troubleshooting.

### [Building from Source](building.md)
How to install prerequisites, configure the build, compile artifacts, install the runtime, and run tests.

## Developer Documentation

### [Architecture](architecture.md)
System architecture, emulator and ROM responsibilities, VFS behavior, emucalls, multitasking, and testing layout.

### [Developer Guide](developer-guide.md)
Project workflow, coding standards, common implementation tasks, debugging tips, and contribution expectations.

### [Testing Guide](testing.md)
Current CTest workflow, Google Test drivers, Unity unit tests, reliability loops, and coverage generation.

## Project Guides

- [../README.md](../README.md) - repository overview and quick start
- [../AGENTS.md](../AGENTS.md) - AI-agent workflow and mandatory project rules
- [../roadmap.md](../roadmap.md) - implementation roadmap and current status
- [../AMIGA_COMMAND_GUIDE.md](../AMIGA_COMMAND_GUIDE.md) - historical command-design reference

## Historical Documents

Some files under `doc/` are preserved as historical planning or audit notes rather than current implementation guidance. When a document is marked historical, prefer the current guides above for day-to-day work.

## Quick Paths

### I want to use `lxa`
- [../README.md](../README.md)
- [configuration.md](configuration.md)
- [building.md](building.md)

### I want to develop `lxa`
- [developer-guide.md](developer-guide.md)
- [architecture.md](architecture.md)
- [testing.md](testing.md)
- [../roadmap.md](../roadmap.md)

### I need the current test workflow
- [testing.md](testing.md)
- [../AGENTS.md](../AGENTS.md)
- [test-reliability-report.md](test-reliability-report.md)

## Documentation Maintenance

When updating the project:
- keep `README.md`, `roadmap.md`, and the relevant guide in sync
- prefer updating current guides instead of copying stale instructions into new files
- mark planning or audit material as historical once it stops reflecting current behavior

Last updated: 2026-03-08
