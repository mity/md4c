
# MD4C Change Log


## Next Release (work in progress)

New features:

 * Add extension for GitHub-style task lists:

   ```
    * [x] foo
    * [x] bar
    * [ ] baz
   ```

 * Renamed structure `MD_RENDERER` to `MD_PARSER` and refactorize its contents
   a little bit. Note this is source-level incompatible and initialization code
   in apps may need to be updated.

   The aim of the change is to be more friendly for long-term ABI compatibility
   we shall maintain, starting with this release.

 * Added `CHANGELOG.md` (this file).

 * Make sure `md_process_table_row()` reports the same count of table cells for
   all table rows, no matter how broken the input is. The cell count is derived
   from table underline line. Bogus cells in other rows are silently ignored.
   Missing cells in other rows are reported as empty ones.

Fixes:

 * CID 1475544: Calling `md_free_attribute()` on uninitialized data.

 * #47: Using bad offsets in `md_is_entity_str()`, in some cases leading to
   buffer overflow.

 * #51: Segfault in `md_process_table_cell()`.

 * #53: With `MD_FLAG_PERMISSIVEURLAUTOLINKS` or `MD_FLAG_PERMISSIVEWWWAUTOLINKS`
   we could generate bad output for ordinary Markdown links, if a non-space
   character immediately follows like e.g. in `[link](http://github.com)X`.


## Version 0.2.7

This was the last version before the changelog has been added.
