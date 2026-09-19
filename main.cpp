// version: OPENQASM 2.0
// Only U and CX are built-in gates
// Third-party library: see NOTICE file

#include"include/init.hpp"

int main(int argc, char* argv[]) {
    /* ----- load file -----
     * there should be two arguments
     * QAssembler.exe target.qasm
     * argv[0] is QAssembler.exe
     * argv[1] is Target.qasm
    */
    if (argc != 2) [[unlikely]] {
        std::cout << "Fatal Error: No input files\n";
        std::cout << "Usage: QAssembler <filepath>\n";
        return 1;
    }
    std::string main_path(argv[1]);
    std::ifstream main_ifs(argv[1]);
    if (!main_ifs) [[unlikely]] {
        std::cout << "Fatal Error: Cannot open file" << main_path << "\n";
        return 1;
    }
    std::ostringstream ss;
    ss << main_ifs.rdbuf();
    
    // ----- setup managers -----
    SrcManager   source_manager;
    ErrorManager error_manager;
    TokenManager token_manager;
    srcman = &source_manager;
    errman = &error_manager;
    tokman = &token_manager;
    
    Lexer lexer;
    Preprocessor preprocessor;

    // ----- parse every file -----
    size_t main_id = source_manager.add_file(main_path, ss.str());
    std::vector<size_t> unanalysed_file_id = {main_id};
    while (!unanalysed_file_id.empty()) {
        // get the current file and pop
        size_t current_file_id = unanalysed_file_id.back();
        SrcFile& current_file = srcman->get_file(current_file_id);
        unanalysed_file_id.pop_back();
        // Process pipeline
        preprocessor.preprocess(&current_file);
        lexer.tokenize(&current_file);
    }

    // output
    std::cout << "----- SourceManager -----\n";
    for (size_t i = 0; i < srcman->size(); i++) {
        std::cout << "File " << srcman->get_path(i) << ": " << srcman->get_file(i).length() << "characters\n";
    }
    std::cout << "----- ErrorManager -----\n";
    for (size_t i = 0; i < errman->size(); i++) {
        std::cout << errman->errors[i] << "\n";
    }
    std::cout << "----- TokenManager -----\n";
    for (size_t i = 0; i < tokman->size(); i++) {
        std::cout << tokman->tokens[i] << "\n";
    }
}

// g++ main.cpp -o main.exe -O3 -Wall -Wextra -g3 -std=c++20 -static -static-libgcc -static-libstdc++
// declare forwarding -> declare null ptrs -> define class/struct -> construct objects -> assign ptrs -> main()
