# Haptic_Shoes_Synthetic_Effects
The Raspberry Pi (RPi) side code needed to render the haptic effects for the Haptic Shoes with VR Display project.
The repository includes the driver software for the 'Time of Flight (ToF)' sensor and the pruredata patches used to generate the haptic effects.
## How to run the code
1) After powering the RPi on make sure there are no `pd-lork` processes running. This can be checked by running the `ps -aux` command. If there is one running make sure to kill it as this may cause an error when activating the DSP audio channel in the later steps.

3) Connect the RPi to a network and pull the repository.
4) Run the command `pd Combined.pd`.
5) Under `Media` from the toolbar, click on `DSP on`. If an error appears on the console, make sure you followed step 1 and there is no `pd-lork` process running in the background.
6) Set the inlet values to the `~osc` blocks to around 255 Hz. (This can be done by clicking and dragging the mouse upwards.)
7) Start all of the `metro` blocks by clicking on the `1` message blocks going into them. (They can be turned off by clicking the `0` message blocks.)
8) Open another terminal and run the command `python3 sensor.py`.
9) Move your foot up and down, you should feel some vibrations.

NOTE: Aggresive foot movements may cause the ToF sensor to lose connection with the RPi, if this happens, restart the `sensor.py` script by running the same command in step 7.
