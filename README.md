[![Build status (travis-ci.com)](https://img.shields.io/travis/mity/md4c/master.svg?label=linux%20build)](https://travis-ci.org/mity/md4c)
[![Build status (appveyor.com)](https://img.shields.io/appveyor/ci/mity/md4c/master.svg?label=windows%20build)](https://ci.appveyor.com/project/mity/md4c/branch/master)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/mity-md4c.svg?label=coverity%20scan)](https://scan.coverity.com/projects/mity-md4c)
[![Codecov](https://img.shields.io/codecov/c/github/mity/md4c/master.svg?label=code%20coverage)](https://codecov.io/github/mity/md4c)

# MD4C Readme

* Home: http://github.com/mity/md4c
* Wiki: http://github.com/mity/md4c/wiki

MD4C stands for "Markdown for C" and, unsurprisingly, it is a C Markdown parser
implementation.


## What is Markdown

In short, Markdown is the markup language this `README.md` file is written in.

The following resources can explain more if you are unfamiliar with it:
* [Wikipedia article](http://en.wikipedia.org/wiki/Markdown)
* [CommonMark site](http://commonmark.org)


## What is MD4C

MD4C is C Markdown parser with the following features:

* **Compliance:** Generally MD4C aims to be compliant to the latest version of
  [CommonMark specification](http://spec.commonmark.org/). Right now we are
  fully compliant to CommonMark 0.29.

* **Extensions:** MD4C supports some commonly requested and accepted extensions.
  See below.

* **Compactness:** MD4C is implemented in one source file and one header file.

* **Embedding:** MD4C is easy to reuse in other projects, its API is very
  straightforward: There is actually just one function, `md_parse()`.

* **Push model:** MD4C parses the complete document and calls callback
  functions provided by the application for each start/end of block, start/end
  of a span, and with any textual contents.

* **Portability:** MD4C builds and works on Windows and Linux, and it should
  be fairly simple to make it run also on most other systems.

* **Encoding:** MD4C can be compiled to recognize ASCII-only control characters,
  UTF-8 and, on Windows, also UTF-16, i.e. what is on Windows commonly called
  just "Unicode". See more details below.

* **Permissive license:** MD4C is available under the MIT license.

* **Performance:** MD4C is [very fast](https://talk.commonmark.org/t/why-is-md4c-so-fast-c/2520).


## Using MD4C

Application has to include the header `md4c.h` and link against MD4C library;
or alternatively it may include `md4c.h` and `md4c.c` directly into its source
base as the parser is only implemented in the single C source file.

The main provided function is `md_parse()`. It takes a text in Markdown syntax
as an input and a pointer to a structure which holds pointers to several
callback functions.

As `md_parse()` processes the input, and it calls the appropriate callbacks
(when entering or leaving any Markdown block or span; and when outputting any
textual content of the document), allowing application to convert it into
another format or render it onto the screen.

More comprehensive guide can be found in the header `md4c.h` and also
on [MD4C wiki].

Example implementation of simple renderer is available in the `md2html`
directory which implements a conversion utility from Markdown to HTML.


## Markdown Extensions

The default behavior is to recognize only elements defined by the [CommonMark
specification](http://spec.commonmark.org/).

However with appropriate flags, the behavior can be tuned to enable some
extensions:

 * With the flag `MD_FLAG_COLLAPSEWHITESPACE`, non-trivial whitespace is
   collapsed into a single space.

 * With the flag `MD_FLAG_TABLES`, GitHub-style tables are supported.

 * With the flag `MD_FLAG_TASKLISTS`, GitHub-style task lists are supported.

 * With the flag `MD_FLAG_STRIKETHROUGH`, strike-through spans are enabled
   (text enclosed in tilde marks, e.g. `~foo bar~`).

 * With the flag `MD_FLAG_PERMISSIVEURLAUTOLINKS` permissive URL autolinks
   (not enclosed in `<` and `>`) are supported.

 * With the flag `MD_FLAG_PERMISSIVEAUTOLINKS`, ditto for e-mail autolinks.

 * With the flag `MD_FLAG_PERMISSIVEWWWAUTOLINKS` permissive WWW autolinks
   (without any scheme specified; `http:` is assumed) are supported.

The syntax of the extensions is described on [MD4C wiki].

Few features (those some people see as mis-features) of CommonMark
specification may be disabled:

 * With the flag `MD_FLAG_NOHTMLSPANS` or `MD_FLAG_NOHTMLBLOCKS`, raw inline
   HTML or raw HTML blocks respectively are disabled.

 * With the flag `MD_FLAG_NOINDENTEDCODEBLOCKS`, indented code blocks are
   disabled.


## Input/Output Encoding

The CommonMark specification generally assumes UTF-8 input, but under closer
inspection, Unicode plays any role in few very specific situations when parsing
Markdown documents:

  * For detection of word boundary when processing emphasis and strong emphasis,
    some classification of Unicode character (whitespace, punctuation) is used.

  * For (case-insensitive) matching of a link reference with corresponding link
    reference definition, Unicode case folding is used.

  * For translating HTML entities (e.g. `&amp;`) and numeric character
    references (e.g. `&#35;` or `&#xcab;`) into their Unicode equivalents.
    However MD4C leaves this translation on the renderer/application; as the
    renderer is supposed to really know output encoding and whether it really
    needs to perform this kind of translation. (Consider that a renderer
    converting Markdown to HTML may leave the entities untranslated and defer
    the work to a web browser.)

MD4C relies on this property of the CommonMark and the implementation is, to
a large degree, encoding-agnostic. Most of MD4C code only assumes that the
encoding of your choice is compatible with ASCII, i.e. that the codepoints
below 128 have the same numeric values as ASCII.

Any input MD4C does not understand is simply seen as part of the document text
and sent to the renderer's callback functions unchanged.

The two situations where MD4C has to understand Unicode are handled accordingly
to the following preprocessor macros:

 * If preprocessor macro `MD4C_USE_UTF8` is defined, MD4C assumes UTF-8
   for word boundary detection and case-folding.

 * On Windows, if preprocessor macro `MD4C_USE_UTF16` is defined, MD4C uses
   `WCHAR` instead of `char` and assumes UTF-16 encoding in those situations.
   (UTF-16 is what Windows developers usually call just "Unicode" and what
   Win32API works with.)

 * By default (when none of the macros is defined), ASCII-only mode is used
   even in the specific situations. That effectively means that non-ASCII
   whitespace or punctuation characters won't be recognized as such and that
   case-folding is performed only on ASCII letters (i.e. `[a-zA-Z]`).

(Adding support for yet another encodings should be relatively simple due
the isolation of the respective code.)


## License

MD4C is covered with MIT license, see the file `LICENSE.md`.


## Reporting Bugs

If you encounter any bug, please be so kind and report it. Unheard bugs cannot
get fixed. You can submit bug reports here:

* http://github.com/mity/md4c/issues


[MD4C home]: http://github.com/mity/md4c
[MD4C wiki]: http://github.com/mity/md4c/wiki
