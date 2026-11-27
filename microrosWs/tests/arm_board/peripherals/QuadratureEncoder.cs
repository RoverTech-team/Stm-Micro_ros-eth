// Optical incremental encoder follower (arm main board, ENC/1 + ENC/2).
//
// PCB truth (docs/pcb_elements.md): 1000 PPR -> 4000 counts/rev with x4
// decode; 5 populated (J6 empty, power-only). A/B in quadrature, Z index
// once per revolution. Firmware v2 samples GPIO
// (Encoder_Test_SampleAll); TIM-quadrature coupling is future work,
// so A/B/Z are live GPIO outputs plus an MMIO position window:
//
//   0x00 POS   (R/W, signed 32, counts)
//   0x04 CTRL  (R/W, bit0 ENABLE drives A/B/Z, bit1 WRAP enables modulo)
//   0x08 STEPS_REV (R/W, default 4000)
//
// Monitor/test hooks: `encN Advance <steps>`, `encN Zero`.

using System;

using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Sensors
{
    public class QuadratureEncoder : BasicDoubleWordPeripheral, IKnownSize
    {
        public QuadratureEncoder(IMachine machine, int stepsPerRevolution = 4000) : base(machine)
        {
            StepsPerRevolution = stepsPerRevolution;
            A = new GPIO();
            B = new GPIO();
            Z = new GPIO();
            Reset();
        }

        public long Size => 0x1000;
        public int StepsPerRevolution { get; set; }

        public GPIO A { get; }
        public GPIO B { get; }
        public GPIO Z { get; }

        public override void Reset()
        {
            position = 0;
            enabled = true;
            wrap = false;
            UpdateOutputs();
        }

        public void Advance(int steps)
        {
            position += steps;
            Normalize();
            UpdateOutputs();
        }

        public void Zero()
        {
            position = 0;
            UpdateOutputs();
        }

        public long GetPosition()
        {
            return position;
        }

        public override uint ReadDoubleWord(long offset)
        {
            switch (offset)
            {
                case 0x00: return (uint)(position & 0xFFFFFFFF);
                case 0x04: return (uint)((enabled ? 1 : 0) | (wrap ? 2 : 0));
                case 0x08: return (uint)StepsPerRevolution;
                default: this.Log(LogLevel.Warning, "Unhandled read offset 0x{0:X}", offset); return 0;
            }
        }

        public override void WriteDoubleWord(long offset, uint value)
        {
            switch (offset)
            {
                case 0x00: position = (int)value; Normalize(); UpdateOutputs(); break;
                case 0x04: enabled = (value & 1) != 0; wrap = (value & 2) != 0; UpdateOutputs(); break;
                case 0x08: StepsPerRevolution = (int)value; Normalize(); UpdateOutputs(); break;
                default: this.Log(LogLevel.Warning, "Unhandled write offset 0x{0:X}", offset); break;
            }
        }

        private void Normalize()
        {
            if (wrap && StepsPerRevolution > 0)
            {
                position %= StepsPerRevolution;
                if (position < 0) position += StepsPerRevolution;
            }
        }

        private void UpdateOutputs()
        {
            if (!enabled)
            {
                A.Set(false); B.Set(false); Z.Set(false);
                return;
            }
            // Gray-code quadrature: state = pos mod 4 -> (A,B).
            var s = ((position % 4) + 4) % 4;
            A.Set(s == 1 || s == 2);
            B.Set(s == 2 || s == 3);
            Z.Set(StepsPerRevolution > 0 && position % StepsPerRevolution == 0);
        }

        private long position;
        private bool enabled;
        private bool wrap;
    }
}
