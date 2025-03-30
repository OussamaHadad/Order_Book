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
    order_type_weights = [ratioGTC, ratioFAK, ratioFOK, ratioGFD, ratioM]
    side_types = ["Bid", "Ask"]
    side_weights = [ratioBid, ratioAsk]
    
    if not canCross:
        minAskPrice = maxBidPrice + 0.01
        maxAskPrice = minAskPrice + (maxBidPrice - minBidPrice) * 2

    order_counts = {t: int(nOrders * w) for t, w in zip(order_types, order_type_weights)}
    side_counts = {s: int(nOrders * w) for s, w in zip(side_types, side_weights)}
    
    # Adjusting for rounding errors
    while sum(order_counts.values()) < nOrders:
        order_counts[max(order_counts, key = order_counts.get)] += 1
    while sum(side_counts.values()) < nOrders:
        side_counts[max(side_counts, key = side_counts.get)] += 1
    
    prices_bid = [round(random.uniform(minBidPrice, maxBidPrice), 2) for _ in range(side_counts["Bid"])]
    prices_ask = [round(random.uniform(minAskPrice, maxAskPrice), 2) for _ in range(side_counts["Ask"])]
    
    orders = []
    for order_type in order_types:
        for _ in range(order_counts[order_type]):
            side = "Bid" if (side_counts["Bid"] > 0) else "Ask"
            side_counts[side] -= 1
            price = prices_bid.pop() if (side == "Bid") else prices_ask.pop()
            shares = random.randint(1, 1000)
            orders.append(f"{order_type} {side} {price:.2f} {shares}\n")
    
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
    
    # Generate pie chart
    labels = [f"{side}-{order_type}" for side in side_types for order_type in order_types]
    sizes = [actual_order_counts[side][order_type] for side in side_types for order_type in order_types]
    
    plt.figure(figsize = (10, 6))
    plt.pie(sizes, labels=labels, autopct='%1.1f%%', startangle=140, colors=plt.cm.Paired.colors)
    plt.axis('equal')
    plt.title("Order Distribution by Side and Type")
    plt.savefig("order_distribution.png")
    #plt.show()
    
    print("Pie chart saved as 'order_distribution.png'.")


if __name__ == "__main__":
    # Example usage
    generate_orders("orders.txt", nOrders = 10)