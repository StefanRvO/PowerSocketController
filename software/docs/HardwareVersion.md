This document describes how the different hardware verisions are detected, and the rules for changing the version

Why versioning the hardware?
- We want all future software to be backout compatible with older hardware. To archive this, the software detect which hardware version is running very early in the boot process. Based on the hardware version, initialisation may happen in different ways.

When should the hardware version be bumped?
We should ONLY change the hardware version if software changes are needed to accomedate the changes. If no software changes are needed, it is not a new hardware version, but rather a new board revision.

How do we detect the version?
Currently, the hardware version is determined based on a single gpio pin, which can either be LOW, floating or HIGH. For now, this pin should only be changed if the other ways (listed below) of reading the hardware version will not be possible in future revisions.

Appart from the gpio pin, the hardware version is detected based on the addresses of the devices on the I2C bus. We can itterate through all 127 addresses and determine which of the requests are responded with an ACK.

The I2C gpio expanders used currently can have eight different addresses (determined by 3 pins). By changing the addresses, the hardware version is changed. the following rules apply:

- The address for the device on the logic board is higher than the one on the power board.
The following versioning scheme apply:
| Hardware version | Address 1 | Address 2 |
|         1        |    0x20   |    0x21   |
|         2        |    0x20   |    0x22   |
|         3        |    0x21   |    0x22   |
|         4        |    0x20   |    0x23   |
|         5        |    0x21   |    0x23   |
|         6        |    0x22   |    0x23   |
|         7        |    0x20   |    0x24   |
|         8        |    0x21   |    0x24   |
|         9        |    0x22   |    0x24   |
|         10       |    0x23   |    0x24   |
|         11       |    0x20   |    0x25   |
|         12       |    0x21   |    0x25   |
|         13       |    0x22   |    0x25   |
|         14       |    0x23   |    0x25   |
|         15       |    0x24   |    0x25   |
|         16       |    0x20   |    0x26   |
|         17       |    0x21   |    0x26   |
|         18       |    0x22   |    0x26   |
|         19       |    0x23   |    0x26   |
|         20       |    0x24   |    0x26   |
|         21       |    0x25   |    0x26   |
|         22       |    0x20   |    0x27   |
|         23       |    0x21   |    0x27   |
|         24       |    0x22   |    0x27   |
|         25       |    0x23   |    0x27   |
|         26       |    0x24   |    0x27   |
|         27       |    0x25   |    0x27   |
|         28       |    0x26   |    0x27   |
