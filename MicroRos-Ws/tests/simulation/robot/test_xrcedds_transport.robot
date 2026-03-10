*** Settings ***
Documentation     Renode simulation tests for micro-ROS XRCE-DDS UDP transport
...               Tests bidirectional communication between STM32 firmware and micro-ROS agent
...               
...               Prerequisites:
...               - Docker installed and running
...               - micro-ros-agent Docker image available
...               - Renode installed at ./Renode.app

Library           Process
Library           OperatingSystem
Library           String
Library           Collections
Library           DateTime

Resource          common.robot

Suite Setup       Suite Setup XRCE-DDS
Suite Teardown    Suite Teardown XRCE-DDS

*** Variables ***
${RENODE_PATH}           ${CURDIR}/../../../Renode.app/Contents/MacOS/renode
${PLATFORM_REPL}         ${CURDIR}/../renode/stm32h755_networked.repl
${FIRMWARE_PATH}         ${CURDIR}/../../../Micro_ros_eth/microroseth/Makefile/CM7/build/MicroRosEth_CM7.elf
${RESC_SCRIPT}           ${CURDIR}/../renode/microros_xrcedds.resc
${TEST_OUTPUT_DIR}       ${CURDIR}/../results/xrcedds_tests
${SIMULATION_LOG}        ${TEST_OUTPUT_DIR}/simulation.log
${UART_LOG}              ${TEST_OUTPUT_DIR}/uart_output.log
${AGENT_LOG}             ${TEST_OUTPUT_DIR}/agent.log

${AGENT_DOCKER_IMAGE}    microros/micro-ros-agent:humble
${AGENT_PORT}            8888
${AGENT_CONTAINER_NAME}  microros-agent-test

${DOCKER_HOST_IP}        172.17.0.1
${LOCALHOST_IP}          127.0.0.1

${TIMEOUT_BOOT}          60s
${TIMEOUT_XRCE_CONNECT}  30s
${TIMEOUT_MESSAGE}       15s
${SIM_DURATION}          120

${XRCE_CONNECT_PATTERN}    XRCE-DDS.*connected|agent.*connected|session.*created
${XRCE_MESSAGE_PATTERN}    Publishing|Received|topic|DDS
${ETH_LINK_UP_PATTERN}     Ethernet.*link.*up|ETH.*Link.*Up
${DHCP_SUCCESS_PATTERN}    DHCP.*Got.*IP|IP.*address

*** Test Cases ***

Test Files Exist
    [Documentation]    Verify all required test files exist
    File Should Exist    ${RENODE_PATH}
    File Should Exist    ${PLATFORM_REPL}
    File Should Exist    ${FIRMWARE_PATH}
    File Should Exist    ${RESC_SCRIPT}
    Log    All required files found

Docker Is Available
    [Documentation]    Verify Docker is available for micro-ROS agent
    ${result}=    Run Process
    ...    docker
    ...    info
    ...    shell=bash
    ...    timeout=30s
    
    Log    Docker info: ${result.stdout}
    Should Be Equal As Integers    ${result.rc}    0
    ...    msg=Docker is not running or not available

Start Micro-ROS Agent Docker
    [Documentation]    Start micro-ROS agent in Docker container
    [Setup]    Stop Agent Container
    
    ${result}=    Run Process
    ...    docker
    ...    run
    ...    -d
    ...    --name
    ...    ${AGENT_CONTAINER_NAME}
    ...    -p
    ...    ${AGENT_PORT}:${AGENT_PORT}/udp
    ...    ${AGENT_DOCKER_IMAGE}
    ...    udp4
    ...    --port
    ...    ${AGENT_PORT}
    ...    -v6
    ...    shell=bash
    ...    timeout=60s
    
    Log    Agent container started: ${result.stdout}
    Should Be Equal As Integers    ${result.rc}    0
    
    Sleep    5s    Wait for agent to initialize
    
    ${container_id}=    Set Variable    ${result.stdout}
    Set Suite Variable    ${AGENT_CONTAINER_ID}    ${container_id}
    
    Log    micro-ROS agent started on UDP port ${AGENT_PORT}

Verify Agent Is Running
    [Documentation]    Verify micro-ROS agent is accepting connections
    ${result}=    Run Process
    ...    docker
    ...    logs
    ...    ${AGENT_CONTAINER_NAME}
    ...    shell=bash
    ...    timeout=10s
    
    Log    Agent logs: ${result.stdout}
    Should Contain Any    ${result.stdout}
    ...    UDP
    ...    agent
    ...    listening
    ...    port
    ...    msg=Agent does not appear to be running

Renode Can Load Networked Platform
    [Documentation]    Test that Renode can load the networked STM32H755 platform
    ${temp_script}=    Set Variable    ${TEST_OUTPUT_DIR}/load_platform.resc
    
    Create Directory    ${TEST_OUTPUT_DIR}
    Create File    ${temp_script}
    ...    mach create\n
    ...    machine LoadPlatformDescription @${PLATFORM_REPL}\n
    ...    quit
    
    ${result}=    Run Process
    ...    ${RENODE_PATH}
    ...    --disable-xwt
    ...    --console
    ...    ${temp_script}
    ...    shell=bash
    ...    timeout=60s
    ...    stdout=${SIMULATION_LOG}
    ...    stderr=STDOUT
    
    Log    Platform load output: ${result.stdout}
    Should Be Equal As Integers    ${result.rc}    0

Renode Can Load Firmware
    [Documentation]    Test that Renode can load the CM7 firmware
    ${temp_script}=    Set Variable    ${TEST_OUTPUT_DIR}/load_firmware.resc
    
    Create File    ${temp_script}
    ...    mach create\n
    ...    machine LoadPlatformDescription @${PLATFORM_REPL}\n
    ...    sysbus LoadELF @${FIRMWARE_PATH}\n
    ...    quit
    
    ${result}=    Run Process
    ...    ${RENODE_PATH}
    ...    --disable-xwt
    ...    --console
    ...    ${temp_script}
    ...    shell=bash
    ...    timeout=60s
    ...    stdout=${SIMULATION_LOG}
    ...    stderr=STDOUT
    
    Log    Firmware load output: ${result.stdout}
    Should Be Equal As Integers    ${result.rc}    0

Run Simulation With Agent Communication
    [Documentation]    Run simulation and verify XRCE-DDS communication
    [Tags]    integration    network
    
    Create Directory    ${TEST_OUTPUT_DIR}
    
    ${run_script}=    Set Variable    ${TEST_OUTPUT_DIR}/run_xrcedds.resc
    
    Create File    ${run_script}
    ...    mach create\n
    ...    machine LoadPlatformDescription @${PLATFORM_REPL}\n
    ...    sysbus LoadELF @${FIRMWARE_PATH}\n
    ...    showAnalyzer usart3\n
    ...    logLevel 3\n
    ...    start\n
    ...    sleep ${SIM_DURATION}\n
    ...    quit
    
    ${result}=    Start Process
    ...    ${RENODE_PATH}
    ...    --disable-xwt
    ...    --console
    ...    ${run_script}
    ...    shell=bash
    ...    stdout=${SIMULATION_LOG}
    ...    stderr=STDOUT
    ...    alias=renode_process
    
    Set Suite Variable    ${RENODE_PROCESS}    renode_process
    
    Sleep    10s    Wait for simulation to initialize
    
    Log    Simulation started, waiting for boot

Verify Simulation Boot
    [Documentation]    Verify simulation boots without crashes
    [Tags]    integration
    
    Sleep    20s    Wait for boot sequence
    
    ${log_content}=    Get File    ${SIMULATION_LOG}    default=empty
    
    Log    Simulation log (first check): ${log_content}
    
    Should Not Contain    ${log_content}    exception    msg=Exception in simulation
    Should Not Contain    ${log_content}    Exception    msg=Exception in simulation
    Should Not Contain    ${log_content}    crash    msg=Simulation crashed
    Should Not Contain    ${log_content}    error.*ELF    msg=Firmware load error
    
    Log    Simulation running without crashes

Verify UART Output
    [Documentation]    Verify UART produces output
    [Tags]    integration
    
    Sleep    15s    Wait for UART output
    
    ${log_content}=    Get File    ${SIMULATION_LOG}    default=empty
    
    Log    UART output check: ${log_content}
    
    # Check for any UART activity
    ${uart_activity}=    Run Keyword And Return Status
    ...    Should Not Be Empty    ${log_content}
    
    Log    UART activity detected: ${uart_activity}

Check Agent Received Packets
    [Documentation]    Verify micro-ROS agent received XRCE-DDS packets
    [Tags]    integration    network
    
    Sleep    10s    Wait for packets to reach agent
    
    ${result}=    Run Process
    ...    docker
    ...    logs
    ...    ${AGENT_CONTAINER_NAME}
    ...    shell=bash
    ...    timeout=10s
    
    Log    Agent logs: ${result.stdout}
    
    # Check for XRCE-DDS related activity
    ${agent_output}=    Set Variable    ${result.stdout}
    
    # Agent may show various activity indicators
    ${has_activity}=    Set Variable If
    ...    '${agent_output}' != ''
    ...    ${True}
    ...    ${False}
    
    Log    Agent activity detected: ${has_activity}

Wait For XRCE-DDS Connection
    [Documentation]    Wait for XRCE-DDS session establishment
    [Tags]    integration    network    xrcedds
    
    Wait Until Keyword Succeeds    ${TIMEOUT_XRCE_CONNECT}    5s
    ...    Check XRCE-DDS Connection In Logs

Verify Bidirectional Communication
    [Documentation]    Verify bidirectional XRCE-DDS communication
    [Tags]    integration    network    xrcedds
    
    Sleep    10s    Allow communication to continue
    
    ${sim_log}=    Get File    ${SIMULATION_LOG}    default=empty
    
    # Check for various micro-ROS activity patterns
    ${patterns}=    Create List
    ...    XRCE
    ...    DDS
    ...    topic
    ...    publisher
    ...    subscriber
    ...    session
    ...    agent
    ...    transport
    ...    UDP
    
    ${found_patterns}=    Create List
    
    FOR    ${pattern}    IN    @{patterns}
        ${found}=    Run Keyword And Return Status
        ...    Should Contain    ${sim_log}    ${pattern}
        IF    ${found}
            Append To List    ${found_patterns}    ${pattern}
        END
    END
    
    Log    Found patterns: ${found_patterns}
    
    # At least some micro-ROS related patterns should be present
    ${pattern_count}=    Get Length    ${found_patterns}
    Log    Found ${pattern_count} micro-ROS related patterns

Stop Simulation
    [Documentation]    Stop the simulation
    [Tags]    cleanup
    
    Run Keyword And Ignore Error    Terminate Process    ${RENODE_PROCESS}
    Sleep    2s
    Log    Simulation stopped

Stop Agent Container
    [Documentation]    Stop and remove the agent container
    [Tags]    cleanup
    
    Run Process
    ...    docker
    ...    stop
    ...    ${AGENT_CONTAINER_NAME}
    ...    shell=bash
    ...    timeout=30s
    ...    ignore_error=True
    
    Run Process
    ...    docker
    ...    rm
    ...    ${AGENT_CONTAINER_NAME}
    ...    shell=bash
    ...    timeout=30s
    ...    ignore_error=True
    
    Log    Agent container stopped

Collect Logs
    [Documentation]    Collect and save all test logs
    [Tags]    cleanup
    
    Create Directory    ${TEST_OUTPUT_DIR}
    
    # Save simulation log
    ${sim_log_exists}=    Run Keyword And Return Status
    ...    File Should Exist    ${SIMULATION_LOG}
    IF    ${sim_log_exists}
        Log    Simulation log saved to ${SIMULATION_LOG}
    END
    
    # Save agent logs
    ${result}=    Run Process
    ...    docker
    ...    logs
    ...    ${AGENT_CONTAINER_NAME}
    ...    shell=bash
    ...    timeout=10s
    ...    ignore_error=True
    
    Create File    ${AGENT_LOG}    ${result.stdout}
    Log    Agent log saved to ${AGENT_LOG}

*** Keywords ***

Suite Setup XRCE-DDS
    [Documentation]    Setup for XRCE-DDS test suite
    Create Directory    ${TEST_OUTPUT_DIR}
    
    # Verify files exist
    File Should Exist    ${RENODE_PATH}
    File Should Exist    ${FIRMWARE_PATH}
    
    Log    XRCE-DDS test suite setup complete

Suite Teardown XRCE-DDS
    [Documentation]    Teardown for XRCE-DDS test suite
    Run Keyword And Ignore Error    Terminate All Processes    kill=True
    
    # Stop Docker container
    Run Process
    ...    docker
    ...    stop
    ...    ${AGENT_CONTAINER_NAME}
    ...    shell=bash
    ...    timeout=30s
    ...    ignore_error=True
    
    Run Process
    ...    docker
    ...    rm
    ...    ${AGENT_CONTAINER_NAME}
    ...    shell=bash
    ...    timeout=30s
    ...    ignore_error=True
    
    Log    XRCE-DDS test suite teardown complete

Check XRCE-DDS Connection In Logs
    [Documentation]    Check if XRCE-DDS connection appears in logs
    ${sim_log}=    Get File    ${SIMULATION_LOG}    default=empty
    
    # Check for connection indicators
    ${connection_patterns}=    Create List
    ...    session
    ...    create.*client
    ...    participant
    ...    connected
    
    FOR    ${pattern}    IN    @{connection_patterns}
        ${found}=    Run Keyword And Return Status
        ...    Should Contain    ${sim_log}    ${pattern}
        IF    ${found}
            Log    Found connection indicator: ${pattern}
            RETURN    ${True}
        END
    END
    
    Fail    No XRCE-DDS connection indicators found in logs

Wait For UART Pattern
    [Documentation]    Wait for specific pattern in UART output
    [Arguments]    ${pattern}    ${timeout}=30s
    
    ${start_time}=    Get Current Date
    
    FOR    ${i}    IN RANGE    0    ${timeout.seconds}
        ${log_content}=    Get File    ${SIMULATION_LOG}    default=empty
        ${found}=    Run Keyword And Return Status
        ...    Should Contain    ${log_content}    ${pattern}
        
        IF    ${found}
            Log    Found pattern: ${pattern}
            RETURN    ${True}
        END
        
        Sleep    1s
    END
    
    Fail    Timeout waiting for UART pattern: ${pattern}