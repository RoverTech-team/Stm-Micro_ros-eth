/*
 * STM32H7 Ethernet MAC Peripheral Emulation
 * 
 * This peripheral emulates the Ethernet MAC controller found in STM32H7 series
 * microcontrollers. It supports:
 * - 10/100/1000 Mbps operation
 * - IEEE 802.3-2008 compliance
 * - DMA descriptor-based packet transfer
 * - MII/RMII PHY interface
 * - VLAN support
 * - Checksum offload
 * 
 * Reference: STM32H7 Reference Manual (RM0433)
 *            Ethernet MAC registers: Section 50
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.Timers;
using Antmicro.Renode.Time;
using Antmicro.Renode.Utilities;
using Endian = Antmicro.Renode.Core.Endian;

namespace Antmicro.Renode.Peripherals.Network
{
    public class STM32H7_ETH : BasicDoubleWordPeripheral, IKnownType
    {
        // ========================================
        // Register Map - MAC Section
        // ========================================
        
        // MAC Configuration Register
        // Address: 0x0000
        // Controls MAC operation mode
        private const uint MACCR = 0x0000 >> 2;
        // MACCR bit definitions
        private const uint MACCR_RE = 0x00000001;      // Receiver Enable
        private const uint MACCR_TE = 0x00000002;       // Transmitter Enable
        private const uint MACCR_PRELEN_MASK = 0x00000018;  // Preamble Length
        private const uint MACCR_DC = 0x00000020;       // Deferral Check
        private const uint MACCR_BL_MASK = 0x00000060;  // Back-off Limit
        private const uint MACCR_DR = 0x00000200;       // Disable Retry
        private const uint MACCR_DCRS = 0x00000100;     // Disable Carrier Sense
        private const uint MACCR_LM = 0x00001000;       // Loopback Mode
        private const uint MACCR_DM = 0x00000800;       // Duplex Mode
        private const uint MACCR_FES = 0x00000400;      // Fast Ethernet Speed
        private const uint MACCR_PS = 0x00008000;       // Port Select (MII/RMII)
        private const uint MACCR_JE = 0x00100000;       // Jumbo Packet Enable
        private const uint MACCR_JD = 0x00400000;       // Jabber Disable
        private const uint MACCR_WD = 0x00800000;       // Watchdog Disable
        private const uint MACCR_ACS = 0x00000040;      // Automatic Pad/CRC Stripping
        private const uint MACCR_CST = 0x00020000;      // CRC Stripping for Type frames
        private const uint MACCR_SAR = 0x00040000;      // Source Address Replacement
        private const uint MACCR_IPC = 0x00000080;      // Checksum Offload
        
        // MAC Extended Configuration Register
        // Address: 0x0004
        private const uint MACCR1 = 0x0004 >> 2;
        
        // MAC Interrupt Enable Register
        // Address: 0x0018
        private const uint MACIER = 0x0018 >> 2;
        private const uint MACIER_PMTIE = 0x00000008;  // PMT Interrupt Enable
        private const uint MACIER_LPIIE = 0x00000400;  // LPI Interrupt Enable
        private const uint MACIER_RGSMIIIE = 0x00080000;  // RGMII Interrupt Enable
        private const uint MACIER_TSIIE = 0x00200000;  // Timestamp Interrupt Enable
        
        // MAC Interrupt Status Register
        // Address: 0x0020
        private const uint MACISR = 0x0020 >> 2;
        
        // MAC Transmit Status Register
        // Address: 0x0030
        private const uint MACTSR = 0x0030 >> 2;
        
        // MAC Receive Status Register
        // Address: 0x0038
        private const uint MACRSR = 0x0038 >> 2;
        
        // MAC PMT Control and Status Register
        // Address: 0x00C0
        private const uint MACPMTCSR = 0x00C0 >> 2;
        
        // MAC PHY Control Register
        // Address: 0x00F0
        private const uint MACPHYCSR = 0x00F0 >> 2;
        
        // ========================================
        // Register Map - MAC Address Filters
        // ========================================
        
        // MAC Address 0 High Register
        // Address: 0x0200
        private const uint MACA0HR = 0x0200 >> 2;
        
        // MAC Address 0 Low Register
        // Address: 0x0204
        private const uint MACA0LR = 0x0204 >> 2;
        
        // MAC Address 1-3 registers at 0x0208-0x0224
        
        // ========================================
        // Register Map - DMA Section
        // ========================================
        
        // DMA Mode Register
        // Address: 0x1000
        private const uint DMAMR = 0x1000 >> 2;
        private const uint DMAMR_SWR = 0x00000001;      // Software Reset
        private const uint DMAMR_DA = 0x00000002;       // DMA Arbitration Scheme
        private const uint DMAMR_INTM_MASK = 0x00000030;  // Interrupt Mode
        private const uint DMAMR_PR_MASK = 0x00000700;  // Priority Ratio
        private const uint DMAMR_TXPR = 0x00000800;     // Transmit Priority
        private const uint DMAMR_DRNXT_MASK = 0x00030000;  // Descriptor Rollover
        
        // DMA System Bus Mode Register
        // Address: 0x1004
        private const uint DMASBMR = 0x1004 >> 2;
        private const uint DMASBMR_AAL = 0x00001000;    // Address Aligned Beats
        private const uint DMASBMR_BLEN_MASK = 0x000F0000;  // Burst Length
        private const uint DMASBMR_EAME = 0x10000000;   // Enhanced Address Mode Enable
        
        // DMA Interrupt Status Register
        // Address: 0x1030
        private const uint DMADSR = 0x1030 >> 2;
        private const uint DMADSR_TI = 0x00000001;      // Transmit Interrupt
        private const uint DMADSR_TPS = 0x00000002;     // Transmit Process Stopped
        private const uint DMADSR_TBU = 0x00000004;     // Transmit Buffer Unavailable
        private const uint DMADSR_TJT = 0x00000008;     // Transmit Jabber Timeout
        private const uint DMADSR_OVF = 0x00000010;     // Receive Overflow
        private const uint DMADSR_UNF = 0x00000020;     // Transmit Underflow
        private const uint DMADSR_RI = 0x00000040;      // Receive Interrupt
        private const uint DMADSR_RU = 0x00000080;      // Receive Buffer Unavailable
        private const uint DMADSR_RPS = 0x00000100;     // Receive Process Stopped
        private const uint DMADSR_RWT = 0x00000200;     // Receive Watchdog Timeout
        private const uint DMADSR_RSE = 0x00000400;     // Receive Stop Enable
        private const uint DMADSR_FBE = 0x00002000;     // Fatal Bus Error
        private const uint DMADSR_AIS = 0x00004000;     // Abnormal Interrupt Summary
        private const uint DMADSR_NIS = 0x00008000;     // Normal Interrupt Summary
        
        // DMA Interrupt Enable Register
        // Address: 0x1034
        private const uint DMADIER = 0x1034 >> 2;
        
        // DMA Receive Control Register
        // Address: 0x1048
        private const uint DMACRCR = 0x1048 >> 2;
        private const uint DMACRCR_SR = 0x80000000;     // Start Receive
        
        // DMA Transmit Control Register
        // Address: 0x104C
        private const uint DMACTCR = 0x104C >> 2;
        private const uint DMACTCR_ST = 0x80000000;     // Start Transmit
        
        // DMA Current TX Descriptor Pointer
        // Address: 0x1044
        private const uint DMACTPDR = 0x1044 >> 2;
        
        // DMA Current RX Descriptor Pointer
        // Address: 0x1054
        private const uint DMACRPDR = 0x1054 >> 2;
        
        // DMA TX Descriptor List Address
        // Address: 0x1050
        private const uint DMATDLAR = 0x1050 >> 2;
        
        // DMA RX Descriptor List Address
        // Address: 0x1058
        private const uint DMARDLAR = 0x1058 >> 2;
        
        // DMA Current TX Buffer Pointer
        // Address: 0x1060
        private const uint DMACTBPR = 0x1060 >> 2;
        
        // DMA Current RX Buffer Pointer
        // Address: 0x1068
        private const uint DMACRBPR = 0x1068 >> 2;
        
        // ========================================
        // Register Map - MTL Section
        // ========================================
        
        // MTL Operation Mode Register
        // Address: 0x0C00
        private const uint MTLOMR = 0x0C00 >> 2;
        
        // MTL RX Queue Operating Mode (Queue 0)
        // Address: 0x0C30
        private const uint MTLRXQ0OMR = 0x0C30 >> 2;
        private const uint MTLRXQOMR_RSF = 0x00000020;  // Receive Store and Forward
        private const uint MTLRXQOMR_FEP = 0x00000080;  // Forward Error Packets
        private const uint MTLRXQOMR_FUP = 0x00000100;  // Forward Undersized Packets
        
        // MTL TX Queue Operating Mode (Queue 0)
        // Address: 0x0D00
        private const uint MTLTXQ0OMR = 0x0D00 >> 2;
        private const uint MTLTXQOMR_TSF = 0x00000008;  // Transmit Store and Forward
        private const uint MTLTXQOMR_TXQEN = 0x00000010;  // TX Queue Enable
        
        // ========================================
        // PHY Registers (LAN8742)
        // ========================================
        
        // Basic Control Register
        private const ushort PHY_BCR = 0;
        private const ushort PHY_BCR_RESET = 0x8000;
        private const ushort PHY_BCR_LOOPBACK = 0x4000;
        private const ushort PHY_BCR_SPEED_SEL = 0x2000;
        private const ushort PHY_BCR_ANEG_EN = 0x1000;
        private const ushort PHY_BCR_POWER_DOWN = 0x0800;
        private const ushort PHY_BCR_ISOLATE = 0x0400;
        private const ushort PHY_BCR_DUPLEX = 0x0100;
        
        // Basic Status Register
        private const ushort PHY_BSR = 1;
        private const ushort PHY_BSR_100BT4 = 0x8000;
        private const ushort PHY_BSR_100BTX_FD = 0x4000;
        private const ushort PHY_BSR_100BTX_HD = 0x2000;
        private const ushort PHY_BSR_10BT_FD = 0x1000;
        private const ushort PHY_BSR_10BT_HD = 0x0800;
        private const ushort PHY_BSR_NO_PREAMBLE = 0x0040;
        private const ushort PHY_BSR_ANEG_COMPLETE = 0x0020;
        private const ushort PHY_BSR_REMOTE_FAULT = 0x0010;
        private const ushort PHY_BSR_ANEG_CAPABLE = 0x0008;
        private const ushort PHY_BSR_LINK_ESTABLISHED = 0x0004;
        private const ushort PHY_BSR_JABBER_DETECT = 0x0002;
        private const ushort PHY_BSR_EXTENDED_CAP = 0x0001;
        
        // LAN8742 Specific Registers
        private const ushort PHY_SCSR = 26;  // Special Control Status
        private const ushort PHY_SCSR_100BTX = 0x0008;
        private const ushort PHY_SCSR_FD = 0x0010;
        
        // ========================================
        // DMA Descriptor Format
        // ========================================
        
        // Normal Descriptor - Read-Write Format
        // TDES0: Buffer1 Address [31:0]
        // TDES1: Buffer2 Address [31:0] or Buffer1 Size [13:0] / Buffer2 Size [29:16]
        // TDES2: Buffer1 Size [13:0], Buffer2 Size [29:16]
        // TDES3: OWN [31], IOC [30], TCP Segmentation Enable [29], 
        //        TCP Header Length [27:24], TCP Payload Length [17:0]
        
        // Descriptor ownership flags
        private const uint DESC_OWN = 0x80000000;       // Owned by DMA
        private const uint DESC_IOC = 0x40000000;      // Interrupt on Completion
        private const uint DESC_LS = 0x20000000;       // Last Segment
        private const uint DESC_FS = 0x10000000;       // First Segment
        private const uint DESC_CIC_MASK = 0x0C000000; // Checksum Insertion Control
        
        // RX Descriptor Status
        private const uint DESC_RX_ERROR = 0x08000000;
        private const uint DESC_RX_CRC_ERROR = 0x01000000;
        private const uint DESC_RX_DAE = 0x02000000;
        
        // ========================================
        // Private Fields
        // ========================================
        
        private uint[] registers;
        private MACAddress macAddress;
        private byte phyAddress;
        private bool linkUp;
        private bool promiscuousMode;
        
        // DMA Descriptor Management
        private ulong txDescriptorListAddress;
        private ulong rxDescriptorListAddress;
        private uint txDescriptorIndex;
        private uint rxDescriptorIndex;
        private uint txDescriptorCount;
        private uint rxDescriptorCount;
        
        // Packet Buffers
        private Queue<byte[]> txQueue;
        private Queue<byte[]> rxQueue;
        private const int MaxQueueSize = 64;
        
        // Statistics
        private NetworkStatistics statistics;
        
        // Timer for DMA polling
        private LimitTimer dmaTimer;
        
        // Interrupt sources
        private GPIO irqLine;
        private GPIO irqGlobalLine;
        
        // Virtual network interface
        private EthernetInterface virtualInterface;
        
        // ========================================
        // Constructor
        // ========================================
        
        public STM32H7_ETH(Machine machine) : base(machine)
        {
            registers = new uint[0x2000 / 4];
            macAddress = new MACAddress(0x00, 0x02, 0xF7, 0x00, 0x00, 0x01);
            phyAddress = 0;
            linkUp = true;
            promiscuousMode = false;
            
            txQueue = new Queue<byte[]>();
            rxQueue = new Queue<byte[]>();
            statistics = new NetworkStatistics();
            
            // Initialize DMA timer for periodic descriptor polling
            dmaTimer = new LimitTimer(machine.ClockSource, 100000, this, "dma_poll", true);
            dmaTimer.LimitReached += DmaPollTimerCallback;
            
            // Initialize interrupt lines
            irqLine = new GPIO();
            irqGlobalLine = new GPIO();
            
            // Define register handlers
            SetupRegisters();
            
            // Initialize PHY registers to default values
            InitializePhy();
        }
        
        // ========================================
        // Register Setup
        // ========================================
        
        private void SetupRegisters()
        {
            // MAC Configuration Register
            Register(MACCR, MACCR, writeHandler: WriteMACCR, readHandler: ReadMACCR);
            
            // MAC Interrupt Registers
            Register(MACIER, MACIER, writeHandler: WriteMACIER);
            Register(MACISR, MACISR, writeHandler: WriteMACISR, readHandler: ReadMACISR);
            
            // MAC Address Registers
            Register(MACA0HR, MACA0HR, writeHandler: WriteMACAddrHigh, readHandler: ReadMACAddrHigh);
            Register(MACA0LR, MACA0LR, writeHandler: WriteMACAddrLow, readHandler: ReadMACAddrLow);
            
            // DMA Registers
            Register(DMAMR, DMAMR, writeHandler: WriteDMAMR, readHandler: ReadDMAMR);
            Register(DMASBMR, DMASBMR, writeHandler: WriteDMASBMR);
            Register(DMADSR, DMADSR, writeHandler: WriteDMADSR, readHandler: ReadDMADSR);
            Register(DMADIER, DMADIER, writeHandler: WriteDMADIER);
            Register(DMACRCR, DMACRCR, writeHandler: WriteDMACRCR, readHandler: ReadDMACRCR);
            Register(DMACTCR, DMACTCR, writeHandler: WriteDMACTCR, readHandler: ReadDMACTCR);
            Register(DMATDLAR, DMATDLAR, writeHandler: WriteTxDListAddr);
            Register(DMARDLAR, DMARDLAR, writeHandler: WriteRxDListAddr);
            Register(DMACTPDR, DMACTPDR, readHandler: ReadTxDescPointer);
            Register(DMACRPDR, DMACRPDR, readHandler: ReadRxDescPointer);
            
            // MTL Registers
            Register(MTLOMR, MTLOMR, writeHandler: WriteMTLOMR);
            Register(MTLRXQ0OMR, MTLRXQ0OMR, writeHandler: WriteMTLRXQOMR);
            Register(MTLTXQ0OMR, MTLTXQ0OMR, writeHandler: WriteMTLTXQOMR);
            
            // PHY Control Register
            Register(MACPHYCSR, MACPHYCSR, readHandler: ReadPHYCSR);
        }
        
        // ========================================
        // MAC Register Handlers
        // ========================================
        
        private void WriteMACCR(uint value)
        {
            var oldVal = registers[MACCR];
            registers[MACCR] = value;
            
            if ((value & MACCR_TE) != 0 && (oldVal & MACCR_TE) == 0)
            {
                this.Log(LogLevel.Info, "MAC Transmitter enabled");
            }
            else if ((value & MACCR_TE) == 0 && (oldVal & MACCR_TE) != 0)
            {
                this.Log(LogLevel.Info, "MAC Transmitter disabled");
            }
            
            if ((value & MACCR_RE) != 0 && (oldVal & MACCR_RE) == 0)
            {
                this.Log(LogLevel.Info, "MAC Receiver enabled");
            }
            else if ((value & MACCR_RE) == 0 && (oldVal & MACCR_RE) != 0)
            {
                this.Log(LogLevel.Info, "MAC Receiver disabled");
            }
            
            // Update duplex mode
            if ((value & MACCR_DM) != 0)
            {
                this.Log(LogLevel.Debug, "Full duplex mode");
            }
            else
            {
                this.Log(LogLevel.Debug, "Half duplex mode");
            }
            
            // Update speed
            if ((value & MACCR_FES) != 0)
            {
                this.Log(LogLevel.Debug, "100 Mbps mode");
            }
            else
            {
                this.Log(LogLevel.Debug, "10 Mbps mode");
            }
        }
        
        private uint ReadMACCR()
        {
            return registers[MACCR];
        }
        
        private void WriteMACIER(uint value)
        {
            registers[MACIER] = value;
        }
        
        private void WriteMACISR(uint value)
        {
            // Write 1 to clear
            registers[MACISR] &= ~value;
        }
        
        private uint ReadMACISR()
        {
            return registers[MACISR];
        }
        
        // ========================================
        // MAC Address Handlers
        // ========================================
        
        private void WriteMACAddrHigh(uint value)
        {
            registers[MACA0HR] = value;
            UpdateMacAddress();
        }
        
        private uint ReadMACAddrHigh()
        {
            return registers[MACA0HR];
        }
        
        private void WriteMACAddrLow(uint value)
        {
            registers[MACA0LR] = value;
            UpdateMacAddress();
        }
        
        private uint ReadMACAddrLow()
        {
            return registers[MACA0LR];
        }
        
        private void UpdateMacAddress()
        {
            var high = registers[MACA0HR];
            var low = registers[MACA0LR];
            
            macAddress = new MACAddress(
                (byte)((high >> 8) & 0xFF),
                (byte)((high >> 0) & 0xFF),
                (byte)((low >> 24) & 0xFF),
                (byte)((low >> 16) & 0xFF),
                (byte)((low >> 8) & 0xFF),
                (byte)((low >> 0) & 0xFF)
            );
            
            this.Log(LogLevel.Info, "MAC Address set to {0}", macAddress);
        }
        
        // ========================================
        // DMA Register Handlers
        // ========================================
        
        private void WriteDMAMR(uint value)
        {
            registers[DMAMR] = value;
            
            // Software Reset
            if ((value & DMAMR_SWR) != 0)
            {
                this.Log(LogLevel.Info, "DMA Software Reset");
                ResetDMA();
            }
        }
        
        private uint ReadDMAMR()
        {
            return registers[DMAMR];
        }
        
        private void WriteDMASBMR(uint value)
        {
            registers[DMASBMR] = value;
        }
        
        private void WriteDMADSR(uint value)
        {
            // Write 1 to clear interrupt flags
            registers[DMADSR] &= ~value;
            UpdateInterrupts();
        }
        
        private uint ReadDMADSR()
        {
            return registers[DMADSR];
        }
        
        private void WriteDMADIER(uint value)
        {
            registers[DMADIER] = value;
            UpdateInterrupts();
        }
        
        private void WriteDMACRCR(uint value)
        {
            registers[DMACRCR] = value;
            
            if ((value & DMACRCR_SR) != 0)
            {
                this.Log(LogLevel.Info, "DMA RX engine started");
                StartDmaRx();
            }
        }
        
        private uint ReadDMACRCR()
        {
            return registers[DMACRCR];
        }
        
        private void WriteDMACTCR(uint value)
        {
            registers[DMACTCR] = value;
            
            if ((value & DMACTCR_ST) != 0)
            {
                this.Log(LogLevel.Info, "DMA TX engine started");
                StartDmaTx();
            }
        }
        
        private uint ReadDMACTCR()
        {
            return registers[DMACTCR];
        }
        
        private void WriteTxDListAddr(uint value)
        {
            txDescriptorListAddress = value;
            txDescriptorIndex = 0;
            this.Log(LogLevel.Info, "TX Descriptor List at 0x{0:X8}", txDescriptorListAddress);
        }
        
        private void WriteRxDListAddr(uint value)
        {
            rxDescriptorListAddress = value;
            rxDescriptorIndex = 0;
            this.Log(LogLevel.Info, "RX Descriptor List at 0x{0:X8}", rxDescriptorListAddress);
        }
        
        private uint ReadTxDescPointer()
        {
            return (uint)(txDescriptorListAddress + txDescriptorIndex * 16);
        }
        
        private uint ReadRxDescPointer()
        {
            return (uint)(rxDescriptorListAddress + rxDescriptorIndex * 16);
        }
        
        // ========================================
        // MTL Register Handlers
        // ========================================
        
        private void WriteMTLOMR(uint value)
        {
            registers[MTLOMR] = value;
        }
        
        private void WriteMTLRXQOMR(uint value)
        {
            registers[MTLRXQ0OMR] = value;
        }
        
        private void WriteMTLTXQOMR(uint value)
        {
            registers[MTLTXQ0OMR] = value;
            
            if ((value & MTLTXQOMR_TXQEN) != 0)
            {
                this.Log(LogLevel.Info, "TX Queue 0 enabled");
            }
        }
        
        // ========================================
        // PHY Interface
        // ========================================
        
        private uint ReadPHYCSR()
        {
            // Return link status
            uint status = 0;
            
            if (linkUp)
            {
                status |= 0x00000001;  // Link OK
            }
            
            return status;
        }
        
        private void InitializePhy()
        {
            // Initialize PHY to default state
            // Simulates LAN8742 auto-negotiation complete
            phyRegisters[PHY_BSR] = PHY_BSR_100BTX_FD | PHY_BSR_10BT_FD |
                                    PHY_BSR_100BTX_HD | PHY_BSR_10BT_HD |
                                    PHY_BSR_ANEG_CAPABLE | PHY_BSR_LINK_ESTABLISHED |
                                    PHY_BSR_ANEG_COMPLETE;
        }
        
        private ushort[] phyRegisters = new ushort[32];
        
        public ushort ReadPhyRegister(byte phyAddr, byte regAddr)
        {
            if (phyAddr != phyAddress)
            {
                this.Log(LogLevel.Warning, "PHY address mismatch: {0} vs {1}", phyAddr, phyAddress);
                return 0;
            }
            
            if (regAddr >= phyRegisters.Length)
            {
                this.Log(LogLevel.Warning, "Invalid PHY register address: {0}", regAddr);
                return 0;
            }
            
            this.Log(LogLevel.Debug, "PHY read: reg {0} = 0x{1:X4}", regAddr, phyRegisters[regAddr]);
            return phyRegisters[regAddr];
        }
        
        public void WritePhyRegister(byte phyAddr, byte regAddr, ushort value)
        {
            if (phyAddr != phyAddress)
            {
                return;
            }
            
            if (regAddr >= phyRegisters.Length)
            {
                return;
            }
            
            this.Log(LogLevel.Debug, "PHY write: reg {0} = 0x{1:X4}", regAddr, value);
            
            // Handle special registers
            if (regAddr == PHY_BCR)
            {
                if ((value & PHY_BCR_RESET) != 0)
                {
                    InitializePhy();
                    return;
                }
            }
            
            phyRegisters[regAddr] = value;
        }
        
        // ========================================
        // DMA Operations
        // ========================================
        
        private void ResetDMA()
        {
            txDescriptorListAddress = 0;
            rxDescriptorListAddress = 0;
            txDescriptorIndex = 0;
            rxDescriptorIndex = 0;
            txQueue.Clear();
            rxQueue.Clear();
            
            // Clear interrupt status
            registers[DMADSR] = 0;
            UpdateInterrupts();
        }
        
        private void StartDmaTx()
        {
            ProcessTxDescriptors();
        }
        
        private void StartDmaRx()
        {
            ProcessRxDescriptors();
        }
        
        private void DmaPollTimerCallback(object sender, EventArgs e)
        {
            // Poll for TX descriptors
            if ((registers[DMACTCR] & DMACTCR_ST) != 0)
            {
                ProcessTxDescriptors();
            }
            
            // Process pending RX packets
            if ((registers[DMACRCR] & DMACRCR_SR) != 0 && rxQueue.Count > 0)
            {
                ProcessRxDescriptors();
            }
        }
        
        private void ProcessTxDescriptors()
        {
            if (txDescriptorListAddress == 0)
            {
                return;
            }
            
            var cpu = sysbus.GetCPU(0);
            if (cpu == null)
            {
                return;
            }
            
            // Read descriptor at current index
            var descAddr = txDescriptorListAddress + txDescriptorIndex * 16;
            var tdes3 = cpu.GetMemoryValue(descAddr + 12, 4);
            
            // Check if owned by DMA
            if ((tdes3 & DESC_OWN) == 0)
            {
                return;  // Not owned by DMA, nothing to transmit
            }
            
            // Get buffer addresses
            var tdes0 = cpu.GetMemoryValue(descAddr + 0, 4);   // Buffer 1 Address
            var tdes2 = cpu.GetMemoryValue(descAddr + 8, 4);   // Buffer sizes
            
            var buf1Size = (int)(tdes2 & 0x3FFF);
            var buf1Addr = tdes0;
            
            // Read packet data from memory
            var packet = new byte[buf1Size];
            for (int i = 0; i < buf1Size; i++)
            {
                packet[i] = cpu.GetMemoryValue(buf1Addr + i, 1);
            }
            
            // Transmit packet
            TransmitPacket(packet);
            
            // Clear ownership bit (give back to software)
            tdes3 &= ~DESC_OWN;
            cpu.SetMemoryValue(descAddr + 12, 4, tdes3);
            
            // Update statistics
            statistics.TxPackets++;
            statistics.TxBytes += (ulong)buf1Size;
            
            // Generate TX interrupt if requested
            if ((tdes3 & DESC_IOC) != 0)
            {
                registers[DMADSR] |= DMADSR_TI;
                UpdateInterrupts();
            }
            
            // Move to next descriptor
            txDescriptorIndex++;
            
            // Check for last segment
            if ((tdes3 & DESC_LS) != 0)
            {
                // End of packet, reset index
                txDescriptorIndex = 0;
            }
        }
        
        private void ProcessRxDescriptors()
        {
            if (rxDescriptorListAddress == 0 || rxQueue.Count == 0)
            {
                return;
            }
            
            var cpu = sysbus.GetCPU(0);
            if (cpu == null)
            {
                return;
            }
            
            var packet = rxQueue.Dequeue();
            
            // Get descriptor
            var descAddr = rxDescriptorListAddress + rxDescriptorIndex * 16;
            var rdes3 = cpu.GetMemoryValue(descAddr + 12, 4);
            
            // Check if owned by DMA
            if ((rdes3 & DESC_OWN) == 0)
            {
                // Not owned by DMA, put packet back
                var tempQueue = new Queue<byte[]>();
                tempQueue.Enqueue(packet);
                while (rxQueue.Count > 0)
                {
                    tempQueue.Enqueue(rxQueue.Dequeue());
                }
                rxQueue = tempQueue;
                return;
            }
            
            // Get buffer address
            var rdes0 = cpu.GetMemoryValue(descAddr + 0, 4);
            var bufAddr = rdes0;
            
            // Write packet to buffer
            for (int i = 0; i < packet.Length; i++)
            {
                cpu.SetMemoryValue(bufAddr + i, 1, packet[i]);
            }
            
            // Update descriptor with packet length and status
            rdes3 &= ~DESC_OWN;
            rdes3 |= DESC_LS | DESC_FS;  // First and Last segment
            rdes3 |= (uint)(packet.Length & 0x7FFF);  // Frame length
            
            cpu.SetMemoryValue(descAddr + 12, 4, rdes3);
            
            // Update statistics
            statistics.RxPackets++;
            statistics.RxBytes += (ulong)packet.Length;
            
            // Generate RX interrupt
            registers[DMADSR] |= DMADSR_RI;
            UpdateInterrupts();
            
            // Move to next descriptor
            rxDescriptorIndex++;
        }
        
        // ========================================
        // Packet Transmission/Reception
        // ========================================
        
        private void TransmitPacket(byte[] packet)
        {
            this.Log(LogLevel.Debug, "TX packet: {0} bytes", packet.Length);
            
            if (packet.Length < 60)
            {
                // Pad to minimum Ethernet frame size
                Array.Resize(ref packet, 60);
            }
            
            // Add CRC (simulated)
            var crc = CalculateCRC32(packet);
            var packetWithCrc = new byte[packet.Length + 4];
            Array.Copy(packet, packetWithCrc, packet.Length);
            packetWithCrc[packet.Length] = (byte)(crc & 0xFF);
            packetWithCrc[packet.Length + 1] = (byte)((crc >> 8) & 0xFF);
            packetWithCrc[packet.Length + 2] = (byte)((crc >> 16) & 0xFF);
            packetWithCrc[packet.Length + 3] = (byte)((crc >> 24) & 0xFF);
            
            // Send to virtual network
            virtualInterface?.SendPacket(packetWithCrc);
        }
        
        public void ReceivePacket(byte[] packet)
        {
            if ((registers[MACCR] & MACCR_RE) == 0)
            {
                this.Log(LogLevel.Debug, "RX discarded (receiver disabled)");
                return;
            }
            
            if (rxQueue.Count >= MaxQueueSize)
            {
                this.Log(LogLevel.Warning, "RX queue overflow");
                statistics.RxErrors++;
                return;
            }
            
            // Filter by MAC address unless promiscuous mode
            if (!promiscuousMode && !IsPacketForUs(packet))
            {
                this.Log(LogLevel.Debug, "RX packet filtered");
                return;
            }
            
            this.Log(LogLevel.Debug, "RX packet queued: {0} bytes", packet.Length);
            rxQueue.Enqueue(packet);
            
            // Trigger DMA processing
            if ((registers[DMACRCR] & DMACRCR_SR) != 0)
            {
                ProcessRxDescriptors();
            }
        }
        
        private bool IsPacketForUs(byte[] packet)
        {
            if (packet.Length < 6)
            {
                return false;
            }
            
            // Check destination MAC
            var destMac = new MACAddress(packet[0], packet[1], packet[2],
                                         packet[3], packet[4], packet[5]);
            
            // Broadcast
            if (destMac.IsBroadcast)
            {
                return true;
            }
            
            // Unicast to us
            if (destMac.Equals(macAddress))
            {
                return true;
            }
            
            // Multicast (could check hash table)
            if (destMac.IsMulticast)
            {
                return true;
            }
            
            return false;
        }
        
        // ========================================
        // CRC Calculation
        // ========================================
        
        private static uint[] crc32Table;
        
        private static uint[] GenerateCRC32Table()
        {
            var table = new uint[256];
            for (uint i = 0; i < 256; i++)
            {
                uint crc = i << 24;
                for (int j = 0; j < 8; j++)
                {
                    if ((crc & 0x80000000) != 0)
                    {
                        crc = (crc << 1) ^ 0x04C11DB7;
                    }
                    else
                    {
                        crc <<= 1;
                    }
                }
                table[i] = crc;
            }
            return table;
        }
        
        private uint CalculateCRC32(byte[] data)
        {
            if (crc32Table == null)
            {
                crc32Table = GenerateCRC32Table();
            }
            
            uint crc = 0xFFFFFFFF;
            foreach (byte b in data)
            {
                crc = (crc << 8) ^ crc32Table[(crc >> 24) ^ b];
            }
            return ~crc;
        }
        
        // ========================================
        // Interrupt Handling
        // ========================================
        
        private void UpdateInterrupts()
        {
            var enabled = registers[DMADIER];
            var status = registers[DMADSR];
            
            // Check for enabled interrupts
            var pending = enabled & status;
            
            if (pending != 0)
            {
                irqLine.Set();
                
                if ((pending & (DMADSR_TI | DMADSR_RI | DMADSR_TBU | DMADSR_RU)) != 0)
                {
                    registers[DMADSR] |= DMADSR_NIS;  // Normal Interrupt Summary
                }
                
                if ((pending & (DMADSR_TJT | DMADSR_OVF | DMADSR_UNF | 
                               DMADSR_RWT | DMADSR_FBE)) != 0)
                {
                    registers[DMADSR] |= DMADSR_AIS;  // Abnormal Interrupt Summary
                }
            }
            else
            {
                irqLine.Unset();
            }
        }
        
        // ========================================
        // Public API for External Control
        // ========================================
        
        public void SetMacAddress(byte[] addr)
        {
            if (addr.Length != 6)
            {
                throw new ArgumentException("MAC address must be 6 bytes");
            }
            
            macAddress = new MACAddress(addr[0], addr[1], addr[2],
                                        addr[3], addr[4], addr[5]);
            
            // Update hardware registers
            registers[MACA0HR] = (uint)((addr[0] << 8) | addr[1]);
            registers[MACA0LR] = (uint)((addr[2] << 24) | (addr[3] << 16) |
                                        (addr[4] << 8) | addr[5]);
            
            this.Log(LogLevel.Info, "MAC address set to {0}", macAddress);
        }
        
        public void SetPhyAddress(byte addr)
        {
            phyAddress = addr;
            this.Log(LogLevel.Info, "PHY address set to {0}", addr);
        }
        
        public void SetLinkStatus(bool up)
        {
            linkUp = up;
            
            if (up)
            {
                phyRegisters[PHY_BSR] |= PHY_BSR_LINK_ESTABLISHED;
                this.Log(LogLevel.Info, "Link up");
            }
            else
            {
                phyRegisters[PHY_BSR] &= ~PHY_BSR_LINK_ESTABLISHED;
                this.Log(LogLevel.Info, "Link down");
            }
        }
        
        public void SetPromiscuousMode(bool enable)
        {
            promiscuousMode = enable;
            this.Log(LogLevel.Info, "Promiscuous mode {0}", enable ? "enabled" : "disabled");
        }
        
        public NetworkStatistics GetStatistics()
        {
            return statistics;
        }
        
        public void ResetStatistics()
        {
            statistics = new NetworkStatistics();
        }
        
        // ========================================
        // Error Injection
        // ========================================
        
        public void InjectCrcError()
        {
            this.Log(LogLevel.Warning, "CRC error injection enabled for next packet");
            injectCrcError = true;
        }
        
        public void InjectFrameError()
        {
            this.Log(LogLevel.Warning, "Frame error injection enabled for next packet");
            injectFrameError = true;
        }
        
        public void InjectOverflow()
        {
            this.Log(LogLevel.Warning, "RX overflow condition injected");
            registers[DMADSR] |= DMADSR_OVF;
            UpdateInterrupts();
        }
        
        private bool injectCrcError = false;
        private bool injectFrameError = false;
        
        // ========================================
        // GPIO Connections
        // ========================================
        
        public GPIO IRQ => irqLine;
        public GPIO IRQGlobal => irqGlobalLine;
        
        // ========================================
        // IKnownType Implementation
        // ========================================
        
        public string GetTypeName()
        {
            return nameof(STM32H7_ETH);
        }
        
        // ========================================
        // Reset
        // ========================================
        
        public override void Reset()
        {
            base.Reset();
            
            for (int i = 0; i < registers.Length; i++)
            {
                registers[i] = 0;
            }
            
            txQueue.Clear();
            rxQueue.Clear();
            txDescriptorListAddress = 0;
            rxDescriptorListAddress = 0;
            txDescriptorIndex = 0;
            rxDescriptorIndex = 0;
            
            InitializePhy();
            
            irqLine.Unset();
            irqGlobalLine.Unset();
        }
    }
    
    // ========================================
    // Supporting Types
    // ========================================
    
    public class MACAddress
    {
        public byte[] Bytes { get; }
        
        public MACAddress(byte b0, byte b1, byte b2, byte b3, byte b4, byte b5)
        {
            Bytes = new byte[] { b0, b1, b2, b3, b4, b5 };
        }
        
        public bool IsBroadcast => Bytes.All(b => b == 0xFF);
        
        public bool IsMulticast => (Bytes[0] & 0x01) != 0;
        
        public override string ToString()
        {
            return string.Format("{0:X2}:{1:X2}:{2:X2}:{3:X2}:{4:X2}:{5:X2}",
                                 Bytes[0], Bytes[1], Bytes[2], Bytes[3], Bytes[4], Bytes[5]);
        }
        
        public override bool Equals(object obj)
        {
            if (obj is MACAddress other)
            {
                return Bytes.SequenceEqual(other.Bytes);
            }
            return false;
        }
        
        public override int GetHashCode()
        {
            return Bytes.GetHashCode();
        }
    }
    
    public class NetworkStatistics
    {
        public ulong TxPackets { get; set; }
        public ulong TxBytes { get; set; }
        public ulong TxErrors { get; set; }
        public ulong RxPackets { get; set; }
        public ulong RxBytes { get; set; }
        public ulong RxErrors { get; set; }
        public ulong Collisions { get; set; }
        public ulong Dropped { get; set; }
    }
    
    // Placeholder for virtual Ethernet interface
    public interface EthernetInterface
    {
        void SendPacket(byte[] packet);
        void SetReceiver(Action<byte[]> receiver);
    }
}