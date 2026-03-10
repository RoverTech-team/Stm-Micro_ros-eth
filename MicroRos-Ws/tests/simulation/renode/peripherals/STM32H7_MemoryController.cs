/*
 * STM32H7 Memory Controller Peripheral Emulation
 * 
 * This peripheral emulates the memory region model found in STM32H7 series
 * microcontrollers. It supports:
 * - Complete STM32H7 memory map
 * - Region-based access validation
 * - Cache coherency checking for DMA operations
 * - Memory access statistics and logging
 * 
 * Reference: STM32H7 Reference Manual (RM0433)
 *            Memory map: Section 2.3
 */

using System;
using System.Collections.Generic;
using Antmicro.Renode.Core;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals.Bus;

namespace Antmicro.Renode.Peripherals.Memory
{
    public class STM32H7_MemoryController : BasicDoubleWordPeripheral, IKnownType
    {
        public enum MemoryRegion
        {
            ITCM_FLASH,
            AXI_FLASH,
            ITCM_RAM,
            DTCM_RAM,
            SRAM_D1,
            SRAM_D2,
            SRAM_D3,
            RAM_EXTERNAL,
            Backup_RAM
        }

        public enum AccessPermission
        {
            None = 0,
            ReadOnly = 1,
            WriteOnly = 2,
            ReadWrite = 3,
            PrivilegedOnly = 4
        }

        public class RegionConfig
        {
            public MemoryRegion Region;
            public ulong BaseAddress;
            public ulong Size;
            public bool Cacheable;
            public bool Bufferable;
            public bool Shareable;
            public bool MPUConfigured;
            public int MPURegion;
            public AccessPermission Permissions;
            public string Name;
        }

        public class MemoryAccessRecord
        {
            public ulong Address;
            public uint Size;
            public bool IsWrite;
            public MemoryRegion Region;
            public DateTime Timestamp;
            public ulong PC;
        }

        public class MemoryStatistics
        {
            public ulong TotalReads;
            public ulong TotalWrites;
            public ulong ReadBytes;
            public ulong WrittenBytes;
            public Dictionary<MemoryRegion, ulong> ReadsPerRegion;
            public Dictionary<MemoryRegion, ulong> WritesPerRegion;
            public Dictionary<MemoryRegion, ulong> BytesReadPerRegion;
            public Dictionary<MemoryRegion, ulong> BytesWrittenPerRegion;
            public ulong CacheCoherencyViolations;
            public ulong DMAAlignmentErrors;
            public ulong InvalidAccessAttempts;

            public MemoryStatistics()
            {
                ReadsPerRegion = new Dictionary<MemoryRegion, ulong>();
                WritesPerRegion = new Dictionary<MemoryRegion, ulong>();
                BytesReadPerRegion = new Dictionary<MemoryRegion, ulong>();
                BytesWrittenPerRegion = new Dictionary<MemoryRegion, ulong>();
                
                foreach (MemoryRegion region in Enum.GetValues(typeof(MemoryRegion)))
                {
                    ReadsPerRegion[region] = 0;
                    WritesPerRegion[region] = 0;
                    BytesReadPerRegion[region] = 0;
                    BytesWrittenPerRegion[region] = 0;
                }
            }
        }

        private Dictionary<MemoryRegion, RegionConfig> regionConfigs;
        private List<MemoryAccessRecord> accessLog;
        private MemoryStatistics statistics;
        private bool logEnabled;
        private int maxLogEntries;
        private ICpuSupportingThumb cpu;

        private const uint SRAM_D2_BASE = 0x30000000;
        private const uint SRAM_D2_END = 0x3001FFFF;
        private const uint SRAM_D2_SIZE = 128 * 1024;

        public STM32H7_MemoryController(Machine machine) : base(machine)
        {
            regionConfigs = new Dictionary<MemoryRegion, RegionConfig>();
            accessLog = new List<MemoryAccessRecord>();
            statistics = new MemoryStatistics();
            logEnabled = true;
            maxLogEntries = 10000;
            
            InitializeMemoryRegions();
            SetupRegisters();
        }

        private void InitializeMemoryRegions()
        {
            regionConfigs[MemoryRegion.ITCM_FLASH] = new RegionConfig
            {
                Region = MemoryRegion.ITCM_FLASH,
                BaseAddress = 0x00000000,
                Size = 1 * 1024 * 1024,
                Cacheable = true,
                Bufferable = false,
                Shareable = false,
                MPUConfigured = false,
                MPURegion = -1,
                Permissions = AccessPermission.ReadWrite,
                Name = "ITCM Flash"
            };

            regionConfigs[MemoryRegion.AXI_FLASH] = new RegionConfig
            {
                Region = MemoryRegion.AXI_FLASH,
                BaseAddress = 0x08000000,
                Size = 2 * 1024 * 1024,
                Cacheable = true,
                Bufferable = false,
                Shareable = false,
                MPUConfigured = false,
                MPURegion = -1,
                Permissions = AccessPermission.ReadWrite,
                Name = "AXI Flash"
            };

            regionConfigs[MemoryRegion.ITCM_RAM] = new RegionConfig
            {
                Region = MemoryRegion.ITCM_RAM,
                BaseAddress = 0x00000000,
                Size = 64 * 1024,
                Cacheable = false,
                Bufferable = true,
                Shareable = false,
                MPUConfigured = false,
                MPURegion = -1,
                Permissions = AccessPermission.ReadWrite,
                Name = "ITCM RAM"
            };

            regionConfigs[MemoryRegion.DTCM_RAM] = new RegionConfig
            {
                Region = MemoryRegion.DTCM_RAM,
                BaseAddress = 0x20000000,
                Size = 128 * 1024,
                Cacheable = false,
                Bufferable = true,
                Shareable = false,
                MPUConfigured = false,
                MPURegion = -1,
                Permissions = AccessPermission.ReadWrite,
                Name = "DTCM RAM"
            };

            regionConfigs[MemoryRegion.SRAM_D1] = new RegionConfig
            {
                Region = MemoryRegion.SRAM_D1,
                BaseAddress = 0x24000000,
                Size = 512 * 1024,
                Cacheable = true,
                Bufferable = true,
                Shareable = false,
                MPUConfigured = false,
                MPURegion = -1,
                Permissions = AccessPermission.ReadWrite,
                Name = "SRAM D1 (AXI SRAM)"
            };

            regionConfigs[MemoryRegion.SRAM_D2] = new RegionConfig
            {
                Region = MemoryRegion.SRAM_D2,
                BaseAddress = SRAM_D2_BASE,
                Size = SRAM_D2_SIZE,
                Cacheable = false,
                Bufferable = true,
                Shareable = true,
                MPUConfigured = false,
                MPURegion = -1,
                Permissions = AccessPermission.ReadWrite,
                Name = "SRAM D2 (Ethernet DMA)"
            };

            regionConfigs[MemoryRegion.SRAM_D3] = new RegionConfig
            {
                Region = MemoryRegion.SRAM_D3,
                BaseAddress = 0x38000000,
                Size = 64 * 1024,
                Cacheable = false,
                Bufferable = true,
                Shareable = true,
                MPUConfigured = false,
                MPURegion = -1,
                Permissions = AccessPermission.ReadWrite,
                Name = "SRAM D3 (AHB SRAM)"
            };

            regionConfigs[MemoryRegion.RAM_EXTERNAL] = new RegionConfig
            {
                Region = MemoryRegion.RAM_EXTERNAL,
                BaseAddress = 0x90000000,
                Size = 256 * 1024 * 1024,
                Cacheable = true,
                Bufferable = true,
                Shareable = false,
                MPUConfigured = false,
                MPURegion = -1,
                Permissions = AccessPermission.ReadWrite,
                Name = "External RAM"
            };

            regionConfigs[MemoryRegion.Backup_RAM] = new RegionConfig
            {
                Region = MemoryRegion.Backup_RAM,
                BaseAddress = 0x38800000,
                Size = 4 * 1024,
                Cacheable = false,
                Bufferable = false,
                Shareable = false,
                MPUConfigured = false,
                MPURegion = -1,
                Permissions = AccessPermission.ReadWrite,
                Name = "Backup RAM"
            };
        }

        private void SetupRegisters()
        {
        }

        public void SetCPU(ICpuSupportingThumb cpuInstance)
        {
            cpu = cpuInstance;
        }

        public void OnMemoryAccess(ulong address, uint size, bool isWrite)
        {
            var region = GetRegionForAddress(address);
            
            if (logEnabled && accessLog.Count < maxLogEntries)
            {
                var record = new MemoryAccessRecord
                {
                    Address = address,
                    Size = size,
                    IsWrite = isWrite,
                    Region = region.HasValue ? region.Value : MemoryRegion.ITCM_FLASH,
                    Timestamp = DateTime.UtcNow,
                    PC = cpu != null ? cpu.PC : 0
                };
                accessLog.Add(record);
            }

            UpdateStatistics(address, size, isWrite, region);
        }

        private void UpdateStatistics(ulong address, uint size, bool isWrite, MemoryRegion? region)
        {
            if (isWrite)
            {
                statistics.TotalWrites++;
                statistics.WrittenBytes += size;
                if (region.HasValue)
                {
                    statistics.WritesPerRegion[region.Value]++;
                    statistics.BytesWrittenPerRegion[region.Value] += size;
                }
            }
            else
            {
                statistics.TotalReads++;
                statistics.ReadBytes += size;
                if (region.HasValue)
                {
                    statistics.ReadsPerRegion[region.Value]++;
                    statistics.BytesReadPerRegion[region.Value] += size;
                }
            }
        }

        public MemoryRegion? GetRegionForAddress(ulong address)
        {
            foreach (var kvp in regionConfigs)
            {
                var config = kvp.Value;
                if (address >= config.BaseAddress && address < config.BaseAddress + config.Size)
                {
                    return config.Region;
                }
            }
            
            this.Log(LogLevel.Debug, "Address 0x{0:X8} does not map to any known memory region", address);
            return null;
        }

        public RegionConfig GetRegionConfig(MemoryRegion region)
        {
            if (regionConfigs.TryGetValue(region, out var config))
            {
                return config;
            }
            return null;
        }

        public RegionConfig GetRegionConfigForAddress(ulong address)
        {
            var region = GetRegionForAddress(address);
            if (region.HasValue)
            {
                return GetRegionConfig(region.Value);
            }
            return null;
        }

        public void ValidateDMAAccess(ulong address, uint size)
        {
            var region = GetRegionForAddress(address);
            
            if (!region.HasValue)
            {
                this.Log(LogLevel.Error, 
                    "DMA access to invalid memory region: address 0x{0:X8}, size {1}",
                    address, size);
                statistics.InvalidAccessAttempts++;
                throw new DMAAccessException(address, size, "Invalid memory region for DMA access");
            }

            var config = GetRegionConfig(region.Value);

            if (address % 4 != 0)
            {
                this.Log(LogLevel.Warning,
                    "DMA access not word-aligned: address 0x{0:X8}",
                    address);
                statistics.DMAAlignmentErrors++;
            }

            if (config.Cacheable)
            {
                this.Log(LogLevel.Error,
                    "DMA access to cacheable region {0} at address 0x{1:X8}! " +
                    "This may cause cache coherency issues on real hardware.",
                    config.Name, address);
                statistics.CacheCoherencyViolations++;
            }

            if (region == MemoryRegion.SRAM_D2)
            {
                ValidateSRAM_D2Access(address, size, config);
            }
        }

        private void ValidateSRAM_D2Access(ulong address, uint size, RegionConfig config)
        {
            if (config.Cacheable)
            {
                this.Log(LogLevel.Error,
                    "CRITICAL: SRAM_D2 (Ethernet DMA region) is configured as cacheable! " +
                    "This WILL cause DMA data corruption on real hardware. " +
                    "Address: 0x{0:X8}",
                    address);
                statistics.CacheCoherencyViolations++;
                throw new CacheCoherencyViolationException(
                    address,
                    "SRAM_D2 must be non-cacheable for Ethernet DMA coherency");
            }

            if (!config.Shareable)
            {
                this.Log(LogLevel.Warning,
                    "SRAM_D2 region should be marked shareable for proper DMA coherency. " +
                    "Address: 0x{0:X8}",
                    address);
            }

            this.Log(LogLevel.Debug,
                "DMA access to SRAM_D2 validated: address 0x{0:X8}, size {1}, " +
                "cacheable={2}, shareable={3}",
                address, size, config.Cacheable, config.Shareable);
        }

        public void ValidateEthernetDMABuffers(ulong txDescAddr, ulong rxDescAddr, 
            ulong txBufferAddr, ulong rxBufferAddr)
        {
            this.Log(LogLevel.Info, "Validating Ethernet DMA buffer placement...");

            if (txDescAddr != 0)
            {
                ValidateDMAAccess(txDescAddr, 16);
                this.Log(LogLevel.Debug, "TX descriptors at 0x{0:X8} - OK", txDescAddr);
            }

            if (rxDescAddr != 0)
            {
                ValidateDMAAccess(rxDescAddr, 16);
                this.Log(LogLevel.Debug, "RX descriptors at 0x{0:X8} - OK", rxDescAddr);
            }

            if (txBufferAddr != 0)
            {
                ValidateDMAAccess(txBufferAddr, 1536);
                this.Log(LogLevel.Debug, "TX buffer at 0x{0:X8} - OK", txBufferAddr);
            }

            if (rxBufferAddr != 0)
            {
                ValidateDMAAccess(rxBufferAddr, 1536);
                this.Log(LogLevel.Debug, "RX buffer at 0x{0:X8} - OK", rxBufferAddr);
            }
        }

        public void SetRegionMPUConfiguration(MemoryRegion region, int mpuRegion, 
            bool cacheable, bool bufferable, bool shareable)
        {
            if (regionConfigs.TryGetValue(region, out var config))
            {
                config.MPUConfigured = true;
                config.MPURegion = mpuRegion;
                config.Cacheable = cacheable;
                config.Bufferable = bufferable;
                config.Shareable = shareable;

                this.Log(LogLevel.Info,
                    "MPU configuration set for {0}: MPU region {1}, " +
                    "cacheable={2}, bufferable={3}, shareable={4}",
                    config.Name, mpuRegion, cacheable, bufferable, shareable);

                if (region == MemoryRegion.SRAM_D2 && cacheable)
                {
                    this.Log(LogLevel.Error,
                        "CRITICAL WARNING: SRAM_D2 configured as cacheable! " +
                        "This will cause Ethernet DMA corruption on real hardware!");
                }
            }
        }

        public void CheckCacheCoherencyForAllDMARegions()
        {
            this.Log(LogLevel.Info, "Checking cache coherency for all DMA-capable regions...");

            var dmaRegions = new[] { MemoryRegion.SRAM_D1, MemoryRegion.SRAM_D2, MemoryRegion.SRAM_D3 };
            
            foreach (var region in dmaRegions)
            {
                var config = GetRegionConfig(region);
                if (config != null && config.Cacheable)
                {
                    this.Log(LogLevel.Error,
                        "DMA region {0} is cacheable - potential coherency issues!",
                        config.Name);
                    statistics.CacheCoherencyViolations++;
                }
                else if (config != null)
                {
                    this.Log(LogLevel.Debug,
                        "DMA region {0} coherency check passed", config.Name);
                }
            }
        }

        public MemoryStatistics GetStatistics()
        {
            return statistics;
        }

        public void ResetStatistics()
        {
            statistics = new MemoryStatistics();
            this.Log(LogLevel.Info, "Memory statistics reset");
        }

        public List<MemoryAccessRecord> GetAccessLog()
        {
            return new List<MemoryAccessRecord>(accessLog);
        }

        public void ClearAccessLog()
        {
            accessLog.Clear();
            this.Log(LogLevel.Info, "Access log cleared");
        }

        public void SetLogEnabled(bool enabled)
        {
            logEnabled = enabled;
            this.Log(LogLevel.Info, "Access logging {0}", enabled ? "enabled" : "disabled");
        }

        public void SetMaxLogEntries(int max)
        {
            maxLogEntries = max;
            this.Log(LogLevel.Info, "Max log entries set to {0}", max);
        }

        public void PrintMemoryMap()
        {
            this.Log(LogLevel.Info, "=== STM32H7 Memory Map ===");
            foreach (var kvp in regionConfigs)
            {
                var config = kvp.Value;
                this.Log(LogLevel.Info,
                    "{0}: 0x{1:X8} - 0x{2:X8} ({3} KB) [{4}{5}{6}]",
                    config.Name,
                    config.BaseAddress,
                    config.BaseAddress + config.Size - 1,
                    config.Size / 1024,
                    config.Cacheable ? "C" : "-",
                    config.Bufferable ? "B" : "-",
                    config.Shareable ? "S" : "-");
            }
        }

        public void PrintStatistics()
        {
            this.Log(LogLevel.Info, "=== Memory Access Statistics ===");
            this.Log(LogLevel.Info, "Total reads: {0} ({1} bytes)", 
                statistics.TotalReads, statistics.ReadBytes);
            this.Log(LogLevel.Info, "Total writes: {0} ({1} bytes)", 
                statistics.TotalWrites, statistics.WrittenBytes);
            this.Log(LogLevel.Info, "Cache coherency violations: {0}", 
                statistics.CacheCoherencyViolations);
            this.Log(LogLevel.Info, "DMA alignment errors: {0}", 
                statistics.DMAAlignmentErrors);
            this.Log(LogLevel.Info, "Invalid access attempts: {0}", 
                statistics.InvalidAccessAttempts);
            
            this.Log(LogLevel.Info, "--- Per-Region Statistics ---");
            foreach (MemoryRegion region in Enum.GetValues(typeof(MemoryRegion)))
            {
                var config = GetRegionConfig(region);
                if (config != null)
                {
                    this.Log(LogLevel.Info,
                        "{0}: R={1} W={2} ({3} bytes read, {4} bytes written)",
                        config.Name,
                        statistics.ReadsPerRegion[region],
                        statistics.WritesPerRegion[region],
                        statistics.BytesReadPerRegion[region],
                        statistics.BytesWrittenPerRegion[region]);
                }
            }
        }

        public string GetTypeName()
        {
            return nameof(STM32H7_MemoryController);
        }

        public override void Reset()
        {
            base.Reset();
            
            foreach (var config in regionConfigs.Values)
            {
                config.MPUConfigured = false;
                config.MPURegion = -1;
            }
            
            InitializeMemoryRegions();
            accessLog.Clear();
            statistics = new MemoryStatistics();
            
            this.Log(LogLevel.Info, "Memory controller reset");
        }
    }

    public class DMAAccessException : Exception
    {
        public ulong Address { get; }
        public uint Size { get; }

        public DMAAccessException(ulong address, uint size, string message) 
            : base($"DMA access error at 0x{address:X8} (size: {size}): {message}")
        {
            Address = address;
            Size = size;
        }
    }

    public class CacheCoherencyViolationException : Exception
    {
        public ulong Address { get; }

        public CacheCoherencyViolationException(ulong address, string message)
            : base($"Cache coherency violation at 0x{address:X8}: {message}")
        {
            Address = address;
        }
    }
}
