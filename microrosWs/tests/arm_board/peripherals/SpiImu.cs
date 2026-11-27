// SPI IMU stub (arm main board, IMU_SPI J22: CS1/CS2/SCK/MOSI/MISO).
//
// PCB truth (docs/pcb_elements.md): 2x IMU on RJ50 (left IMU1, right
// IMU2), CS1->IMU1, CS2->IMU2, IMU2_NENABLE hard LOW, SPI <= 10 MHz,
// MISO (only input) must be FT. No BRINGUP firmware support yet, so
// this is a bring-up-ready stub: WHO_AM_I + accel/gyro data regs +
// data-ready flag, feedable from the monitor for future driver work.
//
// Register map (MPU-60X0-style, the common denominator):
//   0x75 WHO_AM_I (R, default 0x68, settable via `whoAmI` attribute)
//   0x3B-0x40 ACCEL_XOUT_H..ZOUT_L, 0x43-0x48 GYRO_* (R)
//   0x3A INT_STATUS bit0 DATA_RDY (R, set after FeedAccel)
// SPI: bit7 of first byte = read, 7-bit address, then data bytes.
// Monitor hooks: `imuN FeedAccel <ax> <ay> <az> [gx] [gy] [gz]`
// (int16 raw), `imuN WhoAmI`.

using System;

using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.SPI;

namespace Antmicro.Renode.Peripherals.Sensors
{
    public class SpiImu : ISPIPeripheral, IKnownSize
    {
        public SpiImu(IMachine machine, int whoAmI = 0x68)
        {
            WhoAmI = (byte)whoAmI;
            Reset();
        }

        public long Size => 0;
        public byte WhoAmI { get; set; }

        public void Reset()
        {
            phase = Phase.Addr;
            read = false;
            addr = 0;
            dataReady = false;
            Array.Clear(accel, 0, accel.Length);
            Array.Clear(gyro, 0, gyro.Length);
        }

        public byte Transmit(byte data)
        {
            switch (phase)
            {
                case Phase.Addr:
                    read = (data & 0x80) != 0;
                    addr = (byte)(data & 0x7F);
                    phase = Phase.Data;
                    return 0x00;
                default:
                    var ret = read ? ReadReg(addr) : (byte)0x00;
                    if (!read) WriteReg(addr, data);
                    addr++;
                    return ret;
            }
        }

        public void FinishTransmission()
        {
            phase = Phase.Addr;
        }

        public void FeedAccel(int ax, int ay, int az, int gx = 0, int gy = 0, int gz = 0)
        {
            accel[0] = (short)ax; accel[1] = (short)ay; accel[2] = (short)az;
            gyro[0] = (short)gx; gyro[1] = (short)gy; gyro[2] = (short)gz;
            dataReady = true;
        }

        public byte WhoAmIValue()
        {
            return WhoAmI;
        }

        private byte ReadReg(byte a)
        {
            if (a == 0x75) return WhoAmI;
            if (a == 0x3A) return dataReady ? (byte)0x01 : (byte)0x00;
            if (a >= 0x3B && a <= 0x40) return AxisByte(accel, a - 0x3B);
            if (a >= 0x43 && a <= 0x48) return AxisByte(gyro, a - 0x43);
            return 0x00;
        }

        private void WriteReg(byte a, byte v)
        {
            // Stub: control regs accepted, no behavior (log power-mgmt writes).
            if (a == 0x6B) this.Log(LogLevel.Debug, "IMU PWR_MGMT_1 <- 0x{0:X2}", v);
        }

        private static byte AxisByte(short[] v, int off)
        {
            var val = v[off / 2];
            return (off % 2) == 0 ? (byte)(val >> 8) : (byte)(val & 0xFF);
        }

        private enum Phase { Addr, Data }

        private Phase phase;
        private bool read;
        private byte addr;
        private bool dataReady;
        private readonly short[] accel = new short[3];
        private readonly short[] gyro = new short[3];
    }
}
