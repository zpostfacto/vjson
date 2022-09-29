/////////////////////////////////////////////////////////////////////////////
//
// vjson is yet another C JSON parser and DOM
//
// See README for why I reinvented this wheel.
//
// See LICENSE for more information.
//
// This file has internals and stuff that don't belong in a header.
//
// If you are reading this file to understand how to use vjson, then I
// have utterly failed and you should provide feedback on what was unclear.
// The intention is that to *use* vjson, you only need to read the header.
//
/////////////////////////////////////////////////////////////////////////////

#include <stdarg.h>

#include "vjson.h"

namespace vjson {

// It really seems like C++ ought to make this easier, right?
template <typename T> void InvokeDestructor( T &x ) { x.~T(); }
template <typename T, typename A> void InvokeConstructor( T &x, A&& a ) { new (&x) T{ std::forward<A>( a ) }; }
template <typename T> void InvokeConstructor( T &x) { new (&x) T{}; }

const Object &GetStaticEmptyObject()
{
	static Object dummy;
	VJSON_ASSERT( dummy.ObjectSize() == 0 );
	return dummy;
}
const Array &GetStaticEmptyArray()
{
	static Array dummy;
	VJSON_ASSERT( dummy.ArraySize() == 0 );
	return dummy;
}
const Value &GetStaticNullValue()
{
	static Value dummy; // note that default constructor sets to null
	VJSON_ASSERT( dummy.IsNull() );
	return dummy;
}

/////////////////////////////////////////////////////////////////////////////
//
// DOM manipulation
//
/////////////////////////////////////////////////////////////////////////////

void Value::InternalDestruct()
{
	if ( _type == kObject )
		InvokeDestructor( _object );
	else if ( _type == kArray )
		InvokeDestructor( _array );
	else if ( _type == kString )
		InvokeDestructor( _string );
	_type = kDeleted; // Not necessary, but helps to catch bugs
}

void Value::InternalConstruct( const Value &x )
{
	_type = x._type;
	if ( _type == kObject )
		InvokeConstructor( _object, x._object );
	else if ( _type == kArray )
		InvokeConstructor( _array, x._array );
	else if ( _type == kString )
		InvokeConstructor( _string, x._string );
	else
		_dummy = x._dummy; // Some other primitive -- just copy 8 bytes
	static_assert( sizeof(_dummy) >= sizeof(_double), "_dummy must be as big as all primitives" );
}

void Value::InternalConstruct( Value &&x )
{
	_type = x._type;
	if ( _type == kObject )
		InvokeConstructor( _object, std::move( x._object ) );
	else if ( _type == kArray )
		InvokeConstructor( _array, std::move( x._array ) );
	else if ( _type == kString )
		InvokeConstructor( _string, std::move( x._string ) );
	else
		_dummy = x._dummy; // Some other primitive -- just copy 8 bytes
	static_assert( sizeof(_dummy) >= sizeof(_double), "_dummy must be as big as all primitives" );
}

Value *Value::InternalAtIndex( size_t idx, EValueType t ) const
{
	Value *v = (const_cast<Value*>(this))->ValuePtrAtIndex( idx );
	return (v && v->_type == t) ? v : nullptr; 
}

Value *Value::ValuePtrAtKey( const std::string &key )
{
	if ( _type != kObject )
		return nullptr;
	auto it = _object.find( key );
	if ( it == _object.end() )
		return nullptr;
	return &it->second;
}

Value *Value::ValuePtrAtKey( const char *key )
{
	if ( _type != kObject )
		return nullptr;
	auto it = _object.find( std::string( key ) ); // UGGGGG PLEASE KILL ME NOW.  Constructing a std::string just to do lookup?  My kingdom for a decent dict type!  STL is such a failure
	if ( it == _object.end() )
		return nullptr;
	return &it->second;
}

Value *Value::InternalAtKey( const std::string &key, EValueType t ) const
{
	Value *v = (const_cast<Value*>(this))->ValuePtrAtKey( key );
	return (v && v->_type == t) ? v : nullptr; 
}

Value *Value::InternalAtKey( const char *key, EValueType t ) const
{
	Value *v = (const_cast<Value*>(this))->ValuePtrAtKey( key );
	return (v && v->_type == t) ? v : nullptr; 
}

Value::Value( EValueType type ) : _type( type )
{
	if ( _type == kObject )
		InvokeConstructor( _object );
	else if ( _type == kArray )
		InvokeConstructor( _array );
	else if ( _type == kString )
		InvokeConstructor( _string );
	else
		_double = 0.0; // Here we assume that double 0.0 representation is all zeros, so that the bool value will be false.
}

Value::Value( const char *x ) : _type( kString ), _string( x ) {}
Value::Value( const std::string &x ) : _type( kString ), _string( x ) {}
Value::Value( std::string &&x ) : _type( kString ), _string( std::forward<std::string>( x ) ) {}
Value::Value( const RawObject & x ) : _type( kObject ), _object( x ) {}
Value::Value( RawObject &&      x ) : _type( kObject ), _object( std::forward<RawObject>( x ) ) {}
Value::Value( const RawArray &  x ) : _type( kArray ), _array( x ) {}
Value::Value( RawArray &&       x ) : _type( kArray ), _array( std::forward<RawArray>( x ) ) {}

Value &Value::operator=( const Value &x )
{
	if ( _type == x._type )
	{
		if ( this != &x )
		{
			if ( _type == kObject )
				_object = x._object;
			else if ( _type == kArray )
				_array = x._array;
			else if ( _type == kString )
				_string = x._string;
			else
				_dummy = x._dummy; // Some other primitive -- just copy 8 bytes
		}
	}
	else
	{
		InternalDestruct();
		InternalConstruct(x);
	}
	return *this;
}

Value &Value::operator=( Value &&x )
{
	if ( _type == x._type )
	{
		if ( this != &x ) // Not sure if this is necessary.  Do the STL types protect against self-assignment?
		{
			if ( _type == kObject )
				_object = std::move( x._object );
			else if ( _type == kArray )
				_array = std::move( x._array );
			else if ( _type == kString )
				_string = std::move( x._string );
			else
				_dummy = x._dummy; // Some other primitive -- just copy 8 bytes
		}
	}
	else
	{
		InternalDestruct();
		InternalConstruct(x);
	}
	return *this;
}

Value &Value::operator=( const char *x )
{
	if ( _type == kString )
	{
		if ( x != _string.c_str() ) // Not sure if this is necessary.  Do the STL types protect against self-assignment?
			_string = x;
	}
	else
	{
		InternalDestruct();
		_type = kString;
		InvokeConstructor( _string, x );
	}
	return *this;
}
Value &Value::operator=( const std::string &x )
{
	if ( _type == kString )
	{
		if ( &x != &_string ) // Not sure if this is necessary.  Do the STL types protect against self-assignment?
			_string = x;
	}
	else
	{
		InternalDestruct();
		_type = kString;
		InvokeConstructor( _string, x );
	}
	return *this;
}
Value &Value::operator=( std::string &&x )
{
	if ( _type == kString )
	{
		if ( &x != &_string ) // Not sure if this is necessary.  Do the STL types protect against self-assignment?
			_string = std::forward<std::string>( x );
	}
	else
	{
		InternalDestruct();
		_type = kString;
		InvokeConstructor( _string, std::forward<std::string>( x ) );
	}
	return *this;
}

Value &Value::operator=( const RawArray &x )
{
	if ( _type == kArray )
	{
		if ( &x != &_array ) // Not sure if this is necessary.  Do the STL types protect against self-assignment?
			_array = x;
	}
	else
	{
		InternalDestruct();
		_type = kArray;
		InvokeConstructor( _array, x );
	}
	return *this;
}

Value &Value::operator=( RawArray &&x )
{
	if ( _type == kArray )
	{
		if ( &x != &_array ) // Not sure if this is necessary.  Do the STL types protect against self-assignment?
			_array = std::forward<RawArray>( x );
	}
	else
	{
		InternalDestruct();
		_type = kArray;
		InvokeConstructor( _array, std::forward<RawArray>( x ) );
	}
	return *this;
}

Value &Value::operator=( const RawObject &x )
{
	if ( _type == kObject )
	{
		if ( &x != &_object ) // Not sure if this is necessary.  Do the STL types protect against self-assignment?
			_object = x;
	}
	else
	{
		InternalDestruct();
		_type = kObject;
		InvokeConstructor( _object, x );
	}
	return *this;
}

Value &Value::operator=( RawObject &&x )
{
	if ( _type == kObject )
	{
		if ( &x != &_object ) // Not sure if this is necessary.  Do the STL types protect against self-assignment?
			_object = std::forward<RawObject>( x );
	}
	else
	{
		InternalDestruct();
		_type = kObject;
		InvokeConstructor( _object, std::forward<RawObject>( x ) );
	}
	return *this;
}

void Value::SetEmptyObject()
{
	if ( _type == kObject )
	{
		_object.clear();
	}
	else
	{
		InternalDestruct();
		_type = kObject;
		InvokeConstructor( _object );
	}
}

void Value::SetEmptyArray()
{
	if ( _type == kArray )
	{
		_array.clear();
	}
	else
	{
		InternalDestruct();
		_type = kArray;
		InvokeConstructor( _array );
	}
}

ETruthy Value::AsTruthy() const
{
	switch ( _type )
	{
		case kNull:
			return kFalsish;

		case kBool:
			return _bool ? kTruish : kFalsish;

		case kNumber:
			if ( _double == 0.0 )
				return kFalsish;
			if ( _double > 0.0 || _double < 0.0 )
				return kTruish;
			break; // NaN, etc

		case kString:
		{
			if ( _string.empty() )
				return kFalsish;
			const char *s = _string.c_str();

			// Manually do case-sensitive compare against "true" / "false".
			// I don't want to mess with compiler compatibility, locales, etc, etc

			if (
				( s[0] == 't' || s[0] == 'T' ) &&
				( s[1] == 'r' || s[1] == 'R' ) &&
				( s[2] == 'u' || s[2] == 'U' ) &&
				( s[3] == 'e' || s[3] == 'E' ) &&
				s[4] == '\0'
			) {
				return kTruish;
			}

			if (
				( s[0] == 'f' || s[0] == 'F' ) &&
				( s[1] == 'a' || s[1] == 'A' ) &&
				( s[2] == 'l' || s[2] == 'L' ) &&
				( s[3] == 's' || s[3] == 'S' ) &&
				( s[4] == 'e' || s[4] == 'E' ) &&
				s[5] == '\0'
			) {
				return kFalsish;
			}
		} break;

		case kObject:
		case kArray:
			break;

		default:
			VJSON_ASSERT( false );
			break;
	}

	// Neither true nor false
	return kGibberish;
}

EResult Value::GetTruthy( bool &outX ) const
{
	ETruthy t = AsTruthy();
	if ( t == kTruish )
	{
		outX = true;
		return kOK;
	}
	if ( t == kFalsish )
	{
		outX = false;
		return kOK;
	}
	return kWrongType;
}

/////////////////////////////////////////////////////////////////////////////
//
// Printing
//
/////////////////////////////////////////////////////////////////////////////

struct Printer
{
	Printer( const PrintOptions &o )
	: opt(o)
	, indent_len( opt.indent ? strlen( opt.indent ) : 0 )
	{
	}
	const PrintOptions &opt;
	const size_t indent_len; // If nonzero, then we pretty-print
	size_t indent_level = 0;

	std::string buf;

	// Make sure we have room for l additional characters
	inline void Reserve( size_t l )
	{
		l += buf.length();
		if ( l <= buf.capacity() )
			return;
		l = l*3/2; // Grow by 50%
		l = ( l + 0xfff ) >> 12U << 12U; // Round up to nearest 4K

		// Set new capacity
		buf.reserve( l );
	}

	inline void Append( const char * s, size_t l )
	{
		Reserve( l );
		buf.append( s, l );
	}

	inline void Append( char c )
	{
		Reserve( 1 );
		buf.push_back( c );
	}

	void AppendQuotedString( const std::string &s )
	{

		// Make one pass to determine how much space we will
		// need to reserve.
		size_t l = 2;
		for ( char c: s )
		{
			switch ( c )
			{
				case '\"':
				case '\\':
				case '\b':
				case '\f':
				case '\n':
				case '\r':
				case '\t':
					l += 2;
					break;
				default:
					if ( (unsigned char)c < 0x20 )
						l += 6;
					else
						++l;
			}
		}

		Reserve( l );
		buf.push_back( '\"' );
		if ( l == s.length()+2 )
		{
			// Fast path if nothing needs to be escaped
			buf.append( s );
		}
		else
		{
			for ( char c: s )
			{
				switch ( c )
				{
					case '\"': buf.append( "\\\"", 2 ); break;
					case '\\': buf.append( "\\\\", 2 ); break;
					case '\b': buf.append( "\\\b", 2 ); break;
					case '\f': buf.append( "\\\f", 2 ); break;
					case '\n': buf.append( "\\\n", 2 ); break;
					case '\r': buf.append( "\\\r", 2 ); break;
					case '\t': buf.append( "\\\t", 2 ); break;
					default:
						if ( (unsigned char)c < 0x20 )
						{
							static const char hexdigits[] = "0123456789abcdef";
							buf.append( "\\u00", 4 );
							buf.push_back( hexdigits[ (unsigned char)c >> 4U ] );
							buf.push_back( hexdigits[ c & 0xf ] );
						}
						else
						{
							buf.push_back( c );
						}
				}
			}
		}
		buf.push_back( '\"' );
	}

	void BeginBlock( char delim, size_t num_children )
	{
		// Pretty printing?
		if ( indent_len )
		{
			// Reserve
			Reserve(
				indent_len*indent_level // indent
				+ 128 + (indent_len+4)*num_children // body
				+(  ( (indent_len+2)*indent_level*indent_level )>>1 ) // unindent all levels
			);
			buf.push_back( delim );
			buf.push_back( '\n' );
			for ( size_t i = 0 ; i < indent_level ; ++i )
				buf.append( opt.indent, opt.indent + indent_level );
		}
		else
		{
			Reserve( 128 + num_children*2 + indent_level ); // body + unindent
			buf.push_back( delim );
		}

		++indent_level;
	}

	void EndBlock( char delim )
	{
		VJSON_ASSERT( indent_level > 0 );
		--indent_level;

		if ( indent_len )
		{
			// Reserve enough to unindent all remaining levels plus some extra
			Reserve( 32 + (indent_len+2)*(indent_level+1)*indent_level/2 );
			buf.push_back( '\n' );
			Indent();
		}
		else
		{
			// Reserve enough to unindent all remaining levels plus some extra
			Reserve( 32 + indent_level );
		}
		buf.push_back( delim );
	}

	inline void Indent()
	{
		VJSON_ASSERT( indent_len > 0 );
		for ( size_t i = 0 ; i < indent_level ; ++i )
			buf.append( opt.indent );
	}

	// If this is the first element, clear the flag.
	// Otherwise, print a comma
	inline void Comma( bool &is_first )
	{
		if ( is_first )
		{
			is_first = false;
		}
		else if ( indent_len )
		{
			// Pretty print
			buf.append( ",\n", 2 );
			Indent();
		}
		else
		{
			// Minified, just print a comma, and leave the buffering up to std::string,
			// since overflow is very unlikely
			buf.push_back( ',' );
		}
	}

	void PrintArray( const RawArray &arr )
	{
		// Special case for empty
		if ( arr.empty() )
		{
			Append( "[]", 2 );
			return;
		}

		BeginBlock( '[', arr.size() );

		bool is_first = true;
		for ( const Value &v: arr )
		{
			Comma( is_first );
			PrintValue( v );
		}

		EndBlock( ']' );
	}

	void PrintObject( const RawObject &obj )
	{

		// Special case for empty
		if ( obj.empty() )
		{
			Append( "{}", 2 );
			return;
		}

		BeginBlock( '{', obj.size() );
		const char *colon = indent_level>0 ? ": " : ":";
		bool is_first = true;
		for ( const ObjectItem &item: obj )
		{
			Comma( is_first );
			AppendQuotedString( item.first );
			buf.append( colon );
			PrintValue( item.second );
		}
		EndBlock( '}' );
	}

	void PrintValue( const Value &v )
	{
		switch ( v.Type() )
		{
			case kNull:
				Append( "null", 4 );
				break;

			case kString:
				AppendQuotedString( v.AsString().c_str() );
				break;

			case kDouble:
			{
				char temp[ 64 ];
				size_t l = snprintf( temp, sizeof(temp), "%g", v.AsDouble() );
				char *comma = strchr( temp, ',' ); // Check if locale is using comma as digit separator
				if ( comma )
					*comma = '.';
				Append( temp, l );
				break;
			}

			case kBool:
				if ( v.AsBool() )
					Append( "true", 4 );
				else
					Append( "false", 5 );
				break;

			case kObject:
				PrintObject( v.AsObject().Raw() );
				break;

			case kArray:
				PrintArray( v.AsArray().Raw() );
				break;
		}
	}
};

std::string ToString( const Value &v, const PrintOptions &opt )
{
	Printer p( opt );
	p.PrintValue( v );
	return std::move( p.buf );
}

/////////////////////////////////////////////////////////////////////////////
//
// Parsing
//
/////////////////////////////////////////////////////////////////////////////

struct Parser
{
	Parser( ParseContext &c, const char *b, const char *e )
	: ctx(c), begin(b), end(e)
	{
		ptr = begin;
		line = 1;

		ctx.error_byte_offset = 0;
		ctx.error_line = 0;
		ctx.error_message.clear();
	}

	ParseContext &ctx;

	// Original extents
	const char *const begin;
	const char *const end;

	// Current cursor.
	const char *ptr;
	int line;

	// Return the next character, or -1 if we are at EOF
	inline int Peek() const
	{
		if ( ptr >= end )
			return -1;
		return *ptr;
	}

	void Error( const char *msg )
	{
		ctx.error_byte_offset = int( ptr - begin );
		ctx.error_line = line;
		ctx.error_message = msg;
	}

	void Errorf( const char *fmt, ... )
	{
		char msg[ 256 ];
		va_list ap;
		va_start( ap, fmt );
		vsnprintf( msg, sizeof(msg), fmt, ap );
		va_end( ap );
		Error( msg );
	}

	// Advance ptr to skip past any whitespace.
	// If C++ comments are allowed, we will also skip those.
	// There is a bit of extra complexity here to maintain the
	// line number properly for different kinds of newlines.
	void SkipWhitespaceAndComments()
	{
		while ( ptr < end )
		{
			// Handle newlines of various variety
			if ( *ptr == '\n')
			{
				++ptr;
				++line;
				if ( ptr >= end )
					break;
				if ( *ptr == '\r' )
					++ptr;
			}
			else if ( *ptr == '\r' )
			{
				++ptr;
				++line;
				if ( ptr >= end )
					break;
				if ( *ptr == '\n' )
					++ptr;
			}
			else if ( *ptr == ' ' || *ptr == '\t' )
			{
				// Whitespace
				++ptr;
			}
			else if ( *ptr == '/' && ptr+1 < end && ptr[1] == '/' && ctx.allow_cpp_comments )
			{
				// C++ comment.  Skip to the newline
				for (;;)
				{

					if ( *ptr == '\n')
					{
						++ptr;
						++line;
						if ( ptr >= end )
							return;
						if ( *ptr == '\r' )
							++ptr;
						break; // to outer loop.  keep eating whitespace / comments
					}
					if ( *ptr == '\r' )
					{
						++ptr;
						++line;
						if ( ptr >= end )
							return;
						if ( *ptr == '\n' )
							++ptr;
						break; // to outer loop.  keep eating whitespace / comments
					}

					++ptr;
					if ( ptr >= end )
						return;
				}
			}
			else
			{
				// Hit non-whitespace.  Time to stop
				break;
			}
		}
	}

	// If we are at EOF, then report the error and return false.
	// Otherwise, return true.
	bool CheckEOF()
	{
		if ( ptr < end )
			return true;
		Error( "Unexpected end-of-input" );
		return false;
	}

	// Parse the 4 hex digits in an \u-escaped character.  Return the numeric value.
	// If input is bogus, error and return <0
	int ParseUChar( const char *s )
	{
		if ( s+4 > end )
		{
			ptr = s-1; // Set pointer so we can report the location more accurately
			Errorf( "End of input during \\u escape sequence", *s );
			return -1;
		}

		int x = 0;
		for ( int i = 0 ; i < 4 ; ++i )
		{
			x <<= 4;
			char c = *s;
			if ( '0' <= c && c <= '9' )
				x += c - '0';
			else if ( 'a' <= c && c <= 'f' )
				x += c - 'a' + 0xa;
			else if ( 'A' <= c && c <= 'F' )
				x += c - 'A' + 0xa;
			else
			{
				ptr = s; // Set pointer so we can report the location more accurately
				Errorf( "Character 0x%02x is not a hex digit; invalid \\u-escaped sequence", c );
				return false;
			}
			++s;
		}

		return x;
	}

	bool ParseQuotedString( std::string &out )
	{
		VJSON_ASSERT( ptr < end && *ptr == '\"' );
		++ptr;

		// Make one pass through to calculate the length (in bytes)
		size_t escape_overhead = 0;
		const char *s = ptr;
		for (;;)
		{
			if ( s >= end )
			{
unterminated_string:
				// Leave ptr at start of string.  Putting it at the end is usually useless,
				// But sometimes it's hard to find a straw opening quote.
				Error( "Unterminated string" );
				return false;
			}

			// End of string?
			if ( *s == '\"' )
				break;

			// Control characters are illegal inside quoted strings
			if ( *s < 0x20 )
			{
				ptr = s;

				// Provide a more specific error message for newlines,
				// since this is a common mistake and "control character"
				// is overly technical
				if ( *s == '\n' || *s == '\r' )
					Errorf( "Newline character (0x%02x) in string.  (Missing closing quote?)", *s );
				else
					Errorf( "Control character 0x%02x is illegal in string", *s );
				return false;
			}

			// Handle escaped characters
			if ( *s == '\\' )
			{
				++s;
				if ( s >= end )
					goto unterminated_string;

				switch ( *s )
				{
					case 'u':
					{
						++s;
						int x = ParseUChar( s );
						if ( x < 0 )
							return false;

						// 4 hex digits can only encode unicode codepoints up to 0xffff.
						// So that's either 1 or two bytes output.
						if ( x <= 0x7f )
						{
							// 5 bytes input, 1 byte output.
							escape_overhead += 4;
						}
						else if ( x <= 0x7ff )
						{
							// 5 bytes input, 2 bytes output.
							escape_overhead += 3;
						}
						else
						{
							// 5 bytes input, 3 bytes output.
							escape_overhead += 2;
						}
						s += 4;
					} break;

					case '"':
					case '\\':
					case 'b':
					case 'f':
					case 'n':
					case 'r':
					case 't':
						// Two characters, which will be encoded as a single character
						escape_overhead += 1;
						break;

					// Here we could add an option to allow for other escaped characters,
					// for example a single quote.  JSON spec does not allow this, but it's
					// a common mistake when hand-editing

					default:
						ptr = s;
						if ( *s > 0x20 && *s < 128 )
							Errorf( "Invalid escape sequence '\\%c' in string", *s );
						else
							Errorf( "Character 0x%2x is not valid after '\\' in string", *s );
						return false;
				}
			}
			else
			{
				// Ordinary character, nothing to do.
				++s;

				// FIXME - should we check for invalid UTF-8?
				// Right now we are being naive and passing this along, but perhaps
				// we should have a strict mode where we actually check.
			}
		}

		// Fast path for no escaped characters.
		// (Including empty string)
		if ( escape_overhead == 0 )
		{
			out.assign( (const char *)ptr, (const char *)s );
			ptr = s + 1;
		}
		else
		{

			// Some escaped characters.
			VJSON_ASSERT( ptr + escape_overhead < s );

			// Pre-allocate the output string.
			// Ugggggg this is going to initialize the string with a
			// bunch of zeros, which we are then going to overwrite below.
			// The STL is just so insanely dumb sometimes.  We could use
			// reserve() to avoid this, but that would mean we have to use
			// push_back or append() below, which is going to be slower.
			// It's probably faster to just suffer the memset here?
			out.resize( s - ptr - escape_overhead );

			// Get a writable pointer to the string.  Note that this const
			// cast is not necessary beginning with C++17
			char *d = const_cast<char*>( out.data() );

			// Re-scan tthe string, processing the escape sequences
			while ( ptr < s )
			{

				// Regular character?
				if ( *ptr != '\\' )
				{
					*d = *ptr;
					++d;
					++ptr;
				}
				else
				{

					// Escaped character
					++ptr;
					switch ( *(ptr++) )
					{
						case 'u':
						{

							// Parse the character.  Cast to unsigned because it should
							// not have failed.  It somebody is missing with our buffer
							// while we are reading from it, and we do actually fail,
							// we'll handle that below
							unsigned x = (unsigned)ParseUChar( ptr );

							if ( x <= 0x7F )
							{
								d[0] = (unsigned char)x;
								d += 1;
							}
							else if ( x <= 0x7FF )
							{
								d[0] = (unsigned char)(x >> 6) | 0xC0;
								d[1] = (unsigned char)(x & 0x3F) | 0x80;
								d += 2;
							}
							else if ( x <= 0xFFFF )
							{
								d[0] = (unsigned char)(x >> 12) | 0xE0;
								d[1] = (unsigned char)((x >> 6) & 0x3F) | 0x80;
								d[2] = (unsigned char)(x & 0x3F) | 0x80;
								d += 3;
							}
							else
							{
								// Uhhhhh
								Errorf( "Internal parse BUG, or buffer modified while parsing" );
								return false;
							}
							ptr += 4;

						} break;

						case '\"': *(d++) = '\"'; break;
						case '\\': *(d++) = '\\'; break;
						case '/':  *(d++) = '/';  break;
						case 'b':  *(d++) = '\b'; break;
						case 'f':  *(d++) = '\f'; break;
						case 'n':  *(d++) = '\n'; break;
						case 'r':  *(d++) = '\r'; break;
						case 't':  *(d++) = '\t'; break;

						// Here we could add an option to allow for other escaped characters,
						// for example a single quote.  JSON spec does not allow this, but it's
						// a common mistake when hand-editing

						default:
							// Should have detected this above
							--ptr;
							Errorf( "Internal parse BUG" );
							return false;
					}
				}
			}

			// Safety check that we wrote exactly what we expected to
			if ( d != out.c_str() + out.size() )
			{
				Errorf( "Internal parse BUG, or buffer modified while parsing" );
				return false;
			}

			// Eat the final closing quote.
			++ptr;
		}

		return true;
	}

	bool ParseObject( Value &out )
	{
		out.SetEmptyObject();

		// Peek first character, special case for empty object
		SkipWhitespaceAndComments();
		if ( !CheckEOF() )
			return false;
		if ( *ptr == '}' )
		{
			++ptr;
			return true;
		}

		// OK, parse items into the object
		RawObject &rawObj = out.AsObject().Raw();

		for (;;)
		{

			// Next character must be a quote character
			if ( *ptr != '\"' )
			{
				Errorf( "Expected '\"' to begin JSON object key, but found '%c' (0x%02x) instead", *ptr, *ptr );
				return false;
			}

			// Parse the key
			std::string key;
			if ( !ParseQuotedString( key ) )
				return false;

			// Locate and eat the colon
			SkipWhitespaceAndComments();
			if ( !CheckEOF() )
				return false;
			if ( *ptr != ':' )
			{
				Errorf( "Expected ':' but found '%c' (0x%02x) instead", *ptr, *ptr );
				return false;
			}
			++ptr;

			// Add new entry at this key.  NOTE: JSON spec
			// does not specify what to do in case of duplicate key.
			// We are not detecting it, and are using the "last one wins"
			// rule.
			Value &val = rawObj[ key ];
			if ( !ParseRequiredValue( val ) )
				return false;

			// Next thing must be a comma, or a bracket to end the input
			SkipWhitespaceAndComments();
			if ( !CheckEOF() )
				return false;
			if ( *ptr == '}' )
			{
				++ptr;
				break;
			}
			if ( *ptr != ',' )
			{
				Errorf( "Expected '}' or ',' but found '%c' (0x%02x) instead", *ptr, *ptr );
				return false;
			}

			// Eat the comma
			++ptr;

			// End of object here?  (Extra trailing comma)
			SkipWhitespaceAndComments();
			if ( !CheckEOF() )
				return false;
			if ( *ptr == '}' )
			{
				if ( !ctx.allow_trailing_comma )
				{
					Errorf( "JSON value required here.  (Strict parsing mode; trailing comma not permitted)", *ptr, *ptr );
					return false;
				}
				++ptr;
				break;
			}
		}

		return true;
	}

	bool ParseArray( Value &out )
	{
		out.SetEmptyArray();

		// Peek first character, special case for empty array
		SkipWhitespaceAndComments();
		if ( !CheckEOF() )
			return false;
		if ( *ptr == ']' )
		{
			++ptr;
			return true;
		}

		// OK, parse items into the array
		RawArray &rawArray = out.AsArray().Raw();

		for (;;)
		{

			// Parse into a temporary object
			Value val;
			if ( !ParseRequiredValue( val ) )
				return false;

			// Move into array
			rawArray.emplace_back( std::move( val ) );

			// Next thing must be a comma, or a bracket to end the input
			SkipWhitespaceAndComments();
			if ( !CheckEOF() )
				return false;
			if ( *ptr == ']' )
			{
				++ptr;
				break;
			}
			if ( *ptr != ',' )
			{
				Errorf( "Expected ']' or ',' but found '%c' (0x%02x) instead", *ptr, *ptr );
				return false;
			}

			// Eat the comma
			++ptr;

			// End of array here?  (Extra trailing comma)
			SkipWhitespaceAndComments();
			if ( !CheckEOF() )
				return false;
			if ( *ptr == ']' )
			{
				if ( !ctx.allow_trailing_comma )
				{
					Errorf( "JSON value required here.  (Strict parsing mode; trailing comma not permitted)", *ptr, *ptr );
					return false;
				}
				++ptr;
				break;
			}
		}

		return true;
	}

	bool ParseNumber( Value &out )
	{
		const char *start = ptr; // Save, so that we can rewind if we need to report error

		// Copy characters into temp buffer.  We use a max size buffer here, which is NOT strictly
		// according to JSON standard.  In practice, this seems very unlikely to matter.
		// If anybody wants to fix this in a way that doesn't slow down the much more common
		// case of a reasonable input (immediately resorting to dynamic allocation is an example
		// of a method that does not fit this criteria), that would be great.  But I suspect
		// that in practice, supporting these numbers is not worth handling.
		constexpr int N = 256;
		char buf[N];

		// Store the first character
		buf[0] = *(ptr++);
		char *d = buf+1;
		char *const d_end = buf + N - 1; // leave room for '\0'

		// If negative, next character *must* be a digit
		if ( buf[0] == '-' )
		{
			if ( !CheckEOF() )
				return false;
			if ( *ptr < '0' || *ptr > '9' )
			{
				Errorf( "Expected digit after '-' in JSON number, found 0x%02x instead", *ptr );
				return false;
			}
			*(d++) = *(ptr++);
		}
		else
		{
			VJSON_ASSERT( buf[0] >= '0' && buf[0] <= '9' );
		}

		// Macro to store the character into the buffer, handling overflow
		static const char number_too_long[] = "JSON number contains too many characters";
		#define STORE_CHARACTER( char_to_store ) \
			do { \
				if ( d >= d_end ) { \
					ptr = start; \
					Error( number_too_long ); \
					return false; \
				} \
				*(d++) = (char_to_store); \
			} while ( false )

		// Remaining digits before '.'?  Noter that JSON spec does not
		// allow numbers with leading zeros.
		char first_digit = d[-1];
		if ( first_digit != '0' )
		{
			VJSON_ASSERT( first_digit >= '1' && first_digit <= '9' );
			while ( ptr < end && *ptr >= '0' && *ptr <= '9' )
			{
				STORE_CHARACTER( *ptr );
				++ptr;
			}
		}
		else
		{
			// Add a special error handling here with a more specific error message
			if ( ptr < end && *ptr >= '0' && *ptr <= '9' )
			{
				ptr = start;
				Error( "Leading zeros / octal format not allowed in JSON number" );
				return false;
			}
		}

		// Fraction?
		if ( ptr < end && *ptr == '.' )
		{
			++ptr;

			// Store whatever character atof will expect to use as the decimal point
			// NOTE: Here we are assuming that decimal_point will point to a string
			// of exactly one characters.  I am not aware of any locales for which
			// this is not true.  Also, this code could be optimized if we assume that
			// the locale will not change.
			STORE_CHARACTER( *localeconv()->decimal_point );

			// Digits after the decimal
			while ( ptr < end && *ptr >= '0' && *ptr <= '9' )
			{
				STORE_CHARACTER( *ptr );
				++ptr;
			}
		}

		// Exponent?
		if ( ptr < end && *ptr == 'e' || *ptr == 'E' )
		{
			STORE_CHARACTER( *ptr );
			++ptr;

			// Another character is required.
			if ( !CheckEOF() )
				return false;

			// Is it an optional sign?
			if ( *ptr == '-' || *ptr == '+' )
			{
				STORE_CHARACTER( *ptr );
				++ptr;

				// Another character is required.
				if ( !CheckEOF() )
					return false;
			}

			// Next character must be a digit
			if ( *ptr < '0' || *ptr > '9' )
			{
				Errorf( "Digit is required after exponent in JSON number; found 0x%02x instead", *ptr );
				return false;
			}

			// Store all remaining digits
			do
			{
				STORE_CHARACTER( *ptr );
				++ptr;
			} while ( ptr < end && *ptr >= '0' && *ptr <= '9' );
		}

		#undef STORE_CHARACTER

		// Use sscanf to parse it back
		double number_val;
		*d = '\0';
		if ( sscanf( buf, "%lf", &number_val ) != 1 )
		{
			// Our parsing above should have made this impossible!
			VJSON_ASSERT( "Parse bug" );
			ptr = start;
			Error( "Invalid number" );
			return false;
		}

		out = number_val;
		return true;
	}

	// Skip to the next value and parse it
	bool ParseRequiredValue( Value &out )
	{
		SkipWhitespaceAndComments();
		if ( !CheckEOF() )
			return false;
		return InternalParseValue( out );
	}

	// Parse a value, when we know that we are not at EOF,
	// and we have skipped whitespace and comments
	bool InternalParseValue( Value &out )
	{

		// Check character to know what it is
		switch ( *ptr )
		{
			case '\"':
			{
				std::string s;
				if ( !ParseQuotedString( s ) )
					return false;
				out = std::move(s);
				return true;
			}

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '-':
			//case '.': case '+' // Should we enable a less-strict format where these are allowed?
			// NOTE: Also we do not support inf and nan.  Those are illegal according to JSON spec, but it might be useful to add a flag to allow them.
				return ParseNumber( out );

			case '{':
				++ptr;
				return ParseObject( out );

			case '[':
				++ptr;
				return ParseArray( out );

			case 't':
				if ( ptr + 4 <= end && ptr[1] == 'r' && ptr[2] == 'u' && ptr[3] == 'e' )
				{
					out = true;
					ptr += 4;
					return true;
				}
				break;

			case 'f':
				if ( ptr + 5 <= end && ptr[1] == 'a' && ptr[2] == 'l' && ptr[3] == 's' && ptr[4] == 'e' )
				{
					out = false;
					ptr += 5;
					return true;
				}
				break;

			case 'n':
				if ( ptr + 4 <= end && ptr[1] == 'u' && ptr[2] == 'l' && ptr[3] == 'l' )
				{
					out.SetNull();
					ptr += 4;
					return true;
				}
				break;
		}

		// Unexpected here
		Errorf( "Input starting with character '%c' (0x%02x) not a valid JSON value", *ptr, *ptr );
		return false;
	}

};

bool ParseValue( Value &out, const char *begin, const char *end, ParseContext *ctx )
{
	VJSON_ASSERT( begin <= end );
	ParseContext dummy_ctx;
	Parser p( ctx ? *ctx : dummy_ctx, begin, end );
	if ( !p.ParseRequiredValue( out ) )
	{
		out.SetNull();
		return false;
	}

	// Check for any extra characters
	p.SkipWhitespaceAndComments();
	int c = p.Peek();
	if ( c < 0 )
		return true;

	p.Errorf( "Extra text starting with character '%c' (0x%02x)", c, c );
	out.SetNull();
	return false;
}

bool InternalParseTyped( Value &out, const char *begin, const char *end,
	ParseContext *ctx, EValueType expected_type, const char *expected_type_name )
{
	if ( !ParseValue( out, begin, end, ctx ) )
		return false;
	if ( out.Type() == expected_type )
		return true;
	if ( ctx )
	{
		ctx->error_line = 1;
		ctx->error_byte_offset = 0;

		char msg[ 256 ];
		snprintf( msg, sizeof(msg), "Failed to parse JSON %s", expected_type_name );
		ctx->error_message = msg;
	}
	return false;
}

bool ParseObject( Object &out, const char *begin, const char *end, ParseContext *ctx )
{
	if ( InternalParseTyped( out, begin, end, ctx, kObject, "object" ) )
		return true;
	out.SetEmptyObject(); // Type safety in case caller reuses
	return false;
}

bool ParseArray( Array &out, const char *begin, const char *end, ParseContext *ctx )
{
	if ( InternalParseTyped( out, begin, end, ctx, kArray, "array" ) )
		return true;
	out.SetEmptyArray(); // Type safety in case caller reuses
	return false;
}

} // namespace vjson
