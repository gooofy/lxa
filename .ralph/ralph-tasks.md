# Ralph Tasks

- [x] Seed Phase 78 Ralph task list from `roadmap.md`
- [ ] Implement and test `78-Q` expansion.library ConfigDev chain APIs (`FindConfigDev`, `AddConfigDev`, `RemConfigDev`)
- [ ] Implement and test `78-Q` expansion.library memory/config helpers (`AllocBoardMem`, `FreeBoardMem`, `AllocExpansionMem`, `FreeExpansionMem`, `ConfigBoard`, `ConfigChain`)
- [ ] Implement and test `78-Q` expansion.library DOS/binding helpers (`MakeDosNode`, `AddDosNode`, `ObtainConfigBinding`, `ReleaseConfigBinding`, `SetCurrentBinding`, `GetCurrentBinding`)
- [ ] Finish `78-Q-2` review items, update `roadmap.md`, and validate expansion changes with targeted/full tests
- [ ] Implement and test `78-R` `mathffp.library` core conversion/comparison/arithmetic APIs
- [ ] Implement and test `78-R` `mathffp.library` rounding/edge-case behavior plus `mathtrans.library` single-precision transcendentals
- [ ] Implement and test `78-R` `mathieeedoubtrans.library` transcendentals and edge-value coverage
- [ ] Finish `78-R-2` review items, update `roadmap.md`, and validate math-library changes with targeted/full tests
- [ ] Implement and test `78-S` diskfont enumeration/content APIs (`DisposeFontContents`, `NewFontContents`) and verify existing font-surface behavior
- [ ] Implement and test `78-S` proportional and multi-plane diskfont support
- [ ] Finish `78-S-2` review items, update `roadmap.md`, and validate diskfont changes with targeted/full tests
- [ ] Implement and test `78-T` icon parsing/object helpers (`GetDiskObjectNew`, `GetDiskObject`, `PutDiskObject`, `FreeDiskObject`, `FindToolType`, `MatchToolValue`, `DiskObject` layout)
- [ ] Implement and test `78-T` Workbench app-object/screen helpers (`AddAppWindow`, `RemoveAppWindow`, `AddAppIcon`, `RemoveAppIcon`, `AddAppMenuItem`, `RemoveAppMenuItem`, `WBInfo`, `MakeWorkbenchGadgetA`, `OpenWorkbench`, `CloseWorkbench`, `ChangeWorkbench`)
- [ ] Finish `78-T-2` review items, update `roadmap.md`, and validate Workbench/icon changes with targeted/full tests
- [ ] Implement and test `78-U` commodities.library core object/filter/message APIs
- [ ] Implement and test `78-U` `rexxsyslib.library` message/arg/local-var helpers
- [ ] Implement and test `78-U` `reqtools.library` requester APIs
- [ ] Implement and test `78-U` datatypes.library object/source APIs
- [ ] Finish `78-V-2` review items, update `roadmap.md`, and validate commodities/rexx/reqtools/datatypes changes with targeted/full tests
- [ ] Complete `78-V` FS-UAE analysis notes, document actionable findings, and add any verified follow-ups/tests
- [ ] Complete remaining `78-X` regression/integration tasks, run `ctest --test-dir build --output-on-failure -j16`, run targeted app suites, and finalize Phase 78 version/docs updates

Add your tasks below using: `ralph --add-task "description"`
