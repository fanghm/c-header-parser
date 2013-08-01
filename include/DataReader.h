#ifndef _TYPE_DATA_READER_
#define _TYPE_DATA_READER_

#include "TypeParser.h"
#include <string>

using namespace std;

/// Copyright(c) 2013 Frank Fang
///
/// Binary memory data reader for C types
///
/// Based on the type definitions from TypeParser, 
/// this class can correctly read binary memory data for any known C types,
/// and print out in readable text field by field
///
/// @author Frank Fang (fanghm@gmail.com)
/// @date   2013/07/06
///
class DataReader
{
public:
    /// memory data comes from buffer
    DataReader(const TypeParser& parser, char* buffer, size_t size);

    /// memory data comes from binary file
    DataReader(const TypeParser& parser, const string &data_file);

    ~DataReader(void);

    /// print the type fields and their values in a nicely fomatted way
    void PrintTypeData(const string &type_name, bool is_union = false);
    
private:
	void PrepareTypeData(const string &type_name, size_t indent, bool is_union);

    void PrintMemberData(list<VariableDeclaration>& members, size_t indent, bool is_union);
    void PrintVarData(const VariableDeclaration &def, size_t indent, bool is_union);
    void PrintVarValue(const VariableDeclaration &var_def, size_t indent, bool is_union);

    /// read data from binary data file
    void ReadData(const string &data_file);

	/// read data from a array, mainly for test purpose
    //void SetData(char* data, size_t size);
    //char* getData() { return data_buffer_; }

private:
    TypeParser		type_parser_;

    char*			data_buffer_;   ///< buffer to hold the content of the binary memory dump file
    size_t			data_size_;		///< total size of @var data_buffer

    char*			data_ptr_;      ///< the position where the @var data_buffer is read to
	ostringstream	out_stream_;	///< output stream
};

#endif  // _TYPE_DATA_READER_