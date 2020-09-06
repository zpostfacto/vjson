/////////////////////////////////////////////////////////////////////////////
//
// Yes, it's yet another C++ JSON parser/printer and DOM.  Please see the
// README for some of the goals of this code, and why we weren't happy with
// the other parsers we found and decided to reinvent this wheel.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef VJSON_H_INCLUDED
#define VJSON_H_INCLUDED

#include <string.h>
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
	kNotArray, // You asked for an array element, from an object that is not an array
	kBadIndex, // Index was our of range
	kNotObject, // You asked for an object element, from an object that is not an object
	kBadKey, // Key not found in object
};

// Sometimes we want to get a bool value, but we want to be tolerant of
// some common ways that true or false might be encoded.  These values are
// considered "truish":
// - literal true
// - nonzero number
// - string containing "true" (case insensitive) or "1"
//
// These values are considered "falsish":
// - literal false or null
// - 0 number
// - empty string
// - string containing "false" (case insensitive) or "0"
//
// Everything else is "jibberish".  Come on, go with it.
//
// Note that in practice, you can compare the value against zero, so you
// need not type these ridiculous names.
enum ETruthy
{
	kJibberish = -1,
	kFalsish = 0,
	kTruish = 1
};

// Internal stuff.  Nothing to see here, move along
class Value; class Object; class Array;
struct PrintOptions; struct ParseContext;
using RawObject = std::map<std::string, Value>; // Internal storage for for objects.
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
template<> struct TypeTraits<ETruthy> { using AsReturnType = ETruthy; using AsReturnTypeConst = ETruthy; };
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

// A JSON value, either a primitive type (null, string, bool, or number) or
// an aggregate type (object or array).  You will use this class when the
// value is of unknown type, of you want to change the type dynamically.
//
// Note that this is the only "real" class.  The Array and Object classes
// are derived from Value, but do not add any data members.  They just
// expose a specialized idiomatic interface, and are completely optional.
// You can access the children of arrays and objects through this interface.
class Value
{
public:

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

	// Construct Array from initializer list.
	// E.g. Value( { "one", 5.0, false } )
	Value( std::initializer_list<Value> x );

	// FIXME - Would be good to handle a way to have map style "comprehension" and easily
	// construct maps.  Somehow need to make sure { "key", value } is not confused with
	// an array of two elements.

	// Construct from internal object/array storage
	Value( const RawObject & x );
	Value( RawObject &&      x );
	Value( const RawArray &  x );
	Value( RawArray &&       x );

	// Return the type of thing we are
	EValueType Type() const { return _type; }

	// Return true if we are the specified type
	bool IsNull() const { return _type == kNull; }
	bool IsObject() const { return _type == kObject; }
	bool IsArray() const { return _type == kArray; }
	bool IsString() const { return _type == kString; }
	bool IsNumber() const { return _type == kDouble; }
	bool IsDouble() const { return _type == kDouble; }
	bool IsBool() const { return _type == kBool; }

	// Return/cast this value as the specified type.  These will asserts/crash if called on the wrong type!
	// To handle wrong types, use these functions
	// - AsXxxx function below that accept an extra parameter to specify a default return value.
	// - Get()
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

	// Get a bool value, but allow a few other reasonable things instead of strict bool "true" and "false"
	// literals.  See ETruthy for exactly what counts.
	ETruthy AsTruthy() const;
	bool IsTruish() const { return AsTruthy() == kTruish; }
	bool IsFalsish() const  { return AsTruthy() == kFalsish; }

	// Get this value as the specified type.  If wrong type, returns the default you specify
	const char *AsCString( const char *       defaultVal ) const { return _type == kString ? _string.c_str() : defaultVal; }
	std::string AsString(  const char *       defaultVal ) const { return _type == kString ? _string : std::string( defaultVal ); } // NOTE: always returns a copy
	std::string AsString(  const std::string &defaultVal ) const { return _type == kString ? _string : defaultVal; } // NOTE: always returns a copy
	bool        AsBool(    bool               defaultVal ) const { return _type == kBool ? _bool : defaultVal; } // NOTE: requires exact bool type!
	double      AsDouble(  double             defaultVal ) const { return _type == kDouble ? _double : defaultVal; }
	int         AsInt(     int                defaultVal ) const { return _type == kDouble ? (int)_double : defaultVal; }

	// Cast and return reference, returning an empty object/array if we are not the right type
	const Object &AsObjectOrEmpty() const { return _type == kObject ? *(const Object *)this : EmptyObject(); }
	const Array  &AsArrayOrEmpty()  const { return _type == kArray ? *(const Array *)this : EmptyArray(); }

	// Cast and return pointer, returning nullptr if we are not the right type.
	// Similar to dynamic_cast<> (which cannot be used here because we don't have a vtable)
	const Object *AsObjectPtr() const { return _type == kObject ? (const Object *)this : nullptr; }
	Object *      AsObjectPtr()       { return _type == kObject ? (Object *)this : nullptr; }
	const Array * AsArrayPtr()  const { return _type == kArray ? (const Array *)this : nullptr; }
	Array *       AsArrayPtr()        { return _type == kArray ? (Array *)this : nullptr; }

	// Get the value as the specified type into your result value.  Returns kOK or kWrongType
	EResult Get( const char *  &outX ) const { if ( _type != kString ) return kWrongType; outX = _string.c_str(); return kOK; }
	EResult Get( std::string   &outX ) const { if ( _type != kString ) return kWrongType; outX = _string; return kOK; }
	EResult Get( bool          &outX ) const { if ( _type != kBool ) return kWrongType; outX = _bool; return kOK; }
	EResult Get( ETruthy       &outX ) const { outX = AsTruthy(); return kOK; }
	EResult Get( double        &outX ) const { if ( _type != kDouble ) return kWrongType; outX = _double; return kOK; }
	EResult Get( int           &outX ) const { if ( _type != kDouble ) return kWrongType; outX = (int)_double; return kOK; }
	EResult Get( const Object *&outX ) const { if ( _type != kObject ) return kWrongType; outX = (const Object*)this; return kOK; }
	EResult Get( Object*       &outX )       { if ( _type != kObject ) return kWrongType; outX = (Object*)this; return kOK; }
	EResult Get( const Array * &outX ) const { if ( _type != kArray ) return kWrongType; outX = (const Array*)this; return kOK; }
	EResult Get( Array *       &outX )       { if ( _type != kArray ) return kWrongType; outX = (Array*)this; return kOK; }

	// Get the "truthiness" of this value.  Returns kWrongType if the value is jibberish (neither trueish or falseish)
	EResult GetTruthy( bool &outX) const;

	// Template-style access.  Examples:
	//
	//   Value val;
	//   if ( val.Is<std::string>() ) {}
	//   if ( val.Is<nullptr_t>() ) {}
	//   int x = val.As<int>()
	//   printf( "Hello, %s\n, val.As<const char *>() );
	//
	// Some people prefer this coding style in leaf code, instead of the corresponding
	// IsXxx() and AsXxx(), and we're OK with that.  Their real utility comes when
	// called from template code.
	//
	// (See full list of specializations below.)
	template<typename T> bool Is() const;
	template<typename T> typename TypeTraits<T>::AsReturnTypeConst As() const;
	template<typename T> typename TypeTraits<T>::AsReturnType As();

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

	// Assign array from list of T's, where T is anything we can construct a Value from
	// See also class Array constructors
	template <typename T> void SetArray( const T *begin, const T *end );
	template <typename T> void SetArray( size_t n, const T *begin ) { SetArray( begin, begin+n ); }
	template <typename T> void SetArray( std::initializer_list<T> x ) { SetArray( x.begin(), x.end() ); }; // E.g. SetArray( { "one", "two", "three" } );   Note that they must all be the same type.  (If not, wrap all with Value constructors.)

	//
	// Array access
	//
	// This base Value class can do a bunch of things on arrays.  Most functions
	// do checking and will return a sensible "failure" result if called on
	// non-array or if index is out of bounds.  (AtIndex() is the only
	// exception.)  The derived Array class is available, and has
	// range-based for and operator[], for a more idiomatic access.
	//

	// Get the length of the array as an int or size_t, according to your
	// preference pedantic type bullcrap.  Returns 0 if we are not an array.
	int ArrayLen() const { return _type == kArray ? (int)_array.size() : 0; }
	size_t ArraySize() const { return _type == kArray ? _array.size() : 0; }

	// Get item from array, by index.  No checking is performed!  If this is not an array,
	// or index is out of bounds, this will assert, and you are in bad shape.
	const Value &AtIndex( size_t idx ) const { VJSON_ASSERT( _type == kArray && idx < _array.size() ); return _array[idx]; }
	Value &      AtIndex( size_t idx )       { VJSON_ASSERT( _type == kArray && idx < _array.size() ); return _array[idx]; }

	// Same as AtIndex, but if you call this on something that isn't an array,
	// or the index is invalid, it returns nullptr.
	// NOTE: Compare to Array::operator[]
	const Value *ValueAtIndex( size_t idx ) const { return ( _type == kArray && idx < _array.size() ) ? &_array[idx] : nullptr; }
	Value *      ValueAtIndex( size_t idx )       { return ( _type == kArray && idx < _array.size() ) ? &_array[idx] : nullptr; }

	// Get the value at the specified index as the specified type.  If this is not an array,
	// or the index is invalid, or the item is the wrong type, returns your default value
	const char *  CStringAtIndex  ( size_t idx, const char *       defaultVal ) const { const Value *t = InternalAtIndex( idx, kString ); return t ? t->_string.c_str() : defaultVal; }
	std::string   StringAtIndex   ( size_t idx, const char *       defaultVal ) const { const Value *t = InternalAtIndex( idx, kString ); return t ? t->_string : std::string( defaultVal ); } // NOTE: always returns a copy
	std::string   StringAtIndex   ( size_t idx, const std::string &defaultVal ) const { const Value *t = InternalAtIndex( idx, kString ); return t ? t->_string : defaultVal; } // NOTE: always returns a copy
	bool          BoolAtIndex     ( size_t idx, bool               defaultVal ) const { const Value *t = InternalAtIndex( idx, kBool ); return t ? t->_bool : defaultVal; }
	ETruthy       TruthyAtIndex   ( size_t idx                                ) const { const Value *t = ValueAtIndex( idx ); return t ? t->AsTruthy() : kJibberish; } // Returns kJibberish if we're not an array, bad index, or value at index cannot be classified
	bool          TruthyAtIndex   ( size_t idx, bool               defaultVal ) const { ETruthy t = TruthyAtIndex( idx ); return t != kJibberish ? (bool)t : defaultVal; } // Always returns true/false.  Uses your default if we cannot make sense of things
	double        DoubleAtIndex   ( size_t idx, double             defaultVal ) const { const Value *t = InternalAtIndex( idx, kDouble ); return t ? t->_bool : defaultVal; }
	int           IntAtIndex      ( size_t idx, int                defaultVal ) const { const Value *t = InternalAtIndex( idx, kDouble ); return t ? (int)t->_double : defaultVal; }
	const Object *ObjectPtrAtIndex( size_t idx                                ) const { return (const Object *)InternalAtIndex( idx, kObject ); }
	Object *      ObjectPtrAtIndex( size_t idx                                )       { return (Object *)InternalAtIndex( idx, kObject ); }
	const Array * ArrayPtrAtIndex ( size_t idx                                ) const { return (const Array *)InternalAtIndex( idx, kArray ); }
	Array *       ArrayPtrAtIndex ( size_t idx                                )       { return (Array *)InternalAtIndex( idx, kArray ); }

	// Get the item at the specified index as an array/object.  If this is not an array, or the
	// index is invalid or the wrong type, returns reference to empty array/object
	const Array & ArrayAtIndexOrEmpty( size_t idx ) const { const Array *t = (const Array *)InternalAtIndex( idx, kArray ); return t ? *t : EmptyArray(); }
	const Object &ObjectAtIndexOrEmpty( size_t idx ) const { const Object *t = (const Object *)InternalAtIndex( idx, kObject ); return t ? *t : EmptyObject(); }

	// Get the value as the specified type into your result value.
	// Any type for which Get( T & ) is defined will work.
	template <typename T> EResult GetAtIndex( size_t idx, T &outX ) const;

	// Access specific index as generic Value.
	// Note also the GetAtIndex<const Value*> specialization below
	EResult GetAtIndex( size_t idx, Value *&outX );

	// Return a static copy of an empty Array.  Basically same as Value{kArray}
	static const Array &EmptyArray();

	//
	// Object access
	//
	// This base Value class can do a bunch of things on objects.  All functions
	// do checking and will return a sensible "failure" result if called on
	// non-object or if key is not found.  The derived Object class is available,
	// and has range-based for and operator[], for a more idiomatic access.
	//
	// Note: You may pass the key as const char *, or std::string.  This is why
	// many of the functions that accept keys take the key as a template type K.

	// Return number of key/values pairs in object as int or size_t, according to your
	// preference pedantic type bullcrap.  Returns 0 if we are not an object.
	int ObjectLen() const { return _type == kObject ? (int)_object.size() : 0; }
	size_t ObjectSize() const { return _type == kObject ? _object.size() : 0; }

	// Return true if this is an object, and the key is present
	bool HasKey( const std::string &key ) const { return ValueAtKey( key ) != nullptr; }
	bool HasKey( const char        *key ) const { return ValueAtKey( key ) != nullptr; }

	// Locate the value with the specified key, but if you call this on something
	// that isn't an object, or the key is not found, it returns nullptr
	Value *      ValueAtKey( const std::string &key );
	const Value *ValueAtKey( const std::string &key ) const { return const_cast<Value*>(this)->ValueAtKey( key ); }
	Value *      ValueAtKey( const char *       key )       { return ValueAtKey( std::string( key ) ); }
	const Value *ValueAtKey( const char *       key ) const { return const_cast<Value*>(this)->ValueAtKey( std::string( key ) ); }

	// Return reference to the item at the specified key.  If this is not an object,
	// or the key is not found, returns a reference to a null object.  (Note this error
	// handling is different from AtIndex for arrays!)  Also, note that there is no
	// non-const version of this function.  You will need to use ValueAtKey() (and check
	// for null) for this purpose.  Or, use SetKey(), or Object::operator[]
	template <typename K> const Value &AtKey( K key ) const;

	// Get the value at the specified key as the specified type.  If this is not an object,
	// or the key is not found, or the item is the wrong type, returns your default value
	template <typename K> const char *  CStringAtKey  ( K&& key, const char *defaultVal        ) const { const Value *t = InternalAtKey( key, kString ); return t ? t->_string.c_str() : defaultVal; }
	template <typename K> std::string   StringAtKey   ( K&& key, const char *defaultVal        ) const { const Value *t = InternalAtKey( key, kString ); return t ? t->_string : std::string( defaultVal ); } // NOTE: always returns a copy
	template <typename K> std::string   StringAtKey   ( K&& key, const std::string &defaultVal ) const { const Value *t = InternalAtKey( key, kString ); return t ? t->_string : defaultVal; } // NOTE: always returns a copy
	template <typename K> bool          BoolAtKey     ( K&& key, bool               defaultVal ) const { const Value *t = InternalAtKey( key, kBool ); return t ? t->_bool : defaultVal; }
	template <typename K> ETruthy       TruthyAtKey   ( K&& key                                ) const { const Value *t = InternalAtKey( key ); return t ? t->AsTruthy() : kJibberish; } // Returns kJibberish if we're not an object, bad key, or value at key cannot be classified
	template <typename K> bool          TruthyAtKey   ( K&& key, bool               defaultVal ) const { ETruthy t = TruthyAtKey( key );  return t != kJibberish ? (bool)t : defaultVal; } // Always returns true/false.  Uses your default if we cannot make sense of things
	template <typename K> double        DoubleAtKey   ( K&& key, double             defaultVal ) const { const Value *t = InternalAtKey( key, kDouble ); return t ? t->_bool : defaultVal; }
	template <typename K> int           IntAtKey      ( K&& key, int                defaultVal ) const { const Value *t = InternalAtKey( key, kDouble ); return t ? (int)t->_double : defaultVal; }
	template <typename K> const Object *ObjectPtrAtKey( K&& key                                ) const { (const Object *)InternalAtKey( key, kObject ); }
	template <typename K> Object *      ObjectPtrAtKey( K&& key                                )       { return (Object *)InternalAtKey( key, kObject ); }
	template <typename K> const Array * ArrayPtrAtKey ( K&& key                                ) const { return (const Array *)InternalAtKey( key, kArray ); }
	template <typename K> Array *       ArrayPtrAtKey ( K&& key                                )       { return (const Array *)InternalAtKey( key, kArray ); }

	// Get the item at the specified key as an array/object.  If this is not an object, or the
	// key is not found, or the item is the wrong type, returns reference to empty array/object
	template <typename K> const Array & ArrayAtKeyOrEmpty( K&& key ) const { const Array *t = (const Array *)InternalAtKey( key, kArray ); return t ? *t : EmptyArray(); }
	template <typename K> const Object &ObjectAtKeyOrEmpty( K&& key ) const { const Object *t = (const Object *)InternalAtKey( key, kObject ); return t ? *t : EmptyObject(); }

	// Get the value as the specified type into your result value.
	// Any type for which Get( T & ) is defined will work.
	template <typename T, typename K> EResult GetAtKey( K&& key, T &outX ) const;

	// Access specific index as generic Value.  Similar to ValueAtIndex
	template <typename K> EResult GetAtKey( K&& key, const Value *&outX ) const;
	template <typename K> EResult GetAtKey( K&& key, Value *&outX );

	// Set the value at the given key.  If the key is not present, it is added.
	// If the key is present, the value is changed.  Returns false if this
	// is not an object.  See also Object::operator[].  T can be anything
	// that a Value can be constructed and assigned from.
	template <typename T, typename K> bool SetAtKey( K&& key, T&& value );

	// Delete the specified key.  Returns kOK, kNotObject, or kBadKey
	template <typename K> EResult EraseKey( K&& key );

	// Return a static copy of an empty Object.  Basically same as Value{kObject}
	static const Object &EmptyObject();

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

// An Object is a Value that is known to be of type kObject.  Since
// it is known to be an object, we can provide a more idiomatic object
// interface, and we can optimize a few function calls.
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
	// though a const reference.
	Value &operator[]( const std::string &key ) { VJSON_ASSERT( _type == kObject ); return _object[ key ]; }
	Value &operator[]( const char *key ) { VJSON_ASSERT( _type == kObject ); return _object[ std::string(key) ];  }

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

	// TODO - add type-specific iterators
};

// An Array is a Value that is known to be of type kArray.  Since
// it is known to be an array, we can provide a more idiomatic array
// interface, and we can optimize a few function calls.
class Array : public Value
{
public:
	Array() : Value( kArray ) {}
	Array( const Array &x ) : Value( x ) {}
	Array( Array &&x ) : Value( std::forward<Array>(x) ) {}
	Array( const RawArray &x ) : Value( x ) {}
	Array( RawArray && x ) : Value( std::forward<RawArray>( x ) ) {}

	// Override ArrayLen(), we know we are an array.  Also provide shorter versions
	int ArrayLen() const { VJSON_ASSERT( _type == kArray ); return (int)_array.size(); }
	size_t ArraySize() const { VJSON_ASSERT( _type == kArray ); return _array.size(); }
	int Len() const { VJSON_ASSERT( _type == kArray ); return (int)_array.size(); }
	size_t size() const { VJSON_ASSERT( _type == kArray ); return _array.size(); } // not capitalized because we want to be as similar to std::vector as possible

	// Standard array access notation Operator[]
	Value &operator[]( size_t idx ) { return AtIndex(idx); }
	const Value &operator[]( size_t idx ) const { return AtIndex(idx); }

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

// Options for parsing a document
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

// Parse any legitimate JSON entity.
bool ParseValue( Value &out, const char *begin, const char *end, ParseContext &ctx );

// Parse, but fail if the input is not a single JSON object or array
bool ParseObject( Object &out, const char *begin, const char *end, ParseContext &ctx );
bool ParseArray( Array &out, const char *begin, const char *end, ParseContext &ctx );

// Parse from std::string
inline bool ParseValue ( Value  &out, const std::string &s, ParseContext &ctx ) { return ParseValue ( out, s.c_str(), s.c_str() + s.length(), ctx ); }
inline bool ParseObject( Object &out, const std::string &s, ParseContext &ctx ) { return ParseObject( out, s.c_str(), s.c_str() + s.length(), ctx ); }
inline bool ParseArray ( Array  &out, const std::string &s, ParseContext &ctx ) { return ParseArray ( out, s.c_str(), s.c_str() + s.length(), ctx ); }

// Parse from '\0'-terminated C string
inline bool ParseValue ( Value  &out, const char *c_str, ParseContext &ctx ) { return ParseValue ( out, c_str, c_str + strlen(c_str), ctx ); }
inline bool ParseObject( Object &out, const char *c_str, ParseContext &ctx ) { return ParseObject( out, c_str, c_str + strlen(c_str), ctx ); }
inline bool ParseArray ( Array  &out, const char *c_str, ParseContext &ctx ) { return ParseArray ( out, c_str, c_str + strlen(c_str), ctx ); }


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
template<> inline const std::string &Value::As<std::string >() const { VJSON_ASSERT( _type == kString ); return _string; }
template<> inline std::string &      Value::As<std::string >()       { VJSON_ASSERT( _type == kString ); return _string; }
template<> inline const bool &       Value::As<bool        >() const { VJSON_ASSERT( _type == kBool ); return _bool; } // NOTE: requires exact bool type!
template<> inline bool &             Value::As<bool        >()       { VJSON_ASSERT( _type == kBool ); return _bool; } // NOTE: requires exact bool type!
template<> inline ETruthy            Value::As<ETruthy     >()       { return AsTruthy(); }
template<> inline const double &     Value::As<double      >() const { VJSON_ASSERT( _type == kDouble ); return _double; }
template<> inline double &           Value::As<double      >()       { VJSON_ASSERT( _type == kDouble ); return _double; }
template<> inline int                Value::As<int         >() const { VJSON_ASSERT( _type == kDouble ); return (int)_double; }
template<> inline Object &           Value::As<Object      >()       { VJSON_ASSERT( _type == kObject ); return *(Object*)this; }
template<> inline const Object &     Value::As<Object      >() const { VJSON_ASSERT( _type == kObject ); return *(const Object*)this; }
template<> inline Array &            Value::As<Array       >()       { VJSON_ASSERT( _type == kArray ); return *(Array*)this; }
template<> inline const Array &      Value::As<Array       >() const { VJSON_ASSERT( _type == kArray ); return *(const Array*)(this); }


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
EResult Value::EraseKey( K&& key )
{
	if ( _type != kObject ) return kNotObject;
	if ( _object.erase( std::forward<K>( key ) ) == 0 ) return kBadKey;
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

#endif
