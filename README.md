# vjson

vjson is a lightweight but friendly-to-use JSON parser and DOM in C++.

- Small: one ~700-line header, one .cpp file
- No external dependencies
- Use STL containers for storage: ``std::vector`` for arrays;
  ``std::map`` for objects.
- Strings and keys stored and accessed as ``std::string``.  Also can use
  ``const char *`` directly in many places.
- No exceptions, RTTI, ``iostream``, etc.
- DOM-style interface (read the whole document into some data structures).
  No SAX-style (streaming) interface.
- Parser only accepts the document as memory block, so entire source must
  fit in memory.  (``istream``, ``FILE*``, iterator interface, etc)
- Printing options: Some basic options for minified or indented.
  No framework for detailed customization.
- Parsing options: Comments and trailing commas can be optionally ignored.
  Does not remember formatting or retain these extra options, so this
  library cannot be used to modify a file and retain source formatting.

# Really?  Another JSON parser?

Our biggest complaint with other JSON libs is how tedius it can be
to write code to load up a file.  Specifically:

- Accessing elements of arrays/maps that are missing or the wrong type
  requiress too many temporary variables and ``if`` checks.
- Handling "Booleanish" values in a reasonably generous way, such as
  treating a numeric 0 as false.

If you want to be extremely strict about extra/missing keys, values of the
wrong type, etc. then there really is no shortcut for writing detailed,
explicit error handling.  vjson makes it easy write that kind of code when
the situation calls for it.  It's when you have the more modest aim
of doing "something reasonable" with a malformed document, with the least
amount of effort possible, that vjson shines.  If a key is missing, supply
the default in a single function call.  If an array or object is missing,
act as if it is empty.  If a value is present, but the wrong type, act as
if it it missing.  When loadingparsing a document that is supposed to be a
single JSON object, just fail if the input is some other JSON value, and
don't make me write an explicit check for that case.

TL;DR: Error handling should not constitute the majority of the code to
load up a document, when your goal is simply "do something reasonable, and
don't crash."

# Building

Add ``vjson.cpp`` to your project and compile it.
