from pymongo import MongoClient
import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# Connect to MongoDB
client = MongoClient("mongodb://preventive_maintenance:hl6GjO5LlRuQT1n@nosql.smartsystem.id:27017/preventive_maintenance")
db = client['preventive_maintenance']
collection = db['sensor01_acc_xyz']

# Initialize the plot
fig, ax = plt.subplots()
x_data, y_data, z_data, time_data = [], [], [], []

def fetch_data():
    cursor = collection.find().sort("timestamp", -1).limit(10000)  # Fetch last 100 records
    data = list(cursor)
    df = pd.DataFrame(data)
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    df.sort_values('timestamp', inplace=True)
    return df

def update(frame):
    global x_data, y_data, z_data, time_data
    df = fetch_data()

    # Update data lists
    time_data = df['timestamp'].tolist()
    x_data = df['acceleration'].apply(lambda accel: accel['x']).tolist()
    y_data = df['acceleration'].apply(lambda accel: accel['y']).tolist()
    z_data = df['acceleration'].apply(lambda accel: accel['z']).tolist()

    # Clear and redraw
    ax.clear()
    ax.plot(time_data, x_data, label='x-axis')
    ax.plot(time_data, y_data, label='y-axis')
    ax.plot(time_data, z_data, label='z-axis')
    ax.legend()
    ax.set_title('Acceleration Over Time')
    ax.set_xlabel('Time')
    ax.set_ylabel('Acceleration (m/s²)')
    plt.xticks(rotation=45)

ani = FuncAnimation(fig, update, interval=1000)  # Update every second
plt.show()
