#include "../vjson.h"

#include <gtest/gtest.h>

void CheckGet( const vjson::Value &obj, const char *key )
{
	const vjson::Value &val = obj.AtKey( key );
	const char *light_fantastic = "The Light Fantastic";

	if ( !val.IsBool() )
	{
		EXPECT_TRUE( val.GetBool( true ) );
		EXPECT_FALSE( val.GetBool( false ) );
		EXPECT_FALSE( val.GetBool() );
		EXPECT_TRUE( val.BoolAtKey( key, true ) );
		EXPECT_FALSE( val.BoolAtKey( key, false ) );
		EXPECT_FALSE( val.BoolAtKey( key ) );
	}
	if ( !val.IsString() )
	{
		EXPECT_EQ( val.GetCString(), nullptr );
		EXPECT_EQ( obj.CStringAtKey( key ), nullptr );
		EXPECT_EQ( (void *)val.GetCString( light_fantastic ), (void*)light_fantastic ); // Make sure POINTER is equal, not string
		EXPECT_EQ( (void *)obj.CStringAtKey( key, light_fantastic ), (void*)light_fantastic ); // Make sure POINTER is equal, not string
		EXPECT_EQ( val.GetString(), "" );
		EXPECT_EQ( val.GetString( light_fantastic + 4 ), "Light Fantastic" );
		EXPECT_EQ( obj.StringAtKey( key ), "" );
		EXPECT_EQ( obj.StringAtKey( key, light_fantastic + 4 ), "Light Fantastic" );
	}
}

// Load up a basic example document that demonstrates most basic JSON features,
// and exercise most of the basic accessors.
TEST(Misc, Basic) {

	vjson::ParseContext ctx;
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
})JSON", &ctx ) ) << "Parse failed line " << ctx.error_line << " " << ctx.error_message;

	// Exercise a bunch of basic accessors on the root document object
	ASSERT_EQ( doc.Type(), vjson::kObject );
	EXPECT_TRUE( doc.IsObject() );
	EXPECT_TRUE( &doc.AsObject() == &doc );
	EXPECT_TRUE( &doc.GetObjectOrEmpty() == &doc );
	EXPECT_TRUE( doc.GetObjectPtr() == &doc );
	EXPECT_TRUE( doc.GetArrayPtr() == nullptr );

	bool boolVal;
	std::string stringVal;
	double doubleVal;
	int intVal;
	uint64_t uint64Val;

	// Check each of the subkeys

	{
		ASSERT_TRUE( doc.HasKey( "null" ) );
		EXPECT_TRUE( doc.ValuePtrAtKey("null")->IsNull() );
		EXPECT_TRUE( doc["null"].IsNull() );
		EXPECT_TRUE( doc["null"].Is<nullptr_t>() );

		CheckGet( doc, "null" );

		boolVal = true;
		EXPECT_EQ( doc.ConvertAtKey( "null", boolVal ), vjson::kOK );
		EXPECT_FALSE( boolVal );

		doubleVal = 123;
		EXPECT_EQ( doc.ConvertAtKey( "null", doubleVal ), vjson::kOK );
		EXPECT_EQ( doubleVal, 0.0 );

		stringVal = "hello";
		EXPECT_EQ( doc.ConvertAtKey( "null", stringVal ), vjson::kOK );
		EXPECT_EQ( stringVal, "" );
	}

	{
		ASSERT_TRUE( doc.HasKey( "true" ) );
		EXPECT_TRUE( doc["true"].IsBool() );
		EXPECT_TRUE( doc["true"].Is<bool>() );
		EXPECT_TRUE( doc["true"].AsBool() );
		EXPECT_TRUE( doc["true"].As<bool>() );
		EXPECT_TRUE( doc["true"].GetBool( false ) );

		CheckGet( doc, "true" );

		boolVal = false;
		EXPECT_EQ( doc["true"].Convert( boolVal ), vjson::kOK );
		EXPECT_TRUE( boolVal );

		boolVal = false;
		EXPECT_EQ( doc.ConvertAtKey("true", boolVal ), vjson::kOK );
		EXPECT_TRUE( boolVal );

		stringVal = "hello";
		EXPECT_EQ( doc.ConvertAtKey("true", stringVal ), vjson::kOK );
		EXPECT_EQ( stringVal, "true" );
	}

	{
		ASSERT_TRUE( doc.HasKey( "false" ) );
		EXPECT_TRUE( doc["false"].IsBool() );
		EXPECT_TRUE( doc["true"].Is<bool>() );
		EXPECT_FALSE( doc["false"].AsBool() );
		EXPECT_FALSE( doc["false"].As<bool>() );
		EXPECT_FALSE( doc["false"].GetBool( true ) );

		CheckGet( doc, "false" );

		boolVal = true;
		EXPECT_EQ( doc["false"].Convert( boolVal ), vjson::kOK );
		EXPECT_FALSE( boolVal );

		EXPECT_FALSE( doc.BoolAtKey("false", false ) );

		boolVal = true;
		EXPECT_EQ( doc.ConvertAtKey("false", boolVal ), vjson::kOK );
		EXPECT_FALSE( boolVal );
	}

	{
		ASSERT_TRUE( doc.HasKey( "empty_string" ) );

		EXPECT_TRUE( doc["empty_string"].IsString() );
		EXPECT_TRUE( doc["empty_string"].Is<std::string>() );
		EXPECT_TRUE( doc["empty_string"].Is<const char *>() );

		EXPECT_TRUE( doc["empty_string"].GetString( "a non-empty string" ).empty() );
		EXPECT_TRUE( doc["empty_string"].AsString().empty() );
		EXPECT_TRUE( doc["empty_string"].As<std::string>().empty() );

		EXPECT_TRUE( doc["empty_string"].GetCString( "a non-empty string" ) == doc["empty_string"].AsString().c_str() );
		EXPECT_TRUE( doc["empty_string"].AsCString() == doc["empty_string"].AsString().c_str() );
		EXPECT_TRUE( doc["empty_string"].As<const char *>() == doc["empty_string"].AsCString() );

		CheckGet( doc, "empty_string" );

		boolVal = false;
		EXPECT_EQ( doc["empty_string"].Convert( boolVal ), vjson::kWrongType );
		EXPECT_TRUE( boolVal );

		boolVal = true;
		EXPECT_EQ( doc.ConvertAtKey("empty_string", boolVal ), vjson::kWrongType );
		EXPECT_TRUE( boolVal );

		stringVal = "nonempty";
		EXPECT_EQ( doc["empty_string"].Convert( stringVal ), vjson::kOK );
		EXPECT_TRUE( stringVal.empty() );

		stringVal = "nonempty";
		EXPECT_EQ( doc.ConvertAtKey("empty_string", stringVal ), vjson::kOK );
		EXPECT_TRUE( stringVal.empty() );
	}

	{
		ASSERT_TRUE( doc.HasKey( "true_string" ) );

		EXPECT_TRUE( doc["true_string"].IsString() );
		EXPECT_TRUE( doc["true_string"].Is<std::string>() );
		EXPECT_TRUE( doc["true_string"].Is<const char *>() );

		EXPECT_STREQ( doc["true_string"].GetString( "Jabberwocky" ).c_str(), "true" );
		EXPECT_STREQ( doc["true_string"].AsString().c_str(), "true" );
		EXPECT_STREQ( doc["true_string"].As<std::string>().c_str(), "true" );

		EXPECT_TRUE( doc["true_string"].GetCString( "Jabberywocky" ) == doc["true_string"].AsString().c_str() );
		EXPECT_TRUE( doc["true_string"].AsCString() == doc["true_string"].AsString().c_str() );
		EXPECT_TRUE( doc["true_string"].As<const char *>() == doc["true_string"].AsCString() );

		CheckGet( doc, "true_string" );

		stringVal = "bogus";
		EXPECT_EQ( doc["true_string"].Convert( stringVal ), vjson::kOK );
		EXPECT_STREQ( stringVal.c_str(), "true" );

		stringVal = "bogus";
		EXPECT_EQ( doc["true_string"].Convert( stringVal ), vjson::kOK );
		EXPECT_EQ( stringVal, "true" );

		boolVal = false;
		EXPECT_EQ( doc["true_string"].Convert( boolVal ), vjson::kOK );
		EXPECT_TRUE( boolVal );
		EXPECT_FALSE( doc["true_string"].GetBool( false ) );
	}

	EXPECT_FALSE( doc.HasKey( "bogus_key" ) );
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

