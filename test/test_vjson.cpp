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

int main(int argc, char **argv) {
	::testing::InitGoogleTest( &argc, argv );
	return RUN_ALL_TESTS();
}
