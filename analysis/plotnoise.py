import json
import matplotlib.pyplot as plt

# Load JSON data from a file
def load_json_data(file_path):
    with open(file_path, 'r') as file:
        data = json.load(file)
    return data

# Extract first 4000 entries from the "RPDA" array in the JSON
def extract_first_4000_entries(data):
    if "RPDA" in data:
        return data["RPDA"][:4000]  # Extract first 1000 entries from the array
    else:
        raise KeyError("Key 'RPDA' not found in the JSON file.")

# Plot the data
def plot_data(data):
    plt.plot(data)
    plt.title("First 4000 Entries from RPDA Array")
    plt.xlabel("Index")
    plt.ylabel("Value")
    plt.grid(True)
    plt.savefig("plot.png")  # Save the plot to a file
    print("Plot saved as 'plot.png'")

# Main function
if __name__ == "__main__":
    # Path to your JSON file
    file_path = 'Variables.json'

    # Load the JSON data
    data = load_json_data(file_path)

    # Extract the first 4000 entries from the "RPDA" array
    extracted_data = extract_first_4000_entries(data)

    # Plot and save the data
    plot_data(extracted_data)
