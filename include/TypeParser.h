#ifndef _TYPE_PARSER_H_
#define _TYPE_PARSER_H_

/// Copyright(c) 2013 Frank Fang
///
/// This class can parse the type definitions within C header files
///
/// It can either parse a specified file, or search and parse all files under specifed include paths
///
/// It's not a full-functional C header file parser
///     - pre-processing directives: header file inclusion and simple define
///     - struct/union/enum definition of all valid formats, including nested struct/union
///     - global variable definition
///     - one demension array
///
/// @author Frank Fang (fanghm@gmail.com)
/// @date   2013/07/06

#include <string>
#include <utility>  // pair
#include <vector>
#include <list>
#include <map>
#include <set>

#include "defines.h"


class TypeParser
{
/// add this friend class so that the type definitions can be used by it to parse the log data
friend class DataReader;

public:
    TypeParser(void);
    ~TypeParser(void);
    
    void ParseFiles();
    void ParseFile(const string &file);
    void ParseSource(const string &src);

    void SetIncludePaths(set <string> paths);



    string MergeAllLines(const list<string> &lines) const;
    bool GetNextToken(string src, size_t &pos, string &token, bool cross_line = true) const;
    bool GetNextLine(string src, size_t &pos, string &line) const;
    bool GetRestLine(const string &src, size_t &pos, string &line) const;
    void SkipCurrentLine(const string &src, size_t &pos, string &line) const;
    size_t SplitLineIntoTokens(string line, vector<string> &tokens) const;
    
    bool ParseDeclaration(const string &line, VariableDeclaration &decl) const;
    bool ParseEnumDeclaration(const string &line, int &last_value, pair<string, int> &decl, bool &is_last_member) const;
    bool ParseAssignExpression(const string &line);

    void ParsePreProcDirective(const string &src, size_t &pos);
    bool ParseStructUnion(const bool is_struct, const bool is_typedef, const string &src, size_t &pos, VariableDeclaration &decl, bool &is_decl);
    bool ParseEnum(const bool is_typedef, const string &src, size_t &pos, VariableDeclaration &var_decl, bool &is_decl);

    VariableDeclaration MakePadField(const size_t size) const;
    size_t PadStructMembers(list<VariableDeclaration> &members);
    size_t CalcUnionSize(const list<VariableDeclaration> &members) const;

    void StoreStructUnionDef(const bool is_struct, const string &type_name, list<VariableDeclaration> &members);
private:
    /// read in basic data such as keywords/qualifiers, and basic data type sizes
    void Initialize();

    void FindHeaderFiles(string path);
    string GetFile(string& filename) const;

    // pre-processing
    string Preprocess(ifstream& ifs) const;
    void StripComments(list<string>& lines) const;
    void TrimLines(list<string>& lines) const;
    void WrapLines(list<string>& lines) const;

    // file parsing
    void ParseLines(list<string> lines);
    void ParseToken(string& line, size_t start, size_t end);
    bool ParseIdentifier(string& token, VariableDeclaration& def) const;

    // utility functions
    string GetNextToken(const string line, size_t& i) const;//TODO: , string ignore=" \t"
    bool IsIgnorable(string token) const;
    TokenTypes GetTokenType(const string &token) const;
    bool IsNumericToken(const string &token, long& number) const;
    int  GetTypeSize(const string &data_type) const;
    void DumpTypeDefs() const;

/// class members
public:
    static const char   EOL = '$';          ///< end of line, used to delimit the source lines within a string
    static const size_t kAlignment_ = 4;    ///< alignment @toto: make this changeable
    static const size_t kWordSize_ = 4;     ///< size of a machine word on a 32-bit system

    static const string kAnonymousTypePrefix;
    static const string kTokenDelimiters;
    static const string kPaddingFieldName;
    
private:
    /// external input
    set <string> include_paths_;    

    /// basic data that're needed in parsing
    set <string> basic_types_;
    set <string> qualifiers_;
    map <string, TokenTypes> keywords_;
    
    /// header files to parse
    /// key     - filename with relative/absolute path
    /// bool    - whether the file is parsed
    map <string, bool> header_files_;
    
    /// Size of C data types and also user-defined struct/union types
    /// @note All enum types have fixed size, so they're not stored
    map <string, size_t> type_sizes_;
    
    /// Parsing result - extracted type definitions
    /// for below 3 maps:
    /// key     - type name
    /// value   - type members 
    
    /// struct definitons
    map <string, list<VariableDeclaration> > struct_defs_;

    /// union definitions
    map <string, list<VariableDeclaration> > union_defs_;

    /// enum definitions
    map <string, list< pair<string, int> > > enum_defs_;

    /// constants and macros that have integer values
    /// key     - constant/macro name
    /// value   - a integer (all types of number are cast to long type for convenience)
    map <string, long> const_defs_;
    
};

#endif  // _TYPE_PARSER_H_