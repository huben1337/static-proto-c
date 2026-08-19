#!/usr/bin/env nu

def main [buildDir: string] {

use clang-analyzer-util.nu get-avilable-checkers

let availableCheckers = get-avilable-checkers

let usedCheckers = $availableCheckers
    | where { |checker: record<default: bool, name: string>| $checker.name not-like "osx|webkit|cplusplus" }

let checkerEnableFlags = $usedCheckers
    | where { |checker: record<default: bool, name: string>| not $checker.default }
    | each --flatten { |checker| return ["--enable-checker", $checker.name] }

let checkerDisableFlags = $availableCheckers
    | where { |checker: record<default: bool, name: string>| $checker.default and ($checker not-in $usedCheckers) }
    | each --flatten { |checker| return ["--disable-checker", $checker.name] }


print $checkerEnableFlags
print $checkerDisableFlags

let usedCheckersLog = $usedCheckers | each { |x| $"\n  ($x.name)"  } | str join

print $"Using ($usedCheckers | length) checkers:($usedCheckersLog)\n"

let analyzeBuildArguments = [
    "-v",
    "--cdb", ($buildDir | path join "compile_commands.json"),
    "--status-bugs",
    "--analyze-headers",
    "-o", ($buildDir | path join "clang-analyzer-report"),
    ...$checkerDisableFlags,
    ...$checkerEnableFlags
]

^analyze-build ...$analyzeBuildArguments

}
