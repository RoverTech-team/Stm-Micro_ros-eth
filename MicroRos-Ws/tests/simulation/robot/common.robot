*** Settings ***
Documentation     Common keywords and settings for Renode simulation tests
...               This resource file provides reusable keywords for Ethernet,
...               micro-ROS, and FreeRTOS testing in Renode simulation environment.

Library           Process
Library           OperatingSystem
Library           String
Library           Collections

Variables         variables.py

*** Variables ***
${RENODE_PATH}           ${CURDIR}/../../../Renode.app/Contents/MacOS/renode
${RENODE_SCRIPT}         ${CURDIR}/../renode/stm32h7_eth.resc
${FIRMWARE_PATH}         ${CURDIR}/../../../build/stm32h7_eth.elf
${TIMEOUT_BOOT}          30s
${TIMEOUT_UART}          10s
${TIMEOUT_NETWORK}       15s
${TIMEOUT_MICROROS}      20s
${UART_TIMEOUT}          5
${DEFAULT_IP}            192.168.1.100
${AGENT_IP}              192.168.1.10
${AGENT_PORT}            8888
${TEST_UDP_PORT}         12345

${PROMPT}                >
${BOOT_INDICATOR}        FreeRTOS scheduler started
${ETH_LINK_UP}           Ethernet link up
${DHCP_SUCCESS}          DHCP: Got IP address
${MICROROS_CONNECTED}    micro-ROS agent connected
${MICROROS_DISCONNECTED}    micro-ROS agent disconnected

*** Keywords ***

Setup Renode Simulation
    [Documentation]    Initialize Renode simulation environment
    ...                Creates necessary directories and verifies dependencies
    
    Create Directory    ${OUTPUT_DIR}
    Set Suite Variable    ${SIMULATION_LOG}    ${OUTPUT_DIR}/simulation.log
    Set Suite Variable    ${UART_LOG}    ${OUTPUT_DIR}/uart_output.log
    
    ${renode_exists}=    Run Keyword And Return Status    File Should Exist    ${RENODE_PATH}
    IF    not ${renode_exists}    Fatal Error    Renode not found at ${RENODE_PATH}
    
    ${firmware_exists}=    Run Keyword And Return Status    File Should Exist    ${FIRMWARE_PATH}
    IF    not ${firmware_exists}    Fatal Error    Firmware not found at ${FIRMWARE_PATH}
    
    Log    Renode simulation setup complete    level=INFO

Teardown Renode Simulation
    [Documentation]    Clean up Renode simulation environment
    ...                Stops any running Renode processes and cleans temporary files
    
    Terminate All Processes    kill=True
    Log    Renode simulation teardown complete    level=INFO

Start Renode
    [Documentation]    Start Renode with the STM32H7 Ethernet platform
    [Arguments]    ${firmware}=${FIRMWARE_PATH}
    
    ${result}=    Run Process
    ...    ${RENODE_PATH}
    ...    --disable-xwt
    ...    --script ${RENODE_SCRIPT}
    ...    shell=bash
    ...    cwd=${OUTPUT_DIR}
    ...    stdout=${SIMULATION_LOG}
    ...    stderr=STDOUT
    ...    timeout=${TIMEOUT_BOOT}
    
    Set Suite Variable    ${RENODE_PROCESS}    ${result}
    Log    Renode started with PID: ${result.pid}    level=INFO
    RETURN    ${result}

Load Firmware
    [Documentation]    Load firmware binary into the simulated MCU
    [Arguments]    ${firmware_path}=${FIRMWARE_PATH}
    
    File Should Exist    ${firmware_path}
    
    Send Renode Command    sysbus LoadELF @${firmware_path}
    Log    Firmware loaded: ${firmware_path}    level=INFO

Send Renode Command
    [Documentation]    Send a command to the running Renode instance
    [Arguments]    ${command}
    
    ${full_command}=    Set Variable    echo '${command}' | ${RENODE_PATH} -
    ${result}=    Run Process
    ...    bash
    ...    -c
    ...    ${full_command}
    ...    shell=True
    ...    timeout=30s
    
    Log    Sent command: ${command}    level=DEBUG
    RETURN    ${result}

Start Simulation
    [Documentation]    Start the simulation execution
    
    Send Renode Command    start
    Log    Simulation started    level=INFO

Stop Simulation
    [Documentation]    Stop the simulation execution
    
    Send Renode Command    pause
    Log    Simulation stopped    level=INFO

Reset Simulation
    [Documentation]    Reset the simulation to initial state
    
    Send Renode Command    Reset
    Log    Simulation reset    level=INFO

Wait For Boot
    [Documentation]    Wait for the firmware to complete boot sequence
    [Arguments]    ${timeout}=${TIMEOUT_BOOT}
    
    Log    Waiting for boot indicator: ${BOOT_INDICATOR}    level=INFO
    Wait For UART Output    ${BOOT_INDICATOR}    timeout=${timeout}
    Log    Boot sequence completed    level=INFO

Wait For UART Output
    [Documentation]    Wait for specific output on UART
    [Arguments]    ${pattern}    ${timeout}=${TIMEOUT_UART}
    
    ${start_time}=    Get Time    epoch
    ${found}=    Set Variable    ${False}
    
    FOR    ${i}    IN RANGE    0    ${timeout.seconds}
        ${uart_content}=    Get File    ${UART_LOG}    default=empty
        ${found}=    Run Keyword And Return Status
        ...    Should Contain    ${uart_content}    ${pattern}
        Exit For Loop If    ${found}
        Sleep    1s
    END
    
    IF    not ${found}
    ...    Fail    Timeout waiting for UART output: ${pattern}
    
    Log    Found UART output: ${pattern}    level=DEBUG

Check UART Output
    [Documentation]    Check if UART output contains expected pattern
    [Arguments]    ${pattern}
    
    ${uart_content}=    Get File    ${UART_LOG}    default=empty
    Should Contain    ${uart_content}    ${pattern}
    Log    UART output verified: ${pattern}    level=DEBUG

Get UART Output
    [Documentation]    Get the current UART output content
    ${uart_content}=    Get File    ${UART_LOG}    default=empty
    RETURN    ${uart_content}

Clear UART Log
    [Documentation]    Clear the UART log file
    
    Create File    ${UART_LOG}    content=
    Log    UART log cleared    level=DEBUG

Wait For Ethernet Link Up
    [Documentation]    Wait for Ethernet link to come up
    [Arguments]    ${timeout}=${TIMEOUT_NETWORK}
    
    Log    Waiting for Ethernet link up    level=INFO
    Wait For UART Output    ${ETH_LINK_UP}    timeout=${timeout}
    Log    Ethernet link is up    level=INFO

Wait For DHCP Address
    [Documentation]    Wait for DHCP to obtain IP address
    [Arguments]    ${expected_ip}=${DEFAULT_IP}    ${timeout}=${TIMEOUT_NETWORK}
    
    Log    Waiting for DHCP address    level=INFO
    Wait For UART Output    ${DHCP_SUCCESS}    timeout=${timeout}
    Check UART Output    ${expected_ip}
    Log    DHCP address obtained: ${expected_ip}    level=INFO

Send UDP Packet
    [Documentation]    Send a UDP packet to the simulated device
    [Arguments]    ${dest_ip}=${DEFAULT_IP}    ${dest_port}=${TEST_UDP_PORT}    ${data}=test_data
    
    Log    Sending UDP packet to ${dest_ip}:${dest_port}    level=INFO
    
    ${result}=    Run Process
    ...    python3
    ...    ${CURDIR}/../scripts/udp_sender.py
    ...    --ip    ${dest_ip}
    ...    --port    ${dest_port}
    ...    --data    ${data}
    ...    timeout=10s
    
    Log    UDP packet sent: ${data}    level=DEBUG
    RETURN    ${result}

Receive UDP Packet
    [Documentation]    Receive a UDP packet from the simulated device
    [Arguments]    ${listen_port}=${TEST_UDP_PORT}    ${timeout}=10s
    
    Log    Listening for UDP packet on port ${listen_port}    level=INFO
    
    ${result}=    Run Process
    ...    python3
    ...    ${CURDIR}/../scripts/udp_receiver.py
    ...    --port    ${listen_port}
    ...    --timeout    ${timeout.seconds}
    ...    timeout=${timeout}
    
    Log    UDP packet received    level=DEBUG
    RETURN    ${result}

Check MicroROS Connection
    [Documentation]    Verify micro-ROS agent connection status
    [Arguments]    ${expected_status}=connected
    
    ${pattern}=    Set Variable If
    ...    '${expected_status}' == 'connected'
    ...    ${MICROROS_CONNECTED}
    ...    ${MICROROS_DISCONNECTED}
    
    Wait For UART Output    ${pattern}    timeout=${TIMEOUT_MICROROS}
    Log    micro-ROS connection status: ${expected_status}    level=INFO

Start MicroROSAgent
    [Documentation]    Start the micro-ROS agent process
    [Arguments]    ${port}=${AGENT_PORT}
    
    Log    Starting micro-ROS agent on port ${port}    level=INFO
    
    ${result}=    Start Process
    ...    micro-ros-agent
    ...    udp4
    ...    --port    ${port}
    ...    shell=bash
    ...    stdout=${OUTPUT_DIR}/agent_stdout.log
    ...    stderr=${OUTPUT_DIR}/agent_stderr.log
    
    Set Suite Variable    ${AGENT_PROCESS}    ${result}
    Sleep    2s
    Log    micro-ROS agent started    level=INFO

Stop MicroROSAgent
    [Documentation]    Stop the micro-ROS agent process
    
    Run Keyword If Variable Exists    ${AGENT_PROCESS}
    ...    Terminate Process    ${AGENT_PROCESS}
    Log    micro-ROS agent stopped    level=INFO

Create DDS Participant
    [Documentation]    Create a DDS participant for testing
    [Arguments]    ${participant_id}=0
    
    Send Renode Command    microros CreateParticipant ${participant_id}
    Log    DDS participant created: ${participant_id}    level=DEBUG

Create DDS Topic
    [Documentation]    Create a DDS topic for testing
    [Arguments]    ${topic_name}    ${topic_type}=std_msgs/msg/String
    
    Send Renode Command    microros CreateTopic ${topic_name} ${topic_type}
    Log    DDS topic created: ${topic_name}    level=DEBUG

Publish To Topic
    [Documentation]    Publish a message to a DDS topic
    [Arguments]    ${topic_name}    ${message}
    
    Send Renode Command    microros Publish ${topic_name} "${message}"
    Log    Published to ${topic_name}: ${message}    level=DEBUG

Subscribe To Topic
    [Documentation]    Subscribe to a DDS topic
    [Arguments]    ${topic_name}
    
    Send Renode Command    microros Subscribe ${topic_name}
    Log    Subscribed to ${topic_name}    level=DEBUG

Wait For Topic Message
    [Documentation]    Wait for a message on a subscribed topic
    [Arguments]    ${topic_name}    ${timeout}=10s
    
    ${pattern}=    Set Variable    Topic ${topic_name} received:
    Wait For UART Output    ${pattern}    timeout=${timeout}

Simulate Network Disconnect
    [Documentation]    Simulate network disconnection
    
    Send Renode Command    ethernet Disconnect
    Log    Network disconnected    level=INFO

Simulate Network Reconnect
    [Documentation]    Simulate network reconnection
    
    Send Renode Command    ethernet Connect
    Log    Network reconnected    level=INFO

Check Task Status
    [Documentation]    Check the status of a FreeRTOS task
    [Arguments]    ${task_name}
    
    ${pattern}=    Set Variable    Task ${task_name} status:
    Check UART Output    ${pattern}
    Log    Task ${task_name} status checked    level=DEBUG

Wait For Task Start
    [Documentation]    Wait for a specific FreeRTOS task to start
    [Arguments]    ${task_name}    ${timeout}=${TIMEOUT_BOOT}
    
    ${pattern}=    Set Variable    Task ${task_name} started
    Wait For UART Output    ${pattern}    timeout=${timeout}
    Log    Task ${task_name} started    level=INFO

Get Task Count
    [Documentation]    Get the number of running FreeRTOS tasks
    
    ${uart_content}=    Get UART Output
    ${count}=    Get Regexp Matches    ${uart_content}    Total tasks: (\\d+)    1
    RETURN    ${count[0]}

Verify Network Utility
    [Documentation]    Verify network utility function availability
    [Arguments]    ${utility_name}
    
    ${result}=    Run Keyword And Return Status    File Should Exist    ${utility_name}
    IF    not ${result}
    ...    Fail    Network utility not found: ${utility_name}
    Log    Network utility available: ${utility_name}    level=DEBUG

Ping Device
    [Documentation]    Ping the simulated device
    [Arguments]    ${ip_address}=${DEFAULT_IP}    ${count}=3
    
    ${result}=    Run Process
    ...    ping
    ...    -c    ${count}
    ...    ${ip_address}
    ...    timeout=15s
    
    Should Be Equal As Integers    ${result.rc}    0
    Log    Ping successful to ${ip_address}    level=INFO

Generate Test Data
    [Documentation]    Generate random test data of specified size
    [Arguments]    ${size_bytes}=100
    
    ${data}=    Evaluate    ''.join(random.choices(string.ascii_letters + string.digits, k=${size_bytes}))
    RETURN    ${data}

Calculate Checksum
    [Documentation]    Calculate checksum for data integrity verification
    [Arguments]    ${data}
    
    ${checksum}=    Evaluate    hashlib.md5('${data}'.encode()).hexdigest()
    RETURN    ${checksum}

Format Log Message
    [Documentation]    Format a log message with timestamp
    [Arguments]    ${level}    ${message}
    
    ${timestamp}=    Get Time
    ${formatted}=    Set Variable    [${timestamp}] [${level}] ${message}
    RETURN    ${formatted}
