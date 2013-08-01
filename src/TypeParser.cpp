/// Copyright(c) 2013 Frank Fang
///
/// Parse C header files to extract type definitions
///
/// @author Frank Fang (fanghm@gmail.com)
/// @date   2013/07/06

#ifdef WIN32
#include "dirent.h"     // opendir/readdir, @see http://www.softagalleria.net/download/dirent/
#else
#include <sys/stat.h>	// S_ISDIR
#include <sys/types.h>	// opendir/readdir
#include <dirent.h>		// DIR
#include <math.h>		// ceil
#endif

#include <fstream>
#include <iostream>
#include <assert.h>

#include "utility.h"
#include "TypeParser.h"


/// underline ('_') shouldn't be included as it can be part of an identifier
const string TypeParser::kTokenDelimiters = " \t#{[(<&|*>)]}?\':\",%!=/;+*$";

/// prefix that is used to make a fake identifier for anonymous struct/union/enum type
const string TypeParser::kAnonymousTypePrefix = "_ANONYMOUS_";

/// name of padding field into a struct/union, for the purpose of alignment
const string TypeParser::kPaddingFieldName = "_padding_field_";


TypeParser::~TypeParser(void) {
}

TypeParser::TypeParser(void) {
    Initialize();
}

void TypeParser::Initialize() {
    // basic data types
    const string data_types[] = {
        "char", "short", "int", "size_t", "ssize_t", "long", "float", "double", "void", "bool", "__int64", 
        "__WCHAR_T_TYPE__", "__SIZE_T_TYPE__", "__PTRDIFF_T_TYPE__"
    };
        
    basic_types_ = set<string>(data_types, data_types + sizeof(data_types)/sizeof(string));

    // qualifiers to ignore in parsing
    const string qualifiers[] = {
        "static", "const", "signed", "unsigned", "far", "extern", 
        "volatile", "auto", "register", "inline", "__attribute__"
    };

    qualifiers_ = set<string>(qualifiers, qualifiers + sizeof(qualifiers)/sizeof(string));

    // keywords that we care
    keywords_["struct"]  = kStructKeyword;
    keywords_["union"]   = kUnionKeyword;
    keywords_["enum"]    = kEnumKeyword;
    keywords_["typedef"] = kTypedefKeyword;

    // sizes of basic data types on 32-bit system, in bytes
    for (set <string>::const_iterator it = basic_types_.begin(); it != basic_types_.end(); ++it) {
        type_sizes_[*it] = kWordSize_; 
    }
    
    type_sizes_["void"]      = 0;
    type_sizes_["char"]      = 1;
    type_sizes_["short"]     = 2;
    type_sizes_["bool"]      = 1;
    type_sizes_["__WCHAR_T_TYPE__"] = 1;
    
}

/// Set header file includsion path
/// @param[in]  paths   a set of header file inclusion paths, both relative/absolute paths are okay
///
void TypeParser::SetIncludePaths(const set <string> paths) {
    include_paths_ = paths;
}

/// Parse all header files under including paths
///
/// @note current folder will be added by default
///
void TypeParser::ParseFiles() {
    // TODO: add current folder by default
    // since include_paths_ is a set, it won't be added duplicately
    //include_paths_.insert(".");
    
    for (set <string>::const_iterator it = include_paths_.begin(); it != include_paths_.end(); ++it) {
        FindHeaderFiles(*it);

        for (map<string, bool>::const_iterator it = header_files_.begin(); it != header_files_.end(); ++it) {
            ParseFile(it->first);
        }
    }
}

/// Parse a header file
///
/// It can be called individually to parse a specified header file
///
/// @param[in]  file    filename with either absolute or relative path
///
void TypeParser::ParseFile(const string &file) {
    // parse a file only when it's not yet parsed
    if (header_files_.find(file) != header_files_.end() && header_files_[file]) {
        Info("File is already processed: " + file);
        return;
    }

    ifstream ifs(file.c_str(), ios::in);
    if (ifs.fail()) {
        Error("Failed to open file - " + file);
        return;
    }

    // flag to true before parsing the file so that it won't be parsed duplicately
    header_files_[file] = true;
    Debug("Parsing file - " + file);

    ParseSource(Preprocess(ifs));

    DumpTypeDefs();// TODO: remove it
}

/*
 * Recursively find all the header files under specified folder
 * and store them into header_files_ 
 * 
 * Folder name can end with either "\\" or "/", or without any
 *
 * Assumption: header files end with ".h"
 */
void TypeParser::FindHeaderFiles(string folder) {
    DIR *dir;
    struct dirent *ent;
    struct stat entrystat;

    if ((dir = opendir (folder.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            string name(ent->d_name);
            if (name.compare(".") == 0 || name.compare("..") == 0) continue;

            name = folder + "/" + name;
            if (0 == stat(name.c_str(), &entrystat)) {
                if (S_ISDIR(entrystat.st_mode)) {
                    Info("Searching folder: " + name);
                    FindHeaderFiles(name);
                } else {
                    if (name.length() > 2 && name.substr(name.length()-2, 2).compare(".h") == 0) {
                        header_files_[name] = false;    // "false" means not yet parsed
                        Debug("Found header file: " + name);
                    } else {
                        Info("Ignoring file: " + name);
                    }
                }
            } else {
                Error("failed to stat file/folder: " + name);                
            }
        }

        closedir (dir);
    } else {
        Error("failed to open folder: " + folder);
    }
}

// search a file from the include paths
// when found, return full path of the file and also update the input argument
// else return empty string - it's up to the caller to check the return value
string TypeParser::GetFile(string& filename) const {
    DIR *dir;
    struct dirent *ent;
    string path_name;

    for (set <string>::const_iterator it = include_paths_.begin(); it != include_paths_.end(); ++it) {
        string folder = *it;
        if ((dir = opendir (folder.c_str())) == NULL) continue;

        while ((ent = readdir (dir)) != NULL) {
            string name(ent->d_name);
            if (name.compare(filename) == 0) {
                path_name = folder + "/" + filename;
                break;
            }
        }

        closedir (dir);
        if (!path_name.empty()) break;
    }

    return (filename = path_name);
}

string TypeParser::Preprocess(ifstream &ifs) const {
    string line;
    list<string> lines;

    while(ifs.good()) {
        getline(ifs, line);
    
        if (!trim(line).empty()) {
            lines.push_back(line);
        }
    }

    StripComments(lines);
    WrapLines(lines);
 
    return MergeAllLines(lines);
}

/// Strip all comments from code lines
///
/// - either line comment or comment blocks will be removed
/// - all special cases (like multiple comment blocks in one line) can be handled perfectly
///     except fake comments within quotation marks (like str = "/* fake comment */ // non-comment";)
///     but this doesn't impact the parsing
///
/// @param[in,out]  lines   lines of source code, comments will be removed directly from the lines
void TypeParser::StripComments(list<string>& lines) const {
    bool is_block = false;   // whether a comment block starts
    bool is_changed = false; // whether current line is changed after removing comments

    string line;
    size_t pos = 0;

    list<string>::iterator block_start, it = lines.begin();
    while (it != lines.end()) {
        is_block = is_changed = false;

        line = *it;
        pos = 0;

        Info("parsing line: [" + line + "]");

        // search comment start
        while (string::npos != (pos = line.find(kSlash, pos))) {
            if (line.length() <= pos+1) break;    // the 1st '/' is at the end of line, so not a comment

            switch (line.at(pos+1)) {
            case kSlash:    // line comment
                line.erase(pos);
                is_changed = true;
                break;

            case kAsterisk: // comment block
                is_block = true;
                is_changed = true;

                do {
                    size_t found = line.find("*/", pos + 2);
                    if (string::npos != found) {
                        line.erase(pos, found - pos + 2);
                        is_block = false;
                    } else {
                        line.erase(pos);
                        is_block = true;
                        break;
                    }
                } while(!is_block && string::npos != (pos = line.find("/*", pos))); 
                // loop for possible multiple comment blocks on one line
                break;

            default:
                pos++;  // might be other '/' after this one
            }        
        }

        if (!is_changed) {
            ++it;
            continue;
        } else {
            if (!line.empty())
                lines.insert(it, line);

            it = lines.erase(it);
        }

        if (is_block) {
            block_start = it;
            while(it != lines.end()) {
                line = *it;
                if (string::npos != (pos = line.find("*/"))) {
                    lines.erase(block_start, ++it);

                    line.erase(0, pos+2);
                    if (!line.empty()) {
                        it = lines.insert(it, line);   // insert rest part for checking of possible followed comments
                    }

                    is_block = false;
                    break;
                }

                ++it;
            }

            if (is_block) {
                Error("Unclosed comment block exists");
            }
        }
    }
}

// Merge wrapped lines into one line 
// A line will be deemed as line wrap only when the last character is '\'
void TypeParser::WrapLines(list<string>& lines) const {
    string line;

    list<string>::iterator first, it = lines.begin();
    while (it != lines.end()) {
        first = it;

        line = *it;
        assert(!line.empty());

        while ('\\' == line[line.length()-1] && ++it != lines.end()) {
            line  = line.substr(0, line.length() - 1);
            line = rtrim(line) + (*it);
        }

        if (it == lines.end()) {
            Error("Bad syntax: wrap line at last line");
            break;
        }

        if (it != first) {
            lines.insert(first, line);    // insert merged string before "first" without invalidating the iterators
            
            ++it;    // increase so that current line can be erased from the list
            it = lines.erase(first, it);
        } else {
            ++it;
        }
    }
}

// Merge all lines into a string for easier parsing
//    with lines delimited by EOL sign
//
// NOTE: if there're characters like  ',' or kSemicolon within a line,
//    split the line into multiple lines
string TypeParser::MergeAllLines(const list<string> &lines) const {
    string src, line, part;
    size_t pos;

    list<string>::const_iterator it = lines.begin();
    while (it != lines.end()) {
        line = *it;
        assert(!line.empty());

        if ('#' == line.at(0)) {  // don't  split pre-processing line
            src += line + EOL;
        } else {
            while (string::npos !=  (pos = line.find_first_of(",;"))) {
                if (pos == line.length()-1) {
                    break;
                } else { // split line
                    part = line.substr(0, pos + 1);
                    src += trim(part) + EOL;   // trim splited lines

                    line = line.substr(++pos);
                }
            }

            src += line + EOL;
        }

        ++it;
    }

    return src;
}

// Trim trailing spaces for each line
void TypeParser::TrimLines(list<string>& lines) const {
    string line;
    list<string>::iterator it = lines.begin();

    while (it != lines.end()) {
        line = *it;
        if (rtrim(line).compare(*it) != 0) {
            lines.insert(it, line);
            it = lines.erase(it);
        } else {
            ++it;
        }
    }
}

// Get next token starting from the specified position of the line
// pos will be moved to after the token if token not at line end
//
// Assuming not all of the rest characters are blanks as all lines are trimed before
// Note: 
// Among the many punctuations that can be used in C language (like #[(<&|*>)]} ? \' : \", % != /;)
//   only '_' can be part of a token here
string TypeParser::GetNextToken(const string line, size_t& pos) const {
    if (pos >= line.length()) return "";

    // skip blanks
    while (isspace(line[pos])) pos++;
    int start = pos;
    
    char ch;
    while(pos < line.length()) {
        ch = line[pos];
        if ( isspace(ch) 
            || (ispunct(ch) && '_' != ch) ) break;

        ++pos;  // move to the next character
    }

    Debug("Next token: " + line.substr(start, pos-start));
    return (start == pos) ? "" : line.substr(start, pos-start);
}

// return true is it's an empty token or it's a qualifer that can be ignored
bool TypeParser::IsIgnorable(string token) const {
    if (token.empty()) {
        return true;
    } else {
        return (qualifiers_.end() != qualifiers_.find(token));
    }
}

/// Query token type from known keywords/qualifiers or basic/use-defined types
///
/// @param[in]  token   a token
/// @return the corresponding token type, or kUnresolvedToken if not found
///
TokenTypes TypeParser::GetTokenType(const string &token) const {
    if (keywords_.end() != keywords_.find(token)) {
        return keywords_.at(token);
    } else if (qualifiers_.end() != qualifiers_.find(token)) {        
        return kQualifier;
    } else if (basic_types_.end() != basic_types_.find(token)) {
        return kBasicDataType;
    } else if (struct_defs_.end() != struct_defs_.find(token)) {        
        return kStructName;
    } else if (union_defs_.end() != union_defs_.find(token)) {        
        return kUnionName;
    } else if (enum_defs_.end() != enum_defs_.find(token)) {        
        return kEnumName;
    } else {
        return kUnresolvedToken;
    }
}

/// Check whether the token is a number or can be translated into a number
///
/// @param[in]  token   a token
/// @param[out] number  the token's numeric value
/// @return true if 1) the token is a number, or
///                 2) the token is a macro that have a number as its value, or
///                 3) the token is a const variable that has been assigned to a number
bool TypeParser::IsNumericToken(const string &token, long& number) const {
    if (token.empty()) {
        return false;
    }

	bool ret = true;
    
    // stol not supported by gcc, re-write the code to use strtol
    number = strtol (token.c_str(), NULL, 0);
    
    if (0L == number) {	// no valid conversion could be performed
        // the token is not a number, then check whether it can be translated into a number
        if (const_defs_.find(token) != const_defs_.end()) {
            number = const_defs_.at(token);
        } else {
            Debug("Cannot parse token <" + token + "> into a number");
            ret = false;
        }
    }

    return ret;
}

/// Get size of a type (in bytes) 
///
/// @param[in]  data_type    name of a data type, including both C data types and user-defined struct/union/enum types
/// @return -1 if is data type is unknown, else return actual type length
///
/// @note Shouldn't return 0 for unknown data type since "void" type has zero length
///
int TypeParser::GetTypeSize(const string &data_type) const {
    if (type_sizes_.find(data_type) != type_sizes_.end()) {
        return type_sizes_.at(data_type);
    } else if (enum_defs_.find(data_type) != enum_defs_.end()) {
        return sizeof(int);
    } else {
        Error("Unknown data type - " + data_type);
        return -1;
    }
}

/// Dump the extracted type definitions
void TypeParser::DumpTypeDefs() const {
    VariableDeclaration var;

    // dump numeric const variables or macros
    cout << "\nconstant values:" << "\n--------------------" << endl;
    for (map <string, long>::const_iterator it = const_defs_.begin(); it != const_defs_.end(); ++it) {
        cout << "\t" << it->first << "\t = " << it->second << endl;
    }

    // dump struct definitions
    cout << "\nstruct definitions:" << "\n--------------------" << endl;
    for (map <string, list<VariableDeclaration> >::const_iterator it = struct_defs_.begin(); 
        it != struct_defs_.end(); ++it) {

        cout << "struct " << it->first << ":" << endl;
        
        list<VariableDeclaration> members = it->second;
        while (!members.empty()) {
            var = members.front();
            cout << '\t' << var.data_type;
            
            if (var.is_pointer) cout << "* ";

            cout << "\t" << var.var_name;

            if (0 < var.array_size)
                cout << "[" << var.array_size << "]";

            cout << "\t(" << var.var_size << ")" << endl;

            members.pop_front();
        }

        string type = it->first;
        cout << "\t(size = " << type_sizes_.at(type) << ")\n" << endl;
    }

    // dump union definitions
    cout << "\nunion definitions:" << "\n--------------------" << endl;
    for (map <string, list<VariableDeclaration> >::const_iterator itu = union_defs_.begin(); 
        itu != union_defs_.end(); ++itu) {

        cout << "union " << itu->first << ":" << endl;
        
        list<VariableDeclaration> members = itu->second;
        while (!members.empty()) {
            var = members.front();
            cout << '\t' << var.data_type;
            
            if (var.is_pointer) cout << "* ";

            cout << "\t" << var.var_name;

            if (0 < var.array_size)
                cout << "[" << var.array_size << "]";

            cout << "\t(" << var.var_size << ")" << endl;

            members.pop_front();
        }
        cout << "\t(size = " << type_sizes_.at(itu->first) << ")\n" << endl;
    }

    // dump enum definitions
    cout << "\nenum definitions:" << "\n--------------------" << endl;
    for(map <string, list<pair<string, int> > >::const_iterator itv= enum_defs_.begin(); 
        itv != enum_defs_.end(); ++itv) {

        cout << "enum " << itv->first << ":" << endl; 
        
        list< pair<string, int> > members = itv->second;
        while (!members.empty()) {
            pair<string, int> var = members.front();
            cout << '\t' << var.first << "(" << var.second << ")" << endl;
            members.pop_front();
        }
        cout << '\n' << endl; 
    }

}

/// Get next token - it can either be a special character, or a keyword/identifier
///
/// @param[in]      src     source code
/// @param[in,out]  pos     start position to parse the code;
///                         will be updated to the first char after the token after this method is called
/// @param[out]     token   the next token after @var pos
/// @param[in]      cross_line  true by default, false is only used for parsing pre-processing directives
/// @return         false only when file end is reached 
///
/// @note When cross_line is false, only get next token from current line where @var pos resides
/// @note Qualifiers defined in @var qualifiers_ are skipped as they don't matter
///
bool TypeParser::GetNextToken(string src, size_t &pos, string &token, bool cross_line) const {
    // cross_line=false, only check token from current line
    if (!cross_line) {
        size_t p = src.find(EOL, pos);
        if (string::npos != p) src = src.substr(0, p);
    }

    // skip leading blanks or EOL
    while ( pos < src.length()
        && (isspace(src[pos]) || EOL == src[pos]) ) pos++;
    
    if (pos >= src.length()) {
        token.clear();
        return false;
    }

    int start = pos;
    size_t ptk = src.find_first_of(kTokenDelimiters, start);
    if (string::npos == ptk) {
        token = src.substr(start);
        pos = src.length();
    } else if (start == ptk) {
        pos = ptk + 1;
        token = string(1, src[ptk]);
    } else {
        pos = ptk;
        token = src.substr(start, ptk - start);
    }

    // skip possible empty or ignorable tokens
    while(!token.empty() && IsIgnorable(token)) GetNextToken(src, pos, token);

    return (token.empty() ? false : true);
}

/// Get the next line
/// @param[in,out]  pos     start position to parse the code;
///                         will be updated to end of next line after this method is called
/// @return false only when current line is the last line
bool TypeParser::GetNextLine(string src, size_t &pos, string &line) const {
    size_t start = src.find(EOL, pos);
    if (string::npos == start) {
        pos = src.length();
        line.clear();
        return false;
    }

    ++start;
    size_t end = src.find(EOL, start);
    if (string::npos == end) {
        line = src.substr(start);
        pos = src.length();
    } else {
        assert(end != start); // assert fail only when it's an empty line
        line = src.substr(start, end - start);
        pos = end;
    }

    assert(!trim(line).empty());
    return true;
}

/// skip current line that @var pos resides
///
/// @param[in]      src     source code
/// @param[in,out]  pos     start position to parse the code;
///                         will be updated to the first char of next line after this method is called
/// store the line content into "line"
void TypeParser::SkipCurrentLine(const string &src, size_t &pos, string &line) const {
    if (pos >= src.length()) {
        Error("SkipCurrentLine() - string offset larger than its length");
        line.clear();
        return;
    }

    size_t start = src.rfind(EOL, pos);
    size_t end = src.find(EOL, pos);
    if (end == string::npos) {
        line = src.substr(start+1);    // it's ok even if start = string::npos as it equals -1
        pos = src.length();
    } else if (end == pos) {
        start = src.rfind(EOL, pos-1);
        line = src.substr(start+1, end-start-1); // it's ok even if start = string::npos
        pos++;
    } else {
        line = src.substr(start+1, end-start-1);
        pos = end + 1;
    }
}

/// Parse pre-processing directives
///
/// Only below directives with the exact formats are supported:
/// 1) #include "<header filename>"
/// 2) #define <macro name> <number>
/// For others, the whole line will be skipped
///
/// @param src  source code
/// @param pos  start position to parse the code, pointing to the next char after '#';
///             it will be updated to new position when the code is parsed
///
/// @note when a header file inclusion directive is met, the header file is parsed 
/// immediately to ensure the correct parsing sequence
///
void TypeParser::ParsePreProcDirective(const string &src, size_t &pos) {
    string token, last_token, line;
    long number;

    GetNextToken(src, pos, token);
    if (0 == token.compare("include")) {
        assert(GetNextToken(src, pos, token, false));

        // only handle header file included with ""
        if (kQuotation == token[token.length()-1]) {
            // get included header file name
            assert(GetNextToken(src, pos, token, false));

            // parse the header file immediately
            ParseFile(token);

            // ignore the other quotation marks
            SkipCurrentLine(src, pos, line);
        } else {
            // ignore angle bracket (<>)
            SkipCurrentLine(src, pos, line);
            Info("Skip header file included by <> - " + line);
        }
    } else if (0 == token.compare("define")) {
        assert(GetNextToken(src, pos, last_token, false));
                    
        if (GetNextToken(src, pos, token, false) && IsNumericToken(token, number)) {
            const_defs_.insert(make_pair(last_token, number));
        } else {
            SkipCurrentLine(src, pos, line);
            Debug("Ignore define - " + line);
        }
    } else {
        SkipCurrentLine(src, pos, line);
        Info("Skip unsupported pre-processing line - " + line);
    }
}

void TypeParser::ParseSource(const string &src) {
    size_t pos = 0;
    string token, last_token, line;
    bool is_typedef = false;
    TokenTypes type;

    VariableDeclaration decl;
    bool is_decl = false;

    string type_name;
    while (GetNextToken(src, pos, token)) {
        if (token.length() == 1) {
            switch(token[0]) {
            case kPoundSign:
                ParsePreProcDirective(src, pos);
                break;

            // below characters can be ignored silently
            case kBlockStart:
            case kBlockEnd:
            case kSemicolon:
                break;

            default:
                SkipCurrentLine(src, pos, line);
                Debug("Character '" + token + "' unexpected, ignore the line");
            }
        } else {
            type = GetTokenType(token);
            switch (type) {
            case kStructKeyword:
            case kUnionKeyword:
                assert(ParseStructUnion((kStructKeyword == type) ? true : false, 
                    is_typedef, src, pos, decl, is_decl) && !is_decl);

                // reset is_typedef
                is_typedef = false;
                break;

            case kEnumKeyword:
                if (!ParseEnum(is_typedef, src, pos, decl, is_decl) || is_decl) {
                    Error("Bad syntax for nested enum variable declaration");
                    return;
                }
                
                // reset is_typedef
                is_typedef = false;
                break;

            case kTypedefKeyword:
                is_typedef = true;
                break;

            case kBasicDataType:
                // only (const) global variable are supported
                assert(GetRestLine(src, pos, line));
                if (!ParseAssignExpression(line)) {
                    Debug("Expression not supported - " + line);
                }
                break;

            default:
                break;
            }
        }
    }
}

/// Parse enum block
bool TypeParser::ParseEnum(const bool is_typedef, const string &src, size_t &pos, VariableDeclaration &var_decl, bool &is_decl) {
	pair<string, int> member;
	list <pair<string, int> > members;
	string line, token, next_token, type_name;

    int last_value = -1;
    bool is_last_member = false;

    assert(!src.empty() && pos < src.length());

    size_t start = pos; // store the original position for next guess

    // peek rest of current line starting from "pos"
    if (!GetRestLine(src, pos, line)) {
        assert(GetNextLine(src, pos, line));
    }
    
    // it might be just a simple enum variable declaration like: enum Home home;
	if (ParseDeclaration(line, var_decl)) {
		return (is_decl = true);
	}
	
    // else, next token should be either '{' or a typename followed by '{'
    pos = start;    // reset the position
	assert(GetNextToken(src, pos, token));
	if ('{' != token.at(0)) {
		type_name = token;
		assert(GetNextToken(src, pos, token) && kBlockStart == token.at(0));
	}
	
	// the following part should be:
    // 1) enum member declarations within the block
    // 2) something out of the block like "} [<type alias|var>];"
	while (GetNextToken(src, pos, token)) {       
        if (kBlockEnd == token.at(0)) { // reach block end
            // process rest part after block end
            start = 1;
			assert(GetNextToken(src, pos, token));
			
            if (is_typedef) {
                // format 1
                assert(GetNextToken(src, pos, next_token)  && kSemicolon == next_token.at(0));

                is_decl = false;
                enum_defs_[token] = members;  // type alias
                type_sizes_[token] = sizeof(int);   // sizeof a enum variable = sizeof(int)

                if (!type_name.empty() && token.compare(type_name) != 0) {
                    enum_defs_[type_name] = members;  // type name
                    type_sizes_[type_name] = sizeof(int);
                }
            } else {    // non-typedef
			    if (kSemicolon == token.at(0)) {
                    // format 2
                    is_decl = false;
                    assert(!type_name.empty());
                    enum_defs_[type_name] = members;
                    type_sizes_[type_name] = sizeof(int);
                } else {
                    // token must be part of a variable declaration
                    // so it must be format 3 or 4

                    if (type_name.empty()) {
                        // format 4: anonymous type
                        // generate a unique random name for this type so that it can be stored into map
                        do {
                            type_name = kAnonymousTypePrefix + rands();
                        } while (enum_defs_.end() != enum_defs_.find(type_name));
                    }
                                        
                    is_decl = true;
                    enum_defs_[type_name] = members;
                    type_sizes_[type_name] = sizeof(int);

                    if (!GetRestLine(src, pos, line)) {
                        assert(GetNextLine(src, pos, line));
                    }

                    // for easier parsing, make a declaration by adding the <type_name> before <var>
                    line  = type_name + " " + token + " " + line;
                    if (!ParseDeclaration(line, var_decl)) {
                        Error("Bad syntax for enum type of variable declaration after {} block");
                        return false;
                    }
                }
            }

            // break as block ends
            break;
		} else {    // parse enum member declarations
            if (is_last_member) {
                Error("Bad enum member declaration in last line");
                return false;
            }

            // Note: the last enum memeber can only have one token, so the rest line can be empty here!
            GetRestLine(src, pos, line);

            line = token + " " + line;
            if (!ParseEnumDeclaration(line, last_value, member, is_last_member)) {			        
			    Error("Unresolved enum member declaration syntax");
                return false;
		    } 

            Info("Add enum member: " + member.first);
            members.push_back(member);
        }
	}
	
	return true;
}
/// Parse struct/union definition or declaration
///
/// @param[in]  is_typedef  whether there's the "typedef" keyword before this statement
/// @param[in]  src         source code to parse
/// @param[in]  pos         position
/// @param[out] var_decl    will be updated only when @var is_decl is true
/// @param[out] is_decl     true if it's a variable declaration (with/without nested type definition);
///                         false if it's a pure type definition
/// return true if it's a struct/union definition or declaration
///
//     starting from the next character after the "struct" or "union" keyword
//     to end of the definition/declaration
//
/// it might be called recursively to parse nested structure of struct/union
//
/// it supports below formats:
///     1) type definition: typedef <type> [<type_name>] {....} <type_alias>;
///     2) type definition:         <type> <type_name> {....};
///     3) var declaration:         <type> <type_name> {....} <var>;
///     4) var declaration:         <type>             {....} <var>;   // anonymous type
///     5) var declaration:        [<type>] <type_name> <var>;  // type_name is defined elsewhere
///     where, <type> refers to either "struct" or "union" keyword
///            <var> here can be as complicated as "*array[MAX_SIZE]"
//
// after calling this function:
//     struct/union definitons will be stored into class member struct_defs_ or union_defs_
//     pos will point to the next kSemicolon following the block end '}',
//         or equal to src.length() when reaching file end - bad syntax
//     is_decl returns:
//     1) false for definition format 1 and 2;
//     2) true for declaration format 3-5 and "var_decl" argument being updated
///
bool TypeParser::ParseStructUnion(const bool is_struct, const bool is_typedef, const string &src, size_t &pos, VariableDeclaration &var_decl, bool &is_decl) {
	VariableDeclaration member;
	list <VariableDeclaration> members;

	string line, token, next_token;
    string type_name, type_alias;

    assert(!src.empty() && pos < src.length());

    size_t start = pos; // store the original position for next guess

    // peek rest of current line starting from "pos"
    if (!GetRestLine(src, pos, line)) {
        assert(GetNextLine(src, pos, line));
    }
    
    // it might be just a simple struct/union variable declaration as format 5
	if (ParseDeclaration(line, var_decl)) {
		return (is_decl = true);
	}
	
    // else, next token should be either '{' or a typename followed by '{'
    pos = start;    // reset the position
	assert(GetNextToken(src, pos, token));
	if ('{' != token.at(0)) {
		type_name = token;
		assert(GetNextToken(src, pos, token) && '{' == token.at(0));
	}
	
	// the following part should be:
    // 1) struct/union member declarations within the block
    // 2) something out of the block like "} [<type alias|var>];"
	while (GetNextToken(src, pos, token)) {       
        if ('}' == token.at(0)) { // reach block end
            // process rest part after block end
            start = 1;
			assert(GetNextToken(src, pos, token));
			
            if (is_typedef) {
                // format 1
                assert(GetNextToken(src, pos, next_token)  && kSemicolon == next_token.at(0));

                is_decl = false;
                type_alias = token; // token is actually type alias
                StoreStructUnionDef(is_struct, type_alias, members);
                
                // when type_name not empty and not the same as type alias, store a copy in case it's used elsewhere
                if (!type_name.empty() && type_alias.compare(type_name) != 0) {
                    if (is_struct) {
                        struct_defs_[type_name] = members;
                    } else {
                        union_defs_[type_name] = members;
                    }

                    type_sizes_[type_name] = type_sizes_[type_alias];
                }
            } else {    // non-typedef
			    if (kSemicolon == token.at(0)) {
                    // format 2
                    is_decl = false;
                    assert(!type_name.empty());
                    StoreStructUnionDef(is_struct, type_name, members);
                } else {
                    // token must be part of a variable declaration
                    // so it must be format 3 or 4
                    if (type_name.empty()) {
                        // format 4: anonymous type
                        // generate a unique random name for this type so that it can be stored into map
                        do {
                            type_name = kAnonymousTypePrefix + rands();
                        } while (struct_defs_.end() != struct_defs_.find(type_name));
                    }
                                        
                    is_decl = true;
                    StoreStructUnionDef(is_struct, type_name, members);

                    if (!GetRestLine(src, pos, line)) {
                        assert(GetNextLine(src, pos, line));
                    }

                    // for easier parsing, make a declaration by adding the <type_name> before <var>
                    line  = type_name + " " + token + " " + line;
                    if (!ParseDeclaration(line, var_decl)) {
                        Error("Bad syntax for struct/union type of variable declaration after {} block");
                        return false;
                    }
                }
            }

            // break as block ends
            break;
		} else {    // parse struct/union member declarations
            TokenTypes type = GetTokenType(token);

            if (kStructKeyword == type || kUnionKeyword == type) {
                // a nested struct/union variable declaration
                //bool is_declare;
                //VariableDeclaration var_decl;
			    if (!ParseStructUnion((kStructKeyword == type) ? true : false,
                    false, src, pos, member, is_decl)) {
                    Error("Bad syntax for nested struct/union variable declaration");
                    return false;
                }
                
                // TODO: also check position here
			    assert(is_decl);
			    members.push_back(member);
            }else if (kEnumKeyword == type) {
                if (!ParseEnum(false, src, pos, member, is_decl)) {
                    Error("Bad syntax for nested enum variable declaration");
                    return false;
                }
                
                // TODO: also check position here
			    assert(is_decl);                
			    members.push_back(member);
            } else {
                // regular struct/union member declaration, including format 5
                if (!GetRestLine(src, pos, line)) {
                    assert(GetNextLine(src, pos, line));
                }

                line = token + " " + line;
                if (!ParseDeclaration(line, member)) {			        
			        Error("Unresolved struct/union member declaration syntax");
                    return false;
		        } 

                Info("Add member: " + member.var_name);
                members.push_back(member);
		    }
        }
	}

	return true;
}

// get rest part of the line
bool TypeParser::GetRestLine(const string &src, size_t &pos, string &line) const {
    if (EOL == src[pos]) {
        line.clear();
        return false;
    }

    size_t p = src.find(EOL, pos);
    if (string::npos != p) {
        line = src.substr(pos, p-pos);
        pos = p;
    } else {
        line = src.substr(pos);
        pos = src.length();
    }

    return true;
}

/// Parsing enum member declaration
///
/// Possible formats:
///		1) Zhejiang             // only for last enum member 
///		2) Beijing,
///		3) Shenzhen = <value>   // only for last enum member 
///		4) Shanghai = <value>,
///
/// @param[in]		line		the declaration of a enum member
/// @param[in,out]	last_value	[in]the value of last enum member; [out]the value of current enum member
/// @param[out]		decl		enum member declaration
/// @param[out]		is_last_member	true for format 2 & 4, else false
bool TypeParser::ParseEnumDeclaration(const string &line, 
									  int &last_value, pair<string, int> &decl, bool &is_last_member) const {
    // whether this enum variable is the lastest member of the enum type
    is_last_member = false;
    vector<string> tokens;
    long number;

    switch (SplitLineIntoTokens(line, tokens)) {
    case 1:
        is_last_member = true;
        decl.second = ++last_value;
        break;

    case 2:
        assert(kComma == tokens[1].at(0));
        decl.second = ++last_value;
        break;

    case 3:
        assert(kEqual == tokens[1].at(0));
        
        if (!IsNumericToken(tokens[2], number)) {
            Error("Cannot convert token into a number - " + tokens[2]);
            return false;
        }

        is_last_member = true;
        decl.second = last_value = static_cast<int>(number);
        break;

    case 4:
        assert(kEqual == tokens[1].at(0) && kComma == tokens[3].at(0));
        
        if (!IsNumericToken(tokens[2], number)) {
            Error("Cannot convert token into a number - " + tokens[2]);
            return false;
        }
        
        decl.second = last_value = static_cast<int>(number);
        break;

    default:
        Error("Bad syntax for enum member declaration - " + line);
        return false;
    }

    decl.first = tokens[0];
    return true;
}

/// Parse a variable declaration
///
/// A declaration can be as complicated as: 
///     unsigned char *array[MAX_SIZE]; // qualifiers should be removed from "line" argument
///     struct <complex_type> var;      // the struct/union/enum keyword should be removed from "line" argument
/// @note code lines with multiple variables declared consecutively are ignored, like "int a, b, c = MAX;" 
///
/// @param[in]  line    a code line that ends with kSemicolon and is stripped of preceding qualifiers (like unsigned)
///                     and struct/union/enum keywords
/// @param[out] decl    the variable declaration if the line is parsed successfully
/// @return             true if the line can be parsed into a variable declaration successfully
///
/// @note type size are calculated will simple consideration of alignment
/// @note can be improved with consideration of multiple demension array
bool TypeParser::ParseDeclaration(const string &line, VariableDeclaration &decl) const {
    assert(!line.empty());
    if (line[line.length()-1] != kSemicolon) return false;

    vector<string> tokens;
    size_t size = SplitLineIntoTokens(line, tokens);
    assert(size >= 3);  // even the simplest declaration contains 3 tokens: type var ;

    size_t index = 0;
    decl.data_type = tokens[index];
    decl.is_pointer = false;

    size_t length = GetTypeSize(decl.data_type);
    if (0 == length) {
        Debug("Unknown data type - " + decl.data_type);
        return false;
    }

    if (tokens[++index].at(0) == kAsterisk) {
        decl.is_pointer = true;
        length = kWordSize_; // size of a pointer is 4 types on a 32-bit system
        decl.var_name = tokens[++index];
    } else {
        decl.var_name = tokens[index];
    }

    if (tokens[++index].at(0) == '[') {
        long number;
        if (IsNumericToken(tokens[++index], number)) {
            decl.array_size = number;
            length *= number;
        } else {
            Error("Array size cannot be parsed into a number - " + tokens[index]);
            return false;
        }
    } else {
        decl.array_size = 0;
    }

    decl.var_size = length;

    return true;
}

/// Parse assignment expression
///
/// @param[in]  line    an assignment expression with the format: var = number
/// @return             true if the line can be parsed successfully, and @var const_defs_ will be updated
///
bool TypeParser::ParseAssignExpression(const string &line) {
    vector<string> tokens;
    long number;

    // only 4 tokens for an assignment expression: var = number;
    if (4 == SplitLineIntoTokens(line, tokens) 
        && kEqual == tokens[1].at(tokens[1].length()-1) && kSemicolon == tokens[3].at(tokens[3].length()-1)
        && IsNumericToken(tokens[2], number)) {
        const_defs_.insert(make_pair(tokens[0], number));
        return true;
    }

    return false;
}

/// split a line into tokens
///
/// @note: keywords that can be ignored will be removed by GetNextToken()
size_t TypeParser::SplitLineIntoTokens(string line, vector<string> &tokens) const {
    string token;
    size_t start = 0;
    
    while (GetNextToken(line, start, token)) {
        tokens.push_back(token);
    }

    return tokens.size();
}

/// Pad a struct with padding fields for memory alignment
/// 
/// @param[in,out] members  struct members, will be inserted with padding fields when needed
/// @return					struct size after alignment
///
/// @note This method is based on kAlignment_ = 4 on 32-bit system since the padding algorithm can be very complicated
/// considering the different alignment modulus/options of different compiler/OS/CPU
/// About alignment, @see http://c-faq.com/struct/align.esr.html  
size_t TypeParser::PadStructMembers(list<VariableDeclaration> &members) {
    list<VariableDeclaration>::iterator it = members.begin();
    size_t total = 0;
    size_t size;
    size_t last_size = 0;   ///< when last_size>0, padding is needed depending on later members; else no more padding is needed later
    size_t align_size, pad_size;
    
    while (it != members.end()) {
        size = it->var_size;

        if (0 == (size % kAlignment_)) {	// current member itself is aligned
            if (last_size > 0) {    // need padding previous members to alignment
                align_size = static_cast<size_t>(ceil(last_size * 1.0 / kAlignment_) * kAlignment_);
                pad_size = align_size - last_size;
                members.insert(it, MakePadField(pad_size));

				total += align_size;
            } else {
				total += size;
			}

            last_size = 0;
            ++it;
        } else {	// current member itself cannot align
            // size can only be less than 4 (1 or 2) now unless it's an array
            if (size >= kAlignment_) {
				if (it->array_size > 0) {
					Debug("TODO: add array support in PadStructMembers()");
				} else {
					Error("Incorrect type size for " + it->var_name);
				}

                return 0;
            }

            if (0 == last_size) {// last member is aligned
                last_size = size;
				++it;
            } else if (0 == (size + last_size) % kAlignment_) {
                total += size + last_size;
                last_size = 0;
                ++it;
            } else if (1 == last_size) {
				if (1 == size) {
					last_size += 1;
					++it;
				} else if (2 == size) {
					members.insert(it, MakePadField(1));
					total += kAlignment_;
					last_size = 0;
					++it;
				} else {
					Error("Bad member size - " + size);
					return 0;
				}
			} else if (2 == last_size) {
				assert (1 == size);
                members.insert(++it, MakePadField(1));
                total += kAlignment_;
                last_size = 0;
            } else {
				Error("Bad alignment");
				return 0;
			}

        }
    }

    return total;
}

size_t TypeParser::CalcUnionSize(const list<VariableDeclaration> &members) const {
    size_t size = 0;
    for (list<VariableDeclaration>::const_iterator it = members.begin(); it != members.end(); ++ it) {
        size = max(size, it->var_size);
    }
    
    if (0 != size % kAlignment_) {
        size = static_cast<size_t> (ceil(size * 1.0 / kAlignment_) * kAlignment_);
    }

    return size;
}

VariableDeclaration TypeParser::MakePadField(const size_t size) const {
    VariableDeclaration var;

    var.var_name = kPaddingFieldName;
    var.var_size = size;
    var.data_type = "char";
    var.array_size = 0;
    var.is_pointer = false;
    
    return var;
}

/// Store the definition and size of a struct or union
///
/// For structs, the members are padded based on alignment, @see TypeParser::PadStructMembers
///
void TypeParser::StoreStructUnionDef(const bool is_struct, const string &type_name, list<VariableDeclaration> &members) {
    size_t size;

    if (is_struct) {
        size = PadStructMembers(members);
        struct_defs_[type_name] = members;  
    } else {
        size = CalcUnionSize(members);
        union_defs_[type_name] = members;
    }

    type_sizes_[type_name] = size;
}