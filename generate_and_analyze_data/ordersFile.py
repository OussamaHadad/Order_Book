import random
import matplotlib.pyplot as plt
import json

def generate_orders(output_file_name: str, nOrders: int, 
                    ratioGTC: float = 0.6, ratioFAK: float = 0.1, ratioFOK: float = 0.1, ratioGFD: float = 0.1, ratioM: float = 0.1, 
                    ratioBid: float = 0.5, ratioAsk: float = 0.5, 
                    minBidPrice: float = 30.00, maxBidPrice: float = 40.00,
                    minAskPrice: float = 30.00, maxAskPrice: float = 50.00,
                    canCross: bool = False):
    
    order_types = ["GTC", "FAK", "FOK", "GFD", "M"]
    side_types = ["Bid", "Ask"]
    
    # Prevent crossing orders if canCross is False
    if not canCross:
        minAskPrice = maxBidPrice + 0.01
        maxAskPrice = minAskPrice + (maxBidPrice - minBidPrice) * 2

    # Total orders by side
    total_bid_orders = int(nOrders * ratioBid)
    total_ask_orders = int(nOrders * ratioAsk)

    # Fix GFD count separately, since it's split from others
    count_GFD_bid = int(total_bid_orders * ratioGFD)
    count_GFD_ask = int(total_ask_orders * ratioGFD)

    remaining_bid_orders = total_bid_orders - count_GFD_bid
    remaining_ask_orders = total_ask_orders - count_GFD_ask

    # Compute number of orders for each type and side
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

    # Adjust rounding errors to match total exactly
    while sum(order_counts_bid.values()) < total_bid_orders:
        order_counts_bid["GTC"] += 1
    while sum(order_counts_ask.values()) < total_ask_orders:
        order_counts_ask["GTC"] += 1

    # Generate prices for orders
    prices_bid = [round(random.uniform(minBidPrice, maxBidPrice), 2) for _ in range(total_bid_orders)]
    prices_ask = [round(random.uniform(minAskPrice, maxAskPrice), 2) for _ in range(total_ask_orders)]

    orders = []

    # Generate Bid Orders
    for order_type, count in order_counts_bid.items():
        for _ in range(count):
            price = prices_bid.pop()
            shares = random.randint(1, 1000)
            orders.append({
                "type": order_type,
                "side": "Bid",
                "price": price,
                "shares": shares
            })

    # Generate Ask Orders
    for order_type, count in order_counts_ask.items():
        for _ in range(count):
            price = prices_ask.pop()
            shares = random.randint(1, 1000)
            orders.append({
                "type": order_type,
                "side": "Ask",
                "price": price,
                "shares": shares
            })

    # Shuffle order list for randomness
    random.shuffle(orders)

    # Save to JSON file
    with open(output_file_name, "w") as f:
        json.dump(orders, f, indent=2)

    print(f"File {output_file_name} generated with {nOrders} orders.")

    # Compute actual ratios
    total_orders = len(orders)
    actual_side_counts = {s: sum(1 for order in orders if order["side"] == s) for s in side_types}
    actual_order_counts = {
        s: {t: sum(1 for order in orders if order["side"] == s and order["type"] == t) for t in order_types}
        for s in side_types
    }

    print("Actual Ratios:")
    for side in side_types:
        print(f"{side}: {actual_side_counts[side] / total_orders:.2%}")
        for order_type in order_types:
            if actual_side_counts[side] > 0:
                print(f"  {order_type}: {actual_order_counts[side][order_type] / actual_side_counts[side]:.2%}")

    # Generate pie chart of distribution
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
    random.seed(42)  # For reproducibility
    # Example usage
    generate_orders("../orderBook/orders.json", nOrders = 10000)