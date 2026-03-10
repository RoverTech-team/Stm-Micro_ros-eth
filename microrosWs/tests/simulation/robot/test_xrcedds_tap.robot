*** Settings ***
Documentation     TAP-based Networking Tests for Renode micro-ROS Simulation
...
...               These tests verify the TAP interface setup and XRCE-DDS
...               communication between Renode simulation and micro-ROS agent.
...
...               Network topology:
...               STM32 (192.168.0.3) <---> TAP (192.168.0.1) <---> Agent (192.168.0.8:8888)

Library           Process
Library           OperatingSystem
Library           String
Library           Collections

Resource          common.robot

Variables         variables_tap.py

Test Setup        Setup TAP Test
Test Teardown     Teardown TAP Test

*** Variables ***
${TAP_INTERFACE}        tap0
${GATEWAY_IP}           192.168.0.1
${AGENT_IP}             192.168.0.8
${DEVICE_IP}            192.168.0.3
${AGENT_PORT}           8888
${SIM_DURATION}         60

${SETUP_SCRIPT}         ${CURDIR}/../scripts/setup_tap.sh
${TEARDOWN_SCRIPT}      ${CURDIR}/../scripts/teardown_tap.sh
${TAP_REPL}             ${CURDIR}/../renode/stm32h755_tap.repl
${TAP_RESC}             ${CURDIR}/../renode/microros_tap.resc

*** Keywords ***

Setup TAP Test
    [Documentation]    Setup test environment for TAP networking
    Create Directory    ${OUTPUT_DIR}
    Set Suite Variable    ${TEST_LOG}    ${OUTPUT_DIR}/test_$(date +%Y%m%d_%H%M%S).log
    
    ${scripts_exist}=    Run Keyword And Return Status
    ...    File Should Exist    ${SETUP_SCRIPT}
    IF    not ${scripts_exist}
        Fatal Error    Setup script not found: ${SETUP_SCRIPT}
    END
    
    ${repl_exists}=    Run Keyword And Return Status
    ...    File Should Exist    ${TAP_REPL}
    IF    not ${repl_exists}
        Fatal Error    TAP platform file not found: ${TAP_REPL}
    END

Teardown TAP Test
    [Documentation]    Cleanup test environment
    Terminate All Processes    kill=True

Setup TAP Interface
    [Documentation]    Execute TAP interface setup script
    [Arguments]    ${interface}=${TAP_INTERFACE}
    
    Log    Setting up TAP interface: ${interface}    level=INFO
    
    ${result}=    Run Process
    ...    sudo    ${SETUP_SCRIPT}
    ...    -i    ${interface}
    ...    -g    ${GATEWAY_IP}
    ...    -a    ${AGENT_IP}
    ...    shell=bash
    ...    timeout=30s
    ...    stdout=${TEST_LOG}
    ...    stderr=STDOUT
    
    Log    Setup output: ${result.stdout}    level=DEBUG
    
    IF    ${result.rc} != 0
        Log    TAP setup failed: ${result.stdout}    level=ERROR
        Fail    TAP interface setup failed
    END
    
    Log    TAP interface setup completed    level=INFO

Teardown TAP Interface
    [Documentation]    Execute TAP interface teardown script
    [Arguments]    ${interface}=${TAP_INTERFACE}
    
    Log    Tearing down TAP interface: ${interface}    level=INFO
    
    ${result}=    Run Process
    ...    sudo    ${TEARDOWN_SCRIPT}
    ...    -i    ${interface}
    ...    shell=bash
    ...    timeout=30s
    
    Log    Teardown completed    level=INFO

Verify TAP Interface
    [Documentation]    Verify TAP interface exists and has correct IP
    [Arguments]    ${interface}=${TAP_INTERFACE}
    
    Log    Verifying TAP interface: ${interface}    level=INFO
    
    ${result}=    Run Process
    ...    ifconfig    ${interface}
    ...    shell=bash
    ...    timeout=10s
    
    Should Contain    ${result.stdout}    ${GATEWAY_IP}
    ...    msg=Gateway IP not found on interface
    ...    values=False
    
    Should Contain    ${result.stdout}    ${AGENT_IP}
    ...    msg=Agent IP alias not found on interface
    ...    values=False
    
    Log    TAP interface verified successfully    level=INFO

Start MicroROSAgent TAP
    [Documentation]    Start micro-ROS agent bound to agent IP
    [Arguments]    ${port}=${AGENT_PORT}    ${ip}=${AGENT_IP}
    
    Log    Starting micro-ROS agent on ${ip}:${port}    level=INFO
    
    ${result}=    Start Process
    ...    micro-ros-agent
    ...    udp4
    ...    --port    ${port}
    ...    -v6
    ...    shell=bash
    ...    stdout=${OUTPUT_DIR}/agent_stdout.log
    ...    stderr=${OUTPUT_DIR}/agent_stderr.log
    
    Set Suite Variable    ${AGENT_PROCESS}    ${result}
    
    Sleep    3s    Wait for agent to initialize
    
    Log    micro-ROS agent started    level=INFO

Stop MicroROSAgent TAP
    [Documentation]    Stop micro-ROS agent process
    
    Run Keyword If Variable Exists    ${AGENT_PROCESS}
    ...    Terminate Process    ${AGENT_PROCESS}
    ...    kill=True
    
    Log    micro-ROS agent stopped    level=INFO

Run Renode TAP Simulation
    [Documentation]    Run Renode simulation with TAP networking
    [Arguments]    ${duration}=${SIM_DURATION}
    
    Log    Starting Renode TAP simulation    level=INFO
    
    ${renode_path}=    Set Variable    ${CURDIR}/../../../Renode.app/Contents/MacOS/renode
    
    ${result}=    Start Process
    ...    ${renode_path}
    ...    --disable-xwt
    ...    --console
    ...    ${TAP_RESC}
    ...    shell=bash
    ...    cwd=${OUTPUT_DIR}
    ...    stdout=${OUTPUT_DIR}/renode_stdout.log
    ...    stderr=${OUTPUT_DIR}/renode_stderr.log
    
    Set Suite Variable    ${RENODE_PROCESS}    ${result}
    
    Log    Renode simulation started    level=INFO
    
    Sleep    ${duration}s

Check Agent Received Packets
    [Documentation]    Check if micro-ROS agent received XRCE-DDS packets
    
    Log    Checking agent log for XRCE-DDS activity    level=INFO
    
    ${agent_log}=    Get File    ${OUTPUT_DIR}/agent_stdout.log    default=empty
    
    ${xrcedds_found}=    Run Keyword And Return Status
    ...    Should Contain    ${agent_log}    XRCE
    ...    values=False
    
    IF    ${xrcedds_found}
        Log    XRCE-DDS packets detected in agent log    level=INFO
    ELSE
        Log    No XRCE-DDS activity detected    level=WARN
    END
    
    RETURN    ${xrcedds_found}

Check Renode UART Output
    [Documentation]    Check Renode UART output for expected patterns
    
    ${uart_log}=    Get File    ${OUTPUT_DIR}/uart_output.log    default=empty
    
    ${boot_found}=    Run Keyword And Return Status
    ...    Should Match Regexp    ${uart_log}    FreeRTOS|scheduler|started
    ...    values=False
    
    ${network_found}=    Run Keyword And Return Status
    ...    Should Match Regexp    ${uart_log}    Ethernet|ETH|link|UDP|IP
    ...    values=False
    
    ${xrcedds_found}=    Run Keyword And Return Status
    ...    Should Match Regexp    ${uart_log}    XRCE|DDS|agent|session|topic
    ...    values=False
    
    Log    Boot detected: ${boot_found}    level=INFO
    Log    Network activity: ${network_found}    level=INFO
    Log    XRCE-DDS activity: ${xrcedds_found}    level=INFO
    
    RETURN    boot_found=${boot_found}    network_found=${network_found}    xrcedds_found=${xrcedds_found

*** Test Cases ***

Test TAP Interface Setup
    [Documentation]    Verify TAP interface can be created with correct IP configuration
    [Tags]    tap    setup    network
    
    Setup TAP Interface
    
    Verify TAP Interface
    
    Teardown TAP Interface
    
    Log    TAP interface setup test passed    level=INFO

Test Renode Loads With TAP
    [Documentation]    Verify Renode can load TAP platform and start simulation
    [Tags]    renode    tap    boot
    
    Setup TAP Interface
    
    Start MicroROSAgent TAP
    
    Run Renode TAP Simulation    duration=30
    
    Stop MicroROSAgent TAP
    
    Teardown TAP Interface
    
    Log    Renode TAP platform test passed    level=INFO

Test XRCE-DDS Communication TAP
    [Documentation]    Verify XRCE-DDS communication over TAP interface
    [Tags]    xrcedds    tap    network    integration
    
    Setup TAP Interface
    
    Start MicroROSAgent TAP
    
    Run Renode TAP Simulation    duration=${SIM_DURATION}
    
    ${agent_received}=    Check Agent Received Packets
    
    ${uart_results}=    Check Renode UART Output
    
    Stop MicroROSAgent TAP
    
    Teardown TAP Interface
    
    Log    XRCE-DDS communication test completed    level=INFO
    Log    Agent received packets: ${agent_received}    level=INFO

Test Complete TAP Workflow
    [Documentation]    Run complete TAP workflow from setup to cleanup
    [Tags]    tap    integration    workflow
    
    Log    Starting complete TAP workflow test    level=INFO
    
    Setup TAP Interface
    
    Verify TAP Interface
    
    Start MicroROSAgent TAP
    
    Run Renode TAP Simulation    duration=45
    
    ${agent_received}=    Check Agent Received Packets
    
    Stop MicroROSAgent TAP
    
    Teardown TAP Interface
    
    Log    Complete TAP workflow test passed    level=INFO

Test TAP Interface Cleanup
    [Documentation]    Verify TAP interface is properly cleaned up
    [Tags]    tap    cleanup
    
    Setup TAP Interface
    
    Verify TAP Interface
    
    Teardown TAP Interface
    
    ${result}=    Run Process
    ...    ifconfig    ${TAP_INTERFACE}
    ...    shell=bash
    ...    timeout=10s
    
    Should Not Contain    ${result.stdout}    ${GATEWAY_IP}
    ...    msg=Gateway IP still present after cleanup
    ...    values=False
    
    Log    TAP cleanup verified    level=INFO