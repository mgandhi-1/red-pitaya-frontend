import json
import matplotlib.pyplot as plt

# Load JSON data from a file
def load_json_data(file_path):
    with open(file_path, 'r') as file:
        data = json.load(file)
    return data

# Extract the desired 4000 entries from the "RPDA" array in the JSON
def extract_and_convert_entries(data):
    if "RPDA" in data:
        adc_values = data["RPDA"][4000:20000]  # Extract first 4000 entries from the array
        # Convert ADC values to voltage
        voltage_values = [(2 * x) / 16384 for x in adc_values]
        return voltage_values
    else:
        raise KeyError("Key 'RPDA' not found in the JSON file.")

# Plot the data
def plot_data(data):
    plt.plot(data)
    plt.title("First 4000 Entries from RPDA Array (in Voltage)")
    plt.xlabel("Index")
    plt.ylabel("Voltage (V)")
    plt.grid(True)
    plt.savefig("plot.png")  # Save the plot to a file
    print("Plot saved as 'plot.png'")

# Main function
if __name__ == "__main__":
    # Path to your JSON file
    file_path = 'Variables.json'

    # Load the JSON data
    data = load_json_data(file_path)

    # Extract and convert the first 4000 entries from the "RPDA" array
    extracted_data = extract_and_convert_entries(data)

    # Plot and save the data
    plot_data(extracted_data)
