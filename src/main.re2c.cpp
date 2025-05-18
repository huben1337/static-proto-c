#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstdarg>
#include <stdexcept>
#include <string_view>
#include <chrono>

#include "fs.cpp"
#include "memory.cpp"
#include "lexer.re2c.cpp"
#include "decode_code.cpp"
#include "lex_error.cpp"


int main (int argc, const char** argv) {
    logger::debug("spc.exe");
    if (argc <= 2) {
        logger::error("no output and/or input supplied");
        return 1;
    }

    const std::string_view input_path = {argv[1]};
    const std::string_view output_path = {argv[2]};

    fs::OpenWithStatsResult input_file{};
    fs::OpenWithStatsResult output_file{};

    try {
        input_file = fs::open_with_stat(input_path, O_RDONLY | O_BINARY);
        fs::throw_not_regular(input_path, input_file.stat);
        fs::realpath(input_path, input_file_path);
        output_file = fs::open_with_stat(output_path, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, S_IRUSR | S_IWUSR);
        fs::throw_not_regular(output_path, output_file.stat);
    } catch (std::runtime_error& err) {
        logger::error(err.what());
        return 1;
    }

    auto input_file_size = input_file.stat.st_size;
    char input_data[input_file_size + 1];
    input_data[input_file_size] = 0;
    int read_result = read(input_file.fd, input_data, input_file_size);
    if (read_result != input_file_size) {
        logger::error("read size mismatch");
        return 1;
    }
    if (close(input_file.fd) != 0) {
        logger::error("could not close input file");
        return 1;
    }

    input_start = input_data;
    
    logger::debug("Lexing input of length: ", input_file_size);

    auto start_ts = std::chrono::high_resolution_clock::now();

    #define DO_LEX
    #ifdef DO_LEX
    lexer::IdentifierMap identifier_map;
    uint8_t buffer_mem[5000];
    Buffer buffer = {buffer_mem};
    auto target_struct = lexer::lex<false>(input_data, identifier_map, buffer);

    // #define DO_PRINT
    #ifdef DO_PRINT
    printf("\n\n- target: ");
    printf(extract_string(target_struct->name).c_str());
    print_parse_result(identifier_map, buffer);
    #endif

    for (size_t i = 0; i < 1; i++)
    {
        #define DO_CODEGEN
        #ifdef DO_CODEGEN
        // decode_code::generate(target_struct, buffer, output_fd);
        // printf("\n--------------------------------------------------------------\n");
        decode_code::generate(target_struct, buffer, output_file.fd);
        #endif
    }
    
    buffer.dispose();
    #endif

    if (close(output_file.fd) != 0) {
        logger::debug("could not close output file");
        return 1;
    };

    auto end_ts = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts);
    logger::info("Time taken: ", duration.count(), " milliseconds");
    return 0;
}
