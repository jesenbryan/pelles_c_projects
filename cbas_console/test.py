import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("hor.csv")

plt.plot(df["x"], df["y"])
plt.gca().invert_yaxis()  # IMPORTANT for BMP coordinate system
plt.axis("equal")
plt.show()