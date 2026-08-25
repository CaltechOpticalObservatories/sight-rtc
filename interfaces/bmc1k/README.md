# BMC 1k interface

## Install

Run
```bash
./install.sh
```

This will
- Compile the `hwint-bmc1k` executable

Required files:
- bmc_mdlib.h     # From BMC SDK
- ImageStreamIO.h # From MILK
- hvaluts.h       # From SIGHT_RTC github

## Executable `hwint-bmc1k`:

To run loop, execute :

```bash
./hwint-bmc1k -K      # initialize board for 1k DM
./hwint-bmc1k -D 0    # set driver spin delay count to 0
./hwint-bmc1k -L      # real time control loop
```

Or all in one:
```bash
./hwint-bmc1k -A      # (equiv -K, -D 0, -L)
```
If sudo permission required, make sure sudo inherits LD_LIBRARY_PATH to find libimagestreamio:
```bash
sudo -E env LD_LIBRARY_PATH=${LD_LIBRARY_PATH} ./hwint-bmc1k -K
```
