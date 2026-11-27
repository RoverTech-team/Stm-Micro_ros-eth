// 6x powerSTEP01 stepper-driver daisy chain (arm main board, DRIVE_SPI).
//
// Wire truth (see docs/pcb_elements.md): per SPI byte the CM4 firmware
// clocks 6 bytes with the target at [5-active] and NOP (0x00) elsewhere,
// CS toggled per byte. This model collects one 6-byte frame per CS
// assertion and answers each slot from that device's reply FIFO on the
// NEXT frame (pipelined, like the real shift register).
//
// Implemented command subset (cfg_structs.h opcodes):
//   SET_PARAM (0x00-0x1F, byte lengths as in ps01.c), GET_PARAM (0x20-0x3F),
//   MOVE (0x40/0x41)+3B, RUN (0x50/0x51)+3B, GOTO (0x60)+3B,
//   GOTO_DIR (0x68/0x69)+3B, GO_HOME (0x70), GO_MARK (0x78),
//   SOFT/HARD_STOP (0xB0/0xB8), RESET_POS (0xD8), RESET_DEVICE (0xC0),
//   GET_STATUS (0xD0)+2 reply bytes.
// Motion: ABS_POS integrates toward target on every frame
// (StepsPerFrame, default 200). STATUS: HiZ=0/UVLO=0 healthy;
// FaultBlownFuse() latches UVLO+HiZ (2026-09-05 outage), cleared by
// the firmware Motor_RecoverDriver path (SetAlarms re-apply).

using System;
using System.Collections.Generic;

using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.SPI;

namespace Antmicro.Renode.Peripherals.Actuators
{
    public class PowerStep01Chain : ISPIPeripheral, IKnownSize
    {
        public PowerStep01Chain(IMachine machine, int numberOfDrivers = 6)
        {
            NumberOfDrivers = numberOfDrivers;
            devices = new DriverState[NumberOfDrivers];
            for (var i = 0; i < NumberOfDrivers; i++)
            {
                devices[i] = new DriverState();
            }
            Reset();
        }

        public long Size => 0;
        public int NumberOfDrivers { get; }
        public int StepsPerFrame { get; set; } = 200;

        public void Reset()
        {
            lock (sync)
            {
                frame = new List<byte>();
                foreach (var d in devices)
                {
                    d.Reset();
                }
            }
        }

        public byte Transmit(byte data)
        {
            lock (sync)
            {
                frame.Add(data);
                // Reply slot = device index in this frame position.
                var slot = (frame.Count - 1) % NumberOfDrivers;
                return devices[slot].DequeueReply();
            }
        }

        public void FinishTransmission()
        {
            lock (sync)
            {
                // One logical byte per device per CS assertion.
                for (var i = 0; i + NumberOfDrivers <= frame.Count; i += NumberOfDrivers)
                {
                    for (var slot = 0; slot < NumberOfDrivers; slot++)
                    {
                        devices[slot].Consume(frame[i + slot]);
                    }
                }
                frame.Clear();
                foreach (var d in devices)
                {
                    d.Integrate(StepsPerFrame);
                }
            }
        }

        // Test/fault hooks (monitor: `drivers FaultBlownFuse`, `drivers ClearFaults`).
        public void FaultBlownFuse()
        {
            lock (sync)
            {
                foreach (var d in devices)
                {
                    d.Uvlo = true;
                    d.HiZ = true;
                    d.Moving = false;
                }
                this.Log(LogLevel.Warning, "FAULT: 24V fuse blown (UVLO+HiZ latched on all drivers)");
            }
        }

        public void ClearFaults()
        {
            lock (sync)
            {
                foreach (var d in devices)
                {
                    d.Uvlo = false;
                }
            }
        }

        public int GetAbsPos(int index)
        {
            lock (sync) { return devices[index].AbsPos; }
        }

        public ushort GetStatus(int index)
        {
            lock (sync) { return devices[index].StatusWord(); }
        }

        private readonly object sync = new object();
        private DriverState[] devices;
        private List<byte> frame;

        private class DriverState
        {
            public int AbsPos;
            public int Target;
            public bool Moving;
            public bool HiZ = true; // power-on reset state; firmware config brings drivers live
            public bool Uvlo;
            public int Dir = 1;

            private readonly Queue<byte> replies = new Queue<byte>();
            private int pendingArgBytes;
            private uint argAcc;
            private byte pendingOp;

            public void Reset()
            {
                AbsPos = 0; Target = 0; Moving = false;
                HiZ = true; Uvlo = false; Dir = 1;
                replies.Clear(); pendingArgBytes = 0; argAcc = 0;
            }

            public byte DequeueReply()
            {
                return replies.Count > 0 ? replies.Dequeue() : (byte)0x00;
            }

            public ushort StatusWord()
            {
                // STATUS[15:0]: HiZ(0) DIR(1) MOT_STATUS(2-4) SW_EVN(5) SW_F(6)
                // BUSY(7) UVLO(8) UVLO_ADC(9) TH_SD(10) TH_WRN(11) OCD(13).
                ushort s = 0;
                if (HiZ) s |= 0x0001;
                if (Dir < 0) s |= 0x0002;
                s |= (ushort)(Moving ? (0x02 << 2) : (0x00 << 2)); // const-speed vs stopped
                if (!Moving) s |= 0x0080; // BUSY bit reads 1 when idle on powerSTEP01
                if (Uvlo) s |= 0x0100;
                return s;
            }

            public void Integrate(int step)
            {
                if (!Moving || HiZ || Uvlo) return;
                var delta = Target - AbsPos;
                if (delta == 0) { Moving = false; return; }
                var s = Math.Sign(delta) * Math.Min(Math.Abs(delta), step);
                AbsPos += s;
                if (AbsPos == Target) Moving = false;
            }

            public void Consume(byte b)
            {
                if (pendingArgBytes > 0)
                {
                    argAcc = (argAcc << 8) | b;
                    if (--pendingArgBytes == 0) ApplyArgs(pendingOp, argAcc);
                    return;
                }
                if (b == 0x00) return; // NOP (idle slots in chain frames)
                if (b == 0xD0) // GET_STATUS
                {
                    var s = StatusWord();
                    replies.Enqueue((byte)(s >> 8));
                    replies.Enqueue((byte)(s & 0xFF));
                    // Reading status clears latched alarms (firmware recovery path).
                    Uvlo = false;
                    return;
                }
                if ((b & 0xE0) == 0x00 && b != 0x00) { BeginArgs(b, ParamLength(b & 0x1F)); return; } // SET_PARAM
                if ((b & 0xE0) == 0x20) // GET_PARAM
                {
                    foreach (var rb in GetParamBytes(b & 0x1F)) replies.Enqueue(rb);
                    return;
                }
                switch (b & 0xFE)
                {
                    case 0x40: BeginArgs(b, 3); return; // MOVE
                    case 0x50: BeginArgs(b, 3); return; // RUN
                    case 0x68: BeginArgs(b, 3); return; // GOTO_DIR
                    default: break;
                }
                switch (b)
                {
                    case 0x60: BeginArgs(b, 3); return; // GOTO
                    case 0x70: Target = 0; Moving = true; HiZ = false; return; // GO_HOME
                    case 0x78: Target = 0; Moving = true; HiZ = false; return; // GO_MARK
                    case 0xB0: case 0xB8: Moving = false; return; // STOP
                    case 0xD8: AbsPos = 0; Target = 0; Moving = false; return; // RESET_POS
                    case 0xC0: Reset(); HiZ = true; return; // RESET_DEVICE
                    default: return; // unmodeled motion op: ignore
                }
            }

            private void BeginArgs(byte op, int n)
            {
                pendingOp = op; argAcc = 0; pendingArgBytes = n;
            }

            private void ApplyArgs(byte op, uint v)
            {
                if (op == 0x01) { AbsPos = SignExtend22(v); Target = AbsPos; Moving = false; return; } // ABS_POS set
                if ((op & 0xFE) == 0x40) // MOVE dir + N steps
                {
                    Dir = (op & 1) != 0 ? 1 : -1;
                    Target = AbsPos + Dir * (int)(v & 0x3FFFFF);
                    Moving = true; HiZ = false;
                    return;
                }
                if ((op & 0xFE) == 0x50) { Dir = (op & 1) != 0 ? 1 : -1; Moving = true; HiZ = false; return; } // RUN
                if (op == 0x60) { Target = SignExtend22(v); Dir = Target >= AbsPos ? 1 : -1; Moving = true; HiZ = false; return; } // GOTO
                if ((op & 0xFE) == 0x68) { Target = SignExtend22(v); Dir = (op & 1) != 0 ? 1 : -1; Moving = true; HiZ = false; return; }
                // Other SET_PARAM writes: accepted, no behavioral effect in v1.
            }

            private IEnumerable<byte> GetParamBytes(int param)
            {
                if (param == 0x01) // ABS_POS 22 bit
                {
                    var v = (uint)(AbsPos & 0x3FFFFF);
                    yield return (byte)(v >> 16); yield return (byte)(v >> 8); yield return (byte)v;
                    yield break;
                }
                if (param == 0x19) // STATUS mirror (some stacks poll it as param)
                {
                    var s = StatusWord();
                    yield return (byte)(s >> 8); yield return (byte)(s & 0xFF);
                    yield break;
                }
                yield return 0x00;
            }

            private static int ParamLength(int param)
            {
                switch (param)
                {
                    case 0x01: case 0x03: return 3; // ABS_POS, MARK (22 bit)
                    case 0x02: return 2;            // EL_POS (9 bit)
                    case 0x05: case 0x06: return 2; // ACC/DEC (12 bit)
                    case 0x07: return 2;            // MAX_SPEED (10 bit)
                    case 0x08: return 2;            // MIN_SPEED (12 bit)
                    case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: return 1; // KVALs
                    case 0x0E: case 0x0F: return 2; // INT_SPEED, ST_SLP (13 bit)
                    case 0x10: case 0x11: return 2; // FN_SLP_*
                    case 0x12: return 1;            // K_THERM
                    case 0x13: return 1;            // ADC_OUT (read-only)
                    case 0x14: return 1;            // OCD_TH (5 bit)
                    case 0x15: return 1;            // STALL_TH
                    case 0x16: case 0x17: return 2; // FS_SPD, STEP_MODE
                    case 0x18: return 1;            // ALARM_EN
                    case 0x19: return 2;            // STATUS (read-only)
                    case 0x1A: case 0x1B: return 2; // GATECFG
                    default: return 1;
                }
            }

            private static int SignExtend22(uint v)
            {
                v &= 0x3FFFFF;
                return (v & 0x200000) != 0 ? (int)(v | 0xFFC00000) : (int)v;
            }
        }
    }
}
