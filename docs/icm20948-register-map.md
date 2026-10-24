# ICM-20948 Register Map Notes

My notes from the Phase 0 register-map study. Not exhaustive, just the
registers this project uses.

**Sources:**
- SparkFun's `ICM_20948_REGISTERS.h` (a C translation of the datasheet
  register map). If anything disagrees, the TDK datasheet (DS-000189 v1.5)
  wins.
- AK09916 address and access modes cross-checked against SFU's CMPT433
  course notes and the SparkFun forum.

## Bank switching

The ICM-20948 pages most of its registers across four user banks. One
register — `REG_BANK_SEL`, address **0x7F** — is bank-independent and always
selects which bank the rest of the address space currently refers to.

```
REG_BANK_SEL (0x7F)
  bits [5:4] = USER_BANK (0-3)
```

**Every register access writes `REG_BANK_SEL` first.** The driver never
assumes the bank left over from the previous access (see
[CLAUDE.md](../CLAUDE.md#standing-constraints)).

## Bank 0 — data & top-level control

| Register | Addr | Purpose |
|---|---|---|
| `WHO_AM_I` | 0x00 | Should read `0xEA` — sanity check on bring-up |
| `USER_CTRL` | 0x03 | `I2C_MST_EN`, `DMP_EN` (must stay 0 — see constraints), `I2C_IF_DIS` |
| `LP_CONFIG` | 0x05 | Low-power cycle config (gyro/accel/I2C master duty cycling) |
| `PWR_MGMT_1` | 0x06 | `DEVICE_RESET`, `SLEEP`, `LP_EN`, `CLKSEL` — clock source select |
| `PWR_MGMT_2` | 0x07 | Per-axis gyro/accel enable/disable |
| `INT_PIN_CFG` | 0x0F | `BYPASS_EN` — one of the two paths to the AK09916 (see below) |
| `INT_ENABLE` | 0x10 | `PLL_READY_EN`, `WOM_INT_EN`, etc. |
| `INT_ENABLE_1` | 0x11 | `RAW_DATA_0_RDY_EN` — data-ready interrupt |
| `INT_STATUS` | 0x19 | Mirrors `INT_ENABLE` sources |
| `INT_STATUS_1` | 0x1A | `RAW_DATA_0_RDY_INT` |
| `ACCEL_XOUT_H`…`ACCEL_ZOUT_L` | 0x2D–0x32 | Raw accel, big-endian 16-bit pairs |
| `GYRO_XOUT_H`…`GYRO_ZOUT_L` | 0x33–0x38 | Raw gyro, big-endian 16-bit pairs |
| `TEMP_OUT_H` / `TEMP_OUT_L` | 0x39–0x3A | Raw die temperature |
| `EXT_PERIPH_SENS_DATA_00..23` | 0x3B–0x52 | Where I2C-master-mode peripheral reads (e.g. AK09916 via bank 3) land |

`ACCEL_XOUT_H` through `TEMP_OUT_L` is one contiguous 14-byte block, so a
single burst read from 0x2D gets all six axes plus temperature.

## Bank 1 — self-test & accel offsets

| Register | Addr | Purpose |
|---|---|---|
| `SELF_TEST_X/Y/Z_GYRO` | 0x02–0x04 | Factory self-test data |
| `SELF_TEST_X/Y/Z_ACCEL` | 0x0E–0x10 | Factory self-test data |
| `XA_OFFS_H/L`, `YA_OFFS_H/L`, `ZA_OFFS_H/L` | 0x14–0x1B | Factory accel offset trim (not used; calibration is done offline and applied in software) |

## Bank 2 — gyro/accel config & sample rate

| Register | Addr | Purpose |
|---|---|---|
| `GYRO_SMPLRT_DIV` | 0x00 | Gyro output data rate divider |
| `GYRO_CONFIG_1` | 0x01 | `GYRO_FS_SEL` (±250/500/1000/2000 dps), `GYRO_DLPFCFG`, `GYRO_FCHOICE` |
| `GYRO_CONFIG_2` | 0x02 | Gyro self-test enables, averaging |
| `ODR_ALIGN_EN` | 0x09 | Aligns accel/gyro/temp ODRs |
| `ACCEL_SMPLRT_DIV_1/2` | 0x10–0x11 | Accel output data rate divider (12-bit, split across two registers) |
| `ACCEL_INTEL_CTRL` | 0x12 | Wake-on-motion logic |
| `ACCEL_CONFIG` | 0x14 | `ACCEL_FS_SEL` (±2/4/8/16 g), `ACCEL_DLPFCFG`, `ACCEL_FCHOICE` |
| `ACCEL_CONFIG_2` | 0x15 | Accel self-test enables |

## Bank 3 — I2C master (for the AK09916)

| Register | Addr | Purpose |
|---|---|---|
| `I2C_MST_ODR_CONFIG` | 0x00 | I2C master output data rate |
| `I2C_MST_CTRL` | 0x01 | Master clock speed, stop/restart behavior |
| `I2C_MST_DELAY_CTRL` | 0x02 | Per-peripheral read delay enables |
| `I2C_PERIPH0_ADDR/REG/CTRL/DO` | 0x03–0x06 | Configures peripheral-0 transactions (this is how the ICM-20948 auto-reads the AK09916 into `EXT_PERIPH_SENS_DATA_*` on Bank 0) |

## Magnetometer (AK09916, internal to the ICM-20948 package)

The AK09916 is a separate die with its own register map and its own I2C
address, **0x0C**. It is not one of the four banked regions above — it's
reached one of two ways:

1. **I2C master mode** (`USER_CTRL.I2C_MST_EN = 1`): the ICM-20948 itself
   polls the AK09916 over an internal I2C bus and deposits the result in
   Bank 0's `EXT_PERIPH_SENS_DATA_00..`. Configured via the Bank 3
   `I2C_PERIPH0_*` registers above.
2. **Bypass mode** (`INT_PIN_CFG.BYPASS_EN = 1`): the ICM-20948's I2C lines
   pass straight through to the AK09916, and the host addresses it directly
   at 0x0C.

| Register | Addr | Purpose |
|---|---|---|
| `WIA2` | 0x01 | Device ID, should read `0x09` |
| `ST1` | 0x10 | `DRDY` — data ready flag |
| `HXL`/`HXH`, `HYL`/`HYH`, `HZL`/`HZH` | 0x11–0x16 | Raw mag axes, little-endian (opposite byte order from the accel/gyro registers!) |
| `ST2` | 0x18 | Must be read after each measurement to latch/release the data registers; also carries `HOFL` (overflow) |
| `CNTL2` | 0x31 | Operating mode (power-down / single-shot / continuous rates) |
| `CNTL3` | 0x32 | `SRST` — soft reset |

**Open question:** which access mode to use. I2C master mode puts the mag
data in the same Bank 0 burst read (one transaction per sample) but needs
Bank 3 setup. Bypass mode is a simpler driver but adds a second, separately
timed read per sample. Leaning toward master mode; will decide at mag
bring-up.

## Verification plan

First check is `WHO_AM_I` (expect `0xEA`, confirmed), then `WIA2` (expect
`0x09`) once the mag path is chosen. Both confirm bank switching and I2C
framing before any config writes.
