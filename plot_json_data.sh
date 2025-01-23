#!/bin/bash

# Directory containing .json files
JSON_DIR="/home/snolab/red-pitaya-frontend/analysis"  # Updated path from your terminal

# Output directory for extracted data
OUTPUT_DIR="./extracted_data"
mkdir -p "$OUTPUT_DIR"

# Debug log file
DEBUG_LOG="$OUTPUT_DIR/debug.log"
> "$DEBUG_LOG"  # Clear the log file

echo "Extracting and debugging JSON files from $JSON_DIR..."
echo "Debug log will be saved to $DEBUG_LOG"

# Loop through each .json file in the directory
for json_file in "$JSON_DIR"/*.json; do
    echo "Processing file: $json_file"
    echo "Processing file: $json_file" >> "$DEBUG_LOG"

    # Print the top-level keys in the JSON file
    echo "Top-level keys in $json_file:" >> "$DEBUG_LOG"
    jq 'keys' "$json_file" 2>>"$DEBUG_LOG" || echo "Error reading $json_file" >> "$DEBUG_LOG"

    # Navigate to "Equipment" and print its structure
    echo "Equipment keys:" >> "$DEBUG_LOG"
    jq '.Equipment | keys' "$json_file" 2>>"$DEBUG_LOG" || echo "No Equipment key found." >> "$DEBUG_LOG"

    # Navigate to "Equipment -> Periodic" and print its structure
    echo "Periodic keys:" >> "$DEBUG_LOG"
    jq '.Equipment.Periodic | keys' "$json_file" 2>>"$DEBUG_LOG" || echo "No Periodic key found." >> "$DEBUG_LOG"

    # Navigate to "Equipment -> Periodic -> Variables" and print its structure
    echo "Variables keys:" >> "$DEBUG_LOG"
    jq '.Equipment.Periodic.Variables | keys' "$json_file" 2>>"$DEBUG_LOG" || echo "No Variables key found." >> "$DEBUG_LOG"

    # Extract the DATA array
    data=$(jq '.Equipment.Periodic.Variables.DATA' "$json_file" 2>>"$DEBUG_LOG")

    # Check if DATA array exists
    if [[ $data == null || -z $data ]]; then
        echo "No DATA array found in $json_file. Skipping..."
        echo "No DATA array found in $json_file. Skipping..." >> "$DEBUG_LOG"
        continue
    fi

    # Save the DATA array to a separate file
    output_file="$OUTPUT_DIR/$(basename "$json_file" .json)_data.txt"
    echo "$data" | jq -c '.[]' > "$output_file"
    echo "Extracted DATA array saved to $output_file" >> "$DEBUG_LOG"

    # Plot the data using gnuplot
    plot_file="$OUTPUT_DIR/$(basename "$json_file" .json)_plot.png"
    echo "set terminal png size 800,600
set output '$plot_file'
set title 'DATA Plot from $json_file'
set xlabel 'Index'
set ylabel 'Value'
plot '-' with lines title 'DATA'" > "$OUTPUT_DIR/plot_commands.gp"

    # Feed the data to gnuplot
    echo "$data" | jq -c '.[]' >> "$OUTPUT_DIR/plot_commands.gp"
    echo "e" >> "$OUTPUT_DIR/plot_commands.gp"
    gnuplot "$OUTPUT_DIR/plot_commands.gp" >> "$DEBUG_LOG" 2>&1
    echo "Plot saved to $plot_file"
done

echo "Debugging complete. Check $DEBUG_LOG for details."
echo "All plots are saved in $OUTPUT_DIR."
