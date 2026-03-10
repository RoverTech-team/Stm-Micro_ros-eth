//
// JSN-SR04T / AJ-SR04M Ultrasonic Distance Sensor Plugin for Renode
//
// This plugin provides a complete simulation of the waterproof ultrasonic sensor
// JSN-SR04T (and AJ-SR04M) operating in Mode 1 (Digital/Pulse) communication.
//
// Copyright (c) 2026 Test_Board_Sensore
// Licensed under MIT License
//

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.GPIO;
using Antmicro.Renode.Plugins;
using Antmicro.Renode.Time;

namespace Antmicro.Renode.Plugins.JSN_SR04T
{
    /// <summary>
    /// JSN-SR04T / AJ-SR04M Waterproof Ultrasonic Distance Sensor
    /// 
    /// Simulates the sensor operating in Mode 1 (Digital/Pulse) communication.
    /// 
    /// Timing:
    /// - TRIG: MCU sends 10µs HIGH pulse
    /// - ECHO: Sensor outputs pulse width = distance × 58µs
    /// - Range: 20-450cm
    /// - Timeout: 30ms (configurable)
    /// </summary>
    [Plugin(Name = "jsn-sr04t", 
            Description = "JSN-SR04T/AJ-SR04M Ultrasonic Distance Sensor",
            Vendor = "Test_Board_Sensore")]
    public class JSN_SR04T : IExternal, IHasGPIO, IPeripheralRegister
    {
        private readonly Machine machine;
        private readonly string name;
        
        private int distance = 50;
        private readonly int minDistance = 20;
        private readonly int maxDistance = 450;
        private int timeout = 30000;
        private readonly int triggerPulseWidth = 10;
        
        private SensorState state = SensorState.Idle;
        
        private readonly IGPIOConnector trigPin;
        private readonly IGPIOConnector echoPin;
        
        private long triggerStartTime;
        private CancellationTokenSource? measurementCts;
        
        public JSN_SR04T(Machine machine, string name)
        {
            this.machine = machine ?? throw new ArgumentNullException(nameof(machine));
            this.name = name ?? throw new ArgumentNullException(nameof(name));
            
            // Create GPIO connectors
            trigPin = new GPIOConnector();
            echoPin = new GPIOConnector();
            
            // Subscribe to TRIG pin changes
            trigPin.Changed += OnTrigPinChanged;
            
            this.Log(LogLevel.Info, "JSN-SR04T sensor created with default distance: {0}cm", distance);
        }

        #region Properties

        public int Distance
        {
            get => distance;
            set
            {
                if(value < 0)
                {
                    this.Log(LogLevel.Warning, "Distance cannot be negative, setting to 0");
                    distance = 0;
                }
                else if(value > maxDistance)
                {
                    this.Log(LogLevel.Debug, "Distance {0}cm exceeds maximum, clamping to {1}cm", value, maxDistance);
                    distance = maxDistance;
                }
                else
                {
                    distance = value;
                }
                this.Log(LogLevel.Debug, "Distance set to: {0}cm", distance);
            }
        }

        public int MinDistance => minDistance;
        public int MaxDistance => maxDistance;
        
        public int Timeout
        {
            get => timeout;
            set => timeout = value;
        }
        
        public int ExpectedEchoPulseWidth => distance * 58;
        
        public IGPIOConnector TrigPin => trigPin;
        public IGPIOConnector EchoPin => echoPin;
        
        public SensorState State => state;

        #endregion

        #region Public Methods

        public void SetDistance(int newDistance)
        {
            Distance = newDistance;
        }

        public int GetDistance()
        {
            return distance;
        }

        public void Reset()
        {
            measurementCts?.Cancel();
            measurementCts?.Dispose();
            measurementCts = null;
            
            state = SensorState.Idle;
            trigPin.Set(false);
            echoPin.Set(false);
            
            this.Log(LogLevel.Info, "Sensor reset to idle state");
        }

        public void Trigger()
        {
            if(state != SensorState.Idle)
            {
                this.Log(LogLevel.Debug, "Measurement already in progress, ignoring trigger");
                return;
            }
            
            PerformMeasurement().ConfigureAwait(false);
        }

        #endregion

        #region Private Methods

        private void OnTrigPinChanged(bool value)
        {
            if(value)
            {
                // Rising edge - start trigger detection
                if(state == SensorState.Idle)
                {
                    state = SensorState.WaitingTrigger;
                    triggerStartTime = machine.ElapsedTimeTicks;
                    this.Log(LogLevel.Debug, "TRIG rising edge detected");
                    
                    // Schedule trigger validation
                    Task.Delay(TimeSpan.FromMicroseconds(triggerPulseWidth)).ContinueWith(_ =>
                    {
                        ValidateTriggerPulse();
                    });
                }
            }
        }

        private void ValidateTriggerPulse()
        {
            long elapsed = machine.ElapsedTimeTicks - triggerStartTime;
            long pulseWidthUs = elapsed / (TimeSpan.TicksPerMicrosecond);
            
            // Check if pulse width is valid (10µs ± 2µs tolerance)
            if(pulseWidthUs >= triggerPulseWidth - 2 && pulseWidthUs <= triggerPulseWidth + 2)
            {
                this.Log(LogLevel.Debug, "Valid trigger pulse: {0}µs", pulseWidthUs);
                PerformMeasurement().ConfigureAwait(false);
            }
            else
            {
                this.Log(LogLevel.Debug, "Invalid trigger pulse width: {0}µs (expected {1}µs)", 
                    pulseWidthUs, triggerPulseWidth);
                state = SensorState.Idle;
            }
        }

        private async Task PerformMeasurement()
        {
            state = SensorState.Measuring;
            this.Log(LogLevel.Debug, "Starting measurement for distance: {0}cm", distance);
            
            measurementCts = new CancellationTokenSource();
            
            try
            {
                if(distance < minDistance || distance == 0)
                {
                    // Out of range - simulate timeout
                    this.Log(LogLevel.Debug, "Distance out of range, simulating timeout");
                    await Task.Delay(TimeSpan.FromMicroseconds(timeout), measurementCts.Token);
                    HandleTimeout();
                    return;
                }
                
                // Calculate echo pulse width
                int echoPulseWidth = distance * 58;
                this.Log(LogLevel.Debug, "Expected echo pulse width: {0}µs", echoPulseWidth);
                
                // Small processing delay (~100µs)
                await Task.Delay(TimeSpan.FromMicroseconds(100), measurementCts.Token);
                
                // Raise ECHO pin
                echoPin.Set(true);
                this.Log(LogLevel.Debug, "ECHO pin raised (pulse start)");
                
                // Wait for echo pulse width
                await Task.Delay(TimeSpan.FromMicroseconds(echoPulseWidth), measurementCts.Token);
                
                // Lower ECHO pin
                echoPin.Set(false);
                this.Log(LogLevel.Debug, "ECHO pin lowered (pulse end)");
                
                // Measurement complete
                state = SensorState.ResultReady;
                this.Log(LogLevel.Info, "Measurement complete: {0}cm", distance);
                
                // Return to idle
                await Task.Delay(TimeSpan.FromMilliseconds(60), measurementCts.Token);
                state = SensorState.Idle;
            }
            catch(OperationCanceledException)
            {
                this.Log(LogLevel.Debug, "Measurement cancelled");
                echoPin.Set(false);
                state = SensorState.Idle;
            }
            finally
            {
                measurementCts?.Dispose();
                measurementCts = null;
            }
        }

        private void HandleTimeout()
        {
            this.Log(LogLevel.Debug, "Timeout reached, no echo received");
            state = SensorState.Idle;
        }

        #endregion

        #region IExternal

        public void Attach()
        {
            this.Log(LogLevel.Info, "JSN-SR04T sensor attached to machine");
        }

        public void Detach()
        {
            Reset();
            this.Log(LogLevel.Info, "JSN-SR04T sensor detached from machine");
        }

        #endregion

        #region IPeripheralRegister

        public void Register(Type type, Machine machine)
        {
            // Registration callback if needed
        }

        public void Unregister(Type type, Machine machine)
        {
            Detach();
        }

        #endregion

        #region State Enum

        public enum SensorState
        {
            Idle,
            WaitingTrigger,
            Measuring,
            ResultReady
        }

        #endregion

        #region GPIO Connector Helper

        private class GPIOConnector : IGPIOConnector
        {
            private bool value;
            private readonly List<Action<bool>> handlers = new List<Action<bool>>();

            public bool Value => value;

            public event Action<bool> Changed
            {
                add => handlers.Add(value);
                remove => handlers.Remove(value);
            }

            public void Set(bool value)
            {
                if(this.value != value)
                {
                    this.value = value;
                    foreach(var handler in handlers)
                    {
                        handler(value);
                    }
                }
            }

            public void Connect(Action<bool> handler)
            {
                handlers.Add(handler);
            }

            public void Disconnect(Action<bool> handler)
            {
                handlers.Remove(handler);
            }

            public bool IsConnected => handlers.Count > 0;
        }

        #endregion
    }

    /// <summary>
    /// Plugin entry point for Renode
    /// </summary>
    public class JSN_SR04TPlugin : IPlugin
    {
        public void Initialize()
        {
            // This is called when the plugin is loaded
            // Register the sensor type if needed
        }

        public void Reset()
        {
            // Called on plugin reset
        }
    }
}
