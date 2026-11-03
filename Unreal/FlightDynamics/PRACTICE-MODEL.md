# OneClickPilot C172P practice model

The MIT-licensed generator authors the aircraft, engine and propeller XML
required by JSBSim. It has no input dependency on the retired c172x XML.
Run `python3 Unreal/FlightDynamics/generate_practice_model.py Unreal/Content/FlightModels`
from the project root to regenerate the three model files.

The aircraft class and nominal power are informed by FAA type certificate 3A12
and FAA AC 36-1H. The aerodynamic model uses textbook dynamic-pressure force
and moment scaling and NASA Glenn's lift and drag equations as references.
Control derivatives, stall behavior, engine and propeller curves, mass
distribution and tire and suspension behavior are approximate engineering
assumptions for this prototype.

- https://drs.faa.gov/browse/excelExternalWindow/DRSDOCID119659606420240813185757.0001
- https://www.faa.gov/documentLibrary/media/Advisory_Circular/AC_36-1H.pdf
- https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/lift-equation/
- https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/induced-drag-coefficient/

The model has basic pitch, roll, yaw, flap and ground steering responses.
Flight-envelope fidelity, circuit handling and aircraft-specific performance
have yet to be validated. The model supplies no autopilot.
