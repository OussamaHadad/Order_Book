import random
import matplotlib.pyplot as plt

def generate_orders(output_file_name: str, nOrders: int, 
                    ratioGTC: float = 0.6, ratioFAK: float = 0.1, ratioFOK: float = 0.1, ratioGFD: float = 0.1, ratioM: float = 0.1, 
                    ratioBid: float = 0.5, ratioAsk: float = 0.5, 
                    minBidPrice: float = 30.00, maxBidPrice: float = 40.00,
                    minAskPrice: float = 30.00, maxAskPrice: float = 50.00,
                    canCross: bool = False):
    
    # Generate Orders
    order_types = ["GTC", "FAK", "FOK", "GFD", "M"]
    side_types = ["Bid", "Ask"]
    
    if not canCross:
        minAskPrice = maxBidPrice + 0.01
        maxAskPrice = minAskPrice + (maxBidPrice - minBidPrice) * 2

    # Initialize counts for the order types
    total_bid_orders = int(nOrders * ratioBid)
    total_ask_orders = int(nOrders * ratioAsk)
    
    # Create specific counts for GFD for both Bid and Ask, ensuring ratio consistency
    count_GFD_bid = int(total_bid_orders * ratioGFD)
    count_GFD_ask = int(total_ask_orders * ratioGFD)
    
    # Remaining counts for the other order types
    remaining_bid_orders = total_bid_orders - count_GFD_bid
    remaining_ask_orders = total_ask_orders - count_GFD_ask
    
    order_counts_bid = {
        "GTC": int(remaining_bid_orders * ratioGTC / (1 - ratioGFD)),
        "FAK": int(remaining_bid_orders * ratioFAK / (1 - ratioGFD)),
        "FOK": int(remaining_bid_orders * ratioFOK / (1 - ratioGFD)),
        "GFD": count_GFD_bid,
        "M": int(remaining_bid_orders * ratioM / (1 - ratioGFD))
    }

    order_counts_ask = {
        "GTC": int(remaining_ask_orders * ratioGTC / (1 - ratioGFD)),
        "FAK": int(remaining_ask_orders * ratioFAK / (1 - ratioGFD)),
        "FOK": int(remaining_ask_orders * ratioFOK / (1 - ratioGFD)),
        "GFD": count_GFD_ask,
        "M": int(remaining_ask_orders * ratioM / (1 - ratioGFD))
    }

    # Adjust for rounding errors
    while sum(order_counts_bid.values()) < total_bid_orders:
        order_counts_bid["GTC"] += 1
    while sum(order_counts_ask.values()) < total_ask_orders:
        order_counts_ask["GTC"] += 1
    
    # Prices generation
    prices_bid = [round(random.uniform(minBidPrice, maxBidPrice), 2) for _ in range(total_bid_orders)]
    prices_ask = [round(random.uniform(minAskPrice, maxAskPrice), 2) for _ in range(total_ask_orders)]
    
    orders = []
    
    # Generate Bid Orders
    for order_type, count in order_counts_bid.items():
        for _ in range(count):
            price = prices_bid.pop()
            shares = random.randint(1, 1000)
            orders.append(f"{order_type} Bid {price:.2f} {shares}\n")
    
    # Generate Ask Orders
    for order_type, count in order_counts_ask.items():
        for _ in range(count):
            price = prices_ask.pop()
            shares = random.randint(1, 1000)
            orders.append(f"{order_type} Ask {price:.2f} {shares}\n")
    
    random.shuffle(orders)
    
    with open(output_file_name, "w") as file:
        file.writelines(orders)
    
    print(f"File {output_file_name} generated with {nOrders} orders.")

    # Compute actual ratios
    total_orders = len(orders)
    actual_side_counts = {s: sum(1 for order in orders if s in order) for s in side_types}
    actual_order_counts = {s: {t: sum(1 for order in orders if s in order and t in order) for t in order_types} for s in side_types}
    
    print(f"File {output_file_name} generated with {nOrders} orders.")
    print("Actual Ratios:")
    for side in side_types:
        print(f"{side}: {actual_side_counts[side] / total_orders:.2%}")
        for order_type in order_types:
            print(f"  {order_type}: {actual_order_counts[side][order_type] / actual_side_counts[side]:.2%}")
    
    # Filter out zero-percentage entries
    labels = [f"{side}-{order_type}" for side in side_types for order_type in order_types]
    sizes = [actual_order_counts[side][order_type] for side in side_types for order_type in order_types]

    filtered_labels_sizes = [(label, size) for label, size in zip(labels, sizes) if size > 0]
    if not filtered_labels_sizes:
        print("No valid data to plot.")
    else:
        labels, sizes = zip(*filtered_labels_sizes)

        plt.figure(figsize=(10, 6))
        plt.pie(sizes, labels=labels, autopct=lambda p: f'{p:.1f}%' if p > 0 else '', 
                startangle=140, colors=plt.cm.Paired.colors, pctdistance=0.85, labeldistance=1.1)

        plt.axis('equal')
        plt.title("Order Distribution by Side and Type")
        plt.savefig("order_distribution.png")
        print("Pie chart saved as 'order_distribution.png'.")


if __name__ == "__main__":
    # Example usage
    generate_orders("orders.txt", nOrders = 10000)