#!/usr/bin/env python3

"""
Calculate the average current required to charge a capacitor
from an initial voltage to a final voltage over a given time.

Formula:
    Q = C * (Vf - Vi)      # charge needed
    I_avg = Q / t          # average current over charging time t

Edit the values below (capacitance, voltages, and charging time)
to match your scenario.
"""

from datetime import datetime

# ---- Inputs (edit these) ----
C = 100e-6      # capacitance in farads (100 uF)
Vi = 0.018      # initial voltage in volts (12 mV)
Vf = 0.185       # final voltage in volts (1.07 V)
t_start = datetime(2026, 7, 13, 10, 16)
t_end = datetime(2026, 7, 13, 11, 19)

# ---- Calculation ----
t = (t_end - t_start).total_seconds()
dV = Vf - Vi
Q = C * dV          # charge in coulombs
I_avg = Q / t        # average current in amps

# ---- Output ----
print(f"Start:              {t_start}")
print(f"End:                {t_end}")
print(f"Capacitance:        {C*1e6:.2f} uF")
print(f"Initial voltage:    {Vi*1000:.2f} mV")
print(f"Final voltage:      {Vf:.3f} V")
print(f"Voltage change:     {dV:.3f} V")
print(f"Charge required:    {Q*1e6:.4f} uC")
print(f"Charging time:      {t:.4f} s")
print(f"Average current:    {I_avg*1e6:.4f} uA  ({I_avg:.6e} A)")
