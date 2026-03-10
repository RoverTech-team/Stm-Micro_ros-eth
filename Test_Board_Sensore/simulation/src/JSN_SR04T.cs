//
// JSN-SR04T / AJ-SR04M Ultrasonic Distance Sensor Model for Renode
//
// This model simulates the waterproof ultrasonic sensor JSN-SR04T (and AJ-SR04M)
// operating in Mode 1 (Digital/Pulse) communication.
//
// Communication Protocol (Mode 1):
// 1. MCU sends 10µs HIGH pulse on TRIG pin
// 2. Sensor emits ultrasonic burst
// 3. Sensor outputs pulse on ECHO pin (width = distance × 58µs)
// 4. MCU measures ECHO pulse width to calculate distance
//
// Hardware Configuration:
// - TRIG: Output pin (PD1 in this implementation)
// - ECHO: Input pin (PD0 in this implementation)
// - Range: 20-450cm
// - Timeout: 30ms (no echo detected)
//
// Author: Generated for Test_Board_Sensore
// Date: 2026-03-03
//

using System;
using System.Threading;
using Antmicro.Renode.Core;
using Antmicro.Renode.Core.Structure;
using Antmicro.Renode.Core.Structure.Registers;
using Antmicro.Renode.Debugging;
using Antmicro.Renode.Displays;
using Antmicro.Renode.Exceptions;
using Antmicro.Renode.Extensions;
using Antmicro.Renode.Extensions.Analyzers;
using Antmicro.Renode.Logging;
using Antmicro.Renode.Peripherals;
using Antmicro.Renode.Peripherals.GPIO;
using Antmicro.Renode.Plugins.AliasPlugin;
using Antmicro.Renode.Time;

namespace Antmicro.Renode.Plugins.SensorPlugin
{
    /// <summary>
    /// JSN-SR04T / AJ-SR04M Waterproof Ultrasonic Distance Sensor Model
    /// 
    /// Simulates the sensor operating in Mode 1 (Digital/Pulse) communication.
    /// </summary>
    public class JSN_SR04T : IExternal, IHasGPIO, IConnectable
    {
        /// <summary>
        /// Creates a new JSN-SR04T sensor instance.
        /// </summary>
        public JSN_SR04T(Machine machine, string name)
        {
            this.machine = machine;
            this.name = name;
            
            // Initialize default values
            distance = 50;           // Default: 50cm
            minDistance = 20;       // Minimum measurable distance
            maxDistance = 450;       // Maximum measurable distance
            timeout = 30000;        // 30ms timeout in microseconds
            triggerPulseWidth = 10; // 10µs trigger pulse required
            
            // State machine
            state = State.Idle;
            
            // GPIO pins
            trigPin = new GPIO();
            echoPin = new GPIO();
            
            // Register GPIO callbacks
            trigPin.ValueChanged += OnTrigPinChanged;
            
            this.Log(LogLevel.Info, "JSN-SR04T sensor created with default distance: {0}cm", distance);
        }

        #region Properties

        /// <summary>
        /// Current measured distance in centimeters.
        /// </summary>
        public int Distance
        {
            get => distance;
            set
            {
                if (value < 0)
                {
                    this.Log(LogLevel.Warning, "Distance cannot be negative, setting to 0");
                    distance = 0;
                }
                else if (value > maxDistance)
                {
                    this.Log(LogLevel.Debug, "Distance {0}cm exceeds maximum {1}cm, clamping", value, maxDistance);
                    distance = maxDistance;
                }
                else
                {
                    distance = value;
                }
                this.Log(LogLevel.Debug, "Distance set to: {0}cm", distance);
            }
        }

        /// <summary>
        /// Minimum measurable distance in centimeters.
        /// </summary>
        public int MinDistance => minDistance;

        /// <summary>
        /// Maximum measurable distance in centimeters.
        /// </summary>
        public int MaxDistance => maxDistance;

        /// <summary>
        /// Timeout in microseconds when no echo is received.
        /// Default: 30000µs (30ms)
        /// </summary>
        public int Timeout
        {
            get => timeout;
            set => timeout = value;
        }

        /// <summary>
        /// Required trigger pulse width in microseconds.
        /// </summary>
        public int TriggerPulseWidth => triggerPulseWidth;

        /// <summary>
        /// TRIG GPIO pin (connected to MCU).
        /// </summary>
        public IGPIOConnector TrigPin => trigPin;

        /// <summary>
        /// ECHO GPIO pin (connected to MCU).
        /// </summary>
        public IGPIOConnector EchoPin => echoPin;

        /// <summary>
        /// Returns the expected ECHO pulse width for the current distance.
        /// Formula: width(µs) = distance(cm) × 58
        /// </summary>
        public int ExpectedEchoPulseWidth => distance * 58;

        #endregion

        #region Public Methods

        /// <summary>
        /// Sets the simulated distance.
        /// </summary>
        /// <param name="newDistance">Distance in centimeters (0-450)</param>
        public void SetDistance(int newDistance)
        {
            Distance = newDistance;
        }

        /// <summary>
        /// Gets the current simulated distance.
        /// </summary>
        /// <returns>Distance in centimeters</returns>
        public int GetDistance()
        {
            return distance;
        }

        /// <summary>
        /// Resets the sensor to idle state.
        /// </summary>
        public void Reset()
        {
            state = State.Idle;
            trigPin.Set(false);
            echoPin.Set(false);
            this.Log(LogLevel.Info, "Sensor reset to idle state");
        }

        /// <summary>
        /// Triggers a measurement (for testing purposes).
        /// </summary>
        public void TriggerMeasurement()
        {
            if (state != State.Idle)
            {
                this.Log(LogLevel.Debug, "Measurement already in progress, ignoring trigger");
                return;
            }

            PerformMeasurement();
        }

        #endregion

        #region Private Methods

        private void OnTrigPinChanged(bool value)
        {
            if (value)
            {
                // Rising edge on TRIG - start trigger detection
                StartTriggerDetection();
            }
            else
            {
                // Falling edge on TRIG
                if (state == State.WaitingTrigger)
                {
                    // Record trigger pulse width
                    triggerPulseStartTime = Time.ElapsedMicroseconds;
                }
            }
        }

        private void StartTriggerDetection()
        {
            if (state != State.Idle)
            {
                this.Log(LogLevel.Debug, "Ignoring trigger, state: {0}", state);
                return;
            }

            this.Log(LogLevel.Debug, "TRIG rising edge detected, starting trigger validation");
            state = State.WaitingTrigger;
            triggerPulseStartTime = Time.ElapsedMicroseconds;
        }

        private void CheckTriggerPulse()
        {
            long pulseWidth = Time.ElapsedMicroseconds - triggerPulseStartTime;
            
            if (pulseWidth >= triggerPulseWidth - 1 && pulseWidth <= triggerPulseWidth + 1)
            {
                // Valid trigger pulse (10µs ± 1µs tolerance)
                this.Log(LogLevel.Debug, "Valid trigger pulse detected: {0}µs", pulseWidth);
                PerformMeasurement();
            }
            else
            {
                // Invalid trigger pulse width
                this.Log(LogLevel.Debug, "Invalid trigger pulse width: {0}µs (expected ~{1}µs)", pulseWidth, triggerPulseWidth);
                state = State.Idle;
            }
        }

        private void PerformMeasurement()
        {
            state = State.Measuring;
            this.Log(LogLevel.Debug, "Starting measurement for distance: {0}cm", distance);

            if (distance == 0 || distance < minDistance)
            {
                // No object detected - trigger timeout
                this.Log(LogLevel.Debug, "Distance out of range, simulating timeout");
                HandleTimeout();
                return;
            }

            // Calculate echo pulse width
            int echoPulseWidth = distance * 58; // µs
            this.Log(LogLevel.Debug, "Expected echo pulse width: {0}µs", echoPulseWidth);

            state = State.ResultReady;
            this.Log(LogLevel.Info, "Measurement complete: {0}cm (echo width: {1}µs)", distance, ExpectedEchoPulseWidth);

            // Return to idle after a short delay
            state = State.Idle;
        }

        private void HandleTimeout()
        {
            if (state == State.Measuring)
            {
                this.Log(LogLevel.Debug, "Timeout reached, no echo received");
                state = State.Idle;
            }
        }

        #endregion

        #region Fields

        private readonly Machine machine;
        private readonly string name;
        private readonly GPIO trigPin;
        private readonly GPIO echoPin;

        private int distance;
        private readonly int minDistance;
        private readonly int maxDistance;
        private int timeout;
        private readonly int triggerPulseWidth;

        private State state;
        private long triggerPulseStartTime;

        private TimeSource Time => machine.TimeSource;

        #endregion

        #region State Machine

        private enum State
        {
            Idle,              // Waiting for trigger
            WaitingTrigger,    // Waiting for trigger pulse to complete
            Measuring,         // Actively measuring (sent echo, waiting for return)
            ResultReady        // Measurement complete
        }

        #endregion

        #region IExternal

        public void Attach()
        {
            this.Log(LogLevel.Info, "JSN-SR04T sensor attached to machine");
        }

        public void Detach()
        {
            this.Log(LogLevel.Info, "JSN-SR04T sensor detached from machine");
        }

        #endregion

        #region IConnectable

        public void RegisterSocket(Endpoint socket, Object owner)
        {
            // Not used for GPIO-based sensors
        }

        public void UnregisterSocket(Endpoint socket)
        {
            // Not used for GPIO-based sensors
        }

        #endregion

        #region Helper class for sensor pins

        private class GPIO : IGPIOConnector
        {
            public bool Value { get; private set; }
            
            public event Action<bool> ValueChanged;

            public void Set(bool value)
            {
                if (Value != value)
                {
                    Value = value;
                    ValueChanged?.Invoke(value);
                }
            }

            public void Connect(Action<bool> handler)
            {
                ValueChanged += handler;
            }

            public void Disconnect(Action<bool> handler)
            {
                ValueChanged -= handler;
            }

            public bool IsConnected => ValueChanged != null;
        }

        #endregion
    }
}
