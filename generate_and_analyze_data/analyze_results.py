import json
import matplotlib.pyplot as plt
import numpy as np

def plot_mean_latency_with_error(json_file, output_file = "latency_plot.png"):
    with open(json_file, 'r') as f:
        data = json.load(f)

    x = []
    means = []
    stds = []
    labels = []

    for category, entries in data.items():
        if isinstance(entries, list):  # Add / Amend / Cancel
            for entry in entries:
                mean = entry["mean_latency"]
                variance = entry["latency_variance"]
                std = np.sqrt(variance)

                means.append(mean)
                stds.append(std)

                label_parts = [category]
                if "order_type" in entry:
                    label_parts.append(entry["order_type"])
                if "limit_level_status" in entry:
                    label_parts.append(entry["limit_level_status"])
                labels.append("\n".join(label_parts))
        else:  # Match
            mean = entries["mean_latency"]
            variance = entries["latency_variance"]
            std = np.sqrt(variance)

            means.append(mean)
            stds.append(std)
            labels.append("Match")

    x = np.arange(len(means))

    # Plot with error bars
    plt.figure(figsize=(14, 7))
    plt.bar(x, means, yerr=stds, capsize=5, color='steelblue', alpha=0.8)
    plt.yscale('log')
    plt.xticks(x, labels, rotation=45, ha='right')
    plt.ylabel("Mean Latency (μs, log scale)")
    plt.title("Mean Latency with Standard Deviation per Order Type")
    plt.tight_layout()
    plt.grid(True, which="both", linestyle="--", linewidth=0.5)

    # Save the figure
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()

    print(f"Latency plot saved")



if __name__ == "__main__":
    json_path = "../orderBook/stats.json"  # Update this path if necessary
    plot_mean_latency_with_error(json_path)
    print("Plotting complete.")