**Situation**

In an estate house with a large courtyard, there is a remote-controlled automatic gate, a mid-sized pond with a water feature, a large decorative fountain, and a smaller decorative fountain. All of these must be operated remotely and should shut off automatically depending on the wind speed.



- **Gate:** 

  - An automated gate is triggered by an in-ground sensor 15m away. The problem is that it is unsafe to operate the gate during high-wind conditions.

- **Fountains:**

  - A primary water feature in a pond that  uses a ½ HP single-phase 120V AC sump pump. The problem is that high winds cause significant water loss.

  - Fountain A, smaller pump (sub-4 amp), but also negatively impacted by wind.

  - Fountain B; see Fountain A above

    

- **Physical Constraints:**

  - **Distance:** The central control point (the house) is 100m from the gate and pond, 15m from Fountain A, and 30m from Fountain B.

  - **Connectivity:** The gate and the furthest fountain are 140m apart without direct line-of-sight.

    

- **System Objectives:**

  - Provide automated control over the gate, water feature, and fountains based on time, wind speed, and temperature.

  - Display real-time environmental data (wind/temp) inside the house.

  - Support manual overrides and adjustable set-points via a central interface.

    

- **Resources**

  - **Microcontrollers:** 4× RP2040 (Adafruit Feather format) with integrated RFM69HCW packet radios (expandable).

    - **Power Switching:**
      - 2× 10A resistive-rated non-latching relays (120V AC compatible, Feather format).
      - 1× Latching low-voltage relay (Feather format).

    - **Sensors & Input:**
      - 1× RTC module with integrated thermometer (Feather format).
      - 1× Wind sensor (anemometer) with a dedicated power supply.

  - **User Interface:** 1× 128x64 OLED FeatherWing with 3 pushbuttons.

  - **Power Infrastructure:**
    - Assorted 5V power supplies.
      - 1× 2-24V to 5V buck converter for secondary power regulation