*** Settings ***
Documentation     Basic Renode tests for STM32H755 platform
...               Tests that Renode can start and load the platform description

Library           Process
Library           OperatingSystem

Variables         variables.py

Suite Setup       Setup Test
Suite Teardown    Teardown Test

*** Variables ***
${PLATFORM_SCRIPT}       ${CURDIR}/../renode/stm32h755.repl
${TEST_OUTPUT_DIR}       ${CURDIR}/../results

*** Test Cases ***

Renode Executable Exists
    [Documentation]    Verify Renode executable is available
    File Should Exist    ${RENODE_PATH}
    Log    Renode found at: ${RENODE_PATH}

Renode Can Start
    [Documentation]    Test that Renode can start in headless mode
    ${result}=    Run Process
    ...    ${RENODE_PATH}
    ...    --version
    ...    shell=bash
    ...    timeout=30s
    
    Log    Renode version output: ${result.stdout}
    Should Be Equal As Integers    ${result.rc}    0

STM32H755 Platform File Exists
    [Documentation]    Verify STM32H755 platform description file exists
    File Should Exist    ${PLATFORM_SCRIPT}
    Log    Platform file found at: ${PLATFORM_SCRIPT}

Renode Can Load STM32H755 Platform
    [Documentation]    Test that Renode can load the STM32H755 platform description
    ${temp_script}=    Set Variable    ${TEST_OUTPUT_DIR}/load_platform_test.resc
    
    Create Directory    ${TEST_OUTPUT_DIR}
    
    Create File    ${temp_script}    mach create\nmachine LoadPlatformDescription @${PLATFORM_SCRIPT}
    
    ${result}=    Run Process
    ...    ${RENODE_PATH}
    ...    --disable-xwt
    ...    --script    ${temp_script}
    ...    --console
    ...    shell=bash
    ...    timeout=60s
    
    Log    Platform load output: ${result.stdout}
    Log    Platform load errors: ${result.stderr}
    Should Be Equal As Integers    ${result.rc}    0

*** Keywords ***

Setup Test
    [Documentation]    Setup for each test
    Create Directory    ${TEST_OUTPUT_DIR}

Teardown Test
    [Documentation]    Teardown for each test
    Terminate All Processes    kill=True
