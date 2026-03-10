*** Settings ***
Documentation     Renode simulation boot test for STM32H755 CM7 firmware
...               Verifies that Renode can start, load the platform, and run the firmware

Library           Process
Library           OperatingSystem
Library           String

Suite Setup       Setup Test
Suite Teardown    Teardown Test

*** Variables ***
${RENODE_PATH}           ${CURDIR}/../../../Renode.app/Contents/MacOS/renode
${PLATFORM_REPL}         ${CURDIR}/../renode/stm32h755.repl
${FIRMWARE_PATH}         ${CURDIR}/../../../Micro_ros_eth/microroseth/Makefile/CM7/build/MicroRosEth_CM7.elf
${TEST_SCRIPT}           ${CURDIR}/../renode/test_sim.resc
${TEST_OUTPUT_DIR}       ${CURDIR}/../results
${SIMULATION_LOG}        ${TEST_OUTPUT_DIR}/sim_boot.log
${SIM_DURATION}          5

*** Test Cases ***

Renode Executable Exists
    [Documentation]    Verify Renode executable is available
    File Should Exist    ${RENODE_PATH}
    Log    Renode found at: ${RENODE_PATH}

Renode Can Start
    [Documentation]    Test that Renode can start and report version
    ${result}=    Run Process
    ...    ${RENODE_PATH}
    ...    --version
    ...    shell=bash
    ...    timeout=30s
    
    Log    Renode version output: ${result.stdout}
    Should Be Equal As Integers    ${result.rc}    0

STM32H755 Platform File Exists
    [Documentation]    Verify STM32H755 platform description file exists
    File Should Exist    ${PLATFORM_REPL}
    Log    Platform file found at: ${PLATFORM_REPL}

CM7 Firmware File Exists
    [Documentation]    Verify CM7 firmware file exists
    File Should Exist    ${FIRMWARE_PATH}
    Log    Firmware found at: ${FIRMWARE_PATH}

Renode Can Load STM32H755 Platform
    [Documentation]    Test that Renode can load the STM32H755 platform description
    ${temp_script}=    Set Variable    ${TEST_OUTPUT_DIR}/load_platform_test.resc
    
    Create Directory    ${TEST_OUTPUT_DIR}
    
    Create File    ${temp_script}    mach create\nmachine LoadPlatformDescription @${PLATFORM_REPL}
    
    ${result}=    Run Process
    ...    ${RENODE_PATH}
    ...    --disable-xwt
    ...    --console
    ...    ${temp_script}
    ...    shell=bash
    ...    timeout=60s
    
    Log    Platform load output: ${result.stdout}
    Log    Platform load errors: ${result.stderr}
    Should Be Equal As Integers    ${result.rc}    0

Renode Can Load CM7 Firmware
    [Documentation]    Test that Renode can load the CM7 firmware onto the platform
    ${temp_script}=    Set Variable    ${TEST_OUTPUT_DIR}/load_firmware_test.resc
    
    Create File    ${temp_script}    mach create\nmachine LoadPlatformDescription @${PLATFORM_REPL}\nsysbus LoadELF @${FIRMWARE_PATH}
    
    ${result}=    Run Process
    ...    ${RENODE_PATH}
    ...    --disable-xwt
    ...    --console
    ...    ${temp_script}
    ...    shell=bash
    ...    timeout=60s
    
    Log    Firmware load output: ${result.stdout}
    Log    Firmware load errors: ${result.stderr}
    Should Be Equal As Integers    ${result.rc}    0

Simulation Runs Without Crashing
    [Documentation]    Test that simulation runs for 5 seconds without crashing
    ${run_script}=    Set Variable    ${TEST_OUTPUT_DIR}/run_sim_test.resc
    
    Create File    ${run_script}    mach create\nmachine LoadPlatformDescription @${PLATFORM_REPL}\nsysbus LoadELF @${FIRMWARE_PATH}\nstart\nsleep ${SIM_DURATION}\nquit
    
    ${result}=    Run Process
    ...    ${RENODE_PATH}
    ...    --disable-xwt
    ...    --console
    ...    ${run_script}
    ...    shell=bash
    ...    timeout=120s
    ...    stdout=${SIMULATION_LOG}
    ...    stderr=STDOUT
    
    Log    Simulation output saved to: ${SIMULATION_LOG}
    
    ${log_content}=    Get File    ${SIMULATION_LOG}
    Log    Simulation log: ${log_content}
    
    Should Contain Any    ${log_content}    Starting    Started    msg=Simulation did not start properly
    Should Not Contain    ${log_content}    exception
    Should Not Contain    ${log_content}    Exception
    Should Not Contain    ${log_content}    EXCEPTION
    Should Not Contain    ${log_content}    crash
    Should Not Contain    ${log_content}    Crash
    Should Not Contain    ${log_content}    CRASH

*** Keywords ***

Setup Test
    [Documentation]    Setup for test suite
    Create Directory    ${TEST_OUTPUT_DIR}

Teardown Test
    [Documentation]    Teardown for test suite
    Terminate All Processes    kill=True
