*** Settings ***
Documentation     ARM main-board boot suite: platform loads, both cores boot,
...               all PCB models attach, shared protocol + drivers verified.
...               UART is silent in sim (TXE never ready), so asserts read
...               sim state (shared SRAM4, encoder MMIO) instead of log text.
...               Runs via scripts/run_sim.sh (repo-root-relative resc paths).
Library           Process
Library           OperatingSystem
Library           String

*** Variables ***
${ARM_BOARD}         ${CURDIR}/..
${SIM_TIMEOUT}       280s

*** Test Cases ***
Run Step1 Simulation Headless
    [Documentation]    boot step1 fw in Renode, capture log for later asserts
    ${res}=    Run Process    ${ARM_BOARD}/scripts/run_sim.sh    --step    step1
    ...    --headless    --timeout    280
    ...    timeout=${SIM_TIMEOUT}    stdout=${OUTPUT_DIR}/runner.log    stderr=STDOUT
    ...    env:LOG_FILE=${OUTPUT_DIR}/step1.log
    File Should Exist    ${OUTPUT_DIR}/step1.log

Platform Loads With All PCB Models
    [Documentation]    C# models compile, ELFs load, sim starts
    ${log}=    Get File    ${OUTPUT_DIR}/step1.log
    Should Contain    ${log}    RESULT drivers=6
    Should Contain    ${log}    RESULT encoders=5
    Should Contain    ${log}    RESULT imus=2
    Should Not Contain    ${log}    CompilationFailed

Shared Magic Written By CM4
    [Documentation]    cross-core bring-up: CM4 wrote magic+version to SRAM4
    ${log}=    Get File    ${OUTPUT_DIR}/step1.log
    Should Contain    ${log}    RESULT shared_magic=53485244 OK
    Should Contain    ${log}    RESULT shared_version=00000002 OK

Drivers Configured Over Sim SPI
    [Documentation]    CM4 SPI test passed through PowerStep01Chain model
    ${log}=    Get File    ${OUTPUT_DIR}/step1.log
    Should Contain    ${log}    RESULT motor_ready=00000001 OK

Encoder Model Loopback
    [Documentation]    QuadratureEncoder MMIO read/write path works
    ${log}=    Get File    ${OUTPUT_DIR}/step1.log
    Should Contain    ${log}    RESULT enc1_loopback=000004D2 OK

No CPU Faults At Boot
    [Documentation]    no unhandled faults on either core during bring-up
    ${log}=    Get File    ${OUTPUT_DIR}/step1.log
    Should Not Contain    ${log}    HardFault
