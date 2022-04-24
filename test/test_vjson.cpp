#include "../vjson.h"

#include <gtest/gtest.h>

void CheckTruthy( const vjson::Value &v, vjson::ETruthy expected )
{
	EXPECT_EQ( v.AsTruthy(), expected );

	bool truth_val = false;;
	switch ( expected )
	{
		case vjson::kTruish:
			EXPECT_TRUE( v.IsTruish() );
			EXPECT_FALSE( v.IsFalsish() );
			truth_val = false;
			EXPECT_EQ( v.GetTruthy( truth_val ), vjson::kOK );
			EXPECT_TRUE( truth_val );
			break;

		case vjson::kFalsish:
			EXPECT_FALSE( v.IsTruish() );
			EXPECT_TRUE( v.IsFalsish() );
			truth_val = true;
			EXPECT_EQ( v.GetTruthy( truth_val ), vjson::kOK );
			EXPECT_FALSE( truth_val );
			break;

		case vjson::kJibberish:
			EXPECT_FALSE( v.IsTruish() );
			EXPECT_FALSE( v.IsFalsish() );
			EXPECT_EQ( v.GetTruthy( truth_val ), vjson::kWrongType );
			break;

		default:
			FAIL();
	}
}

void CheckTruthyAtKey( const vjson::Object &obj, const char *key, vjson::ETruthy expected )
{
	EXPECT_EQ( obj.TruthyAtKey( key ), expected );
	CheckTruthy( obj.AtKey( key ), expected );
}

// Load up a basic example document that demonstrates most basic JSON features,
// and exercise most of the basic accessors.
TEST(Misc, Basic) {

	vjson::ParseContext ctx;
	vjson::Object doc;
	EXPECT_TRUE( vjson::ParseObject( doc,
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
	"empty_array": [],
	"array_123": [ 1, 2, 3 ],
	"empty_object": {}
})JSON", &ctx ) ) << "Parse failed.  " << ctx.error_message;

	// Exercise a bunch of basic accessors on the root document object
	ASSERT_EQ( doc.Type(), vjson::kObject );
	EXPECT_TRUE( doc.IsObject() );
	EXPECT_TRUE( &doc.AsObject() == &doc );
	EXPECT_TRUE( &doc.AsObjectOrEmpty() == &doc );
	EXPECT_TRUE( doc.AsObjectPtr() == &doc );
	EXPECT_TRUE( doc.AsArrayPtr() == nullptr );
	CheckTruthy( doc, vjson::kJibberish );

	// Check each of the subkeys

	{
		ASSERT_TRUE( doc.HasKey( "null" ) );
		EXPECT_TRUE( doc["null"].IsNull() );
		EXPECT_TRUE( doc["null"].Is<nullptr_t>() );
		EXPECT_TRUE( doc.ValuePtrAtKey("null")->IsNull() );
		CheckTruthyAtKey( doc, "null", vjson::kFalsish );
	}

	{
		bool val;

		ASSERT_TRUE( doc.HasKey( "true" ) );
		EXPECT_TRUE( doc["true"].IsBool() );
		EXPECT_TRUE( doc["true"].Is<bool>() );
		EXPECT_TRUE( doc["true"].AsBool( false ) );
		EXPECT_TRUE( doc["true"].AsBool() );
		EXPECT_TRUE( doc["true"].As<bool>() );

		val = false;
		EXPECT_EQ( doc["true"].Get( val ), vjson::kOK );
		EXPECT_TRUE( val );

		EXPECT_TRUE( doc.BoolAtKey("true", false ) );

		val = false;
		EXPECT_EQ( doc.GetAtKey("true", val ), vjson::kOK );
		EXPECT_TRUE( val );

		CheckTruthyAtKey( doc, "true", vjson::kTruish );
	}

	{
		bool val;

		ASSERT_TRUE( doc.HasKey( "false" ) );
		EXPECT_TRUE( doc["false"].IsBool() );
		EXPECT_TRUE( doc["true"].Is<bool>() );
		EXPECT_FALSE( doc["false"].AsBool( true ) );
		EXPECT_FALSE( doc["false"].AsBool() );
		EXPECT_FALSE( doc["false"].As<bool>() );

		val = true;
		EXPECT_EQ( doc["false"].Get( val ), vjson::kOK );
		EXPECT_FALSE( val );

		EXPECT_FALSE( doc.BoolAtKey("false", false ) );

		val = true;
		EXPECT_EQ( doc.GetAtKey("false", val ), vjson::kOK );
		EXPECT_FALSE( val );

		CheckTruthyAtKey( doc, "false", vjson::kFalsish );
	}

	{
		std::string sval;
		const char *cval;

		ASSERT_TRUE( doc.HasKey( "empty_string" ) );

		EXPECT_TRUE( doc["empty_string"].IsString() );
		EXPECT_TRUE( doc["empty_string"].Is<std::string>() );
		EXPECT_TRUE( doc["empty_string"].Is<const char *>() );

		EXPECT_TRUE( doc["empty_string"].AsString( "a non-empty string" ).empty() );
		EXPECT_TRUE( doc["empty_string"].AsString().empty() );
		EXPECT_TRUE( doc["empty_string"].As<std::string>().empty() );

		EXPECT_TRUE( doc["empty_string"].AsCString( "a non-empty string" ) == doc["empty_string"].AsString().c_str() );
		EXPECT_TRUE( doc["empty_string"].AsCString() == doc["empty_string"].AsString().c_str() );
		EXPECT_TRUE( doc["empty_string"].As<const char *>() == doc["empty_string"].AsString() );

		sval = "bogus";
		EXPECT_EQ( doc["empty_string"].Get( sval ), vjson::kOK );
		EXPECT_TRUE( sval.empty() );

		cval = "bogus";
		EXPECT_EQ( doc["empty_string"].Get( cval ), vjson::kOK );
		EXPECT_TRUE( *cval == '\0' );

		CheckTruthyAtKey( doc, "empty_string", vjson::kFalsish ); // Empty string is falsish
	}

	EXPECT_FALSE( doc.HasKey( "bogus_key" ) );
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

