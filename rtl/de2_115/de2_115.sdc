# Timing constraints for the DE2-115 build. NOT YET RUN through Quartus: whether the design meets the core clock
# below is unknown until TimeQuest reports it. The core clock is the 50 MHz clock divided by 2^CLK_DIV
# (CLK_DIV = 3 in de2_115_top.v, so 6.25 MHz); the name of the divider register may differ after synthesis.
create_clock -name CLOCK_50 -period 20.000 [get_ports CLOCK_50]
create_generated_clock -name core_clk -source [get_ports CLOCK_50] -divide_by 8 [get_registers {div[2]}]
derive_clock_uncertainty
# Switches and keys are human-speed inputs and are synchronised in the design; LEDs and displays are outputs only.
set_false_path -from [get_ports {KEY[*] SW[*]}]
set_false_path -to [get_ports {LEDR[*] LEDG[*] HEX0[*] HEX1[*] HEX2[*] HEX3[*]}]
