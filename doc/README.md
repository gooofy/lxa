# lxa Documentation

Welcome to the lxa documentation. This directory contains detailed guides for users and developers.

## User Documentation

### [Configuration Guide](configuration.md)
Complete guide to configuring lxa:
- Configuration file format and location
- Drive and floppy mapping
- Virtual filesystem setup
- Environment variables
- Troubleshooting configuration issues

### [Building from Source](building.md)
Detailed build instructions:
- Prerequisites and dependencies
- Installing the Amiga cross-compiler
- Build process and options
- CMake configuration
- Troubleshooting build issues

## Developer Documentation

### [Architecture](architecture.md)
Internal design and technical details:
- System architecture overview
- Memory layout and management
- Component descriptions (emulator, ROM, VFS)
- Emulator calls (emucalls) interface
- Multitasking and interrupt system
- Build system structure
- Testing architecture
- Performance considerations

### [Developer Guide](developer-guide.md)
Essential information for developers:
- Getting started with development
- Project structure walkthrough
- Development workflow (roadmap-driven, TDD)
- Coding standards (ROM, system commands, host code)
- Common development tasks
- Debugging techniques
- Best practices
- Tools and utilities
- Contributing guidelines

### [Testing Guide](testing.md)
Comprehensive testing documentation:
- Integration tests structure and execution
- Unit tests with Unity framework
- Writing new tests
- Test helpers and patterns
- Debugging failed tests
- Code coverage
- Best practices and common patterns
- Continuous integration

## Quick Links

### For End Users
- Getting started: [../README.md](../README.md)
- Configuration: [configuration.md](configuration.md)
- Build from source: [building.md](building.md)

### For Developers
- Start here: [developer-guide.md](developer-guide.md)
- Architecture: [architecture.md](architecture.md)
- Testing: [testing.md](testing.md)
- AI development: [../AGENTS.md](../AGENTS.md)
- Roadmap: [../roadmap.md](../roadmap.md)

## Additional Resources

### Project Files
- **[AGENTS.md](../AGENTS.md)** - Guidelines for AI agents working on lxa (includes quality standards)
- **[roadmap.md](../roadmap.md)** - Development roadmap and current status
- **[AMIGA_COMMAND_GUIDE.md](../AMIGA_COMMAND_GUIDE.md)** - Reference for Amiga commands

### External Resources
- [bebbo/amiga-gcc](https://github.com/bebbo/amiga-gcc) - m68k cross-compiler
- [AROS](http://aros.sourceforge.net/) - Open-source AmigaOS
- [AmigaOS Developer Docs](http://amigadev.elowar.com/) - Official documentation
- [Musashi](https://github.com/kstenerud/Musashi) - 68000 CPU emulator

## Document Organization

### By Topic

**Installation & Setup**
1. [README.md](../README.md#quick-start) - Quick start guide
2. [building.md](building.md) - Build from source
3. [configuration.md](configuration.md) - Configuration

**Development**
1. [developer-guide.md](developer-guide.md) - Start here
2. [architecture.md](architecture.md) - System internals
3. [testing.md](testing.md) - Writing tests
4. [AGENTS.md](../AGENTS.md) - AI development

**Reference**
1. [architecture.md](architecture.md#emulator-calls-emucalls) - Emucalls reference
2. [configuration.md](configuration.md#configuration-file-format) - Config file syntax
3. [testing.md](testing.md#unity-assertions) - Test assertion reference

### By Audience

**I want to use lxa:**
→ [README.md](../README.md) → [configuration.md](configuration.md)

**I want to build lxa:**
→ [building.md](building.md)

**I want to develop for lxa:**
→ [developer-guide.md](developer-guide.md) → [architecture.md](architecture.md) → [testing.md](testing.md)

**I'm an AI agent working on lxa:**
→ [AGENTS.md](../AGENTS.md) → [roadmap.md](../roadmap.md) → [developer-guide.md](developer-guide.md)

## Getting Help

### Common Questions

**Q: How do I install lxa?**
A: See [README.md](../README.md#quick-start)

**Q: How do I configure drive mappings?**
A: See [configuration.md](configuration.md#drive-mapping)

**Q: How do I build from source?**
A: See [building.md](building.md)

**Q: How do I add a new DOS function?**
A: See [developer-guide.md](developer-guide.md#adding-a-new-dos-function)

**Q: How do I write tests?**
A: See [testing.md](testing.md#writing-integration-tests)

**Q: What are the coding standards?**
A: See [developer-guide.md](developer-guide.md#coding-standards)

### Troubleshooting

- **Build issues**: [building.md](building.md#troubleshooting)
- **Configuration issues**: [configuration.md](configuration.md#troubleshooting)
- **Runtime issues**: [README.md](../README.md#troubleshooting)
- **Test failures**: [testing.md](testing.md#debugging-tests)

## Contributing Documentation

Documentation improvements are welcome! When contributing:

1. **Keep it organized** - Use appropriate sections and headers
2. **Be concise** - Clear and direct language
3. **Include examples** - Code snippets help understanding
4. **Cross-reference** - Link to related documentation
5. **Update index** - Update this file when adding new docs

### Documentation Style

- Use Markdown format
- Code blocks with syntax highlighting
- Tables for structured data
- Links to related sections
- Real-world examples

### Where to Document

- **README.md** - General overview, quick start, basic usage
- **doc/configuration.md** - All configuration details
- **doc/building.md** - Build process and toolchain
- **doc/architecture.md** - Technical internals
- **doc/developer-guide.md** - Development workflow
- **doc/testing.md** - Testing procedures
- **AGENTS.md** - AI development guidelines
- **roadmap.md** - Project planning and status

## Documentation Status

Last updated: January 2026

### Recent Changes
- Reorganized documentation into doc/ directory
- Created comprehensive guides for users and developers
- Separated concerns (building, configuration, development, testing)
- Improved cross-referencing between documents
- Added this index file for navigation

### TODO
- API reference documentation (when API stabilizes)
- More troubleshooting examples
- Performance tuning guide
- Security considerations document
