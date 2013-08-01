/// Copyright(c) 2013 Frank Fang
///
/// Main program
///
/// It can either parse a specified file, or search and parse all files under specifed include paths
///
/// This program follows Google C++ Style, and the comments follow doxgen C++ style
/// Line size: 120
///
/// Reference:
///   Google C++ Style: http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml
///   C BNF Grammer:    http://lists.canonical.org/pipermail/kragen-hacks/1999-October/000201.html
///
/// @author Frank Fang (fanghm@gmail.com)
/// @date   2013/07/06

#ifndef WIN32
#include <unistd.h>
#endif

#include <string>
#include <iostream>
#include <set>

#include "utility.h"
#include "TypeParser.h"
#include "DataReader.h"

using namespace std;

/// Logging level
LogLevels g_log_level = kInfo;

void usage(char* prog) {
    cout << "Usage:\n\t" << prog << " -s <struct_name> -b <binary_file> -i<inclue_path> [-h]" << endl;
}

#ifndef WIN32
void ParseOptions(int argc, char **argv, string &struct_name, string &bin_file, set<string> &inc_paths) {
    char c;
    while ((c = getopt (argc, argv, "s:b:i:h")) != -1) {
        switch (c) {
        case 's':
            struct_name = string(optarg);
            break;

        case 'b':
            bin_file = string(optarg);
            break;

        case 'h':
        case 'i':
            inc_paths.insert(string(optarg));
            break;

        default:
            usage(argv[0]);
        }
    }

    if (struct_name.empty() || bin_file.empty() || inc_paths.empty()) {
        usage(argv[0]);
        return;
    }

    Info("Struct: " + struct_name);
    Info("Binary: " + bin_file);
   
    for(set<string>::iterator it = inc_paths.begin(); it != inc_paths.end(); ++it) {
            Info("Include path: " + *it);
    }
}
#else
void ParseOptions(int argc, char **argv, string &struct_name, string &bin_file, set<string> &inc_paths) {
    struct_name = "Employee";
    bin_file    = "../test/Employee.bin";
    inc_paths.insert("../test");
}
#endif

int main(int argc, char **argv) {
	string struct_name, bin_file;
    set<string> inc_paths;
    
    ParseOptions(argc, argv, struct_name, bin_file, inc_paths);
    
    TypeParser parser;
    parser.SetIncludePaths(inc_paths);
    parser.ParseFiles();
    
    //DataReader reader(parser, bin_file);
    //reader.PrintTypeData(struct_name, false/* struct */);
	
    getchar();
    return 0;
}


