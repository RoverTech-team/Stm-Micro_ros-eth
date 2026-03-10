*** Settings ***
Documentation     micro-ROS communication tests for STM32H7 project
...               Tests DDS participant creation, topic publishing,
...               and agent connection handling.

Resource          common.robot

Suite Setup       Setup MicroROS Test Suite
Suite Teardown    Teardown MicroROS Test Suite

Test Setup        Reset Simulation State
Test Teardown     Log Test Results

Test Timeout      90s

*** Variables ***
${TEST_TOPIC_NAME}      /test_topic
${TEST_MESSAGE}         Hello micro-ROS
${PARTICIPANT_ID}       0
${DEFAULT_DOMAIN_ID}    0

*** Test Cases ***

MicroROS Agent Connection Established
    [Documentation]    Verify micro-ROS agent connection is established
    [Tags]    microros    connection    critical
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    
    Wait For UART Output    micro-ROS agent connected    timeout=30s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    micro-ROS agent connected
    Log    micro-ROS agent connection verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

Can Create DDS Participant
    [Documentation]    Verify DDS participant creation
    [Tags]    microros    dds    participant
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    Wait For UART Output    DDS participant created    timeout=15s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Participant ID: ${PARTICIPANT_ID}
    Log    DDS participant creation verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

Can Create DDS Publisher
    [Documentation]    Verify DDS publisher creation
    [Tags]    microros    dds    publisher
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    Wait For UART Output    Publisher created    timeout=15s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Publisher for topic
    Log    DDS publisher creation verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

Can Create DDS Subscriber
    [Documentation]    Verify DDS subscriber creation
    [Tags]    microros    dds    subscriber
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    Wait For UART Output    Subscriber created    timeout=15s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Subscriber for topic
    Log    DDS subscriber creation verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

Can Publish To Topic
    [Documentation]    Verify publishing messages to DDS topic
    [Tags]    microros    dds    publish
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    Wait For UART Output    Publisher created    timeout=15s
    
    Publish To Topic    ${TEST_TOPIC_NAME}    ${TEST_MESSAGE}
    
    Wait For UART Output    Published: ${TEST_MESSAGE}    timeout=10s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Published to ${TEST_TOPIC_NAME}
    Log    Topic publish verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

Can Subscribe To Topic
    [Documentation]    Verify subscribing to DDS topic
    [Tags]    microros    dds    subscribe
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    Wait For UART Output    Subscriber created    timeout=15s
    
    Subscribe To Topic    ${TEST_TOPIC_NAME}
    
    Wait For UART Output    Subscribed to ${TEST_TOPIC_NAME}    timeout=10s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Subscription active
    Log    Topic subscription verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

Can Receive Topic Message
    [Documentation]    Verify receiving messages from subscribed topic
    [Tags]    microros    dds    receive
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    Wait For UART Output    Subscriber created    timeout=15s
    
    Subscribe To Topic    ${TEST_TOPIC_NAME}
    
    Publish To Topic    ${TEST_TOPIC_NAME}    ${TEST_MESSAGE}
    
    Wait For UART Output    Received: ${TEST_MESSAGE}    timeout=15s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Topic message received
    Log    Topic message receive verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

Handles Agent Disconnection
    [Documentation]    Verify system handles micro-ROS agent disconnection
    [Tags]    microros    resilience    critical
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    Stop MicroROSAgent
    
    Wait For UART Output    micro-ROS agent disconnected    timeout=15s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Agent connection lost
    
    Wait For UART Output    Reconnecting    timeout=10s
    
    Log    Agent disconnection handling verified    level=INFO

Agent Reconnection
    [Documentation]    Verify system reconnects to micro-ROS agent
    [Tags]    microros    resilience    reconnection
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    Stop MicroROSAgent
    
    Wait For UART Output    micro-ROS agent disconnected    timeout=15s
    
    Start MicroROSAgent
    
    Wait For UART Output    micro-ROS agent connected    timeout=30s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Reconnection successful
    Log    Agent reconnection verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

Multiple Topics
    [Documentation]    Verify multiple DDS topics can be created
    [Tags]    microros    dds    topics
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    FOR    ${i}    IN RANGE    0    5
        Wait For UART Output    Topic /topic_${i} created    timeout=10s
    END
    
    ${uart_output}=    Get UART Output
    
    FOR    ${i}    IN RANGE    0    5
        Should Contain    ${uart_output}    /topic_${i}
    END
    
    Log    Multiple topics verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

DDS Domain Configuration
    [Documentation]    Verify DDS domain configuration
    [Tags]    microros    dds    domain
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    Wait For UART Output    DDS Domain ID: ${DEFAULT_DOMAIN_ID}    timeout=15s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Domain ID: ${DEFAULT_DOMAIN_ID}
    Log    DDS domain configuration verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

RMW Implementation
    [Documentation]    Verify RMW implementation initialization
    [Tags]    microros    rmw
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    
    Wait For UART Output    RMW implementation initialized    timeout=15s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    RMW: micro-ROS
    Log    RMW implementation verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

Message Serialization
    [Documentation]    Verify message serialization works correctly
    [Tags]    microros    message    serialization
    
    Start MicroROSAgent
    
    Start Renode
    Load Firmware
    Start Simulation
    
    Wait For Boot
    Wait For Ethernet Link Up
    Wait For DHCP Address
    Check MicroROS Connection
    
    ${complex_message}=    Set Variable    {"data": "test_value", "count": 42}
    
    Publish To Topic    ${TEST_TOPIC_NAME}    ${complex_message}
    
    Wait For UART Output    Serialization complete    timeout=10s
    
    ${uart_output}=    Get UART Output
    Should Contain    ${uart_output}    Serialized
    Log    Message serialization verified    level=INFO
    
    [Teardown]    Stop MicroROSAgent

*** Keywords ***

Setup MicroROS Test Suite
    [Documentation]    Setup micro-ROS test suite
    
    Setup Renode Simulation
    Verify Network Utility    micro-ros-agent
    Log    micro-ROS test suite setup complete    level=INFO

Teardown MicroROS Test Suite
    [Documentation]    Teardown micro-ROS test suite
    
    Teardown Renode Simulation
    Log    micro-ROS test suite teardown complete    level=INFO

Reset Simulation State
    [Documentation]    Reset simulation state before each test
    
    Clear UART Log
    Log    Simulation state reset    level=DEBUG

Log Test Results
    [Documentation]    Log test results after each test
    
    ${uart_output}=    Get UART Output
    Log    UART Output:\n${uart_output}    level=DEBUG
