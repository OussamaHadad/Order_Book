import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns


# TO DO: Update the plot

# Load the statistics file
df = pd.read_csv("stats.txt")

# Filter out only "Add" orders for the plot
df_add = df[df["order"] == "Add"]

# Create a combined label for x-axis
df_add["label"] = df_add["type"].astype(str) + "_" + df_add["limit_level_status"].astype(str)

# Sort labels for better visualization
df_add = df_add.sort_values(by=["type", "limit_level_status"])

# Plotting
plt.figure(figsize=(12, 6))
sns.barplot(x="label", y="mean_latency", data=df_add, yerr=df_add["latency_variance"]**0.5, capsize=0.2, color="royalblue")

# Labels and formatting
plt.xticks(rotation=45)
plt.xlabel("Order Type & Limit Level Status")
plt.ylabel("Mean Latency (µs)")
plt.title("Mean Latency by Order Type & Limit Level Status")
plt.grid(axis="y", linestyle="--", alpha=0.7)

# Save the plot
#plt.savefig("add_orders_latency.png")
plt.show()
