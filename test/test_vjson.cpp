#include "../vjson.h"

#include <gtest/gtest.h>

// Verify that when accessing the wrong type with default-returning accessors,
// the defaults come back correctly.
void CheckGetWrongType( const vjson::Value &obj, const char *key )
{
	const vjson::Value &val = obj.AtKey( key );
	const char *light_fantastic = "The Light Fantastic";

	if ( !val.IsBool() )
	{
		EXPECT_TRUE(  val.AsBool( true ) );
		EXPECT_FALSE( val.AsBool( false ) );
		EXPECT_TRUE(  obj.BoolAtKey( key, true ) );
		EXPECT_FALSE( obj.BoolAtKey( key, false ) );
	}
	if ( !val.IsString() )
	{
		EXPECT_EQ( val.AsCString( nullptr ), nullptr );
		EXPECT_EQ( obj.CStringAtKey( key, nullptr ), nullptr );
		EXPECT_EQ( (void *)val.AsCString( light_fantastic ), (void *)light_fantastic );
		EXPECT_EQ( (void *)obj.CStringAtKey( key, light_fantastic ), (void *)light_fantastic );
		EXPECT_EQ( val.AsString( "" ), "" );
		EXPECT_EQ( val.AsString( light_fantastic + 4 ), "Light Fantastic" );
		EXPECT_EQ( obj.StringAtKey( key, "" ), "" );
		EXPECT_EQ( obj.StringAtKey( key, light_fantastic + 4 ), "Light Fantastic" );
	}
}

// Parse a basic document covering most JSON value types and exercise accessors.
TEST(Misc, Basic) {

	vjson::ParseContext ctx;
	ctx.allow_trailing_comma = true;
	vjson::Object doc;
	EXPECT_TRUE( doc.ParseJSON(
R"JSON({
	"null": null,
	"true": true,
	"false": false,
	"empty_string": "",
	"true_string": "true",
	"false_string": "false",
	"zero": 0,
	"one": 1,
	"negative_one": -1,
	"zero_float": 0.0,
	"float": 123.45,
	"uint64_as_string": "123456789",
	"big_double": 12345678900000,
	"double_exponents": [ 123e45, 1.23e45, 123e-45, 1.23E-45 ],
	"neg_double_exponents": [ -123e45, -1.23e45, -123e-45, -1.23E-45 ],
	"empty_array": [],
	"array_123": [ 1, 2, 3 ],
	"empty_object": {},
	"string_escaped_characters": "tab\tand\nnewline",
	"tab\tin\tkey": null,
	"array_of_objects": [
		{ "key1": "value1" },
		{ "key2": 2 },
		{ "key3": false },
		{ "key4": [ "hello", "world" ] },
	]
})JSON", &ctx ) ) << "Parse failed line " << ctx.error_line << ": " << ctx.error_message;

	ASSERT_EQ( doc.Type(), vjson::kObject );
	EXPECT_TRUE( doc.IsObject() );
	EXPECT_TRUE( &doc.GetObject() == &doc );
	EXPECT_TRUE( &doc.AsObjectOrEmpty() == &doc );
	EXPECT_TRUE( doc.AsObjectPtr() == &doc );
	EXPECT_TRUE( doc.AsArrayPtr() == nullptr );

	bool boolVal;
	std::string stringVal;
	double doubleVal;
	int intVal;
	uint64_t uint64Val;

	{
		ASSERT_TRUE( doc.HasKey( "null" ) );
		EXPECT_TRUE( doc.ValuePtrAtKey("null")->IsNull() );
		EXPECT_TRUE( doc["null"].IsNull() );
		EXPECT_TRUE( doc["null"].Is<std::nullptr_t>() );

		CheckGetWrongType( doc, "null" );

		boolVal = true;
		EXPECT_EQ( doc.TryInterpretAtKey( "null", boolVal ), vjson::kOK );
		EXPECT_FALSE( boolVal );

		doubleVal = 123;
		EXPECT_EQ( doc.TryInterpretAtKey( "null", doubleVal ), vjson::kOK );
		EXPECT_EQ( doubleVal, 0.0 );

		stringVal = "hello";
		EXPECT_EQ( doc.TryInterpretAtKey( "null", stringVal ), vjson::kOK );
		EXPECT_EQ( stringVal, "" );
	}

	{
		ASSERT_TRUE( doc.HasKey( "true" ) );
		EXPECT_TRUE( doc["true"].IsBool() );
		EXPECT_TRUE( doc["true"].Is<bool>() );
		EXPECT_TRUE( doc["true"].AsBool( false ) );
		EXPECT_TRUE( doc["true"].GetBool() );

		CheckGetWrongType( doc, "true" );

		boolVal = false;
		EXPECT_EQ( doc["true"].TryInterpret( boolVal ), vjson::kOK );
		EXPECT_TRUE( boolVal );

		boolVal = false;
		EXPECT_EQ( doc.TryInterpretAtKey( "true", boolVal ), vjson::kOK );
		EXPECT_TRUE( boolVal );

		stringVal = "hello";
		EXPECT_EQ( doc.TryInterpretAtKey( "true", stringVal ), vjson::kOK );
		EXPECT_EQ( stringVal, "true" );
	}

	{
		ASSERT_TRUE( doc.HasKey( "false" ) );
		EXPECT_TRUE( doc["false"].IsBool() );
		EXPECT_TRUE( doc["false"].Is<bool>() );
		EXPECT_FALSE( doc["false"].AsBool( true ) );
		EXPECT_FALSE( doc["false"].GetBool() );

		CheckGetWrongType( doc, "false" );

		boolVal = true;
		EXPECT_EQ( doc["false"].TryInterpret( boolVal ), vjson::kOK );
		EXPECT_FALSE( boolVal );

		EXPECT_FALSE( doc.BoolAtKey( "false", true ) );

		boolVal = true;
		EXPECT_EQ( doc.TryInterpretAtKey( "false", boolVal ), vjson::kOK );
		EXPECT_FALSE( boolVal );
	}

	{
		ASSERT_TRUE( doc.HasKey( "empty_string" ) );
		EXPECT_TRUE( doc["empty_string"].IsString() );
		EXPECT_TRUE( doc["empty_string"].Is<std::string>() );
		EXPECT_TRUE( doc["empty_string"].Is<const char *>() );

		EXPECT_TRUE( doc["empty_string"].AsString( "a non-empty string" ).empty() );
		EXPECT_TRUE( doc["empty_string"].GetString().empty() );

		// AsCString should return the internal pointer for string values
		EXPECT_TRUE( doc["empty_string"].AsCString( "a non-empty string" ) == doc["empty_string"].GetString().c_str() );
		EXPECT_TRUE( doc["empty_string"].AsCString( nullptr ) == doc["empty_string"].GetString().c_str() );
		EXPECT_TRUE( doc["empty_string"].GetCString() == doc["empty_string"].AsCString( nullptr ) );

		CheckGetWrongType( doc, "empty_string" );

		boolVal = false;
		EXPECT_EQ( doc["empty_string"].TryInterpret( boolVal ), vjson::kWrongType );
		EXPECT_FALSE( boolVal ); // unchanged on failure

		boolVal = true;
		EXPECT_EQ( doc.TryInterpretAtKey( "empty_string", boolVal ), vjson::kWrongType );
		EXPECT_TRUE( boolVal ); // unchanged on failure

		stringVal = "nonempty";
		EXPECT_EQ( doc["empty_string"].TryInterpret( stringVal ), vjson::kOK );
		EXPECT_TRUE( stringVal.empty() );

		stringVal = "nonempty";
		EXPECT_EQ( doc.TryInterpretAtKey( "empty_string", stringVal ), vjson::kOK );
		EXPECT_TRUE( stringVal.empty() );
	}

	{
		ASSERT_TRUE( doc.HasKey( "true_string" ) );
		EXPECT_TRUE( doc["true_string"].IsString() );
		EXPECT_STREQ( doc["true_string"].AsString( "Jabberwocky" ).c_str(), "true" );
		EXPECT_STREQ( doc["true_string"].GetString().c_str(), "true" );
		EXPECT_TRUE( doc["true_string"].AsCString( nullptr ) == doc["true_string"].GetString().c_str() );

		CheckGetWrongType( doc, "true_string" );

		stringVal = "bogus";
		EXPECT_EQ( doc["true_string"].TryInterpret( stringVal ), vjson::kOK );
		EXPECT_EQ( stringVal, "true" );

		// TryInterpret converts "true" string to bool, but AsBool does strict type check
		boolVal = false;
		EXPECT_EQ( doc["true_string"].TryInterpret( boolVal ), vjson::kOK );
		EXPECT_TRUE( boolVal );
		EXPECT_FALSE( doc["true_string"].AsBool( false ) );
	}

	EXPECT_FALSE( doc.HasKey( "bogus_key" ) );
}

// Special characters in strings must be escaped in the JSON output so that
// the result is valid JSON and round-trips correctly.
TEST(Print, StringEscaping) {
	vjson::Object obj;
	obj["newline"]   = "line1\nline2";
	obj["cr"]        = "line1\rline2";
	obj["tab"]       = "col1\tcol2";
	obj["backslash"] = "a\\b";
	obj["quote"]     = "say \"hello\"";
	obj["backspace"] = "a\bb";
	obj["formfeed"]  = "a\fb";
	obj["control"]   = std::string( "a\x01" "b", 3 );

	vjson::PrintOptions minified;
	minified.indent = nullptr;
	std::string json = obj.PrintJSON( minified );

	// No raw control characters should appear in the output
	for ( size_t i = 0; i < json.size(); ++i )
	{
		char c = json[i];
		if ( c == '"' )
		{
			// skip over string contents
			++i;
			while ( i < json.size() && json[i] != '"' )
			{
				if ( json[i] == '\\' ) ++i;
				++i;
			}
			continue;
		}
		EXPECT_GE( (int)(unsigned char)c, 0x20 )
			<< "raw control char 0x" << std::hex << (int)(unsigned char)c
			<< " at position " << std::dec << i << " in: " << json;
	}

	// The escape sequences we expect to see in the raw JSON text
	EXPECT_NE( json.find( "\\n"  ), std::string::npos ) << "newline not escaped in: " << json;
	EXPECT_NE( json.find( "\\r"  ), std::string::npos ) << "CR not escaped in: " << json;
	EXPECT_NE( json.find( "\\t"  ), std::string::npos ) << "tab not escaped in: " << json;
	EXPECT_NE( json.find( "\\\\" ), std::string::npos ) << "backslash not escaped in: " << json;
	EXPECT_NE( json.find( "\\\"" ), std::string::npos ) << "quote not escaped in: " << json;
	EXPECT_NE( json.find( "\\b"  ), std::string::npos ) << "backspace not escaped in: " << json;
	EXPECT_NE( json.find( "\\f"  ), std::string::npos ) << "formfeed not escaped in: " << json;
	EXPECT_NE( json.find( "\\u0001" ), std::string::npos ) << "control char not escaped in: " << json;

	// Round-trip: parse the output back and verify values survived
	vjson::Object roundtrip;
	vjson::ParseContext ctx;
	ASSERT_TRUE( roundtrip.ParseJSON( json, &ctx ) )
		<< "Round-trip parse failed: " << ctx.error_message << " at line " << ctx.error_line
		<< "\nJSON: " << json;

	EXPECT_EQ( roundtrip.StringAtKey( "newline",   "" ), "line1\nline2" );
	EXPECT_EQ( roundtrip.StringAtKey( "cr",        "" ), "line1\rline2" );
	EXPECT_EQ( roundtrip.StringAtKey( "tab",       "" ), "col1\tcol2" );
	EXPECT_EQ( roundtrip.StringAtKey( "backslash", "" ), "a\\b" );
	EXPECT_EQ( roundtrip.StringAtKey( "quote",     "" ), "say \"hello\"" );
	EXPECT_EQ( roundtrip.StringAtKey( "backspace", "" ), "a\bb" );
	EXPECT_EQ( roundtrip.StringAtKey( "formfeed",  "" ), "a\fb" );
	EXPECT_EQ( roundtrip.StringAtKey( "control",   "" ), std::string( "a\x01" "b", 3 ) );
}

// Parse a document, print it, parse it again -- values must survive the trip.
TEST(Print, RoundTrip) {
	const char *original = R"JSON({
	"string": "hello world",
	"number": 42,
	"float": 3.14,
	"bool_true": true,
	"bool_false": false,
	"null_val": null,
	"array": [ 1, 2, 3 ],
	"nested": { "a": "alpha", "b": 2 }
})JSON";

	vjson::Object obj;
	vjson::ParseContext ctx;
	ASSERT_TRUE( obj.ParseJSON( original, &ctx ) )
		<< "Initial parse failed: " << ctx.error_message;

	// Pretty-print round-trip
	std::string pretty = obj.PrintJSON();
	vjson::Object obj2;
	ASSERT_TRUE( obj2.ParseJSON( pretty, &ctx ) )
		<< "Pretty round-trip parse failed: " << ctx.error_message
		<< "\nJSON:\n" << pretty;
	EXPECT_EQ( obj2.StringAtKey( "string",     "" ),    "hello world" );
	EXPECT_EQ( obj2.DoubleAtKey( "number",     0.0 ),   42.0 );
	EXPECT_EQ( obj2.DoubleAtKey( "float",      0.0 ),   3.14 );
	EXPECT_TRUE(  obj2.BoolAtKey( "bool_true",  false ) );
	EXPECT_FALSE( obj2.BoolAtKey( "bool_false", true  ) );
	EXPECT_TRUE( obj2.ValuePtrAtKey( "null_val" ) && obj2["null_val"].IsNull() );
	EXPECT_EQ( obj2["array"].ArrayLen(), 3 );
	EXPECT_EQ( obj2["nested"].StringAtKey( "a", "" ), "alpha" );

	// Minified round-trip
	vjson::PrintOptions minified;
	minified.indent = nullptr;
	std::string mini = obj.PrintJSON( minified );
	vjson::Object obj3;
	ASSERT_TRUE( obj3.ParseJSON( mini, &ctx ) )
		<< "Minified round-trip parse failed: " << ctx.error_message
		<< "\nJSON: " << mini;
	EXPECT_EQ( obj3.StringAtKey( "string", "" ), "hello world" );
	EXPECT_EQ( obj3.DoubleAtKey( "number", 0.0 ), 42.0 );
}

// All items in a pretty-printed object/array must be indented -- including
// the first one (regression for BeginBlock indent bug).
TEST(Print, PrettyPrintIndent) {
	vjson::Object obj;
	obj["alpha"] = "one";
	obj["beta"]  = "two";
	obj["gamma"] = "three";

	vjson::PrintOptions tabs;
	tabs.indent = "\t";
	std::string json = obj.PrintJSON( tabs );

	EXPECT_NE( json.find( "\t\"alpha\"" ), std::string::npos )
		<< "first key not indented\nJSON:\n" << json;
	EXPECT_NE( json.find( "\t\"beta\""  ), std::string::npos )
		<< "second key not indented\nJSON:\n" << json;
	EXPECT_NE( json.find( "\t\"gamma\"" ), std::string::npos )
		<< "third key not indented\nJSON:\n" << json;

	// Also works with nested arrays
	vjson::Array arr;
	arr.push_back( "x" );
	arr.push_back( "y" );
	vjson::Object outer;
	outer["items"] = arr;
	std::string json2 = outer.PrintJSON( tabs );
	EXPECT_NE( json2.find( "\t\t\"x\"" ), std::string::npos )
		<< "first array element not double-indented\nJSON:\n" << json2;
	EXPECT_NE( json2.find( "\t\t\"y\"" ), std::string::npos )
		<< "second array element not double-indented\nJSON:\n" << json2;
}

// Minified output must have no structural whitespace.
TEST(Print, Minified) {
	vjson::Object obj;
	obj["a"] = 1.0;
	obj["b"] = "two";

	vjson::PrintOptions minified;
	minified.indent = nullptr;
	std::string json = obj.PrintJSON( minified );

	EXPECT_EQ( json.find( '\n' ), std::string::npos ) << "newline in minified: " << json;
	EXPECT_EQ( json.find( '\t' ), std::string::npos ) << "tab in minified: " << json;
	EXPECT_EQ( json.find( ": " ), std::string::npos ) << "space after colon in minified: " << json;
	EXPECT_EQ( json.find( ", " ), std::string::npos ) << "space after comma in minified: " << json;

	vjson::Object roundtrip;
	vjson::ParseContext ctx;
	ASSERT_TRUE( roundtrip.ParseJSON( json, &ctx ) );
	EXPECT_EQ( roundtrip.DoubleAtKey( "a", 0.0 ), 1.0 );
	EXPECT_EQ( roundtrip.StringAtKey( "b", ""  ), "two" );
}

// DoubleAtIndex must return the double value, not the bool field of the union
// (regression for _bool/_double mix-up in InternalAtIndex).
TEST(Misc, DoubleAtIndex) {
	vjson::Array arr;
	arr.push_back( 1.0 );
	arr.push_back( 2.0 );
	arr.push_back( 3.14 );
	arr.push_back( -7.5 );

	EXPECT_EQ( arr.DoubleAtIndex( 0, 0.0 ), 1.0 );
	EXPECT_EQ( arr.DoubleAtIndex( 1, 0.0 ), 2.0 );
	EXPECT_EQ( arr.DoubleAtIndex( 2, 0.0 ), 3.14 );
	EXPECT_EQ( arr.DoubleAtIndex( 3, 0.0 ), -7.5 );

	// Out-of-range returns default
	EXPECT_EQ( arr.DoubleAtIndex( 99, -1.0 ), -1.0 );
}

// Basic typed array iteration: Iter<T>() visits only elements of the matching type.
TEST(Array, IterTyped) {
	vjson::Array strs;
	strs.push_back( "alpha" );
	strs.push_back( "beta" );
	strs.push_back( "gamma" );

	std::vector<std::string> got;
	for ( const char *s: strs.Iter<const char *>() )
		got.push_back( s );
	ASSERT_EQ( got.size(), 3u );
	EXPECT_EQ( got[0], "alpha" );
	EXPECT_EQ( got[1], "beta" );
	EXPECT_EQ( got[2], "gamma" );

	// Same elements via std::string overload
	std::vector<std::string> got2;
	for ( const std::string &s: strs.Iter<std::string>() )
		got2.push_back( s );
	EXPECT_EQ( got2, got );

	// Double array
	vjson::Array nums;
	nums.push_back( 1.0 );
	nums.push_back( 2.5 );
	nums.push_back( -3.0 );

	double sum = 0.0;
	for ( double d: nums.Iter<double>() )
		sum += d;
	EXPECT_EQ( sum, 0.5 );
}

// Iter<T>() silently skips elements of every other type.
TEST(Array, IterSkipsWrongType) {
	vjson::Array arr;
	arr.push_back( "first" );    // string
	arr.push_back( 42.0 );       // double
	arr.push_back( true );       // bool
	arr.push_back( vjson::Value{} ); // null
	arr.push_back( "second" );   // string
	vjson::Object inner;
	inner["x"] = 1.0;
	arr.push_back( inner );      // object
	vjson::Array nested;
	nested.push_back( 99.0 );
	arr.push_back( nested );     // array

	// Only the two strings come through
	std::vector<std::string> strings;
	for ( const char *s: arr.Iter<const char *>() )
		strings.push_back( s );
	ASSERT_EQ( strings.size(), 2u );
	EXPECT_EQ( strings[0], "first" );
	EXPECT_EQ( strings[1], "second" );

	// Only the one double
	std::vector<double> doubles;
	for ( double d: arr.Iter<double>() )
		doubles.push_back( d );
	ASSERT_EQ( doubles.size(), 1u );
	EXPECT_EQ( doubles[0], 42.0 );

	// Only the one bool
	int bool_count = 0;
	bool bool_val = false;
	for ( bool b: arr.Iter<bool>() )
	{
		bool_val = b;
		++bool_count;
	}
	EXPECT_EQ( bool_count, 1 );
	EXPECT_TRUE( bool_val );

	// Only the one object
	int obj_count = 0;
	for ( const vjson::Object &o: arr.Iter<vjson::Object>() )
	{
		EXPECT_EQ( o.DoubleAtKey( "x", 0.0 ), 1.0 );
		++obj_count;
	}
	EXPECT_EQ( obj_count, 1 );

	// Only the one nested array
	int arr_count = 0;
	for ( const vjson::Array &a: arr.Iter<vjson::Array>() )
	{
		EXPECT_EQ( a.DoubleAtIndex( 0, 0.0 ), 99.0 );
		++arr_count;
	}
	EXPECT_EQ( arr_count, 1 );
}

// Iter<T>() on an empty array, or an array with no matching elements, yields zero iterations.
TEST(Array, IterEmpty) {
	vjson::Array empty;
	int count = 0;
	for ( const char *s: empty.Iter<const char *>() )
		(void)s, ++count;
	EXPECT_EQ( count, 0 );

	// All-wrong-type array also yields nothing for the requested type
	vjson::Array nums;
	nums.push_back( 1.0 );
	nums.push_back( 2.0 );
	count = 0;
	for ( const char *s: nums.Iter<const char *>() )
		(void)s, ++count;
	EXPECT_EQ( count, 0 );
}

// Mutable Iter<T>() allows in-place modification; non-matching elements are untouched.
TEST(Array, IterMutable) {
	vjson::Array arr;
	arr.push_back( 1.0 );
	arr.push_back( "skip me" );
	arr.push_back( 2.0 );
	arr.push_back( 3.0 );

	for ( double &d: arr.Iter<double>() )
		d *= 10.0;

	EXPECT_EQ( arr.DoubleAtIndex( 0, 0.0 ),  10.0 );
	EXPECT_EQ( arr.StringAtIndex( 1, "" ),   "skip me" ); // untouched
	EXPECT_EQ( arr.DoubleAtIndex( 2, 0.0 ),  20.0 );
	EXPECT_EQ( arr.DoubleAtIndex( 3, 0.0 ),  30.0 );
}

// Common pattern: array of objects parsed from JSON, with stray non-object
// elements that should be silently skipped.
TEST(Array, IterObjectsParsed) {
	vjson::Value doc;
	vjson::ParseContext ctx;
	ASSERT_TRUE( doc.ParseJSON(
	R"JSON([
		{ "name": "Alice", "score": 10 },
		42,
		{ "name": "Bob",   "score": 20 },
		"garbage",
		{ "name": "Carol", "score": 30 }
	])JSON", &ctx ) ) << ctx.error_message;

	std::vector<std::string> names;
	double total = 0.0;
	for ( const vjson::Object &o: doc.AsArrayOrEmpty().Iter<vjson::Object>() )
	{
		names.push_back( o.StringAtKey( "name", "" ) );
		total += o.DoubleAtKey( "score", 0.0 );
	}

	ASSERT_EQ( names.size(), 3u );
	EXPECT_EQ( names[0], "Alice" );
	EXPECT_EQ( names[1], "Bob" );
	EXPECT_EQ( names[2], "Carol" );
	EXPECT_EQ( total, 60.0 );
}

// Object range-based for iterates all key-value pairs in key-sorted order.
TEST(Object, RangeFor) {
	vjson::Object obj;
	obj["banana"] = 2.0;
	obj["apple"]  = 1.0;
	obj["cherry"] = 3.0;

	std::vector<std::string> keys;
	std::vector<double>      vals;
	for ( const auto &item: obj )
	{
		keys.push_back( item.first );
		vals.push_back( item.second.AsDouble( -1.0 ) );
	}

	// std::map with ObjectKeyLess maintains lexicographic order
	ASSERT_EQ( keys.size(), 3u );
	EXPECT_EQ( keys[0], "apple"  ); EXPECT_EQ( vals[0], 1.0 );
	EXPECT_EQ( keys[1], "banana" ); EXPECT_EQ( vals[1], 2.0 );
	EXPECT_EQ( keys[2], "cherry" ); EXPECT_EQ( vals[2], 3.0 );
}

// Object range-based for with mixed value types; filter by type in the loop body.
// (Object has no Iter<T>(); per-type filtering is done manually.)
TEST(Object, RangeForMixedTypes) {
	vjson::Object obj;
	obj["name"]   = "Alice";
	obj["score"]  = 42.0;
	obj["active"] = true;
	obj["tag"]    = "player";
	obj["level"]  = 7.0;

	// Collect only string values (keys sorted: "name" < "tag")
	std::vector<std::string> strings;
	for ( const auto &item: obj )
		if ( item.second.IsString() )
			strings.push_back( item.second.GetString() );
	ASSERT_EQ( strings.size(), 2u );
	EXPECT_EQ( strings[0], "Alice" );   // key "name"
	EXPECT_EQ( strings[1], "player" );  // key "tag"

	// Sum only numeric values; non-numeric entries skipped
	double total = 0.0;
	for ( const auto &item: obj )
		if ( item.second.IsDouble() )
			total += item.second.GetDouble();
	EXPECT_EQ( total, 49.0 ); // 42.0 + 7.0
}

// Mutable object iteration: modify values in-place via item.second.
TEST(Object, RangeForMutable) {
	vjson::Object obj;
	obj["x"] = 1.0;
	obj["y"] = 2.0;
	obj["z"] = "leave me";

	for ( auto &item: obj )
		if ( item.second.IsDouble() )
			item.second.GetDouble() *= 10.0;

	EXPECT_EQ( obj.DoubleAtKey( "x", 0.0 ), 10.0 );
	EXPECT_EQ( obj.DoubleAtKey( "y", 0.0 ), 20.0 );
	EXPECT_EQ( obj.StringAtKey( "z", "" ),  "leave me" ); // untouched
}

// Helper: parse input and verify it fails with the expected error location and message.
static void CheckParseError( const char *input, int expected_line, int expected_byte,
                              const char *msg_substr = nullptr )
{
	vjson::Value val;
	vjson::ParseContext ctx;
	EXPECT_FALSE( val.ParseJSON( input, &ctx ) )
		<< "Expected parse error but succeeded on: " << input;
	EXPECT_EQ( ctx.error_line, expected_line ) << "input: " << input;
	EXPECT_EQ( ctx.error_byte_offset, expected_byte ) << "input: " << input;
	if ( msg_substr )
		EXPECT_NE( ctx.error_message.find( msg_substr ), std::string::npos )
			<< "Expected '" << msg_substr << "' in error '" << ctx.error_message << "'";
}

// Unexpected end-of-input in various contexts; also verifies blank lines advance the line counter.
TEST(ParseErrors, UnexpectedEOF) {
	// Empty input: ptr=begin=end → byte 0, line 1
	CheckParseError( "", 1, 0, "Unexpected end-of-input" );
	// "[": after consuming '[', ptr=1=end → byte 1, line 1
	CheckParseError( "[", 1, 1, "Unexpected end-of-input" );
	// "{": same for objects
	CheckParseError( "{", 1, 1, "Unexpected end-of-input" );
	// {"a": — EOF while looking for value: ptr=5=end
	CheckParseError( "{\"a\":", 1, 5, "Unexpected end-of-input" );
	// Three newlines before unclosed '[': each newline bumps line; EOF at byte 4, line 4
	CheckParseError( "[\n\n\n", 4, 4, "Unexpected end-of-input" );
}

// String parsing errors: unterminated, illegal characters, bad escape sequences.
TEST(ParseErrors, StringErrors) {
	// Unterminated: error ptr stays at byte 1 (first char inside the opening quote)
	// "hello  →  " at 0; ptr=1 after ++ptr
	CheckParseError( "\"hello", 1, 1, "Unterminated string" );

	// Newline inside a string: ptr set to position of the '\n' (byte 4)
	// "abc<NL>def"  →  " at 0, NL at 4
	CheckParseError( "\"abc\ndef\"", 1, 4, "Newline character" );

	// Control character 0x01: ptr set to its position (byte 3)
	// "ab<0x01>cd"  →  " at 0, 0x01 at 3
	// (string-literal concat prevents compiler from extending \x01 into "cd")
	CheckParseError( "\"ab\x01" "cd\"", 1, 3, "Control character" );

	// Invalid escape sequence (printable): ptr at the char after '\' (byte 2)
	// "\q"  →  \ at 1, q at 2
	CheckParseError( R"("\q")", 1, 2, "Invalid escape sequence" );

	// Invalid char after '\' (space = 0x20, which is NOT > 0x20): byte 2
	// "\ "  →  \ at 1, space at 2
	CheckParseError( "\"\\ \"", 1, 2, "not valid after" );
}

// \uXXXX escape sequence errors.
TEST(ParseErrors, UEscapeErrors) {
	// EOF during \u: ptr set to s-1 = position of 'u' (byte 2)
	// "\u"  →  \ at 1, u at 2, " at 3; s=3, s-1=2
	CheckParseError( R"("\u")", 1, 2, "End of input during" );

	// Non-hex digit at the third hex position: ptr at the bad char (byte 5)
	// "\u00zz"  →  \ at 1, u at 2, 0 at 3, 0 at 4, z at 5
	CheckParseError( R"("\u00zz")", 1, 5, "not a hex digit" );
}

// Object syntax errors.
TEST(ParseErrors, ObjectSyntax) {
	// Key not quoted: ptr at the unquoted char (byte 1)
	// {abc}  →  { at 0, a at 1
	CheckParseError( "{abc}", 1, 1, "Expected '\"'" );

	// Missing colon after key: ptr at the offending char (byte 5)
	// {"a" "b":2}  →  " at 5 is where ':' was expected
	CheckParseError( "{\"a\" \"b\":2}", 1, 5, "Expected ':'" );

	// Missing comma or closing brace: ptr at the offending char (byte 7)
	// {"a":1 "b":2}  →  " at 7 where ',' or '}' expected
	CheckParseError( "{\"a\":1 \"b\":2}", 1, 7, "Expected '}' or ','" );

	// Trailing comma, strict mode: ptr at '}' (byte 7)
	// {"a":1,}  →  } at 7
	CheckParseError( "{\"a\":1,}", 1, 7, "trailing comma not permitted" );
}

// Array syntax errors.
TEST(ParseErrors, ArraySyntax) {
	// Missing comma or closing bracket: ptr at offending char (byte 3)
	// [1 2]  →  2 at 3 is where ',' or ']' was expected
	CheckParseError( "[1 2]", 1, 3, "Expected ']' or ','" );

	// Trailing comma, strict mode: ptr at ']' (byte 3)
	// [1,]  →  ] at 3
	CheckParseError( "[1,]", 1, 3, "trailing comma not permitted" );
}

// Number parsing errors.
TEST(ParseErrors, NumberErrors) {
	// Digit required after '-': ptr at the bad char (byte 1)
	// -a  →  - at 0, a at 1
	CheckParseError( "-a", 1, 1, "Expected digit after '-'" );

	// Leading zeros: ptr reset to start of number (byte 0)
	// 01  →  start of number is byte 0
	CheckParseError( "01", 1, 0, "Leading zeros" );

	// Digit required after exponent: ptr at the bad char (byte 2)
	// 1ex  →  1 at 0, e at 1, x at 2
	CheckParseError( "1ex", 1, 2, "Digit is required after exponent" );

	// Number too long (300 digits): ptr reset to start (byte 0)
	CheckParseError( std::string( 300, '1' ).c_str(), 1, 0, "too many characters" );
}

// Top-level value errors.
TEST(ParseErrors, ToplevelErrors) {
	// Character that cannot start any JSON value: ptr at it (byte 0)
	CheckParseError( "@", 1, 0, "not a valid JSON value" );

	// Valid value followed by extra garbage: ptr at start of extra text (byte 2)
	// "1 2"  →  1 at 0, space at 1, 2 at 2
	CheckParseError( "1 2", 1, 2, "Extra text" );
}

// Object::ParseJSON with non-object input: line and byte are reset to 1/0.
TEST(ParseErrors, ObjectParseTyped) {
	vjson::Object obj;
	vjson::ParseContext ctx;
	EXPECT_FALSE( obj.ParseJSON( "[]", &ctx ) );
	EXPECT_EQ( ctx.error_line, 1 );
	EXPECT_EQ( ctx.error_byte_offset, 0 );
	EXPECT_NE( ctx.error_message.find( "Failed to parse JSON object" ), std::string::npos )
		<< "error was: " << ctx.error_message;
}

// Multi-line inputs: extra blank lines must advance the line counter correctly.
TEST(ParseErrors, LineNumbers) {
	// Three newlines before unclosed '[': line advances to 4, EOF at byte 4
	CheckParseError( "[\n\n\n", 4, 4, "Unexpected end-of-input" );

	// Syntax error on line 2 of an object
	// "{\n  abc}"  →  { at 0, \n bumps to line 2, 'a' at byte 4 is the bad key char
	CheckParseError( "{\n  abc}", 2, 4, "Expected '\"'" );

	// Syntax error on line 3, with an extra blank line (line 2 is blank)
	// "[\n\n  garbage\n]"  →  2 newlines push to line 3; 'g' at byte 5
	CheckParseError( "[\n\n  garbage\n]", 3, 5, "not a valid JSON value" );
}

// Same logical error in minified vs. pretty-printed input:
// byte offset and line number differ, but the error message is the same.
TEST(ParseErrors, MinifiedVsPretty) {
	// Minified: {"a":1 "b":2}  →  " at byte 7, line 1
	CheckParseError( "{\"a\":1 \"b\":2}", 1, 7, "Expected '}' or ','" );

	// Pretty: same missing comma, but " at byte 13, line 3
	// byte: {=0 \n=1 sp=2 sp=3 "=4 a=5 "=6 :=7 sp=8 1=9 \n=10 sp=11 sp=12 "=13
	// line: \n at 1 → line 2; \n at 10 → line 3; " at 13 is the offending char
	CheckParseError( "{\n  \"a\": 1\n  \"b\": 2\n}", 3, 13, "Expected '}' or ','" );
}

// Verify all the API calls in the README "quick example" section compile and
// produce the documented results.
TEST(Misc, ReadmeExample) {
	const char *response = R"({
		"ranked": 1,
		"server": "Seattle #4",
		"match_id": "14889406635632900096",
		"players": [
			{ "name": "Alice", "score": 1500 },
			42,
			{ "name": "Bob",   "score": 1200 },
			{ "name": "Carl" }
		],
		"top_scores": []
	})";

	vjson::Object doc;
	vjson::ParseContext ctx;
	ASSERT_TRUE( doc.ParseJSON( response, &ctx ) ) << ctx.error_message;

	bool ranked = doc.InterpretAsBoolAtKey( "ranked", false );
	EXPECT_TRUE( ranked );

	double timeout = doc.DoubleAtKey( "timeout_ms", 5000.0 );
	EXPECT_EQ( timeout, 5000.0 );

	int spectator_count = 0;
	for ( const vjson::Object &p : doc.ArrayAtKeyOrEmpty( "spectators" ).Iter<vjson::Object>() )
		(void)p, ++spectator_count;
	EXPECT_EQ( spectator_count, 0 );

	std::vector<std::string> names;
	std::vector<double> scores;
	for ( const vjson::Object &p : doc.ArrayAtKeyOrEmpty( "players" ).Iter<vjson::Object>() )
	{
		names.push_back( p.StringAtKey( "name", "?" ) );
		scores.push_back( p.DoubleAtKey( "score", 0.0 ) );
	}
	ASSERT_EQ( names.size(), 3u );
	EXPECT_EQ( names[0], "Alice" );  EXPECT_EQ( scores[0], 1500.0 );
	EXPECT_EQ( names[1], "Bob" );    EXPECT_EQ( scores[1], 1200.0 );
	EXPECT_EQ( names[2], "Carl" );   EXPECT_EQ( scores[2], 0.0 );    // missing → default

	const char *leader = doc["top_scores"].AtIndex(0).CStringAtKey( "name", "(nobody)" );
	EXPECT_STREQ( leader, "(nobody)" );

	uint64_t match_id = doc.InterpretAsUint64AtKey( "match_id", 0 );
	EXPECT_EQ( match_id, 14889406635632900096ull );
}

int main(int argc, char **argv) {
	::testing::InitGoogleTest( &argc, argv );
	return RUN_ALL_TESTS();
}
