# vjson

[![CI](https://github.com/zpostfacto/vjson/actions/workflows/ci.yml/badge.svg)](https://github.com/zpostfacto/vjson/actions/workflows/ci.yml)

vjson is a lightweight but friendly-to-use JSON parser and DOM in C++.

Design goals:

- Good ergonomics when traversing the DOM (see below).  This is the most
  unique goal among the many other libraries available.
- Small: one ~750-line header, one ~1700-line .cpp file
- No external dependencies.  (No, not even boost.)
- Use STL containers for storage: ``std::vector`` for arrays;
  ``std::map`` for objects.
- Strings and keys stored as ``std::string``, but access using
  ``const char *`` is possible in most places.
- No use of exceptions, RTTI, ``iostream``, etc.  Safe to use in
  codebases that disable exceptions entirely.
- DOM-style interface: read the whole document into some data
  structures at once.  (No SAX-style / streaming interface.)
- Parser only accepts the document as memory block, so entire source must
  fit in memory.  (No ``istream``, ``FILE*``, iterator interface, etc)
- Printing options: Some basic options for minified or indented.
  (No framework for detailed customization.)
- If parsing fails, provide a good error message with a line number
  (important for "pretty" / hand-edited JSON) and byte offset (important
  for "minified" JSON).
- Parsing options: Comments and trailing commas can be optionally ignored.

Here are some goals this library doesn't have.  (If you need these, try
one of the other libraries below.)

- Remember formatting or comments, to support automated modification of
  documents
- Super-fast or efficient.  This lib aims to not be *grossly* inefficient,
  but it also avoids weird/complicated stuff in the name of efficiency.
  See some of the libs below if you need fast parsing or to load in huge
  documents.
- Header-only library.  I consider putting all the guts of the parsing code
  in a header an anti-pattern.
- Super streamlined syntax for constructing a DOM in C++ code

# Really?  Another JSON parser?

My biggest complaint with other JSON libs is how tedious it can be
to write code to load up a file.  Specifically:

- Code to traverse the DOM should not require an excessive number of ``if``
  statements to handle common situations:
  - An object does not have a value with the specified name
  - An object or array has a value, but it's the wrong type
- Handling "Booleanish" values in a reasonably generous way, such as
  treating a numeric 0 as false.
- Make it easy to deal with 64-bit numbers as strings.  (JSON only
  supports "numbers", which are usually represented as doubles.  A 64-bit
  value encoded as a "number" is very likely to be mutated in transit.)

If you want to be extremely strict about extra/missing keys, values of the
wrong type, etc. then there really is no shortcut for writing detailed,
explicit error handling.  vjson makes it easy to write that kind of code when
the situation calls for it.  But most of us have the more modest aim of
"detect common errors, but otherwise just do 'something reasonable' with
a malformed document, with the least amount of coding effort possible".
That's when vjson shines.  For example:

- If a key is missing, it can supply the default in a single function call.
- If an array or object is missing, return an empty one.
- If a value is present at the given key, but the wrong type, just act as
  if it is missing.
- If an array is only supposed to contain a certain type of value, iterate
  over the elements with that type in an idiomatic way, ignoring any elements
  that are the wrong type.
- When loading/parsing a document that is supposed to be a single JSON object,
  just fail if the input is some other JSON value, and don't make me write
  an explicit check for that case.

TL;DR: Error handling should not constitute the majority of the code to
load up a document, when your goal is simply "do something reasonable and
don't crash."

# Building

Add ``vjson.cpp`` to your project and compile it.

# Other C++ JSON libraries

Here are a few C++ JSON libraries.

JsonCpp is the library I found that came closest to meeting my needs.
The thing I ended up with meets my needs better (especially the
ergonomics and avoiding all the if() statements when traversing a
DOM), and I like my library better for my needs, but I must admit that
the difference is small enough that if I had found this library
earlier I might not have written mine.

- [JsonCpp](https://github.com/open-source-parsers/jsoncpp) Good
  library that is pretty lean.  Uses exceptions, but only if you
  have a bug.  (Importantly: does not use exceptions for bad
  files.)  Has a unique feature to preserve comments and reserialize.
  Slightly bigger than my lib, but satisfies many of the same goals.

Here are some other JSON libraries I looked into.  They don't satisfy
the particular goals I have for most of my projects, but they offer
unique features and tradeoffs and might be more appropriate for your
project.

- [JSON for Modern C++](https://github.com/nlohmann/json) A single
  header file (currently ~24K lines).  One design goal was the ability
  to put "JSON" looking syntax directly in C++ code and use Modern C++
  to parse it into a data structure.
- [minijson](https://giacomodrago.github.io/minijson/).  Just a parser,
  no DOM.
- [ThorsSerializer](https://github.com/Loki-Astari/ThorsSerializer) not a
  simple DOM.  More like go's approach to JSON serialization, it wants
  you to annotate your classes and load data directly into them.
- [jvar](https://github.com/YasserAsmi/jvar) Has a boost dependency.
  Looks like a good, simple alternative if you don't mind that.
- [JSONCONS](https://github.com/danielaparker/jsoncons) is a very
  fully-featured library.  Much bigger than mine.  Uses exceptions.
  (Note: no longer has a boost dependency as of recent versions.)
- [rapidjson](https://github.com/Tencent/rapidjson) Claims to be very
  fast.  Has a SAX model, which might be useful if you want that.  Too
  big for my needs, nearly 40 headers.
- [simdjson](https://github.com/simdjson/simdjson) Parses JSON using
  SIMD instructions; claims to parse faster than disk I/O speed.  If
  you need to parse very large documents at high throughput, this is
  worth a look.
- [Boost.JSON](https://boost.org/libs/json) A solid DOM-style library
  that has been part of Boost since 1.75 (2020).  Good choice if you
  are already using Boost.
- [picojson](https://github.com/kazuho/picojson) I used this at Valve
  for a while after abandoning ujson.  It uses exceptions and streams.
- [ujson](https://github.com/awangk/ujson) A tiny library that I was
  using for a while at Valve.  I found some bugs and reported them,
  the project has been abandoned.  The main reason for mentioning it
  is that it is partly the namesake of this library.
