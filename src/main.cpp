#include <cstddef>
#include <cstdio>
#include <gsl/util>
#include <string>
#include <chrono>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnrvo"
#pragma GCC diagnostic ignored "-Wunsafe-buffer-usage"
#include <nameof.hpp>
#pragma GCC diagnostic pop

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
        console.error("no output and/or input supplied");
        return 1;
    }

    const std::string input_path {argv[1]};
    const std::string output_path {argv[2]};

    auto input_file = fs::File::open(
        input_path,
        estd::variadic_v<
            fs::OPEN_FLAGS::RDONLY
        >{},
        {},
        [](const sys::OPEN_ERROR, const std::string& path) {
            console.error("Failed to open input file: ", path);
        }
    );

    auto input_file_stat = input_file.stat([](const sys::STAT_ERROR) {
        std::perror("Failed to get file stats for input file.");
    });
    fs::assert_regular(input_path, input_file_stat);

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
        [](const sys::OPEN_ERROR, const std::string& path) {
            console.error("Failed to open output file: ", path);
        }
    );
    
    fs::assert_regular(output_path, output_file.stat([](const sys::STAT_ERROR) {
        std::perror("Failed to get file stats for output file.");
    }));

    if (input_file_stat.st_size <= 0) {
        console.error("Input file had invalid size of: ", input_file_stat.st_size);
        return 1;
    }
    const size_t input_file_size = gsl::narrow_cast<size_t>(input_file_stat.st_size);

    char input_buffer[input_file_size + 1];
    const auto read_result = input_file.read(input_buffer, input_file_size);
    if (read_result.has_error()) {
        std::perror("Failed to read input file.");
    }

    const size_t read_input_length = read_result.uvalue();
    if (read_input_length != input_file_size) {
        console.error("read size mismatch");
        return 1;
    }

    input_buffer[input_file_size] = 0; // Null termineate input for lexer

    global::input::start = input_buffer;

    console.debug("Lexing input of length: ", input_file_size);

    auto start_ts = std::chrono::high_resolution_clock::now();

    lexer::IdentifierMap identifier_map;
    Buffer ast_buffer = BUFFER_INIT_STACK(4096);
    const lexer::StructDefinition& target_struct = lexer::lex<false>(input_buffer, identifier_map, ast_buffer, {});

    decode_code::generate(target_struct, ReadOnlyBuffer{ast_buffer}, std::move(output_file));

    auto end_ts = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts);
    console.info("Time taken: ", duration.count(), " milliseconds");
    return 0;
}
