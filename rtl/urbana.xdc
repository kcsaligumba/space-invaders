# Space Invaders - Urbana Board Constraints
# Spartan-7 xc7s50csga324-1

# --- Bank voltage / config ---
set_property CFGBVS VCCO [current_design]
set_property CONFIG_VOLTAGE 3.3 [current_design]
set_property BITSTREAM.CONFIG.SPI_BUSWIDTH 4 [current_design]
set_property BITSTREAM.CONFIG.UNUSEDPIN PULLUP [current_design]
set_property BITSTREAM.GENERAL.COMPRESS TRUE [current_design]
set_property INTERNAL_VREF 0.675 [get_iobanks 34]

# --- 200 MHz differential DDR reference clock ---
set_property PACKAGE_PIN C1 [get_ports sys_clk_p]
set_property PACKAGE_PIN B1 [get_ports sys_clk_n]
set_property IOSTANDARD LVDS_25 [get_ports sys_clk_p]
set_property IOSTANDARD LVDS_25 [get_ports sys_clk_n]
create_clock -period 10.000 -name sys_clk [get_ports sys_clk_p]

# --- Reset (active-low) ---
set_property PACKAGE_PIN J2 [get_ports cpu_resetn]
set_property IOSTANDARD LVCMOS25 [get_ports cpu_resetn]

# --- HDMI TX ---
set_property PACKAGE_PIN U16 [get_ports hdmi_clk_p]
set_property PACKAGE_PIN V17 [get_ports hdmi_clk_n]
set_property IOSTANDARD TMDS_33 [get_ports hdmi_clk_p]
set_property IOSTANDARD TMDS_33 [get_ports hdmi_clk_n]

set_property PACKAGE_PIN U17 [get_ports {hdmi_tx_p[0]}]
set_property PACKAGE_PIN U18 [get_ports {hdmi_tx_n[0]}]
set_property PACKAGE_PIN R16 [get_ports {hdmi_tx_p[1]}]
set_property PACKAGE_PIN R17 [get_ports {hdmi_tx_n[1]}]
set_property PACKAGE_PIN R14 [get_ports {hdmi_tx_p[2]}]
set_property PACKAGE_PIN T14 [get_ports {hdmi_tx_n[2]}]
set_property IOSTANDARD TMDS_33 [get_ports {hdmi_tx_p[*]}]
set_property IOSTANDARD TMDS_33 [get_ports {hdmi_tx_n[*]}]

# --- UART ---
set_property PACKAGE_PIN B16 [get_ports uart_txd]
set_property PACKAGE_PIN A16 [get_ports uart_rxd]
set_property IOSTANDARD LVCMOS33 [get_ports uart_txd]
set_property IOSTANDARD LVCMOS33 [get_ports uart_rxd]

# --- USB SPI (MAX3421E) ---
set_property PACKAGE_PIN V14 [get_ports usb_sclk]
set_property PACKAGE_PIN V15 [get_ports usb_mosi]
set_property PACKAGE_PIN U12 [get_ports usb_miso]
set_property PACKAGE_PIN T12 [get_ports usb_ss_n]
set_property IOSTANDARD LVCMOS33 [get_ports usb_sclk]
set_property IOSTANDARD LVCMOS33 [get_ports usb_mosi]
set_property IOSTANDARD LVCMOS33 [get_ports usb_miso]
set_property IOSTANDARD LVCMOS33 [get_ports usb_ss_n]
