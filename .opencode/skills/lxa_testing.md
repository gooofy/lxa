# lxa Testing Skill

This skill details the testing requirements and strategies for the lxa project.

## 1. Philosophy
- **Rock-solid stability**: No crashes allowed.
- **TDD**: Write tests first.
- **100% Coverage**: Mandatory.

## 2. Test Types
1. **Integration Tests** (`tests/`):
   - `tests/dos/`, `tests/exec/`, etc.
   - m68k code testing complete workflows.
   - Run with `make run-test` (compiles, runs, diffs `expected.out`).
2. **Unit Tests** (`tests/unit/`):
   - For complex logic (parsing, algos).
3. **Stress Tests**:
   - For stability (memory leaks, concurrency).

## 3. Checklist
Before completing a task:
- [ ] Functionality (Happy Path)
- [ ] Error Handling (NULLs, resources, permissions)
- [ ] Edge Cases (Boundaries, empty/max inputs)
- [ ] Break Handling (Ctrl+C)
- [ ] Memory Safety (Alloc/Free paired)
- [ ] Concurrency
- [ ] Documentation (Clear test purpose)

## 4. TDD Workflow
1. Read Roadmap.
2. Plan Tests (Edge cases, errors).
3. Write Test Skeleton (`main.c`, `expected.out`).
4. Implement Feature.
5. Run Tests: `make -C tests/<cat>/<test> run-test`.
6. Measure Coverage.
7. Iterate until 100% coverage and passing.

## 5. Common Patterns
- **DOS**: `Lock`, `Examine`, `Open`, `IoErr`.
- **ReadArgs**: Verify parsing with `RDArgs`.
- **Memory**: `AllocMem` (check NULL), `MEMF_CLEAR` (check zeros), `FreeMem`.

## 6. Debugging Failures
1. **Diff**: `diff expected.out actual.out`
2. **Debug Log**: Run with `-d`.
3. **Crashes**: Check segfaults (host) or assertions (ROM).
4. **Print**: Add `Printf` ("DEBUG: ...").

## 7. Running Tests
- All: `cd tests && make test`
- Single: `make -C tests/dos/helloworld run-test`
