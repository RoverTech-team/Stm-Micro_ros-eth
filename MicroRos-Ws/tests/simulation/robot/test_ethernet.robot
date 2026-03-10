*** Settings ***
Documentation     Ethernet communication tests for STM32H7 micro-ROS project
...               Tests Ethernet link, DHCP client, and UDP communication
...               using Renode simulation environment.

Resource          common.robot

Suite Setup       Setup Ethernet Test Suite
Suite Teardown    Teardown Ethernet Test Suite

Test Setup        Reset Simulation State
Test Teardown     Log Test Results

Test Timeout      60s

*** Variables ***
${ETH_INTERFACE}       eth0
${TEST_MESSAGE}        Hello STM32H7
${LARGE_PACKET_SIZE}   1024

*** Test Cases ***

Ethernet Link Comes Up
    [Documentation]    Verify Ethernet link is established after boot
    [Tags]    ethernet    boot    critical
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up    timeout=20s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    ETH link up
    Log    Ethernet link up verified    level=INFO

DHCP Client Gets IP Address
    [Documentation]    Verify DHCP client obtains IP address
    [Tags]    ethernet    dhcp    critical
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    
    Wait For DHCP Address    expected_ip=192.168.1.100    timeout=30s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    DHCP: Got IP address 192.168.1.100
    Log    DHCP address verified    level=INFO

Can Send UDP Packet
    [Documentation]    Verify UDP packet transmission works
    [Tags]    ethernet    udp    send
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    
    Send UDP Packet
    ...    dest_ip=192.168.1.100
    ...    dest_port=12345
    ...    data=${TEST_MESSAGE}
    
    Wait For UART Output    UDP packet sent    timeout=10s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    UDP sent: ${TEST_MESSAGE}
    Log    UDP send verified    level=INFO

Can Receive UDP Packet
    [Documentation]    Verify UDP packet reception works
    [Tags]    ethernet    udp    receive
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    
    ${result}=    Receive UDP Packet    listen_port=12345    timeout=10s
    
    Wait For UART Output    UDP packet received    timeout=10s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    UDP received
    Log    UDP receive verified    level=INFO

Handles Link Down Gracefully
    [Documentation]    Verify system handles Ethernet link disconnection
    [Tags]    ethernet    resilience    critical
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    
    Simulate Network Disconnect
    
    Wait For UART Output    ETH link down    timeout=10s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    ETH link down
    
    Send UDP Packet    data=test_after_disconnect
    
    Wait For UART Output    UDP send failed    timeout=10s
    
    Simulate Network Reconnect
    Wait For Ethernet Link Up
    
    Log    Link down handling verified    level=INFO

UDP Large Packet Transfer
    [Documentation]    Verify large UDP packet handling
    [Tags]    ethernet    udp    stress
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    
    ${large_data}=    Generate Test Data    size_bytes=${LARGE_PACKET_SIZE}
    
    Send UDP Packet
    ...    dest_ip=192.168.1.100
    ...    dest_port=12345
    ...    data=${large_data}
    
    Wait For UART Output    UDP packet sent    timeout=15s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    UDP sent
    Log    Large packet transfer verified    level=INFO

Multiple UDP Packets
    [Documentation]    Verify multiple consecutive UDP packets
    [Tags]    ethernet    udp    stress
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    
    FOR    ${i}    IN RANGE    0    10
        Send UDP Packet
        ...    dest_ip=192.168.1.100
        ...    dest_port=12345
        ...    data=test_packet_${i}
        Sleep    100ms
    END
    
    FOR    ${i}    IN RANGE    0    10
        Wait For UART Output    test_packet_${i}    timeout=5s
    END
    
    Log    Multiple UDP packets verified    level=INFO

Static IP Configuration
    [Documentation]    Verify static IP configuration when DHCP fails
    [Tags]    ethernet    ip    fallback
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    
    Wait For UART Output    Using static IP    timeout=35s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Static IP: 192.168.1.200
    Log    Static IP fallback verified    level=INFO

Network Interface Status
    [Documentation]    Verify network interface status reporting
    [Tags]    ethernet    status
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    
    Wait For UART Output    Network interface status    timeout=10s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    IP: 192.168.1.100
    Should Contain    ${uart_output}    Netmask: 255.255.255.0
    Should Contain    ${uart_output}    Gateway: 192.168.1.1
    Log    Network status verified    level=INFO

*** Keywords ***

Setup Ethernet Test Suite
    [Documentation]    Setup Ethernet test suite
    
    Setup Renode Simulation
    Log    Ethernet test suite setup complete    level=INFO

Teardown Ethernet Test Suite
    [Documentation]    Teardown Ethernet test suite
    
    Teardown Renode Simulation
    Log    Ethernet test suite teardown complete    level=INFO

Reset Simulation State
    [Documentation]    Reset simulation state before each test
    
    Clear UART Log
    Log    Simulation state reset    level=DEBUG

Log Test Results
    [Documentation]    Log test results after each test
    
    ${uart_output}=    Get UART Output
    Log    UART Output:\n${uart_output}    level=DEBUG
