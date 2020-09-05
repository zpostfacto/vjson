# VJSON

A lightweight but friendly-to-use json parser and DOM in C++.

- Small: one ~700-line header, one .cpp file
- No external dependencies
- Use STL containers for storage: std::vector for arrays, std::map for objects
- Strings and keys stored and accessed as std::string.  Also can use
  ``const char *`` directly in many places.
- No exceptions, RTTI, ``iostream``, etc.
- DOM-style interface (read the whole document into some data structures).
  No SAX-style (streaming) interface.
- Parser only accepts the document as memory block, so entire source must
  fit in memory.  (``istream``, ``FILE*``, iterator interface, etc)
- Reports line/column of first parse error.
- Printing options: Some basic options for minified or indented.
  No framework for detailed customization.
- Parsing options: Comments and trailing commas can be optionally ignored.
  Does not remember formatting or retain these extra options, so this
  library cannot be used to modify a file and retain source formatting.

# BUILDING

Add ``vjson.cpp`` to your project and compile it.

# REALLY?  ANOTHER JSON PARSER?

Our biggest complaint with other json parsers is how tedius it can be
to write code to load up a file.  Specifically:

- Accessing elements of arrays/maps that are missing or the wrong type
  requiress too many temporary variables and ``if`` checks.
- Handling "bool"-ish values in a generous way, such as treating a
  numeric 0 as false.

If you want to be extremely strict about extra keys, value of the
wrong type, etc then there really is no shortcut for writing detailed,
explicit error handling.  This interface makes it easy write that kind of
code when the situation calls for it.  But in our experience it's
more common to have the more modest aim to do "something reasonable"
with a malformed document, with the least amount of effort possible.
If a key is missing, supply the default in a single function call.  If
an array or object is missing, act as if it is empty.  If a value
is present, but the wrong type, act as if it it missing.  Basically,
we don't want every other line to be error handling.
