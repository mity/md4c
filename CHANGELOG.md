
# MD4C Change Log


## Next Version (Work in Progress)

Changes:

 * Parsing of tables (with `MD_FLAG_TABLES`) is now closer to the way how
   cmark-gfm parses tables as we do not require every row of the table to
   contain a pipe `|` anymore.

   As a consequence, paragraphs now cannot interrupt tables. A paragraph which
   follows the table has to be delimited with a blank line.

 * With `MD_FLAG_LATEXMATHSPANS`, LaTeX math spans (`$...$`) and LaTeX display
   math spans (`$$...$$`) are now recognized. (Note though that the HTML
   renderer outputs them verbatim in a custom `<x-equation>` tag.)

   Contributed by [Tilman Roeder](https://github.com/dyedgreen).

 * With `MD_FLAG_WIKILINKS`, Wiki-style links (`[[...]]`) are now recognized.
   (Note htough that the HTML renderer renders them as a custom `<x-wikilink>`
   tag.)

   Contributed by [Nils Blomqvist](https://github.com/niblo).

Fixes:

 * [#94](https://github.com/mity/md4c/issues/94):
   `md_build_ref_def_hashtable()`: Do not allocate more memory then strictly
   needed.

 * [#95](https://github.com/mity/md4c/issues/95):
   `md_is_container_mark()`: Ordered list mark requires at least one digit.

 * [#96](https://github.com/mity/md4c/issues/96):
   Some fixes for link label comparison.


## Version 0.3.4

Changes:

 * Make Unicode-specific code compliant to Unicode 12.1.

 * Structure `MD_BLOCK_CODE_DETAIL` got new member `fenced_char`. Application
   can use it to detect character used to form the block fences (`` ` `` or
   `~`). In the case of indented code block, it is set to zero.

Fixes:

 * [#77](https://github.com/mity/md4c/issues/77):
   Fix maximal count of digits for numerical character references, as requested
   by CommonMark specification 0.29.

 * [#78](https://github.com/mity/md4c/issues/78):
   Fix link reference definition label matching for Unicode characters where
   the folding mapping leads to multiple codepoints, as e.g. in `ẞ` -> `SS`.

 * [#83](https://github.com/mity/md4c/issues/83):
   Fix recognition of an empty blockquote which interrupts a paragraph.


## Version 0.3.3

Changes:

 * Make permissive URL autolink and permissive WWW autolink extensions stricter.

   This brings the behavior closer to GFM and mitigates risk of false positives.
   In particular, the domain has to contain at least one dot and parenthesis
   can be part of the link destination only if `(` and `)` are balanced.

Fixes:

 * [#73](https://github.com/mity/md4c/issues/73):
   Some raw HTML inputs could lead to quadratic parsing times.

 * [#74](https://github.com/mity/md4c/issues/74):
   Fix input leading to a crash. Found by fuzzing.

 * [#76](https://github.com/mity/md4c/issues/76):
   Fix handling of parenthesis in some corner cases of permissive URL autolink
   and permissive WWW autolink extensions.


## Version 0.3.2

Changes:

 * Changes mandated by CommonMark specification 0.29.

   Most importantly, the white-space trimming rules for code spans have changed.
   At most one space/newline is trimmed from beginning/end of the code span
   (if the code span contains some non-space contents, and if it begins and
   ends with space at the same time). In all other cases the spaces in the code
   span are now left intact.

   Other changes in behavior are in corner cases only. Refer to [CommonMark
   0.29 notes](https://github.com/commonmark/commonmark-spec/releases/tag/0.29)
   for more info.

Fixes:

 * [#68](https://github.com/mity/md4c/issues/68):
   Some specific HTML blocks were not recognized when EOF follows without any
   end-of-line character.

 * [#69](https://github.com/mity/md4c/issues/69):
   Strike-through span not working correctly when its opener mark is directly
   followed by other opener mark; or when other closer mark directly precedes
   its closer mark.


## Version 0.3.1

Fixes:

 * [#58](https://github.com/mity/md4c/issues/58),
   [#59](https://github.com/mity/md4c/issues/59),
   [#60](https://github.com/mity/md4c/issues/60),
   [#63](https://github.com/mity/md4c/issues/63),
   [#66](https://github.com/mity/md4c/issues/66):
   Some inputs could lead to quadratic parsing times. Thanks to Anders Kaseorg
   for finding all those issues.

 * [#61](https://github.com/mity/md4c/issues/59):
   Flag `MD_FLAG_NOHTMLSPANS` erroneously affected also recognition of
   CommonMark autolinks.


## Version 0.3.0

New features:

 * Add extension for GitHub-style task lists:

   ```
    * [x] foo
    * [x] bar
    * [ ] baz
   ```

   (It has to be explicitly enabled with `MD_FLAG_TASKLISTS`.)

 * Added support for building as a shared library. On non-Windows platforms,
   this is now default behavior; on Windows static library is still the default.
   The CMake option `BUILD_SHARED_LIBS` can be used to request one or the other
   explicitly.

   Contributed by Lisandro Damián Nicanor Pérez Meyer.

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

 * CID 1475544:
   Calling `md_free_attribute()` on uninitialized data.

 * [#47](https://github.com/mity/md4c/issues/47):
   Using bad offsets in `md_is_entity_str()`, in some cases leading to buffer
   overflow.

 * [#51](https://github.com/mity/md4c/issues/51):
   Segfault in `md_process_table_cell()`.

 * [#53](https://github.com/mity/md4c/issues/53):
   With `MD_FLAG_PERMISSIVEURLAUTOLINKS` or `MD_FLAG_PERMISSIVEWWWAUTOLINKS`
   we could generate bad output for ordinary Markdown links, if a non-space
   character immediately follows like e.g. in `[link](http://github.com)X`.


## Version 0.2.7

This was the last version before the changelog has been added.
