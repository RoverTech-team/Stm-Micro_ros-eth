*** Settings ***
Documentation     Common utilities for JSN-SR04T simulation tests

Library           Collections
Library           OperatingSystem
Library           Process
Library           String
Library           BuiltIn

*** Variables ***
${SIM_ROOT}               ${CURDIR}/../..
${RESULTS_DIR}            ${SIM_ROOT}/results
${RENODE_PATH_ENV}        %{RENODE_PATH=}
${RENODE_APP_PATH}        ${SIM_ROOT}/../Renode.app/Contents/MacOS/renode
${DEFAULT_DISTANCE}       50
${MIN_DISTANCE}           20
${MAX_DISTANCE}           450
${DEFAULT_TIMEOUT_US}     30000

*** Keywords ***
Get Renode Path
    [Documentation]    Resolve Renode executable path.
    ${has_env}=    Run Keyword And Return Status    File Should Exist    ${RENODE_PATH_ENV}
    IF    ${has_env}
        RETURN    ${RENODE_PATH_ENV}
    END

    ${has_app}=    Run Keyword And Return Status    File Should Exist    ${RENODE_APP_PATH}
    IF    ${has_app}
        RETURN    ${RENODE_APP_PATH}
    END

    Fail    Renode executable not found. Set RENODE_PATH env var or install Renode at ${RENODE_APP_PATH}

Prepare Results Directory
    [Documentation]    Ensure results output directory exists.
    Create Directory    ${RESULTS_DIR}

Run Renode Script
    [Documentation]    Run a script from simulation/scripts and return process result.
    [Arguments]    ${script_name}    ${timeout}=60s

    ${renode}=    Get Renode Path
    ${script_path}=    Set Variable    ${SIM_ROOT}/scripts/${script_name}

    ${result}=    Run Process
    ...    ${renode}
    ...    --disable-xwt
    ...    --console
    ...    ${script_path}
    ...    cwd=${SIM_ROOT}
    ...    shell=False
    ...    timeout=${timeout}

    RETURN    ${result}

Extract Result Value
    [Documentation]    Parse RESULT key=value from script output using Python helper.
    [Arguments]    ${output}    ${key}

    ${temp_file}=    Set Variable    ${RESULTS_DIR}/renode_output.txt

    # Write output to temp file for Python to read
    Create File    ${temp_file}    ${output}

    # Call Python parser
    ${result}=    Run Process
    ...    python
    ...    ${SIM_ROOT}/tests/python/parse_renode_output.py
    ...    ${key}
    ...    ${temp_file}
    ...    shell=False

    Should Be Equal As Integers    ${result.rc}    0
    ...    msg=Failed to extract key '${key}'. Python stderr: ${result.stderr}

    ${value}=    Set Variable    ${result.stdout}
    ${value}=    Strip String    ${value}
    RETURN    ${value}

Assert Result Int Equals
    [Arguments]    ${output}    ${key}    ${expected}
    ${value}=    Extract Result Value    ${output}    ${key}
    ${value_int}=    Convert To Integer    ${value}
    Should Be Equal As Integers    ${value_int}    ${expected}

Assert Result Int At Least
    [Arguments]    ${output}    ${key}    ${min_value}
    ${value}=    Extract Result Value    ${output}    ${key}
    ${value_int}=    Convert To Integer    ${value}
    Should Be True    ${value_int} >= ${min_value}
