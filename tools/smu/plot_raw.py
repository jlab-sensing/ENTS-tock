import sys

import matplotlib.pyplot as plot
import pandas as pd

files = sys.argv[1:] or ['data/defbuffer1_1020_151002.csv']

for path in files:
    df = pd.read_csv(path, skiprows=8)
    plot.plot(df['Relative Time'], df['Reading'], label=path.split('/')[-1])

plot.xlabel('Relative time (s)')
plot.ylabel('Power (W)')
plot.legend(fontsize='small')
plot.show()
