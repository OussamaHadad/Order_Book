import random
import matplotlib.pyplot as plt
from datetime import datetime
import mysql.connector
from getpass import getpass  # Hides password input

def generate_orders(cursor: mysql.connector.cursor, tableName: str, nOrders: int, 
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

    actual_side_counts = {s: 0 for s in side_types}
    actual_order_counts = {s: {t: 0 for t in order_types} for s in side_types}

    # Generate Bid Orders
    for order_type, count in order_counts_bid.items():
        for _ in range(count):
            price = prices_bid.pop()
            shares = random.randint(1, 1000)
            timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')    # Get current timestamp as a string

            cursor.execute(f""" INSERT INTO {tableName} (price, shares, side, type)
                                VALUES (%s, %s, %s, %s) """, 
                            (price, shares, "Bid", order_type)
            )

            actual_side_counts["Bid"] += 1
            actual_order_counts["Bid"][order_type] += 1

    # Generate Ask Orders
    for order_type, count in order_counts_ask.items():
        for _ in range(count):
            price = prices_ask.pop()
            shares = random.randint(1, 1000)

            cursor.execute(f""" INSERT INTO {tableName} (price, shares, side, type)
                                VALUES (%s, %s, %s, %s) """, 
                            (price, shares, "Ask", order_type)
            )

            actual_side_counts["Ask"] += 1  
            actual_order_counts["Ask"][order_type] += 1


    total_orders = sum(actual_side_counts.values())
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

    # Get Database Configuration information from user
    host = input("Enter host: ")
    user = input("Enter user: ")
    password = getpass("Enter password: ")
    databaseName = input("Enter database name: ")

    # Database Configuration
    DB_CONFIG = {'host': host, 'user': user, 'password': password, 'database': databaseName}
    del host, user, password    # Clear data from memory

    database = mysql.connector.connect(**DB_CONFIG) # Connect to the database
    print(f"Successfully connected to Database {databaseName}!")

    cursor = database.cursor()

    # Create Database
    cursor.execute(f"CREATE DATABASE IF NOT EXISTS {databaseName}")
    cursor.execute(f"USE {databaseName}")
    # Create Table
    tableName = "orders"
    cursor.execute(f"DROP TABLE IF EXISTS {tableName}")
    cursor.execute(f"""
                    CREATE TABLE IF NOT EXISTS {tableName} (
                        orderId INT AUTO_INCREMENT PRIMARY KEY,
                        price FLOAT,
                        shares INT,
                        side VARCHAR(3),
                        type VARCHAR(3),
                        timestamp DATETIME(6) DEFAULT CURRENT_TIMESTAMP(6)
                    )
    """)

    # Generate orders & Insert them in table
    generate_orders(cursor, tableName, nOrders = 3000)

    database.commit()  # Commit the changes to the database
    cursor.close()  # Close the cursor