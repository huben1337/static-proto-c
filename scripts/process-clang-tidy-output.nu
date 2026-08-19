#!/usr/bin/env nu

def main [outFile: string] {
    mut inside_error = false
    mut content = {}
    for line in (open $outFile | lines) {
        if ($line | is-empty) {
            $inside_error = false
        } else {
            let parsed = $line | parse --regex "^(\\/.+:\\d+:\\d+): (.+): .+ \\[(.+)\\]" | get 0 -o
            if ($parsed == null) {
                continue
            }
            let codeReference = $parsed.capture0
            let severity = $parsed.capture1
            let ruleName = $parsed.capture2
            let entry = { ref: $codeReference, severity: $severity }
            let itemsForRule = $content | get $ruleName -o
            if ($itemsForRule == null) {
                $content = ($content | insert $ruleName [$entry])
            } else {
                $content = ($content | upsert $ruleName ($itemsForRule | append $entry))
            }
        }
    }

    let html_report = (
        $content 
        | items { |category, entries| 
            # Calculate badge item count for main header
            let badge = $"<span style='background:#e1ecf4; color:#39739d; padding:2px 8px; border-radius:12px; font-size:0.85em; margin-left:8px; font-weight:500;'>(($entries | length)) items</span>"
            
            # Process each individual nested log entry record
            let nested_entries = ($entries | each { |entry| 
                # Generate table rows dynamically based on the record fields
                let table_rows = (
                    $entry 
                    | reject ref # Remove title column from table body
                    | items { |key, val| 
                        $"<tr>
                            <td style='padding:8px; border:1px solid #ddd; font-weight:bold; background:#f9f9f9; width:30%;'>($key)</td>
                            <td style='padding:8px; border:1px solid #ddd; font-family:monospace;'>($val)</td>
                        </tr>"
                    } 
                    | str join "\n"
                )

                # Build the inner toggleable item layout
                $"<details style='background:#fff; border:1px solid #e1e4e8; border-radius:4px; margin-bottom:8px; padding:10px;'>
                    <summary style='font-weight:500; cursor:pointer; color:#24292e;'>($entry.ref)</summary>
                    <div style='margin-top:10px; padding:4px;'>
                        <table style='width:100%; border-collapse:collapse; font-size:0.9em; text-align:left;'>
                            ($table_rows)
                        </table>
                    </div>
                </details>"
            } | str join "\n")
            
            # Build parent category card layout
            $"<details style='background:#f6f8fa; border:1px solid #d0d7de; border-radius:6px; margin-bottom:16px; padding:16px; font-family:sans-serif;'>
                <summary style='font-weight:bold; font-size:1.1em; cursor:pointer; margin-bottom:10px;'>($category) ($badge)</summary>
                <div style='margin-top:12px;'>
                    ($nested_entries)
                </div>
            </details>"
        } 
        | str join "\n"
    )

    # Combine elements into a styled wrapper document
    let final_document = $"
    <div style='max-width:800px; margin:20px auto; font-family:sans-serif;'>
        <h1 style='color:#111; border-bottom:2px solid #eaeaea; padding-bottom:8px;'>Log Parser Summary Report</h1>
        ($html_report)
    </div>"

    $final_document | save -f log_report.html
}