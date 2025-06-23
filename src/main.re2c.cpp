#include <cstdlib>
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

    lexer::IdentifierMap identifier_map;
    Buffer ast_buffer = {MEMORY_INIT_STACK(5000)};
    auto target_struct = lexer::lex<false>(input_data, identifier_map, ast_buffer, {});

    decode_code::generate(target_struct, ast_buffer, output_file.fd);
    
    ast_buffer.dispose();

    if (close(output_file.fd) != 0) {
        logger::error("could not close output file");
        return 1;
    };

    auto end_ts = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts);
    logger::info("Time taken: ", duration.count(), " milliseconds");
    return 0;
}
