#include <string>
#include <chrono>
#include "estd/utility.hpp"

#if IS_MINGW
#include <cstdlib>
#include <cstdarg>
#endif

#include "./global.hpp"
#include "./fs.hpp"
#include "./container/memory.hpp"
#include "./parser/lexer.re2c.hpp"
#include "./decode_code.hpp"

// #include "./subset_sum_solving/test.dp_bitset_base.hpp"
// #include "./subset_sum_solving/bench.dp_bitset_base.ones_up_to.hpp"
// #include "./subset_sum_solving/bench.dp_bitset_base.apply_num_unsafe.hpp"


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
        [](const fs::OPEN_ERROR, const std::string& path) {
            console.error("Failed to open input file: ", path);
        }
    );

    auto input_file_stat = input_file.stat([](const fs::STAT_ERROR) {
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
        [](const fs::OPEN_ERROR, const std::string& path) {
            console.error("Failed to open output file: ", path);
        }
    );
    
    fs::assert_regular(output_path, output_file.stat([](const fs::STAT_ERROR) {
        std::perror("Failed to get file stats for output file.");
    }));

    const auto input_file_size = input_file_stat.st_size;
    if (input_file_size <= 0) {
        console.error("Input file had invalid size of: ", input_file_size);
        return 1;
    }

    char input_buffer[input_file_size + 1];
    auto read_result = input_file.read(input_buffer, input_file_size);
    input_buffer[input_file_size] = 0;
    if (read_result != input_file_size) {
        console.error("read size mismatch");
        return 1;
    }

    global::input::start = input_buffer;

    console.debug("Lexing input of length: ", input_file_size);

    auto start_ts = std::chrono::high_resolution_clock::now();

    lexer::IdentifierMap identifier_map;
    Buffer ast_buffer = BUFFER_INIT_STACK(4096);
    const auto *const target_struct = lexer::lex<false>(global::input::start, identifier_map, ast_buffer, {});

    decode_code::generate(target_struct, ReadOnlyBuffer{ast_buffer}, std::move(output_file));

    auto end_ts = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_ts - start_ts);
    console.info("Time taken: ", duration.count(), " milliseconds");
    return 0;
}
