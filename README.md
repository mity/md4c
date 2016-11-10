[![Build status (travis-ci.com)](https://img.shields.io/travis/mity/md4c/master.svg?label=linux%20build)](https://travis-ci.org/mity/md4c)
[![Build status (appveyor.com)](https://img.shields.io/appveyor/ci/mity/md4c/master.svg?label=windows%20build)](https://ci.appveyor.com/project/mity/md4c/branch/master)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/mity-md4c.svg)](https://scan.coverity.com/projects/mity-md4c)
[![Coverage](https://img.shields.io/coveralls/mity/md4c/master.svg)](https://coveralls.io/github/mity/md4c)

# MD4C Readme

Home: http://github.com/mity/md4c

MD4C stands for "MarkDown for C" and, unsurprisingly, it is a C Markdown parser
implementation.

**Warning:** This project is very young (read "immature") and work in progress.
Most important features are not yet implemented. See the current status below.
And there may be bugs.


## What is Markdown

In short, Markdown is the markup language this `README.md` file is written in.

The following resources can explain more if you are unfamiliar with it:
* [Wikipedia article](http://en.wikipedia.org/wiki/Markdown)
* [CommonMark site](http://commonmark.org)


## What is MD4C

Main features:
* **Compactness:** MD4C is implemented in one source file and one header file.
* **Flexibility:** Flags allow to tune the desired dialect of the Markdown
    parser.
* **Encoding agnosticism:** As much as possible, MD4C by design does not care
    about input text encoding, relying only on the Markdown control characters
    being ASCII compatible. (The actual text data are propagated back to the
    caller in the same encoding unchanged.)
* **UTF-16LE support:** On Windows, MD4C may be built to consume (and produce)
    wide strings (`WCHAR*` instead of `char*`).
* **Easily embeddable:** MD4C depends only on few functions of C standard
    library.
* **Portability:** MD4C builds and works on Windows and Linux, and it should
    be fairly trivial to build it also on other systems.
* **Permissive license:** MD4C is available under the MIT license.


## Using MD4C

The parser is implemented in a single C source file `md4c.c` and its
accompanying header `md4c.h`.

The main provided function is `md_parse()`. It takes a text in Markdown syntax
as an input and a renderer structure which holds pointers to few callback
functions. As `md_parse()` eats the input, it calls appropriate callbacks
allowing application to convert it into another format or render it onto
the screen.

Refer to the header file for more details, the API is mostly self-explaining
and there are some explanatory comments.

Example implementation of simple renderer is available in the `md2html`
directory which implements a conversion utility from Markdown to HTML.


## Current Status

### CommonMark Specification

The goal is be compliant to the latest version of
[CommonMark specification](http://spec.commonmark.org/).

The list below corresponds to chapters of the specification version 0.26 and
more or less forms our to do list.

- **Preliminaries:**
  - [x] 2.1 Character and lines
  - [x] 2.2 Tabs
  - [x] 2.3 Insecure characters

- **Blocks and Inlines:**
  - [x] 3.1 Precedence
  - [ ] 3.2 Container blocks and leaf blocks

- **Leaf Blocks:**
  - [x] 4.1 Thematic breaks
  - [x] 4.2 ATX headings
  - [x] 4.3 Setext headings
  - [x] 4.4 Indented code blocks
  - [x] 4.5 Fenced code blocks
  - [x] 4.6 HTML blocks
  - [x] 4.7 Link reference definitions
  - [x] 4.8 Paragraphs
  - [x] 4.9 Blank lines

- **Container Blocks:**
  - [x] 5.1 Block quotes
  - [ ] 5.2 List items
  - [ ] 5.3 Lists

- **Inlines:**
  - [x] 6.1 Backslash escapes
  - [x] 6.2 Entity and numeric character references
  - [x] 6.3 Code spans
  - [x] 6.4 Emphasis and strong emphasis
  - [ ] 6.5 Links
  - [ ] 6.6 Images
  - [x] 6.7 Autolinks
  - [x] 6.8 Raw HTML
  - [x] 6.9 Hard line breaks
  - [x] 6.10 Soft line breaks
  - [x] 6.11 Textual content


### Considered Extensions

Aside of CommonMark features, various Markdown implementations out there support
various extensions and/or some deviations from the CommonMark specification
which may be found desired or useful in some situations.

Therefore some extensions or deviations from the CommonMark specification may
be considered and implemented. However, such extensions and deviations from the
standard shall be enabled only if explicitly enabled by the application.

Default behavior shall stick to the CommonMark specification.

The list below is incomplete list of extensions I see as worth of
consideration.

- **Block Extensions:**
  - [ ] Tables
  - [ ] Header anchors: `## Chapter {#anchor}`
    (allowing fragment links pointing to it, e.g. `[link text](#anchor)`)

- **Inline Extensions:**
  - [ ] Underline: `__foo bar__`
  - [ ] Strikethrough: `~~foo bar~~`
  - [ ] Highlight: `==foo bar==`
  - [ ] Quote: `"foo bar"`
  - [ ] Superscript: `a^2^ + b^2^ = c^2^`
  - [ ] Subscript: `matrix A~i,j~`

- **Miscellaneous:**
  - [x] Permissive ATX headers: `###Header` (without space)
  - [x] Permissive URL autolinks: `http://google.com` (without `<`...`>`)
  - [x] Permissive e-mail autolinks: `john.dow@example.com`
        (without `<`...`>` and `mailto:`)
  - [x] Disabling indented code blocks
  - [x] Disabling raw HTML blocks/spans


## License

MD4C is covered with MIT license, see the file `LICENSE.md`.


## Reporting Bugs

If you encounter any bug, please be so kind and report it. Unheard bugs cannot
get fixed. You can submit bug reports here:

* http://github.com/mity/md4c/issues
