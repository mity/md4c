[![Linux Build Status (travis-ci.com)](https://img.shields.io/travis/mity/md4c/master.svg?logo=linux&label=linux%20build)](https://travis-ci.org/mity/md4c)
[![Windows Build Status (appveyor.com)](https://img.shields.io/appveyor/ci/mity/md4c/master.svg?logo=windows&label=windows%20build)](https://ci.appveyor.com/project/mity/md4c/branch/master)
[![Code Coverage Status (codecov.io)](https://img.shields.io/codecov/c/github/mity/md4c/master.svg?logo=codecov&label=code%20coverage)](https://codecov.io/github/mity/md4c)
[![Coverity Scan Status](https://img.shields.io/coverity/scan/mity-md4c.svg?label=coverity%20scan)](https://scan.coverity.com/projects/mity-md4c)


# MD4C Readme

* Home: http://github.com/mity/md4c
* Wiki: http://github.com/mity/md4c/wiki
* Issue tracker: http://github.com/mity/md4c/issues

MD4C stands for "Markdown for C" and that's exactly what this project is about.


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
  There are no dependencies other then standard C library.

* **Embedding:** MD4C is easy to reuse in other projects, its API is very
  straightforward: There is actually just one function, `md_parse()`.

* **Push model:** MD4C parses the complete document and calls callback
  functions provided by the application for each start/end of block, start/end
  of a span, and with any textual contents.

* **Portability:** MD4C builds and works on Windows and POSIX-compliant systems,
  and it should be fairly simple to make it run also on most other systems.

* **Encoding:** MD4C can be compiled to recognize ASCII-only control characters,
  UTF-8 and, on Windows, also UTF-16, i.e. what is on Windows commonly called
  just "Unicode". See more details below.

* **Permissive license:** MD4C is available under the MIT license.

* **Performance:** MD4C is [very fast](https://talk.commonmark.org/t/2520).


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

Example implementation of simple renderer is available in the `md2html`
directory which implements a conversion utility from Markdown to HTML.


## Markdown Extensions

The default behavior is to recognize only Markdown syntax defined by the
[CommonMark specification](http://spec.commonmark.org/).

However with appropriate flags, the behavior can be tuned to enable some
additional extensions:

* With the flag `MD_FLAG_COLLAPSEWHITESPACE`, a non-trivial whitespace is
  collapsed into a single space.

* With the flag `MD_FLAG_TABLES`, GitHub-style tables are supported.

* With the flag `MD_FLAG_TASKLISTS`, GitHub-style task lists are supported.

* With the flag `MD_FLAG_STRIKETHROUGH`, strike-through spans are enabled
  (text enclosed in tilde marks, e.g. `~foo bar~`).

* With the flag `MD_FLAG_PERMISSIVEURLAUTOLINKS` permissive URL autolinks
  (not enclosed in `<` and `>`) are supported.

* With the flag `MD_FLAG_PERMISSIVEEMAILAUTOLINKS`, permissive e-mail
  autolinks (not enclosed in `<` and `>`) are supported.

* With the flag `MD_FLAG_PERMISSIVEWWWAUTOLINKS` permissive WWW autolinks
  without any scheme specified (e.g. `www.example.com`) are supported. MD4C
  then assumes `http:` scheme.

* With the flag `MD_FLAG_LATEXMATHSPANS` LaTeX math spans (`$...$`) and
  LaTeX display math spans (`$$...$$`) are supported. (Note though that the
  HTML renderer outputs them verbatim in a custom tag `<x-equation>`.)

* With the flag `MD_FLAG_WIKILINKS`, wiki-style links (`[[link label]]` and
  `[[target article|link label]]`) are supported. (Note that the HTML renderer
  outputs them in a custom tag `<x-wikilink>`.)

* With the flag `MD_FLAG_UNDERLINE`, underscore (`_`) denotes an underline
  instead of an ordinary emphasis or strong emphasis.

Few features of CommonMark (those some people see as mis-features) may be
disabled:

* With the flag `MD_FLAG_NOHTMLSPANS` or `MD_FLAG_NOHTMLBLOCKS`, raw inline
  HTML or raw HTML blocks respectively are disabled.

* With the flag `MD_FLAG_NOINDENTEDCODEBLOCKS`, indented code blocks are
  disabled.


## Input/Output Encoding

The CommonMark specification generally assumes UTF-8 input, but under closer
inspection, Unicode plays any role in few very specific situations when parsing
Markdown documents:

1. For detection of word boundaries when processing emphasis and strong
   emphasis, some classification of Unicode characters (whether it is
   a whitespace or a punctuation) is needed.

2. For (case-insensitive) matching of a link reference label with the
   corresponding link reference definition, Unicode case folding is used.

3. For translating HTML entities (e.g. `&amp;`) and numeric character
   references (e.g. `&#35;` or `&#xcab;`) into their Unicode equivalents.

   However MD4C leaves this translation on the renderer/application; as the
   renderer is supposed to really know output encoding and whether it really
   needs to perform this kind of translation. (For example, when the renderer
   outputs HTML, it may leave the entities untranslated and defer the work to
   a web browser.)

MD4C relies on this property of the CommonMark and the implementation is, to
a large degree, encoding-agnostic. Most of MD4C code only assumes that the
encoding of your choice is compatible with ASCII, i.e. that the codepoints
below 128 have the same numeric values as ASCII.

Any input MD4C does not understand is simply seen as part of the document text
and sent to the renderer's callback functions unchanged.

The two situations (word boundary detection and link reference matching) where
MD4C has to understand Unicode are handled as specified by the following rules:

* If preprocessor macro `MD4C_USE_UTF8` is defined, MD4C assumes UTF-8 for the
  word boundary detection and for the case-insensitive matching of link labels.

  When none of these macros is explicitly used, this is the default behavior.

* On Windows, if preprocessor macro `MD4C_USE_UTF16` is defined, MD4C uses
  `WCHAR` instead of `char` and assumes UTF-16 encoding in those situations.
  (UTF-16 is what Windows developers usually call just "Unicode" and what
  Win32API generally works with.)

  Note that because this macro affects also the types in `md4c.h`, you have
  to define the macro both when building MD4C as well as when including
  `md4c.h`.

  Also note this is only supported in the parser (`md4c.[hc]`). The HTML
  renderer does not support this and you will have to write your own custom
  renderer to use this feature.

* If preprocessor macro `MD4C_USE_ASCII` is defined, MD4C assumes nothing but
  an ASCII input.

  That effectively means that non-ASCII whitespace or punctuation characters
  won't be recognized as such and that link reference matching will work in
  a case-insensitive way only for ASCII letters (`[a-zA-Z]`).


## Documentation

The API is quite well documented in the comments in the `md4c.h` header.

There is also [project wiki](http://github.com/mity/md4c/wiki) which provides
some more comprehensive documentation. However note it is incomplete and some
details may be little-bit outdated.


## FAQ

**Q: In my code, I need to convert Markdown to HTML. How?**

**A:** Indeed the API, as provided by `md4c.h`, is just a SAX-like Markdown
parser. Nothing more and nothing less.

That said, there is a complete HTML generator built on top of the parser in the
directory `md2html` (the files `render_html.[hc]` and `md2html/entity.[hc]`).
At this time, you have to directly reuse that code in your project.

There is [some discussion](https://github.com/mity/md4c/issues/82) whether this
should be changed (and how) in the future.

**Q: How does MD4C compare to a parser XY?**

**A:** Some other implementations combine Markdown parser and HTML generator
into a single entangled code hidden behind an interface which just allows the
conversion from Markdown to HTML, and they are unusable if you want to process
the input in any other way.

Even when the parsing is available as a standalone feature, most parsers (if
not all of them; at least within the scope of C/C++ language) are full DOM-like
parsers: They construct abstract syntax tree (AST) representation of the whole
Markdown document. That takes time and it leads to bigger memory footprint.

It's completely fine as long as you really need it. If you don't need the full
AST, there is very high chance that using MD4C will be faster and much less
memory-hungry.

Last but not least, some Markdown parsers are implemented in a naive way. When
fed with a [smartly crafted input pattern](test/pathological_tests.py), they
may exhibit quadratic (or even worse) parsing times. What MD4C can still parse
in a fraction of second may turn into long minutes or possibly hours with them.
Hence, when such a naive parser is used to process an input from an untrusted
source, the possibility of denial-of-service attacks becomes a real danger.

A lot of our effort went into providing linear parsing times no matter what
kind of crazy input MD4C parser is fed with. (If you encounter an input pattern
which leads to a sub-linear parsing times, please do not hesitate and report it
as a bug.)

**Q: Does MD4C perform any input validation?**

**A:** No.

CommonMark specification declares that any sequence of (Unicode) characters is
a valid Markdown document; i.e. that it does not matter whether some Markdown
syntax is in some way broken or not. If it is broken, it will simply not be
recognized and the parser should see the broken syntax construction just as a
verbatim text.

MD4C takes this a step further. It sees any sequence of bytes as a valid input,
following completely the GIGO philosophy (garbage in, garbage out).

If you need to validate that the input is, say, a valid UTF-8 document, you
have to do it on your own. You can simply validate the whole Markdown document
before passing it to the MD4C parser.

Alternatively, you may perform the validation on the fly during the parsing,
in the `MD_PARSER::text()` callback. (Given how MD4C works internally, it will
never break a sequence of bytes into multiple calls of `MD_PARSER::text()`,
unless that sequence is already broken to multiple pieces in the input by some
whitespace, new line character(s) and/or any Markdown syntax construction.)


## License

MD4C is covered with MIT license, see the file `LICENSE.md`.


## Links to Related Projects

Ports and bindings to other languages:

* [commonmark-d](https://github.com/AuburnSounds/commonmark-d):
  Port of MD4C to D language.
* [markdown-wasm](https://github.com/rsms/markdown-wasm):
  Markdown parser and HTML generator for WebAssembly, based on MD4C.

Software using MD4C:

* [Qt toolkit](https://www.qt.io/)
* [Textosaurus](https://github.com/martinrotter/textosaurus)
* [8th](https://8th-dev.com/)
