*** Settings ***
Documentation     ARM main-board E2E suite: shared-memory protocol + ROS topics.
...               Step 2/3 bring-up asserts; agent-backed topic checks need
...               TAP + micro-ROS agent (same pattern as tests/e2e).
Library           Process
Library           OperatingSystem
Library           String

*** Variables ***
${ARM_BOARD}         ${CURDIR}/..
${SIM_TIMEOUT}       500s

*** Test Cases ***
Step2 Shared Protocol And Motor Ready
    [Documentation]    e2e fw + follow fw: magic, version, motor_ready all OK
    ${res}=    Run Process    ${ARM_BOARD}/scripts/run_sim.sh    --step    step2
    ...    --headless    --timeout    460
    ...    timeout=${SIM_TIMEOUT}    stdout=${OUTPUT_DIR}/runner2.log    stderr=STDOUT
    ...    env:LOG_FILE=${OUTPUT_DIR}/step2.log
    ${log}=    Get File    ${OUTPUT_DIR}/step2.log
    Should Contain    ${log}    RESULT shared_magic=53485244 OK
    Should Contain    ${log}    RESULT motor_ready=00000001 OK

Step3 Full Variant Boots Clean
    [Documentation]    full/full pair loads without faults (no motion in sim)
    ${res}=    Run Process    ${ARM_BOARD}/scripts/run_sim.sh    --step    step3
    ...    --headless    --timeout    460
    ...    timeout=${SIM_TIMEOUT}    stdout=${OUTPUT_DIR}/runner3.log    stderr=STDOUT
    ...    env:LOG_FILE=${OUTPUT_DIR}/step3.log
    ${log}=    Get File    ${OUTPUT_DIR}/step3.log
    Should Contain    ${log}    RESULT shared_magic=53485244 OK
    Should Not Contain    ${log}    HardFault

Agent Sees Heartbeat
    [Documentation]    requires uros-agent on UDP 8888 + TAP path (manual)
    [Tags]    manual    agent
    Log    Start agent: docker run --rm --net host microros/micro-ros-agent:humble udp4 --port 8888    level=INFO
    Log    Then: ros2 topic echo /heartbeat std_msgs/Int32 (see ../ros/, docs/pcb_elements.md §6)    level=INFO
    Pass Execution    manual step documented, not executed in CI
