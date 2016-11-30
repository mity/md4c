[![Build status (travis-ci.com)](https://img.shields.io/travis/mity/md4c/master.svg?label=linux%20build)](https://travis-ci.org/mity/md4c)
[![Build status (appveyor.com)](https://img.shields.io/appveyor/ci/mity/md4c/master.svg?label=windows%20build)](https://ci.appveyor.com/project/mity/md4c/branch/master)
[![Coverity Scan Build Status](https://img.shields.io/coverity/scan/mity-md4c.svg)](https://scan.coverity.com/projects/mity-md4c)
[![Coverage](https://img.shields.io/coveralls/mity/md4c/master.svg)](https://coveralls.io/github/mity/md4c)

# MD4C Readme

Home: http://github.com/mity/md4c

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
  very close to CommonMark 0.27.

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
  UTF-8 and, on Windows, also UTF-16 little endian, i.e. what is on Windows
  commonly called just "Unicode". See more details below.

* **Permissive license:** MD4C is available under the MIT license.

* **Performance:** MD4C is very fast. Preliminary tests show its quite faster
  then [Hoedown](https://github.com/hoedown/hoedown) or
  [Cmark](https://github.com/jgm/cmark).


## Using MD4C

The parser is implemented in a single C source file `md4c.c` and its
accompanying header `md4c.h`.

The main provided function is `md_parse()`. It takes a text in Markdown syntax
as an input and a pointer to renderer structure which holds pointers to few
callback functions.

As `md_parse()` processes the input, it calls the appropriate callbacks
allowing application to convert it into another format or render it onto
the screen.

Refer to the header file for more details, the API is mostly self-explaining
and there are some explanatory comments.

Example implementation of simple renderer is available in the `md2html`
directory which implements a conversion utility from Markdown to HTML.


## Markdown Extensions

The default behavior is to recognize only elements defined by the CommonMark
specification.

However with appropriate renderer flags, the behavior can be tuned to enable
some extensions or allowing some deviations from the specification.

 * With the flag `MD_FLAG_COLLAPSEWHITESPACE`, non-trivial whitespace is
   collapsed into a single space.

 * With the flag `MD_FLAG_TABLES`, GitHub-style tables are supported.

 * With the flag `MD_FLAG_PERMISSIVEURLAUTOLINKS` permissive URL autolinks
   (not enclosed in '<' and '>') are supported.

 * With the flag `MD_FLAG_PERMISSIVEAUTOLINKS`, ditto for e-mail autolinks.

 * With the flag `MD_FLAG_NOHTMLSPANS` or `MD_FLAG_NOHTML`, raw inline HTML
   or raw HTML blocks respectively are disabled.

 * With the flag `MD_FLAG_NOINDENTEDCODEBLOCKS`, indented code blocks are
   disabled.


## Input/Output Encoding

The CommonMark specification generally assumes UTF-8 input, but under closer
inspection Unicode is actually used on very few occasions:

  * Classification of Unicode character as a Unicode whitespace or Unicode
    punctuation. This is used for detection of word boundary when processing
    emphasis and strong emphasis.

  * Unicode case folding. This is used to perform case-independent matching
    of link labels when resolving reference links.

MD4C uses this property of the standard and its implementation is, to a large
degree, encoding-agnostic. Most of the code only assumes that the encoding of
your choice is compatible with ASCII, i.e. that the codepoints below 128 have
the same numeric values as ASCII.

All input MD4C does not understand is seen as a text and sent to the callbacks
unchanged.

The behavior of MD4C in the isolated situations where the encoding really
matters is determined by preprocessor macros:

 * If preprocessor macro `MD4C_USE_UNICODE` is defined, MD4C assumes UTF-8
   in the specific situations.

 * On Windows, if preprocessor macro `MD4C_USE_WIN_UNICODE` is defined, MD4C
   assumes little-endian UTF-16 and uses `WCHAR` instead of `char`. This allows
   usage of MD4C directly within Unicode applications on Windows, without any
   text conversion.

 * When none of the macros is defined, ASCII-only approach is used even in
   the listed situations. This effectively means that non-ASCII whitespace or
   punctuation characters won't be recognized as such and that case-folding is
   performed only on ASCII letters (i.e. `[a-zA-Z]`).

(Adding support for yet another encodings should be relatively simple due
the isolation of the respective code.)


## License

MD4C is covered with MIT license, see the file `LICENSE.md`.


## Reporting Bugs

If you encounter any bug, please be so kind and report it. Unheard bugs cannot
get fixed. You can submit bug reports here:

* http://github.com/mity/md4c/issues
