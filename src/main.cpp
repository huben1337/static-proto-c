#include <cstddef>
#include <cstdio>
#include <gsl/util>
#include <string>
#include <chrono>

#include "estd/utility.hpp"
#include "parser/lexer_types.hpp"


#include "./global.hpp"
#include "./sys/errno.hpp"
#include "./sys/fs.hpp"
#include "./container/memory.hpp"
#include "./parser/lexer.re2c.hpp"
#include "./decode_code.hpp"

int main (const int argc, const char* const* const argv) {
    console.debug("spc");
    if (argc <= 2) {
        error_exit("no output and/or input supplied");
    }

    const std::string input_path {argv[1]};
    const std::string output_path {argv[2]};

    auto input_file = fs::File::open(
        input_path,
        estd::variadic_v<
            fs::OPEN_FLAGS::RDONLY
        >{},
        {},
        [&input_path](const sys::OPEN_ERROR) {
            error_exit("Failed to open input file: ", input_path);
        }
    );

    const auto input_file_stat = input_file.stat([](const sys::STAT_ERROR) {
        error_exit("Failed to get file stats for input file.");
    });

    if (!fs::is_regular(input_file_stat)) {
        error_exit("Expected regular file for input.");
    }

    global::input::file_path = fs::realpath(input_path);
    
    auto output_file = fs::File::open(
        output_path,
        estd::variadic_v<
            fs::OPEN_FLAGS::WRONLY,
            fs::OPEN_FLAGS::CREAT,
            fs::OPEN_FLAGS::TRUNC
        >{},
        estd::variadic_v<
            fs::PERMISSION_MODE::IRUSR,
            fs::PERMISSION_MODE::IWUSR
        >{},
        [&output_path](const sys::OPEN_ERROR) {
            error_exit("Failed to open output file: ", output_path);
        }
    );
    
    if (!fs::is_regular(output_file.stat([](const sys::STAT_ERROR) {
        error_exit("Failed to get file stats for output file.");
    }))) {
        error_exit("Expected regular file for outupt.");
    }

    if (input_file_stat.st_size <= 0) {
        error_exit("Input file had invalid size of: ", input_file_stat.st_size);
    }
    const size_t input_file_size = gsl::narrow_cast<size_t>(input_file_stat.st_size);

    char input_buffer[input_file_size + 1];    

    for (size_t read = 0; read < input_file_size;) {
        read = input_file.read(
            input_buffer + read,
            input_file_size - read,
            [](const auto e) {
                error_exit("Failed to read input file: ", std::strerror(e));
            }
        );
    }

    input_buffer[input_file_size] = 0; // Null termineate input for lexer

    global::input::start = input_buffer;

    console.debug("Lexing input of length: ", input_file_size);

    auto start_ts = std::chrono::high_resolution_clock::now();

    lexer::IdentifierMap identifier_map;
    
    Buffer::backing_t initial_ast_buffer[BUFFER_INIT_ARRAY_SIZE<char, 4096>];
    Buffer ast_buffer {initial_ast_buffer};
    const lexer::StructDefinition& target_struct = lexer::lex<false>(global::input::start, identifier_map, ast_buffer, {});

    decode_code::generate(target_struct, std::move(output_file));

    auto end_ts = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts);
    console.info("Time taken: ", duration.count(), " milliseconds");
    return 0;
}
