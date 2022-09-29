/////////////////////////////////////////////////////////////////////////////
//
// Yes, it's yet another C++ JSON parser/printer and DOM.  Please see the
// README for some of the goals of this code, and why I wasn't happy with
// the other JSON DOMs/parsers I found and decided to reinvent this wheel.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef VJSON_H_INCLUDED
#define VJSON_H_INCLUDED

#include <string.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>

// #define VJSON_ASSERT to something else if you want to customize
// assertion behaviour.  Assertions during parsing are only for bugs, not for
// malformed input.  Some functions may assert if used incorrectly, and so
// if you write code that is totally intolierant of a document that doesn't
// match your expections, you might assert.  (There's always an easy way to
// avoid such assertions, though.)
#ifndef VJSON_ASSERT
	#include <assert.h>
	#define VJSON_ASSERT assert
#endif

// Default printing options.
#ifndef VJSON_DEFAULT_INDENT
	#define VJSON_DEFAULT_INDENT "\t"
#endif

namespace vjson {

// The different type of JSON values
enum EValueType
{
	kNull,
	kObject, // E.g. { "key1": "value1, "key2": 456 }  Aka "dictionary" or "map".  In JSON, they are usually called "objects"
	kArray, // E.g. [ "value1", 456, { } ]
	kString,
	kDouble,
	kNumber = kDouble, // Add a type alias - numbers are always doubles in JSON
	kBool,
	kDeleted, // used for debugging only
};

// Different things that can happen if you try to fetch a value.  You'll interact with
// this type when you want to have robust error handling against malformed documents.
// Note that using accessors that return defaults on if anything goes wrong is another
// approach.  Also, not all result values are appropriate in all situations
enum EResult
{
	kOK,
	kWrongType, // You asked for a value of type X, but we are type Y
	kNotArray, // You tried to access at item by array index from a value that is not an array
	kBadIndex, // Index was out of range
	kNotObject, // You tried to access at item by object key from a value that is not an object
	kBadKey, // Key not found in object
};

// Internal implementation details.  Nothing to see here, move along...
class Value; class Object; class Array;
struct PrintOptions; struct ParseContext;
struct ObjectKeyLess
{
	bool operator()( const std::string &l, const std::string &r ) const { return strcmp( l.c_str(), r.c_str() ) < 0; }

	// With C++14, we don't need to make a copy of the key to do lookup,
	// so long as the comparator can compare the types.
	bool operator()( const std::string &l, const char *r ) const { return strcmp( l.c_str(), r ) < 0; }
	bool operator()( const char *l, const std::string &r ) const { return strcmp( l, r.c_str() ) < 0; }

	// FIXME string_view?
};
using RawObject = std::map<std::string, Value, ObjectKeyLess>; // Internal storage for for objects.
using RawArray = std::vector<Value>; // Internal storage for arrays
using ObjectItem = RawObject::value_type; // A.k.a. std::pair<const std::string,Value>.  You'll get a reference to this when you iterate an Object
template<typename T> struct TypeTraits {};
template<> struct TypeTraits<nullptr_t> { static constexpr EValueType kType = kNull; using AsReturnType = void; using AsReturnTypeConst = void; };
template<> struct TypeTraits<bool> { static constexpr EValueType kType = kBool; using AsReturnType = bool&; using AsReturnTypeConst = const bool&; };
template<> struct TypeTraits<double> { static constexpr EValueType kType = kDouble; using AsReturnType = double&; using AsReturnTypeConst = const double&; };
template<> struct TypeTraits<int> { static constexpr EValueType kType = kNumber; using AsReturnType = int; using AsReturnTypeConst = int; };
template<> struct TypeTraits<const char *> { static constexpr EValueType kType = kString; using AsReturnType = const char *; using AsReturnTypeConst = const char *; };
template<> struct TypeTraits<std::string> { static constexpr EValueType kType = kString; using AsReturnType = std::string &; using AsReturnTypeConst = const std::string &; };
template<> struct TypeTraits<Object> { static constexpr EValueType kType = kObject; using AsReturnType = Object &; using AsReturnTypeConst = const Object &; };
template<> struct TypeTraits<Array> { static constexpr EValueType kType = kObject; using AsReturnType = Array &; using AsReturnTypeConst = const Array &; };
template<typename T, typename V, typename A, typename R> struct ArrayIter;
template<typename T> using ConstArrayIter = ArrayIter<T, const Value *, const RawArray &, typename TypeTraits<T>::AsReturnTypeConst >;
template<typename T> using MutableArrayIter = ArrayIter<T, Value *, RawArray &, typename TypeTraits<T>::AsReturnType >;
template<typename T, typename V, typename A, typename I> struct ArrayRange;
template<typename T> using ConstArrayRange = ArrayRange< T, const Value *, const RawArray &, ConstArrayIter<T> >;
template<typename T> using MutableArrayRange = ArrayRange< T, Value *, RawArray &, MutableArrayIter<T> >;

/////////////////////////////////////////////////////////////////////////////
//
// DOM classes
//
/////////////////////////////////////////////////////////////////////////////

// Return a reference to a Null value, empty array, or empty object.
// Do not modify this object!
extern const Value &GetStaticNullValue();
extern const Array &GetStaticEmptyArray();
extern const Object &GetStaticEmptyObject();

// A Value is a "node" in the DOM, either a primitive type (null, string,
// bool, or number) or an aggregate type (object or array).
class Value
{
public:

	//
	// Read this Value as a specific data type.
	//
	// There are a few basic types of accessors, depending on how you want
	// to handle the value being the wrong type
	//
	// - {Type} As{Typename}() : will assert or misbehave if the Value is
	//   not the right type.
	//   There's also template-style: Type As<Type>()
	// - {Type} As{Typename}( {Type} defaultValue ) : return the
	//   supplied default value if the Valwue is not the right type.
	// - bool Get( {Type} &out ) : put the result in your output variable,
	//   return true if we were successful, false if it was the wrong type.
	//
	// NOTE: very often you are interested in accessing a child value from an Object
	//       or Array.  In that case, use one of the array/object lookup functions
	//       below, which combine lookup and type conversion in one conveinent call!
	//

	// Get this value as the specified type.  If the value is not the exact
	// JSON type, returns the default you specify.  No conversions are attempted.
	const char *AsCString( const char *       defaultVal ) const { return _type == kString ? _string.c_str() : defaultVal; }
	std::string AsString(  const char *       defaultVal ) const { return _type == kString ? _string : std::string( defaultVal ); } // NOTE: always returns a copy
	std::string AsString(  const std::string &defaultVal ) const { return _type == kString ? _string : defaultVal; } // NOTE: always returns a copy, because defaultVal could be a temp!
	bool        AsBool(    bool               defaultVal ) const { return _type == kBool ? _bool : defaultVal; } // NOTE: requires exact bool type!
	double      AsDouble(  double             defaultVal ) const { return _type == kDouble ? _double : defaultVal; }
	int         AsInt(     int                defaultVal ) const { return _type == kDouble ? (int)_double : defaultVal; }

	// Cast and return reference, returning an empty object/array if we are not the right type
	const Object &AsObjectOrEmpty() const { return _type == kObject ? *(const Object *)this : GetStaticEmptyObject(); }
	const Array  &AsArrayOrEmpty()  const { return _type == kArray ? *(const Array *)this : GetStaticEmptyArray(); }

	// Cast and return pointer, returning nullptr if we are not the right type.
	// Similar to dynamic_cast<> (which cannot be used here because we don't have a vtable)
	const Object *AsObjectPtr() const { return _type == kObject ? (const Object *)this : nullptr; }
	Object *      AsObjectPtr()       { return _type == kObject ? (Object *)this : nullptr; }
	const Array * AsArrayPtr()  const { return _type == kArray ? (const Array *)this : nullptr; }
	Array *       AsArrayPtr()        { return _type == kArray ? (Array *)this : nullptr; }

	// Perform a "static cast" of the value to the specified type.  The value must be
	// already be the correct type, no converstions or type checks are attempted.
	// NOTE: These will assert/crash if called on the wrong type!
	const char *       AsCString() const { VJSON_ASSERT( _type == kString ); return _string.c_str(); }
	const std::string &AsString()  const { VJSON_ASSERT( _type == kString ); return _string; }
	std::string &      AsString()        { VJSON_ASSERT( _type == kString ); return _string; }
	const bool &       AsBool()    const { VJSON_ASSERT( _type == kBool ); return _bool; } // NOTE: requires exact bool type!
	bool &             AsBool()          { VJSON_ASSERT( _type == kBool ); return _bool; } // NOTE: requires exact bool type!
	const double &     AsDouble()  const { VJSON_ASSERT( _type == kDouble ); return _double; }
	double &           AsDouble()        { VJSON_ASSERT( _type == kDouble ); return _double; }
	int                AsInt()     const { VJSON_ASSERT( _type == kDouble ); return (int)_double; }
	const Object &     AsObject()  const { VJSON_ASSERT( _type == kObject ); return *(const Object*)this; }
	Object &           AsObject()        { VJSON_ASSERT( _type == kObject ); return *(Object*)this; }
	const Array &      AsArray()   const { VJSON_ASSERT( _type == kArray ); return *(const Array*)(this); }
	Array &            AsArray()         { VJSON_ASSERT( _type == kArray ); return *(Array*)this; }

	// Template-style As<T> for the AsXxxxx static-cast methods above.
	// (See full list of specializations at the bottom of this file.)
	template<typename T> typename TypeTraits<T>::AsReturnTypeConst As() const;
	template<typename T> typename TypeTraits<T>::AsReturnType As();

	// Get the value as the specified type into your result value.
	// If the value is not the correct JSON type, returns kWrongType.
	// no conversions are attempted.
	EResult Get( const char *  &outX ) const { if ( _type != kString ) return kWrongType; outX = _string.c_str(); return kOK; }
	EResult Get( std::string   &outX ) const { if ( _type != kString ) return kWrongType; outX = _string; return kOK; }
	EResult Get( bool          &outX ) const { if ( _type != kBool ) return kWrongType; outX = _bool; return kOK; }
	EResult Get( double        &outX ) const { if ( _type != kDouble ) return kWrongType; outX = _double; return kOK; }
	EResult Get( int           &outX ) const { if ( _type != kDouble ) return kWrongType; outX = (int)_double; return kOK; }
	EResult Get( const Object *&outX ) const { if ( _type != kObject ) return kWrongType; outX = (const Object*)this; return kOK; }
	EResult Get( Object*       &outX )       { if ( _type != kObject ) return kWrongType; outX = (Object*)this; return kOK; }
	EResult Get( const Array * &outX ) const { if ( _type != kArray ) return kWrongType; outX = (const Array*)this; return kOK; }
	EResult Get( Array *       &outX )       { if ( _type != kArray ) return kWrongType; outX = (Array*)this; return kOK; }

	// Get the value as the specified type, performing "reasonable" conversions
	// such as parsing strings, treating 0 and 1 as false and true,
	// converting to string, etc.  If conversion is not possible, your default
	// is returned.
	// NOTE that ToString() does not do JSON formatting, escaping, etc.
	std::string ToString(  const char *       defaultVal = "" ) const; // All "scalar" values can be converted to string.  Arrays and objects will fail.
	std::string ToString(  const std::string &defaultVal = std::string{} ) const;
	bool        ToBool(    bool               defaultVal = false ) const;
	double      ToDouble(  double             defaultVal = 0.0 ) const;
	int         ToInt(     int                defaultVal = 0 ) const;
	uint64_t    ToUInt64(  uint64_t           defaultVal = 0 ) const;

	// Get the value as the specified type into your result value, performing
	// reasonable conversions if necessary.  If conversion is not possible,
	// kWrongType is returned and your value is not modified.
	EResult Convert( std::string   &outX ) const;
	EResult Convert( bool          &outX ) const;
	EResult Convert( double        &outX ) const;
	EResult Convert( int           &outX ) const;
	EResult Convert( double        &outX ) const;
	EResult Convert( uint64_t      &outX ) const;

	//
	// Construction and assignment
	//

	// Default constructor sets us to a null value
	Value() : _type( kNull ) {}

	// Construct value with the given kind and default value for that kind (0 or empty)
	Value( EValueType type );

	// Basic C++ object lifetime stuff
	Value( const Value &x ) { InternalConstruct( x ); }
	Value( Value &&x ) { InternalConstruct( x ); }
	~Value() { InternalDestruct(); }

	// Construct directly from primitive values.
	Value( bool               x ) : _type( kBool ) { _bool = x; }
	Value( double             x ) : _type( kDouble ) { _double = x; }
	Value( int                x ) : _type( kDouble ) { _double = (double)x; }
	Value( const char *       x );
	Value( const std::string &x );
	Value( std::string &&     x );
	Value( nullptr_t            ) = delete; // To avoid confusion.  Use default constructor or kNull

	// Construct from internal object/array storage
	Value( const RawObject & x );
	Value( RawObject &&      x );
	Value( const RawArray &  x );
	Value( RawArray &&       x );

	// Assignment
	Value &operator=( const Value &      x );
	Value &operator=( Value &&           x );
	Value &operator=( bool               x ) { InternalDestruct(); _type = kBool; _bool = x; return *this; }
	Value &operator=( double             x ) { InternalDestruct(); _type = kDouble; _double = x; return *this; }
	Value &operator=( int                x ) { InternalDestruct(); _type = kDouble; _double = (double)x; return *this; }
	Value &operator=( const char *       x );
	Value &operator=( const std::string &x );
	Value &operator=( std::string &&     x );
	Value &operator=( nullptr_t            ) { SetNull(); return *this; }
	Value &operator=( const RawArray &   x );
	Value &operator=( RawArray &&        x );
	Value &operator=( const RawObject &  x );
	Value &operator=( RawObject &&       x );
	void SetNull() { InternalDestruct(); _type = kNull; }
	void SetEmptyObject();
	void SetEmptyArray();
	void SetUint64AsString( uint64_t x );

	// Assign array from list of T's, where T is anything we can construct a Value from
	// See also class Array constructors
	template <typename T> void SetArray( const T *begin, const T *end );
	template <typename T> void SetArray( size_t n, const T *begin ) { SetArray( begin, begin+n ); }
	template <typename T> void SetArray( std::initializer_list<T> x ) { SetArray( x.begin(), x.end() ); }; // E.g. SetArray( { "one", "two", "three" } );   Note that they must all be the same type.  (If not, wrap all with Value constructors.)

	//
	// Type checking
	//
	// NOTE: One of the goals of this library is to reduce the amount of
	// tedius type checking that must be done when traversing the DOM.  If you
	// just wish to ignore missing or wrongly typed data, there's usually
	// a way to avoid an explicit type check!
	//

	// Return true if we are the specified type
	bool IsNull() const { return _type == kNull; }
	bool IsObject() const { return _type == kObject; }
	bool IsArray() const { return _type == kArray; }
	bool IsString() const { return _type == kString; }
	bool IsNumber() const { return _type == kDouble; }
	bool IsDouble() const { return _type == kDouble; }
	bool IsBool() const { return _type == kBool; }

	// Template-style access, e.g. if ( val.Is<bool>() ).
	// See the full list of specializations below.
	template<typename T> bool Is() const;

	// Return the type of thing we are
	EValueType Type() const { return _type; }

	//
	// Object access
	//
	// This base Value class can do a bunch of things on objects.  All functions
	// do checking and will return a sensible "failure" result if called on
	// non-object, or if key is not found.  The derived Object class is available,
	// and has range-based for and operator[], for a more idiomatic access.
	// Note that to iterate the key/value pairs, you can use something like
	//
	//    for ( auto it: val.AsObjectOrEmpty() ) {}
	//
	// Many lookup functions accept the key argument as a tmeplate argument.
	// This was done primarily to keep the code small.  You really can only
	// pass a std::string or a const char *.
	//

	//
	// Read the child from an object at a given key as a specified type.
	//
	// - All {TypeName}AtKey() functions will return some default value if called
	//   on a non-Object, the key is not present, or the child is the wrong type.  Use
	//   these when you don't care about the reason for failure.
	// - GetAtKey() overloads place the result into your output variable, and return
	//   a status code to indicate sucess or the different failure cases.
	//

	// Get the value at the specified key as the specified type.  If this is not an object,
	// or the key is not found, or the item is the wrong type, returns your default value
	template <typename K> const char *  CStringAtKey  ( K&& key, const char *defaultVal        ) const { const Value *t = InternalAtKey( key, kString ); return t ? t->_string.c_str() : defaultVal; }
	template <typename K> std::string   StringAtKey   ( K&& key, const char *defaultVal        ) const { const Value *t = InternalAtKey( key, kString ); return t ? t->_string : std::string( defaultVal ); } // NOTE: always returns a copy
	template <typename K> std::string   StringAtKey   ( K&& key, const std::string &defaultVal ) const { const Value *t = InternalAtKey( key, kString ); return t ? t->_string : defaultVal; } // NOTE: always returns a copy
	template <typename K> bool          BoolAtKey     ( K&& key, bool               defaultVal ) const { const Value *t = InternalAtKey( key, kBool ); return t ? t->_bool : defaultVal; } // Requires strict bool type
	template <typename K> double        DoubleAtKey   ( K&& key, double             defaultVal ) const { const Value *t = InternalAtKey( key, kDouble ); return t ? t->_bool : defaultVal; }
	template <typename K> int           IntAtKey      ( K&& key, int                defaultVal ) const { const Value *t = InternalAtKey( key, kDouble ); return t ? (int)t->_double : defaultVal; }
	template <typename K> const Object *ObjectPtrAtKey( K&& key                                ) const { (const Object *)InternalAtKey( key, kObject ); }
	template <typename K> Object *      ObjectPtrAtKey( K&& key                                )       { return (Object *)InternalAtKey( key, kObject ); }
	template <typename K> const Array * ArrayPtrAtKey ( K&& key                                ) const { return (const Array *)InternalAtKey( key, kArray ); }
	template <typename K> Array *       ArrayPtrAtKey ( K&& key                                )       { return (const Array *)InternalAtKey( key, kArray ); }

	// Get the item at the specified key as an array/object.  If this is not an object, or the
	// key is not found, or the item is the wrong type, returns reference to empty array/object
	template <typename K> const Array &ArrayAtKeyOrEmpty( K&& key ) const { const Array *t = (const Array *)InternalAtKey( key, kArray ); return t ? *t : GetStaticEmptyArray(); }
	template <typename K> const Object &ObjectAtKeyOrEmpty( K&& key ) const { const Object *t = (const Object *)InternalAtKey( key, kObject ); return t ? *t : GetStaticEmptyObject(); }

	// Get the value as the specified type into your result value.
	// Any type for which Get( T & ) is defined will work.
	template <typename T, typename K> EResult GetAtKey( K&& key, T &outX ) const;
	template <typename K> EResult GetAtKey( K&& key, const Value *&outX ) const;
	template <typename K> EResult GetAtKey( K&& key, Value *&outX );

	//
	// Lookup by key for generic Values (does not check the type of the child).
	//

	// Get pointer to Value at the specified key.  If called on a Value that
	// isn't an Object, or if the key is not found, returns nullptr
	Value *      ValuePtrAtKey( const std::string &key );
	const Value *ValuePtrAtKey( const std::string &key ) const { return const_cast<Value*>(this)->ValuePtrAtKey( key ); }
	Value *      ValuePtrAtKey( const char *       key );
	const Value *ValuePtrAtKey( const char *       key ) const { return const_cast<Value*>(this)->ValuePtrAtKey( key ); }

	// Return reference to the value at the specified key.  If this is not an object,
	// or the key is not found, returns a reference to a statically-allocated null
	// value.
	//
	// Note that there is no non-const version of this function!
	// - To mutate the child if it exists, use ValuePtrAtKey() (and check for null)
	// - To add or replace a key, use SetKey() or Object::operator[]
	// - To get a mutable reference to the value, adding a new value if one does
	//   not exist, use Object::operator[]
	// - Access the underlying RawObject (which is just a std::map) directly.
	template <typename K> const Value &AtKey( K&& key ) const { const Value *t = ValuePtrAtKey( key ); return t ? *t : GetStaticNullValue(); }

	//
	// Other object accessors
	//

	// Return true if this is an object, and the key is present
	bool HasKey( const std::string &key ) const { return ValuePtrAtKey( key ) != nullptr; }
	bool HasKey( const char        *key ) const { return ValuePtrAtKey( key ) != nullptr; }

	// Return number of key/values pairs in object as int or size_t, according to your
	// predeliction for pedantic bullcrap related size_t and the C type system.
	// Returns 0 if this value is not an object.
	int ObjectLen() const { return _type == kObject ? (int)_object.size() : 0; }
	size_t ObjectSize() const { return _type == kObject ? _object.size() : 0; }

	// Set the value at the given key.  If the key is not present, it is added.
	// If the key is present, the value is changed.  Returns kNotObject if this
	// is not an object.  See also Object::operator[].  T can be anything
	// that a Value can be constructed from.
	template <typename T, typename K> EResult SetAtKey( K&& key, T&& value );

	// Delete the specified key.  Returns kOK, kNotObject, or kBadKey
	template <typename K> EResult EraseAtKey( K&& key );

	// Effeciently move the value at the specified key into your
	// result and remove it from this object.  Returns kOK, kNotObject,
	// or kBadKey.  Your value is not modified on failure.
	template <typename K> EResult DetachAtKey( K&& key, Value *pOutDetached );

	//
	// Array access
	//
	// This base Value class can do a bunch of things on array.  All functions
	// do checking and will return a sensible "failure" result if called on
	// non-array, or if the index is invalid.  The derived Array class is available,
	// and has range-based for and operator[], for a more idiomatic access.
	// Note that to iterate the items in an array, you can use something like
	//
	//    for ( auto it: val.AsArrayOrEmpty() ) {}
	//

	//
	// Read the child from an array at a given index as a specified type.
	//
	// - All {TypeName}AtIndex() functions will return some default value if called
	//   on a non-array, the index is invalid, or the child is the wrong type.  Use
	//   these when you don't care about the reason for failure.
	// - GetAtIndex() overloads place the result into your output variable, and return
	//   a status code to indicate sucess or the different failure cases.
	//

	// Get the value at the specified index as the specified type.  If this is not an array,
	// or the index is invalid, or the item is the wrong type, returns your default value
	const char *  CStringAtIndex  ( size_t idx, const char *       defaultVal ) const { const Value *t = InternalAtIndex( idx, kString ); return t ? t->_string.c_str() : defaultVal; }
	std::string   StringAtIndex   ( size_t idx, const char *       defaultVal ) const { const Value *t = InternalAtIndex( idx, kString ); return t ? t->_string : std::string( defaultVal ); } // NOTE: always returns a copy
	std::string   StringAtIndex   ( size_t idx, const std::string &defaultVal ) const { const Value *t = InternalAtIndex( idx, kString ); return t ? t->_string : defaultVal; } // NOTE: always returns a copy
	bool          BoolAtIndex     ( size_t idx, bool               defaultVal ) const { const Value *t = InternalAtIndex( idx, kBool ); return t ? t->_bool : defaultVal; } // Requires strict bool type
	ETruthy       TruthyAtIndex   ( size_t idx                                ) const { const Value *t = ValuePtrAtIndex( idx ); return t ? t->AsTruthy() : kGibberish; } // Returns kGibberish if we're not an array, bad index, or value at index cannot be classified
	double        DoubleAtIndex   ( size_t idx, double             defaultVal ) const { const Value *t = InternalAtIndex( idx, kDouble ); return t ? t->_bool : defaultVal; }
	int           IntAtIndex      ( size_t idx, int                defaultVal ) const { const Value *t = InternalAtIndex( idx, kDouble ); return t ? (int)t->_double : defaultVal; }
	const Object *ObjectPtrAtIndex( size_t idx                                ) const { return (const Object *)InternalAtIndex( idx, kObject ); }
	Object *      ObjectPtrAtIndex( size_t idx                                )       { return (Object *)InternalAtIndex( idx, kObject ); }
	const Array * ArrayPtrAtIndex ( size_t idx                                ) const { return (const Array *)InternalAtIndex( idx, kArray ); }
	Array *       ArrayPtrAtIndex ( size_t idx                                )       { return (Array *)InternalAtIndex( idx, kArray ); }

	// Get the item at the specified index as an array/object.  If this is not an array, or the
	// index is invalid or the wrong type, returns reference to empty array/object
	const Array &ArrayAtIndexOrEmpty( size_t idx ) const { const Array *t = (const Array *)InternalAtIndex( idx, kArray ); return t ? *t : GetStaticEmptyArray(); }
	const Object &ObjectAtIndexOrEmpty( size_t idx ) const { const Object *t = (const Object *)InternalAtIndex( idx, kObject ); return t ? *t : GetStaticEmptyObject(); }

	// Get the value as the specified type into your result value.
	// Any type for which Get( T & ) is defined will work.
	template <typename T> EResult GetAtIndex( size_t idx, T &outX ) const;
	EResult GetAtIndex( size_t idx, Value *&outX );

	// Test child at index for being truish or falsish.  You'll usually
	// use this instead of using TruthyAtIndex.  If this is not an array
	// or the index is invalid, both functions return false.
	bool IsTruishAtIndex( size_t idx ) const { const Value *t = ValuePtrAtIndex( idx ); return t && t->IsTruish(); }
	bool IsFalsishAtIndex( size_t idx ) const { const Value *t = ValuePtrAtIndex( idx ); return t && t->IsFalsish(); }

	//
	// Lookup by index for generic Values (does not check the type of the child).
	//

	// Return reference to the value at the specified index.  If this is not an array,
	// or the index is out of bounds, returns a reference to a statically-allocated null
	// value.
	//
	// Note that there is no non-const version of this function!  To modify an at
	// a given index, either use ValuePtrAtIndex() or Array::operator[]
	const Value &AtIndex( size_t idx ) const { return ( _type == kArray && idx < _array.size() ) ? _array[idx] : GetStaticNullValue(); }

	// Get pointer to Value at the specified index.  If you call this on a Value
	// that isn't an Array, or the index is invalid, returns nullptr
	const Value *ValuePtrAtIndex( size_t idx ) const { return ( _type == kArray && idx < _array.size() ) ? &_array[idx] : nullptr; }
	Value *      ValuePtrAtIndex( size_t idx )       { return ( _type == kArray && idx < _array.size() ) ? &_array[idx] : nullptr; }

	// Get the length of the array as an int or size_t, according to your
	// predeliction for pedantic bullcrap related size_t and the C type system.
	// Returns 0 if this value is not an array.
	int ArrayLen() const { return _type == kArray ? (int)_array.size() : 0; }
	size_t ArraySize() const { return _type == kArray ? _array.size() : 0; }

protected:

	EValueType _type;
	union
	{
		double _double;
		bool _bool;
		RawObject _object;
		RawArray _array;
		std::string _string;
		struct { char x[8]; } _dummy;
	};

	void InternalDestruct();
	void InternalConstruct( const Value &x );
	void InternalConstruct( Value &&x );
	Value *InternalAtIndex( size_t idx, EValueType t ) const;
	Value *InternalAtKey( const std::string &key, EValueType t ) const;
	Value *InternalAtKey( const char *key, EValueType t ) const;
};

// An Object is a Value that is known (or at least assumed) to be of type
// kObject.  Since it is assumed to be an object, we can provide a more
// idiomatic object interface, and we can optimize a few function calls.
class Object : public Value
{
public:
	Object() : Value( kObject ) {}
	Object( const Object &x ) : Value( x ) {}
	Object( Object &&x ) : Value( std::forward<Object>(x) ) {}

	// Override ObjectLen(), we know we are an Object
	int ObjectLen() const { VJSON_ASSERT( _type == kObject ); return (int)_object.size(); }
	size_t ObjectSize() const { VJSON_ASSERT( _type == kObject ); return _object.size(); }
	int Len() const { VJSON_ASSERT( _type == kObject ); return (int)_object.size(); }
	size_t size() const { VJSON_ASSERT( _type == kObject ); return _object.size(); }

	// Standard array access notation Operator[].  This works just like the std::map
	// version.  It inserts the default argument if not found, and cannot be invoked
	// though a const reference.  (Use Value::AtKey() for read-only access that won't
	// add a new key if the key is not already present. )
	Value &operator[]( const std::string &key ) { VJSON_ASSERT( _type == kObject ); return _object[ key ]; }
	Value &operator[]( const char *key ) { VJSON_ASSERT( _type == kObject ); return _object[ std::string(key) ];  }

	// Return true if the object is empty
	bool empty() const { VJSON_ASSERT( _type == kObject ); return _object.empty(); }

	// Remove all the items from the object
	void clear() { VJSON_ASSERT( _type == kObject ); _object.clear(); }

	// Access underying object
	inline RawObject       &Raw()       { VJSON_ASSERT( _type == kObject ); return _object; }
	inline RawObject const &Raw() const { VJSON_ASSERT( _type == kObject ); return _object; }

	// Range-based for.  Example:
	//
	// Object obj;
	// for ( ObjectItem &item: obj )
	// {
	//    const std::string & key = item.first;
	//    Value &val = item.second;
	// }

	// Iterate all values.
	RawObject::iterator begin() { VJSON_ASSERT( _type == kObject ); return _object.begin(); }
	RawObject::iterator end() { VJSON_ASSERT( _type == kObject ); return _object.end(); }
	RawObject::const_iterator begin() const { VJSON_ASSERT( _type == kObject ); return _object.begin(); }
	RawObject::const_iterator end() const { VJSON_ASSERT( _type == kObject ); return _object.end(); }

	// TODO - add type-specific iterators, so you can easily iterate, e.g. all the ints
};

// An Array is a Value that is known (or at least assumed) to be of type
// kArray.  Since it is assumed to be an array, we can provide a more
// idiomatic object interface, and we can optimize a few function calls.
class Array : public Value
{
public:
	Array() : Value( kArray ) {}
	Array( const Array &x ) : Value( x ) {}
	Array( Array &&x ) : Value( std::forward<Array>(x) ) {}
	Array( const RawArray &x ) : Value( x ) {}
	Array( RawArray && x ) : Value( std::forward<RawArray>( x ) ) {}

	// Construct Array from initializer list.
	// E.g. Value( { "one", 5.0, false } )
	//Value( std::initializer_list<Value> x ); FIXME

	// Override ArrayLen(), we know we are an array.  Also provide shorter versions
	int ArrayLen() const { VJSON_ASSERT( _type == kArray ); return (int)_array.size(); }
	size_t ArraySize() const { VJSON_ASSERT( _type == kArray ); return _array.size(); }
	int Len() const { VJSON_ASSERT( _type == kArray ); return (int)_array.size(); }
	size_t size() const { VJSON_ASSERT( _type == kArray ); return _array.size(); } // not capitalized because we want to be as similar to std::vector as possible

	// Standard array access notation Operator[]
	Value &operator[]( size_t idx ) { VJSON_ASSERT( _type == kArray ); return _array[idx]; }
	const Value &operator[]( size_t idx ) const { VJSON_ASSERT( _type == kArray ); return _array[idx]; }

	// Return true if the array is empty
	bool empty() const { VJSON_ASSERT( _type == kArray ); return _array.empty(); }

	// Remove all the items from the array
	void clear() { VJSON_ASSERT( _type == kArray ); _array.clear(); }

	// Add a null value to the end of the the array, and return a reference
	Value &push_back() { VJSON_ASSERT( _type == kArray ); _array.push_back( Value{} ); return _array[ _array.size()-1 ]; }

	// Push something to the end of the array, and return a reference to the newly created
	// thing.  Any argument from which you can construct a Value will work.
	template< typename Arg > Value &push_back( Arg &&a ) { VJSON_ASSERT( _type == kArray ); _array.push_back( std::forward<Arg>( a ) ); return _array[ _array.size()-1 ]; }

	// Get direct access to the underlying vector
	const RawArray &Raw() const { VJSON_ASSERT( _type == kArray ); return _array; }
	RawArray &Raw() { VJSON_ASSERT( _type == kArray ); return _array; }

	// Iterate all elements.  Example:
	//
	//   Array arr;
	//   for ( Value &val: arr ) {}
	Value *begin() { VJSON_ASSERT( _type == kArray ); return _array.data(); }
	Value *end() { VJSON_ASSERT( _type == kArray ); return _array.data() + _array.size(); }
	const Value *begin() const { VJSON_ASSERT( _type == kArray ); return _array.data(); }
	const Value *end() const { VJSON_ASSERT( _type == kArray ); return _array.data() + _array.size(); }

	// Iterate only the specified types.  (Skip elements of other types.)
	// Any T for which you can do Value::As<T> will work.
	//
	// Examples:
	//
	//   Array arr;
	//   for ( const char *s: arr.Iter<const char *>() ) {}
	//   for ( Object &x: arr.Iter<Object>() ) {}
	template <typename T> ConstArrayRange<T> Iter() const;
	template <typename T> MutableArrayRange<T> Iter();
};

/////////////////////////////////////////////////////////////////////////////
//
// Printing and parsing
//
/////////////////////////////////////////////////////////////////////////////

// Specify options for how you want your JSON formatted.
struct PrintOptions
{
	// How to indent.  If empty, we use "minified" json with absolutely
	// no extra whitespace.  Otherwise, we assume you want to
	// pretty-print, with the specified indentation repeated for each
	// indentation level.  You can use tabs or spaces, as your preference.
	const char *indent = VJSON_DEFAULT_INDENT;
};

// Print the value to JSON text.
std::string ToString( const Value &v, const PrintOptions &opt );

// Struct used to pass parsing options, and receive the error message
struct ParseContext
{
	// Options
	bool allow_trailing_comma = false;
	bool allow_cpp_comments = false;

	// If there's an error, it will be returned here
	std::string error_message;

	// Line where error occurred.  1-based
	int error_line = 0;

	// Byte offset where the error occurred.  0-based.
	int error_byte_offset = 0;
};

// Parse any legitimate JSON entity.  If you are OK with the default parsing
// options and don't care about good error handling, you don't need to provide
// the ParseContext.
bool ParseValue( Value &out, const char *begin, const char *end, ParseContext *ctx = nullptr );

// Very similar to ParseValue, but fail if the input is not a single JSON object/array.
bool ParseObject( Object &out, const char *begin, const char *end, ParseContext *ctx = nullptr );
bool ParseArray( Array &out, const char *begin, const char *end, ParseContext *ctx = nullptr );

// Parse from std::string
inline bool ParseValue ( Value  &out, const std::string &s, ParseContext *ctx = nullptr ) { return ParseValue ( out, s.c_str(), s.c_str() + s.length(), ctx ); }
inline bool ParseObject( Object &out, const std::string &s, ParseContext *ctx = nullptr ) { return ParseObject( out, s.c_str(), s.c_str() + s.length(), ctx ); }
inline bool ParseArray ( Array  &out, const std::string &s, ParseContext *ctx = nullptr ) { return ParseArray ( out, s.c_str(), s.c_str() + s.length(), ctx ); }

// Parse from '\0'-terminated C string
inline bool ParseValue ( Value  &out, const char *c_str, ParseContext *ctx = nullptr ) { return ParseValue ( out, c_str, c_str + strlen(c_str), ctx ); }
inline bool ParseObject( Object &out, const char *c_str, ParseContext *ctx = nullptr ) { return ParseObject( out, c_str, c_str + strlen(c_str), ctx ); }
inline bool ParseArray ( Array  &out, const char *c_str, ParseContext *ctx = nullptr ) { return ParseArray ( out, c_str, c_str + strlen(c_str), ctx ); }


/////////////////////////////////////////////////////////////////////////////
//
// Internal stuff
//
// Why are you still reading this?
//
/////////////////////////////////////////////////////////////////////////////

template<> inline bool Value::Is<nullptr_t   >() const { return _type == kNull; }
template<> inline bool Value::Is<Object      >() const { return _type == kObject; }
template<> inline bool Value::Is<Array       >() const { return _type == kArray; }
template<> inline bool Value::Is<const char *>() const { return _type == kString; }
template<> inline bool Value::Is<std::string >() const { return _type == kString; }
template<> inline bool Value::Is<double      >() const { return _type == kDouble; }
template<> inline bool Value::Is<bool        >() const { return _type == kBool; }
template<> inline const char *       Value::As<const char *>() const { VJSON_ASSERT( _type == kString ); return _string.c_str(); }
template<> inline const char *       Value::As<const char *>()       { VJSON_ASSERT( _type == kString ); return _string.c_str(); }
template<> inline const std::string &Value::As<std::string >() const { VJSON_ASSERT( _type == kString ); return _string; }
template<> inline std::string &      Value::As<std::string >()       { VJSON_ASSERT( _type == kString ); return _string; }
template<> inline const bool &       Value::As<bool        >() const { VJSON_ASSERT( _type == kBool ); return _bool; } // NOTE: requires exact bool type!
template<> inline bool &             Value::As<bool        >()       { VJSON_ASSERT( _type == kBool ); return _bool; } // NOTE: requires exact bool type!
template<> inline ETruthy            Value::As<ETruthy     >() const { return AsTruthy(); }
template<> inline ETruthy            Value::As<ETruthy     >()       { return AsTruthy(); }
template<> inline const double &     Value::As<double      >() const { VJSON_ASSERT( _type == kDouble ); return _double; }
template<> inline double &           Value::As<double      >()       { VJSON_ASSERT( _type == kDouble ); return _double; }
template<> inline int                Value::As<int         >() const { VJSON_ASSERT( _type == kDouble ); return (int)_double; }
template<> inline int                Value::As<int         >()       { VJSON_ASSERT( _type == kDouble ); return (int)_double; }
template<> inline const Object &     Value::As<Object      >() const { VJSON_ASSERT( _type == kObject ); return *(const Object*)this; }
template<> inline Object &           Value::As<Object      >()       { VJSON_ASSERT( _type == kObject ); return *(Object*)this; }
template<> inline const Array &      Value::As<Array       >() const { VJSON_ASSERT( _type == kArray ); return *(const Array*)(this); }
template<> inline Array &            Value::As<Array       >()       { VJSON_ASSERT( _type == kArray ); return *(Array*)this; }


template <typename T>
inline EResult Value::GetAtIndex( size_t idx, T &outX ) const
{
	if ( _type != kArray ) return kNotArray;
	if ( idx >= _array.size() ) return kBadIndex;
	return _array[idx].Get( outX );
}

template <>
inline EResult Value::GetAtIndex<const Value*>( size_t idx, const Value *&outX ) const
{
	if ( _type != kArray ) return kNotArray;
	if ( idx >= _array.size() ) return kBadIndex;
	outX = &_array[idx];
	return kOK;
}

inline EResult Value::GetAtIndex( size_t idx, Value *&outX )
{
	if ( _type != kArray ) return kNotArray;
	if ( idx >= _array.size() ) return kBadIndex;
	outX = &_array[idx];
	return kOK;
}

template <typename T, typename K>
inline EResult Value::GetAtKey( K&& key, T &outX ) const
{
	if ( _type != kObject ) return kNotObject;
	RawObject::const_iterator i = _object.find( key );
	if ( i == _object.end() ) return kBadKey;
	return i->second.Get( outX );
}

template <typename K>
inline EResult Value::GetAtKey( K&& key, const Value *&outX ) const
{
	if ( _type != kObject ) return kNotObject;
	RawObject::const_iterator i = _object.find( key );
	if ( i == _object.end() ) return kBadKey;
	outX = &i->second;
	return kOK;
}

template <typename K>
inline EResult Value::GetAtKey( K&& key, Value *&outX )
{
	if ( _type != kObject ) return kNotObject;
	RawObject::iterator i = _object.find( key );
	if ( i == _object.end() ) return kBadKey;
	outX = &i->second;
	return kOK;
}

template <typename T, typename K>
inline bool Value::SetAtKey( K&& key, T&& value )
{
	if ( _type != kObject ) return false;
	_object[ std::forward<K>( key ) ] = std::forward<T>( value );
	return true;
}

template <typename K>
EResult Value::EraseAtKey( K&& key )
{
	if ( _type != kObject ) return kNotObject;
	if ( _object.erase( std::forward<K>( key ) ) == 0 ) return kBadKey;
	return kOK;
}

template <typename K>
EResult Value::DetachAtKey( K&& key, Value *pOutDetached )
{
	if ( _type != kObject ) return kNotObject;
	RawObject::iterator i = _object.find( key );
	if ( i == _object.end() ) return kBadKey;
	if ( pOutDetached )
		*pOutDetached = std::move( i->second );
	_object.erase( i );
	return kOK;
}

template <typename T>
void Value::SetArray( const T *begin, const T *end )
{
	VJSON_ASSERT( begin <= end );
	InternalDestruct();
	_type = kArray;
	new (&_array) RawArray( begin, end );
}


template<typename T, typename V, typename A, typename R>
struct ArrayIterator {
	V v; A arr;
	ArrayIterator( V b, A a ) : v(b), arr(a) { this->Next(); } 
	R operator*() const { return this->v->template As<T>(); }
	void operator++() {
		VJSON_ASSERT( this->v <= this->end() ); // incremented, but already at the end.  Or, array was modified during iteration
		++this->v; this->Next();
	}

	template<typename XV, typename XA, typename XR>
	bool operator!=(const ArrayIterator<T,XV,XA,XR> &x ) const {
		VJSON_ASSERT( &this->arr == &x.arr ); // Should only compare two iterators created from the same array
		VJSON_ASSERT( this->v <= this->end() && x.v <= this->end() ); // Should not delete from array during iteration
		return this->v != x.v;
	}
	void Next() {
		auto e = this->end();
		while ( this->v < e && this->v->Type() != TypeTraits<T>::kType )
			++this->v; 
	}
	V end() const { return this->arr.data() + this->arr.size(); }
};
template<typename T, typename V, typename A, typename I> struct ArrayRange
{
	A arr;
	I begin() const { return I{arr.data(), arr}; }
	I end() const { return I{arr.data()+arr.size(), arr}; }
};

template <typename T> ConstArrayRange<T> Array::Iter() const { return ConstArrayRange<T>{this->_array}; }
template <typename T> MutableArrayRange<T> Array::Iter() { return MutableArrayRange<T>{this->_array}; }

} // namespace vjson

#endif // _H
