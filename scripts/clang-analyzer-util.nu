#!/usr/bin/env nu

export def get-avilable-checkers []: nothing -> table<default: bool, name: string> {
    ^analyze-build --help-checkers-verbose
    | complete
    | get stdout
    | lines
    | parse --regex '^ ([+ ]) (\w+(?:\.\w+)+)'
    | each { |x| {default: ($x.capture0 == "+"), name: $x.capture1} }
}