#ifndef _COMMON_DEFINES_H_
#define _COMMON_DEFINES_H_

/// Copyright(c) 2013 Frank Fang
///
/// Common definitions that will be used in this project
///
/// @author Frank Fang (fanghm@gmail.com)
/// @date   2013/07/06


#include <string>
using namespace std;

/// @beief Struct for variable declaration
///
/// A variable declaration may contain 4 parts
/// (take this statement as example: char* argv[2]):
///    - data_type:     char
///    - var_name:      argv
///    - array_size:    2
///    - is_pointer:    true
/// @note Only one-demension array is supported here, but it's easy to extend with this awareness
///
typedef struct {
    string  data_type;    ///< name of a data type, either basic type or user-defined type
    string  var_name;     ///< variable name
    size_t  array_size;   ///< array size: 0 for non-array
    bool    is_pointer;   ///< true when it's a pointer
    size_t  var_size;     ///< size in bytes
} VariableDeclaration;

/// @enum type for token types
enum TokenTypes {
    kUnresolvedToken,

    // keywords
    kStructKeyword,
    kUnionKeyword,
    kEnumKeyword,
    kTypedefKeyword,
    
    kBasicDataType,
    kAbstractType,
    kComplexType,
    kQualifier,

    // user-defined tokens
    kStructName,
    kUnionName,
    kEnumName,

};

/// @enum type for sigle character tokens
enum SingleToken {
    kBlockStart = '{',
    kBlockEnd   = '}',
    kPoundSign  = '#',

    kComma      = ',',
    kSemicolon  = ';',
    kEqual      = '=',
    kSlash      = '/',
    kAsterisk   = '*',
    kQuotation  = '\"',
};

#endif  // _COMMON_DEFINES_H_