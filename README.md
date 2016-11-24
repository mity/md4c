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

* **Compliance:** Generally MD4C aims to be compliant to the latest version of
  [CommonMark specification](http://spec.commonmark.org/). Right now we are
  quite close to CommonMark 0.27.

* **Extensions:** If explicitly enabled, the parser supports some commonly
  requested and accepted extensions. See below.

* **Compactness:** MD4C is implemented in one source file and one header file.

* **Embedding:** MD4C is easy to reuse in other projects, its API is very
  straightforward.

* **Portability:** MD4C builds and works on Windows and Linux, and it should
    be fairly trivial to build it also on other systems.

* **Encoding:** MD4C can compiled to recognize ASCII-only control characters,
  UTF-8 and, on Windows, also UTF-16 little endian, i.e. what is commonly called
  Unicode on Windows.

* **Permissive license:** MD4C is available under the MIT license.

* **Performance:** MD4C is quite fast.


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



## Extensions

By default, MD4C recognizes only elements defined by CommonMark specification.

Currently, these extensions are available:

 * With the flag `MD_FLAG_COLLAPSEWHITESPACE`, non-trivial whitespace is
   colalpsed into single space.
 * With the flag `MD_FLAG_TABLES`, GitHub-style tables are supported.
 * With the flag `MD_FLAG_PERMISSIVEURLAUTOLINKS` permissive URL autolinks
   (not enclosed in '<' and '>') are supported.
 * With the flag `MD_FLAG_PERMISSIVEAUTOLINKS`, ditto for e-mail autolinks.
 * With the flag `MD_FLAG_NOHTMLSPANS` or `MD_FLAG_NOHTML`, raw inline HTML
   or raw HTML blocks respectively are disabled.
 * With the flag `MD_FLAG_NOINDENTEDCODEBLOCKS`, indented code blocks are
   disabled.


## License

MD4C is covered with MIT license, see the file `LICENSE.md`.


## Reporting Bugs

If you encounter any bug, please be so kind and report it. Unheard bugs cannot
get fixed. You can submit bug reports here:

* http://github.com/mity/md4c/issues
