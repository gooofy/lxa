# Test Fixtures

This directory contains shared test data and fixtures for both unit and integration tests.

## Directory Structure

```
fixtures/
├── fs/                     # Filesystem test fixtures
│   ├── empty/              # Empty directory
│   ├── single_file/        # Directory with single file
│   │   └── test.txt
│   ├── nested/             # Nested directory structure
│   │   ├── level1/
│   │   │   ├── level2/
│   │   │   │   └── deep.txt
│   │   │   └── file.txt
│   │   └── top.txt
│   └── special_chars/      # Files with special characters
│       └── file with spaces.txt
├── scripts/                # Test script fixtures
│   ├── simple.txt          # Simple shell script
│   └── controlflow.txt     # Control flow test script
└── README.md
```

## Usage

### In Integration Tests

Integration tests (running under lxa) can access fixtures through the SYS: drive
when configured properly.

### In Unit Tests

Unit tests copy fixture data to temporary directories at runtime.

## Creating New Fixtures

1. Add the fixture files to the appropriate subdirectory
2. Update this README with the new fixture description
3. If the fixture requires setup, add a setup script or document the process
