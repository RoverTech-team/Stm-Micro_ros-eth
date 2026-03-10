*** Settings ***
Documentation     JSN-SR04T ultrasonic simulation test suite
Resource          common.robot

Suite Setup       Setup Test Suite
Suite Teardown    Teardown Test Suite

*** Variables ***
${SENSOR_OUTPUT}    ${EMPTY}
${FIRMWARE_OUTPUT}    ${EMPTY}

*** Test Cases ***
F01 - Firmware Loads Successfully
    Assert Result Int Equals    ${FIRMWARE_OUTPUT}    firmware_loaded    1

F02 - Firmware Initialization Complete
    Assert Result Int Equals    ${FIRMWARE_OUTPUT}    firmware_init_ok    1

T01 - Sensor Initialization
    Assert Result Int Equals    ${SENSOR_OUTPUT}    initialized    1
    Assert Result Int Equals    ${SENSOR_OUTPUT}    default_distance    ${DEFAULT_DISTANCE}

T02 - Read Default Distance
    Assert Result Int Equals    ${SENSOR_OUTPUT}    default_distance    ${DEFAULT_DISTANCE}

T03 - Update Distance
    Assert Result Int Equals    ${SENSOR_OUTPUT}    distance_100    100

T04 - Minimum Distance
    Assert Result Int Equals    ${SENSOR_OUTPUT}    distance_min    ${MIN_DISTANCE}
    ${echo_min}=    Extract Result Value    ${SENSOR_OUTPUT}    echo_min
    ${expected}=    Evaluate    ${MIN_DISTANCE} * 58
    Should Be Equal As Integers    ${echo_min}    ${expected}

T05 - Maximum Distance
    Assert Result Int Equals    ${SENSOR_OUTPUT}    distance_max    ${MAX_DISTANCE}
    ${echo_max}=    Extract Result Value    ${SENSOR_OUTPUT}    echo_max
    ${expected}=    Evaluate    ${MAX_DISTANCE} * 58
    Should Be Equal As Integers    ${echo_max}    ${expected}

T06 - Out Of Range Clamping
    Assert Result Int Equals    ${SENSOR_OUTPUT}    clamped_high    ${MAX_DISTANCE}

T07 - Negative Distance Handling
    Assert Result Int At Least    ${SENSOR_OUTPUT}    clamped_negative    0

T08 - Multiple Readings
    Assert Result Int Equals    ${SENSOR_OUTPUT}    sequence_ok    1

T09 - Echo Pulse Width Calculation
    ${echo_20}=    Extract Result Value    ${SENSOR_OUTPUT}    echo_20
    ${echo_50}=    Extract Result Value    ${SENSOR_OUTPUT}    echo_50
    ${echo_100}=    Extract Result Value    ${SENSOR_OUTPUT}    echo_100
    ${echo_200}=    Extract Result Value    ${SENSOR_OUTPUT}    echo_200
    ${echo_450}=    Extract Result Value    ${SENSOR_OUTPUT}    echo_450

    ${exp_20}=    Evaluate    20 * 58
    ${exp_50}=    Evaluate    50 * 58
    ${exp_100}=   Evaluate    100 * 58
    ${exp_200}=   Evaluate    200 * 58
    ${exp_450}=   Evaluate    450 * 58

    Should Be Equal As Integers    ${echo_20}     ${exp_20}
    Should Be Equal As Integers    ${echo_50}     ${exp_50}
    Should Be Equal As Integers    ${echo_100}    ${exp_100}
    Should Be Equal As Integers    ${echo_200}    ${exp_200}
    Should Be Equal As Integers    ${echo_450}    ${exp_450}

T10 - Sensor Reset
    Assert Result Int Equals    ${SENSOR_OUTPUT}    distance_after_reset    ${DEFAULT_DISTANCE}

T11 - Timeout Configuration
    Assert Result Int Equals    ${SENSOR_OUTPUT}    timeout_us    ${DEFAULT_TIMEOUT_US}

T12 - Integration With Platform
    ${result}=    Run Renode Script    run_simulation.resc    timeout=40s
    Should Be Equal As Integers    ${result.rc}    0
    Should Contain    ${result.stdout}    RESULT sim_distance=
    Should Not Contain    ${result.stdout}    exception
    Should Not Contain    ${result.stdout}    Exception
    Should Not Contain    ${result.stdout}    Error

*** Keywords ***
Setup Test Suite
    Prepare Results Directory
    # Run firmware validation first
    ${fw_result}=    Run Renode Script    firmware_validation.resc    timeout=30s
    Should Be Equal As Integers    ${fw_result.rc}    0
    Set Suite Variable    ${FIRMWARE_OUTPUT}    ${fw_result.stdout}
    # Run sensor tests
    ${result}=    Run Renode Script    sensor_test.resc    timeout=30s
    Should Be Equal As Integers    ${result.rc}    0
    Set Suite Variable    ${SENSOR_OUTPUT}    ${result.stdout}

Teardown Test Suite
    No Operation
