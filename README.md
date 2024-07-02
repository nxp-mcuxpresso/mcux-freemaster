# NXP Middleware FreeMASTER
This repository is for FreeMASTER driver code, enabling communication between [FreeMASTER](https://www.nxp.com/design/design-center/software/development-software/freemaster-run-time-debugging-tool:FREEMASTER) desktop or FreeMASTER Lite tools and MCU application,  
supporting various physical interfaces e.g. Serial, CAN, USB, Network and BDM/JTAG.

## Documentation
User guide is provided to introduce more details of the FreeMASTER driver.

## License
This repository is under **LA_OPT_Online Code Hosting NXP_Software_License**.

## Examples
FreeMASTER examples demonstrate use of [FreeMASTER desktop](https://www.nxp.com/design/design-center/software/development-software/freemaster-run-time-debugging-tool:FREEMASTER) tool to visualize internal variables, control the application flow by modifying variables  
and show use of advanced FreeMASTER features like TSA tables, Recorders and Pipes.  
Check repository [mcux-sdk-examples](https://github.com/nxp-mcuxpresso/mcux-sdk-examples) for specific board and available FreeMASTER examples.

- **fmstr_example_any**
  - FreeMASTER example fully configured by [MCUXpresso ConfigTools](https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/mcuxpresso-config-tools-pins-clocks-and-peripherals:MCUXpresso-Config-Tools).  
    Serial communication is used by default, but it can be changed easily to CAN or other  
    in the MCUXpresso Peripheral Tool. Also FreeMASTER driver features are configured  
    graphically in this tool. The Pins and Clock Tool are used to configure pin  
    multiplexers and clocks.

- **fmstr_example_can**
  - FreeMASTER example using the CAN bus to communicate with target microcontroller.

- **fmstr_example_eonce**
  - FreeMASTER example using DSC JTAG-EOnCE Realtime Data Transfer unit to communicate with target microcontroller.

- **fmstr_example_net**
  - FreeMASTER example using TCP/UDP socket communication with the target microcontroller.

- **fmstr_example_pdbdm**
  - FreeMASTER example using a special packet-driven protocol on top of JTAG or BDM direct memory access.

- **fmstr_example_rtt**
  - FreeMASTER example using Segger RTT communication over J-Link interface.

- **fmstr_example_uart**
  - FreeMASTER example using Serial-UART communication with the target microcontroller.

- **fmstr_example_usb_cdc**
  - FreeMASTER example using virtual serial communication at USB port and CDC VCOM class.

- **fmstr_example_wifi**
  - FreeMASTER example using WiFi network and TCP/UDP socket communication with the target microcontroller.
