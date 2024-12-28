from pymongo import MongoClient
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from scipy.fft import fft, fftfreq

# Connect to MongoDB
client = MongoClient("mongodb://preventive_maintenance:hl6GjO5LlRuQT1n@nosql.smartsystem.id:27017/preventive_maintenance")
db = client['preventive_maintenance']
collection = db['sensor01_acc_xyz']

def fetch_data():
    """Fetch acceleration data from MongoDB"""
    cursor = collection.find().sort("timestamp", -1).limit(10000)
    data = list(cursor)
    df = pd.DataFrame(data)
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    df.sort_values('timestamp', inplace=True)
    return df

def perform_fft(data):
    """
    Perform Fast Fourier Transform on the input data
    
    Parameters:
    -----------
    data : array-like
        Time series data to transform
    
    Returns:
    --------
    tuple: (frequencies, magnitudes)
    """
    # Perform FFT
    n = len(data)
    
    # Compute FFT and get magnitudes
    yf = fft(data)
    xf = fftfreq(n, 1/100)  # Assuming 1000 Hz sampling rate, adjust if different
    
    # Compute the magnitude spectrum (absolute value of the FFT)
    magnitudes = np.abs(yf)
    
    # Return only the positive frequencies (first half of the spectrum)
    return xf[:n//2], magnitudes[:n//2]

def plot_time_domain_and_frequency_domain(df):
    """
    Create a 2x3 subplot showing time domain and frequency domain for x, y, z axes
    
    Parameters:
    -----------
    df : pandas.DataFrame
        DataFrame containing acceleration data
    """
    # Extract acceleration data
    x_data = df['acceleration'].apply(lambda accel: accel['x']).tolist()
    y_data = df['acceleration'].apply(lambda accel: accel['y']).tolist()
    z_data = df['acceleration'].apply(lambda accel: accel['z']).tolist()
    time_data = df['timestamp'].tolist()

    # Create subplots
    fig, axs = plt.subplots(2, 3, figsize=(18, 10))
    
    # Time domain plots (first row)
    axs[0, 0].plot(time_data, x_data)
    axs[0, 0].set_title('X-Axis Time Domain')
    axs[0, 0].set_xlabel('Time')
    axs[0, 0].set_ylabel('Acceleration (m/s²)')
    
    axs[0, 1].plot(time_data, y_data)
    axs[0, 1].set_title('Y-Axis Time Domain')
    axs[0, 1].set_xlabel('Time')
    axs[0, 1].set_ylabel('Acceleration (m/s²)')
    
    axs[0, 2].plot(time_data, z_data)
    axs[0, 2].set_title('Z-Axis Time Domain')
    axs[0, 2].set_xlabel('Time')
    axs[0, 2].set_ylabel('Acceleration (m/s²)')
    
    # Frequency domain plots (second row)
    x_freq, x_mag = perform_fft(x_data)
    y_freq, y_mag = perform_fft(y_data)
    z_freq, z_mag = perform_fft(z_data)
    
    axs[1, 0].plot(x_freq, x_mag)
    axs[1, 0].set_title('X-Axis Frequency Domain')
    axs[1, 0].set_xlabel('Frequency (Hz)')
    axs[1, 0].set_ylabel('Magnitude')
    
    axs[1, 1].plot(y_freq, y_mag)
    axs[1, 1].set_title('Y-Axis Frequency Domain')
    axs[1, 1].set_xlabel('Frequency (Hz)')
    axs[1, 1].set_ylabel('Magnitude')
    
    axs[1, 2].plot(z_freq, z_mag)
    axs[1, 2].set_title('Z-Axis Frequency Domain')
    axs[1, 2].set_xlabel('Frequency (Hz)')
    axs[1, 2].set_ylabel('Magnitude')
    
    plt.tight_layout()
    plt.show()

def main():
    # Fetch data
    df = fetch_data()
    
    # Plot time and frequency domain
    plot_time_domain_and_frequency_domain(df)

if __name__ == "__main__":
    main()