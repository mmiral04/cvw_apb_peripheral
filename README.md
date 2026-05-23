# Nexys A7: Seven Segment Display peripheral

This repository describes the implementation and use of the seven segment display available in the Nexys A7 FPGA board as a peripheral of the RISC-V core [Wally](https://github.com/openhwgroup/cvw). This text begins with a guide to install and configure the environment necessary to work with Wally, after which, a description of the system and how the peripheral was implemented is provided.

# Installation and first boot

## Environment configuration

Instructions to install and configure the environment:

1. Clone the Wally repository with its submodules. The following command clones a fork of the repository in which the peripheral is implemented.

```bash
$ git clone --recurse-submodules https://github.com/mmiral04/cvw
$ cd cvw
$ git remote add upstream https://github.com/openhwgroup/cvw
$ git checkout apb_periph
```

2. Install Wally's toolchain. By default, the script installs everything in the `/opt/riscv` directory:

```bash
$ sudo bin/wally-tool-chain-install.sh
```

3. Set the environment values by sourcing the setup script:

```bash
$ source ./setup.sh
```

> [!NOTE] 
> 
> Optionally, the previous command can be added to the `.bashrc` file in order to execute it when a shell is opened.

4. To test that everything works until this point, the HelloWally example can be executed:

```bash
$ cd examples/C/hello
$ make
$ wsim --sim verilator rv64gc --elf hello
Hello Wally!
0 1 2 3 4 5 6 7 8 9
$ spike hello
Hello Wally!
0 1 2 3 4 5 6 7 8 9
```

## Vivado installation

[Vivado 2025.1](https://www.xilinx.com/support/download/index.html/content/xilinx/en/downloadNav/vivado-design-tools/2025-1.html) must be installed by following the instructions from the GUI installer. This guide will assume that Vivado is installed in the default location.

After installation, Vivado needs some additional configuration:

1. Add Wally's custom board files to Vivado's data:

```bash
$ sudo cp -r $WALLY/addins/vivado-boards/new/board_files /tools/Xilinx/2025.1/Vivado/data/boards
```

2. Install the cable drivers, which are not installed by default in Linux:

```bash
$ /tools/Xilinx/2025.1/Vivado/data/xicom/cable_drivers/lin64/install_script/install_drivers
```

## SD card preparation

This sections explains how to flash the Operating System to the SD card so it can be used in Wally. The instructions assume that the SD card has been connected and can be accessed through the file `/dev/sda`.

1. Run the script to flash the SD card. The `-b` option specifies the location of the Buildroot installation and the `-d` option the location of the compiled devicetree.

```bash
$ $WALLY/linux/sdcard/flash-sd.sh -b $RISCV/buildroot -d $RISCV/buildroot/output/images/wally-nexysa7.dtb /dev/sda
```

The script should end by showing the information of the created partitions on the SD card, which should look similar to:

![Flash SD card](/assets/flash-sd.png)

### Compilation and use of C programs

To use custom C programs in Wally, they first need to be cross-compiled to the target architecture. Once compiled, they can be stored in the fourth partition of the SD card and loaded in Wally. As an example, the compilation of the `devmem2` program, which will later be used to access the peripheral's registers, is shown here. The source code for the program can be downloaded [here](https://github.com/freelancer-leon/devmem2).

1. Compile the program using the cross-compiler built when installing the toolchain:

```bash
$ $RISCV/buildroot/output/host/bin/riscv64-buildroot-linux-gnu-gcc --static devmem2.c -o devmem2
```

2. Mount the fourth partition of the SD card and store the binary:

```bash
$ sudo mount -m /dev/sda4 /mnt/sd
$ sudo cp devmem2 /mnt/sd
$ sudo umount /mnt/sd
```

3. In Wally, mount the fourth partition again and run the program:

```bash
$ mkdir /mnt/sd
$ mount /dev/mmcblk0p4 /mnt/sd
$ cd /mnt/sd
$ ./devmem2
```

## Wally synthesis and first boot

Once the configuration is finished, Wally can be synthesized for the FPGA. This can be achieved through a Makefile in `$WALLY/fpga/generator`, which takes care of the entire synthesis process of Vivado:

```bash
$ cd $WALLY/fpga/generator
$ make nexysa7
```

Once the synthesis is completed and the SD card is flashed with the OS, the FPGA can be programmed and Linux can be booted:

1. Open the project in `$WALLY/fpga/generator/` with Vivado

2. Plug in the FPGA and open it through the hardware manager in Vivado

3. Program the device with the bitstream generated in`$WALLY/fpga/generator/WallyFPGA.runs/`

4. Open a UART connection with`screen`:

```bash
$ screen /dev/ttyUSB0 1152000
```

The boot process should begin by showing a banner and loading the blocks of the SD card:

![Start of boot process](/assets/boot-banner.png)

Once it is finished, it starts the OpenSBI bootloader and the boot process begins. After it ends, a list of the current processes is shown and a prompt for the username is displayed. The username `root` can be entered:

![End of boot process](/assets/boot-end.png)

## Peripheral test

To check that everything is working, a simple test can be performed by accessing the peripheral's registers with `devmem2`. The program should be loaded in the SD card and mounted to Wally as explained [here](https://github.com/mmiral04/cvw_apb_peripheral#compilation-and-use-of-c-programs).
To test the peripheral:

```bash
$ ./devmem2 0x00100000 w 0x77
```

The display should show the character `A` .

---

# Implementation

## Wally

Wally is a configurable RISC-V core, it is composed of a 5 stage pipeline and can compiled with all the standard RISC-V extensions. In addition, it includes virtual memory, caches, branch prediction and several peripherals, all of these can be configured to be included or not.

The following image shows a block diagram of the wally system on chip:

![Wally top level block diagram](https://github.com/openhwgroup/cvw/blob/main/wallyriscvTopAll.png?raw=true)

The pipeline, is located in the module `core`, with each of the stages and components divided into smaller modules. The peripherals are in the `uncore` and Wally supports:

- **ROM:** holds the bootloader

- **RAM:** on-chip RAM

- **GPIO:** general Purpose Input Output peripheral

- **UART:** used to connect via USB

- **PLIC:** Platform-Level Interrupt Controller, handles interrupts from the peripherals

- **CLINT:** Core Local Interruptor, handles timers and software interrupts

- **SDC:** SD card controller

Additionally, the uncore outputs a bus that goes out of the system on chip and connects to the DDR3 controller on the FPGA implementation.

### Operating System: Buildroot

Another important feature of Wally is that it can boot an Operating System, specifically, a Linux Buildroot system customized for the needs of the processor. Buildroot is a tool to build Linux-based operating systems mostly used in embedded systems. It takes care of building a cross-compilation toolchain, the kernel image, the bootloader and the root file system, and allows the customization of every component. Wally's system is customized with the minimum amount of tools necessary in order to reduce the memory footprint, but if needed, one could modify the configuration and recompile Buildroot to add new drivers, packages or tools.

## Bus hierarchy

To add new peripherals to the system, most of the relevant areas that need to be studied are in the `uncore`. The peripherals are interconnected with a bus hierarchy composed of the three main types of AMBA buses: APB, AHB and AXI. The structure of the interconnection is shown here:

![Bus hierarchy](/assets/bus-hierarchy.png)

### Advanced Peripheral Bus (APB)

The APB bus is the lowest complexity bus among the three, as well as the slowest one. Its purpose is usually to interconnect memory-mapped peripherals, where a bridge between AHB and APB acts as the master of the connection, called *Requester*, and the peripherals act as the slaves, called *Completers*. This is the interconnection used in Wally to connect the majority of the peripherals.

The APB bus works without pipelining the transfers, and each one takes at least two cycles to complete.  The interface defined by the bus is composed of:

- **Address bus:** A single address bus `PADDR` for both read and write transfers. 

- **Data bus:** A data bus for read transfers `PWDATA` and another one for read transfers `PRDATA`. Additionally, for write transfers, the signal `PSTRB` indicates which byte lanes to update during a write transfer, that is, if bit 0 of the `PSTRB` is low, the least significant byte of `PWDATA` is ignored.

- **Control signals:** These includes the signals to initiate transfers and respond with the completion status. 
  
  - The `PSEL` signal is generated by the Requester, who generates a different signal for each of the peripherals, and asserts the signal corresponding to the peripheral who is the target of the transfer. 
  
  - The `PENABLE` signal indicates that the second phase of the transfer has started. 
  
  - The `PWRITE` signal indicates that the transfer is a write transfer. If low, its a read transfer.
  
  - The `PREADY` signal is generated by the Completer and is used to indicate if a transfer is finished. 

The interface includes more signals, but the basic operation of APB can be explained without them.

An APB transfer is composed of two phases:

![APB interface states](/assets/apb-states.png)

- **Setup:** When a transfer is initiated, the interface moves from the Idle state to the setup state. This phase only lasts a single cycle in which the Requester asserts the `PSELx` signal of the target peripheral, and sets `PENABLE` to 0 and the signals for the address and write data to the desired values for the transfer.

- **Access:** The requester asserts `PENABLE`, indicating that the interface is in the Access stage. During this stage, the signals `PADDR`, `PWRITE`, `PWDATA` (only in write transfers), and `PSTRB` must remain unchanged. While the `PREADY` signal is low, the interface stays at the Access phase, whenever the Completer raises the signal, the transfer ends and can go directly to the Setup phase if another transfer follows, or to the Idle state.

This cronogram shows the timing of a write transfer without wait states, that is, the peripheral asserts `PREADY` immediately in the Access phase:

![APB write transfer without wait states](/assets/apb-crono-1.png)

- The Setup phase starts in cycle 2, `PWRITE` is 1 so the transfer is a write operation

- The Access phase starts in cycle 3 and only lasts that cycle, as `PREADY` is 1.

The timing with wait states is:

![APB write transfer with wait states](/assets/apb-crono-2.png)

- In this case, the Access phase also starts in cycle 3 but ends in cycle 6 because `PREADY` is asserted in cycle 5. During the cycles where `PREADY` is 0, the other signals must remain constant.

Regarding, the read transactions, the timing without wait states is:

![APB read transfer without wait states](/assets/apb-crono-3.png)

- This works the same as a write transaction, except that `PWRITE` is 0 and `PRDATA`is generated by the Completer in the Access phase (cycle 3).

And with wait states:

![APB read transfer with wait states](/assets/apb-crono-4.png)

### Advanced High-Performance Bus (AHB)

The AHB bus supports a higher performance than the APB bus, but the interface and interconnection is also more complex. The AHB specification defines the interfaces for the manager and subordinate, as well as the interconnection that must be used. This bus is used in Wally as the main bus which connects the `core` to the peripherals in the `uncore`, although the bus is transformed into APB or AXI for some peripherals. 

The main signals of the AHB interface are:

- <u>Manager signals:</u>
  
  - **HADDR:** Address of the target peripheral for the transaction
  
  - **HWDATA:** Write data for the transaction during write operations. A **HWSTRB** write strobe signal is also available, it works the same as APB.
  
  - **HWRITE:** When high, it indicates a write transfer, otherwise, a read transfer.
  
  - There are other signals used to indicate the size of transfers and to handle burst transfers, which will not be explained here.

- <u>Subordinate signals:</u>
  
  - **HRDATA:** Data for a read operation
  
  - **HREADYOUT:** The signal is raised by the subordinate when a transfer has finished.
  
  - **HRESP:** Transfer response.

- <u>Interconnect signals:</u>
  
  - **HSELx:** Generated by the address decoder, one for each of the subordinates.
  
  - **HREADY:** Indicates to the manager and all subordinates that a transfer has been completed. It is generated by the multiplexer by combining the `HREADYOUT` signals.

The interconnection of the AHB bus in the uncore is composed of the following components:

![uncore block diagram](/assets/uncore.png)

- **Manager:** In Wally, the manager is the `core`, which outputs its AHB peripheral through the *External Bus Unit* (EBU). The interconnection is single-manager.

- **Subordinates:** The subordinates are the peripherals, only the RAM and ROM are connected directly through AHB. The AHB-APB bridge connects the APB peripherals and and external AHB bus goes out of the system on chip to connect to external peripherals.

- **Address decoder:** The module named `adrdecs` is in charge of decoding the address from the signal `HADDR` and assert the corresponding `HSELx` signal of the target peripheral, similarly to how the `PSELx` signals work in APB.

- **multiplexer:** The multiplexer determines the correct `HREADY` signal and data of the peripheral that was just accessed. For example, if the address decoder selects the ROM, the multiplexer will connect the ROM's output signals to the AHB output signals. It also propagates the active subordinate's output signals to the manager.

- **Select delay:** The Flip-Flop placed between the address decoder and multiplexer acts as a delay so that the select signal of the multiplexer (`HSELD`) remains constant while a transfer is active. Thus, if a subordinate takes several cycles to respond to a transfer, the `HSELD` signal will not change from the select signal of the corresponding peripheral.

AHB transfers are pipelined and consist of two phases:

- **Address:** Equivalent to the Setup phase in an APB transfer. It lasts for a single cycle unless extended by the previous transfer.

- **Data:** Can take more than one cycle to complete. It lasts until the `HREADY` signal is asserted.

A simple transfer without wait states consists of:

1. The manager drives the `HADDR` signal in the Address phase. The corresponding `HSELx` signal for the subordinate is asserted.

2. On the next cycle, during the Data phase, the target subordinate samples the address and control information for the transfer, and drives the `HREADYOUT` signal and read data if applicable.

This behavior is show in the following cronogram, exemplified with a read transfer:

![AHB read transfer](/assets/ahb-crono-1.png)

- The Address phase for transfer 1 occurs in cycle 2 and its Data phase in cycle 3.

- As the bus is pipelined, the Address phase for transfer 2 begins at the same time as the Data phase for transfer 1.

- A write transaction is exactly the same, except that the value of `HWRITE` is 1.

If the subordinate can not complete the Data phase in a single cycle, it can insert wait states by driving its `HREADYOUT` signal low for the number of cycles it needs. A delay in the Data phase means that the Address phase that is occurring in parallel is also delayed. The following cronogram shows a series of transfers, first a write transfer without wait states, then a read transfer with one wait state, and finally a write transfer without wait states:

![AHB consecutive transfers](/assets/ahb-crono-2.png)

- The Address phase of transfer 1 starts in cycle 2 and its Data phase in cycle 3. It only lasts one cycle as the `HREADY` signal is high.

- Transfer's 2 Address phase occurs in cycle 3, but in the Data phase in cycle 4, the subordinate lowers the `HREADY` signal for a single cycle, thus, the transfer is not completed in cycle 5. At the same time, the Address phase of transfer 3 is started, and it is also delayed another cycle.

- On cycle 5, the subordinate raises `HREADY` and drives the read data, completing transfer 2 in cycle 6, as well as allowing the Data phase of transfer 3 to start.

- The Data phase of transfer 3 completes in a single cycle.

### Advanced eXtensible Interface (AXI)

This last bus supports the highest speed of the three, and also has the most complex interface. The bus consists of five channels to increase the parallelism possible in read and write transactions. The high-throughput of this bus makes it suitable for high-speed peripherals, such as the DDR3 memory controller located outside the system on chip. This peripheral is connected through an AHB to AXI bridge using the external AHB bus that goes out of the uncore. Since this bus will not be used in the implementation of the new peripheral, it will not be explained in this guide.

## Seven Segment Display peripheral

In this section, we will cover the design and implementation of the Seven Segment Display (SSD) peripheral.

### Design

Firstly, we define how the peripheral is going to behave. The goal is to design a memory mapped peripheral which can handle the eight SSDs available in the Nexys A7. To achieve this, the memory map of the peripheral will have the following structure:

![SSD peripheral registers](/assets/registers.png)

There are eight registers that store the enabled bits of the eight segments. The register with offset 0 will control the right-most display.

The module's interface will consist of:

![SSD module interface](/assets/interface.png)

- **APB interface:** Subordinate interface of the APB bus.

- **ssd_segments:** 8-bit wide signal with the enabled bits of the selected display.

- **ssd_select:** 8-bit wide, one-hot encoded signal that indicates the selected display. Only one display can be active.

We want to be able to display numbers in multiple displays at the same time, however, the pins to turn on segments of the displays only allow for one display on at a time. There are eight pins that correspond to the eight segments of the display and eight pins that correspond to the eight display, but only one of them can be high at the same time. Thus, to turn on segments in different displays, the peripheral shifts the select signal to scan through the eight displays at a very fast speed, switching the enabled segments to the enabled segment of the current peripheral.

Moreover, the Nexys A7 manual indicates that the select and segments signals are active-low, that is the enabled segments are set to 0 and the selected display is set to 0, every other bit is 1.

### Implementation

The implementation involves changing multiple modules. First, a new module `ssd_apb` is created in the `uncore` to implement the peripheral. After that, the bus interconnection in the `uncore` needs to be modified, including the address decoder, which is part of the Memory Management Unit (`mmu`). Finally, the configuration is also updated to include the new peripheral.

The implementation and all the changes needed to integrate the peripheral are described in the following sections.

#### APB transfer logic

Firstly, in the new `ssd_apb` module, we are going to implement the logic to handle read and write transfers in the APB bus. This follows a similar approach to all the APB peripherals supported by Wally.

The transfer logic is handled by four auxiliary signals:

```verilog
logic memwrite;
logic [4:0] entry;
logic [31:0] Din, Dout;
```

- `memwrite`: when high, it indicates that a write transfer is occurring.

- `entry`: word-aligned address to fix alignment issues. Note that the address is only 5-bits wide, the same with the `PADDR` signal in the APB interface, because the last register has an offset of `0x1C` so a 5-bit address is enough.

- `Din` and `Dout`: auxiliary signals to limit the data bus to 32-bits when working with RISC-V 64, which is the case for the configuration of Wally we are working with.

The logic for this signals is implemented by:

```verilog
assign entry = {PADDR[4:2], 2'b00};
assign memwrite = PWRITE & PENABLE & PSEL;
assign Din = PWDATA[31:0];
if (P.XLEN == 64)  assign PRDATA = {Dout, Dout};
else               assign PRDATA = Dout;
```

- The first assignment aligns the address to 32-bit words by setting the two least significant bits to 0.

- The second assignment sets `memwrite` to 1 only if the signal `PWRITE`, `PENABLE` and `PSEL` are one, that is, when a write transaction is targeted to this peripheral and the interface is in the Access phase.

- The remaining code limits the data buses to 32 bits.

One last assignment is needed for the transfer logic:

```verilog
assign PREADY = 1'b1;
```

`PREADY` can be tied to 1 because the peripheral is always able to respond to a transfer in a single cycle, that is, the Access phase always happens in one cycle.

#### Register logic

This next part explains how the registers are implemented to handle the read and write operations. To store the values of the registers, the following 8-bit array is used:

```verilog
logic [7:0][7:0] segments;
```

Each `segments[i]` holds the enabled bits for the i-th display.  The logic to read and write elements to this array is simple:

```verilog
always_ff @(posedge PCLK) begin: read_register
    case(entry)
        REG0: Dout <= ~segments[0][7:0];
        REG1: Dout <= ~segments[1][7:0];
        REG2: Dout <= ~segments[2][7:0];
        REG3: Dout <= ~segments[3][7:0];
        REG4: Dout <= ~segments[4][7:0];
        REG5: Dout <= ~segments[5][7:0];
        REG6: Dout <= ~segments[6][7:0];
        REG7: Dout <= ~segments[7][7:0];
    endcase
end

always_ff @(posedge PCLK) begin: write_register
    if (~PRESETn) begin
        segments[0] <= '1;
        segments[1] <= '1;
        segments[2] <= '1;
        segments[3] <= '1;
        segments[4] <= '1;
        segments[5] <= '1;
        segments[6] <= '1;
        segments[7] <= '1;
    end
    else if (memwrite)
        case(entry)
            REG0: segments[0][7:0] <= ~Din[7:0];
            REG1: segments[1][7:0] <= ~Din[7:0];
            REG2: segments[2][7:0] <= ~Din[7:0];
            REG3: segments[3][7:0] <= ~Din[7:0];
            REG4: segments[4][7:0] <= ~Din[7:0];
            REG5: segments[5][7:0] <= ~Din[7:0];
            REG6: segments[6][7:0] <= ~Din[7:0];
            REG7: segments[7][7:0] <= ~Din[7:0];
      endcase
  end
```

> [!Note]
> 
> The values `REG0`, `REG1`, ... are defined in the top of the module as local parameters with the offset of each register.

- The read register simply updates `Dout` (which is assigned to `PRDATA`) each cycle with the data in the display selected by `entry`. This is not constraint to only change `PRDATA` in the Access phase because it is not needed, if the interface is not on the Access phase or if the operation is a write transfer, the Requester will simply ignore the data in `PRDATA`, so this simple logic is enough.

- In contrast, the write register only updates the register in the Access phase, because it is at that time were the `PWDATA` has the correct value to be written.

Note that the values of `Din` are inverted before writing them to the registers, and the values of the registers are inverted before updating `Dout`. This way, the registers hold the active-low values needed for the displays but the inputs can be active-high, which is more natural.

#### SSD logic

Finally, in this section we implement the logic to scan through the displays and output the segments of each display consecutively. This needs to be done at a speed between 1KHz and 60Hz, according to the specification. The best results were obtained with a frequency divider of 14 bits, reducing the 20MHz clock to about 1.2KHz. The frequency divider is implemented with:

```verilog
logic [13:0] clkcounter;
logic        clkDivided;

always_ff @(posedge PCLK) begin: clock_counter
    if (~PRESETn) clkcounter <= 'b0;
    else if (clkcounter == {14{1'b1}}) clkcounter <= 'b0;
    else clkcounter <= clkcounter + 1;
end

always_ff @(posedge PCLK) begin: clock_divider
    if (~PRESETn) clkDivided <= 0;
    else if (clkcounter == {14{1'b1}}) clkDivided <= ~clkDivided;
end
```

Then, to scan through the displays, an 8-bit select signal is used. The signal is one-hot encoded and active-low:

```verilog
logic [7:0] selected = 8'b11111110;
```

A Flip-Flop shifts the signal in each cycle of the divided clock:

```verilog
always_ff @(posedge clk9KHz) begin: shift_select
    if (~PRESETn)
        selected <= 8'b11111110;
    else
        selected <= {selected[6:0], selected[7]};
end

assign ssd_select = selected;
```

And another Flip-Flop sets the output signal `ssd_segments` to the segments of the selected register:

```verilog
always_ff @(posedge clk9KHz) begin: segments_register
    if (~PRESETn)
        ssd_segments[7:0] = 'b1;
    else begin
        case (selected)
            8'b11111110: ssd_segments[7:0] <= segments[1][7:0];
            8'b11111101: ssd_segments[7:0] <= segments[2][7:0];
            8'b11111011: ssd_segments[7:0] <= segments[3][7:0];
            8'b11110111: ssd_segments[7:0] <= segments[4][7:0];
            8'b11101111: ssd_segments[7:0] <= segments[5][7:0];
            8'b11011111: ssd_segments[7:0] <= segments[6][7:0];
            8'b10111111: ssd_segments[7:0] <= segments[7][7:0];
            8'b01111111: ssd_segments[7:0] <= segments[0][7:0];
            default: ssd_segments = '1;
        endcase
    end
end
```

With this, the implementation of the peripheral is completed, and we can move on to integrating the peripheral in the system.

### Integration

#### Address decoder and uncore

The new peripheral is instantiated in the `uncore` and needs to be connected to the AHB to APB bridge.

We first need to modify the address decoder in the AHB interconnection to extend it with the decoder for the new peripheral. This module is located in `mmu/adrdecs.sv` and consists of an instance of `adrdec` for each peripheral. Thus, the address decoder is extended by adding:

```verilog
adrdec #(P.PA_BITS) ssddec(PhysicalAddress, P.SSD_BASE[P.PA_BITS-1:0], P.SSD_RANGE[P.PA_BITS-1:0], P.SSD_SUPPORTED, AccessRW, Size, 4'b0100, SelRegions[12]);
```

- The parameters `SSD_BASE`, `SSD_RANGE` and `SSD_SUPPORTED` are created later, in the configuration section, with the values for the base address and range of the peripheral.

- The parameter `4'b0111` indicates the supported access size of the peripheral. The bits correspond to 64-bit, 32-bit, 16-bit and 8-bit accesses respectively; therefore, `4'b0111` forbids 64-bit accesses and allows everything else.

- The last parameter is the bit of the `SelRegions` signal that is asserted when the input address matches the address of the peripheral. Said signal must be extended by one bit, the peripheral will use the most significant bit.

The `SelRegions` signal is the one used in the AHB interconnection to set the `PSELx` signals. However, before changing the interconnection, the `mmu` module also uses the address decoder, so the change to `SelRegions` is reflected in that module as well, where the signal's width is increased by one bit.

Then, in the `uncore`, the `SelRegions` signal is also updated and new select signals are created:

- `HSELSSD`: Corresponds to `SelRegions[12]`

- `HSELSSDD`: Delayed signal for `HSELSSD`.

To connect the new peripheral to the AHB-APB bridge, we modify the instantiation of the component, increasing the number of peripherals by one and adding the new select signal:

```verilog
ahbapbbridge #(P, 7) ahbapbbridge (
    HCLK, .HRESETn, .HSEL({HSELSSD, HSELSDC, HSELSPI, HSELUART, HSELPLIC, HSELCLINT, HSELGPIO}), .HADDR, .HWDATA, .HWSTRB, .HWRITE, .HTRANS, .HREADY,
    .HRDATA(HREADBRIDGE), .HRESP(HRESPBRIDGE), .HREADYOUT(HREADYBRIDGE),
    .PCLK, .PRESETn, .PSEL, .PWRITE, .PENABLE, .PADDR, .PWDATA, .PSTRB, .PREADY, .PRDATA);
assign HSELBRIDGE = HSELGPIO | HSELCLINT | HSELPLIC | HSELUART | HSELSPI | HSELSDC | HSELSSD; // if any of the bridge signals are selected
```

We can see that the bridge AHB output consists of the signals `HREADBRIDGE`, `HRESPBRIDGE`, `HREADYBRIDGE` and `HSELBRIDGE`, which the AHB bridge sets according to the peripheral that is active.

Finally, to instantiate the peripheral:

```verilog
if (P.SSD_SUPPORTED == 1) begin : seven_seg_display
    ssd_apb #(P) ssd_apb(
        .PCLK, .PRESETn, .PSEL(PSEL[6]), .PADDR(PADDR[4:0]), .PWDATA, .PSTRB, .PWRITE, .PENABLE,
        .PREADY(PREADY[6]), .PRDATA(PRDATA[6]),
        .ssd_segments, .ssd_select);
end else begin: seven_seg_display
    assign ssd_segments = '0;
    assign ssd_select = '0;
end
```

The conditional is used to only instantiate the peripheral if the configuration value `SSD_SUPPORTED` is 1. This is the mechanism that Wally uses to include different components of the system on chip by changing the configuration.

The signals `ssd_segments` and `ssd_select` are added as outputs to the `uncore` and routed all the way as outputs of the system on chip.

#### Connection with the top level

The new signals, `ssd_segments` and `ssd_select`, are also connected directly as outputs in the top level module. To connect the outputs to the pins of the FPGA, the XDC constraints file is used. The information of which pins are used to control the seven segment displays can be found in the Nexys A7 manual or in a [master](https://github.com/Digilent/digilent-xdc/blob/master/Nexys-A7-100T-Master.xdc) XDC file.

In the constraints file of the Nexys A7, we only need to change the pins for the SSD to connect them to the corresponding signals of `ssd_segments` and `ssd_select`.

#### Configuration

The final modification needed before we can synthesize the module are related with the configuration. As we saw in the changes to the address decoder, three new configuration options are added in order to set the properties of the peripheral. The values we chose for the peripheral are:

- `SSD_BASE`: The base address will be `0x100000`. It has been chosen arbitrarily, taking into account that there are no conflicts with other regions in the memory map.

- `SSD_RANGE`: The address range of the peripheral is `0x1F`, the space needed to be able to address all registers.

- `SSD_SUPPORTED`: This parameter is set to 0 by default. It indicates if the peripheral should be instantiated, we don't want this to be the case by default as the Nexys A7 is the only board that supports the peripheral.

When choosing the address base and range, besides choosing an unoccupied region, the range must be a thermometer code, meaning that the upper bits are 0 and the lower bits 1 (for example, `0x1C` = `0b0001_1100` is not a thermometer code but `0x1F` = `0b0001_1111` is).

This new configuration options must be declared in three different files:

- `config/rv64gc/config.vh`: In this file, the parameters are declared as logic signals, specifying their width and default value;

- `src/cvw.sv`: The signals are also declared in this file, without specifying the default value.

- `config/shared/parameter-defs.vh`: This file contains only the name of the signals, without type or value.

Additionally, there is another important file that is used during the synthesis: `config/derivlists.txt`. This file defines derivations of the configuration for different purposes. For instance, there are derivations for the different FPGA boards supported where the value for the range of the external memory changes. For the peripheral, we need to change the value of `SSD_SUPPORTED` to 1 in the derivation `fpganexysa7` in order to instantiate the peripheral when compiling for the Nexys A7.

After this changes, Wally can be synthesized again using the Makefile, as explained in the installation section of this guide. However, the system is not ready to be tested yet, as the Operating System is still unaware that the peripheral exists.

#### Devicetree

The devicetree is a data structure that describes the hardware components of a particular computer. The kernel uses the devicetree to obtain the information about the capabilities of the system and the devices available. The devicetree for the Nexys A7 is located at: `linux/devicetree/wally-nexysa7.dts`. 

The structure of the devicetree is a tree of nodes that describe the devices in a system, including nodes to describe the properties of the memory, CPU and every peripheral. The properties are key/value pairs where the value type depends on the property. The name of a node is composed of a name and an address in the form `node_name@address`, except for the root node, which is identified by `/`.

The devicetree specification defines several standard properties that describe the basic capabilities of the device. Some of the more important properties are:

- **`compatible`:** a list of strings that indicate the compatible drivers associated with the peripheral. When the devicetree is loaded, if the compatible property is defined, the kernel searches for a driver that shares the same value, and activates it if it exists.

- **`phandle`:** sets a reference identifier that is unique within the devicetree, and can be used by other nodes to refer to it. This is used, for example, to link the peripheral's interrupt lines to the PLIC.

- **`#address-cells` and `#size-cells`:** These properties hold unsigned 32-bit integer values that define how child nodes should be addressed. The values are used in the `reg` property, which is explained next.

- **`reg`:** The `reg` property describes the address of the device within the address space defined by its parent, for our purpose, the address base and range of the peripheral in the memory map.  The value for this property is an arbitrary number of `<address, length>` pairs, usually one but could have more if the address space of the peripheral has multiple regions. The values for `address` and `length` are a variable number of unsigned 32-bit integers cells, where the number of cells depends on the value of `#address-cells` and `#size-cells` of its parents. For example, let the property be `reg = <0x3000 0x0020 0x0001 0x0100>`:
  
  - If both `#address-cells` and `#size-cells` are 1, the property describes a peripheral with two blocks of registers:
    
    - A 32-byte block at offset `0x3000`
    
    - A 256-byte block at offset `0x0001`
  
  - However, if `#address-cells` and `#size-cells` are 2, the property describes a peripheral with a single register block:
    
    - A 65792-byte block at offset `0x30000020`

The last property is the only one that we are going to use for a simple node that describes the SSD peripheral. The node is added as a child of the `soc` node, along the other peripherals of the uncore, and it is described by:

```dts
ssd: ssd@100000 {
    reg = <0x00 0x100000 0x00 0x20>;
};
```

- The values of `#address-cells` and `#size-cells` for the `soc` node are both 2, so the `reg` describes a single block register where:
  
  - The base address is `0x100000`
  
  - The size is 32-bytes

The devicetree needs to be compiled and the flashed to the SD card. The compilation is automatically done when compiling Buildroot, but it can also be compiled manually with the following command:

```bash
$ dtc -I dts -O dtb wally-nexysa7.dts > wally-nexysa7.dtb
```

Then when flashing the SD card with the corresponding script, the `-d` specifies the path to the devicetree:

```bash
$ $WALLY/linux/sdcard/flash-sd.sh -b $RISCV/buildroot -d $WALLY/linux/devicetree/wally-nexysa7.dtb /dev/sda
```

After flashing, we can boot Linux again and repeat the test with `devmem2` explained [previously](https://github.com/mmiral04/cvw_apb_peripheral#compilation-and-use-of-c-programs) to check that everything is working correctly.

## Linux driver

A final thing we can do to improve the usability of the peripheral is to create a Linux driver that handles the read and write operations without having to manually do memory maps with `devmem2`. A Linux driver operates in kernel-space and can use I/O functions to map the physical address a single time and use the same mapping in each access, instead of relying on the `mmap` system call to achieve the same result from user-space. In addition, creating a driver allows us to define an interface through which we can abstract the interaction with the device instead of writing to the registers directly. The source code for the driver can be found in the folder `driver` in this repository.

We are going to implement the driver as a character driver, which exposes a special device file in the `/dev/` directory through which we can interact with the device with system calls such as `read` or `write`. The driver's interface can be designed in different ways, for example:

- A single device file `/dev/ssd` that, when a list of pair of values `n,m:i,j:k,l:...` is written, it updates the n-th display to the value `m`, the i-th display to `j`, the k-th display to `l`...

- Eight devices files `/dev/ssd0`, `/dev/ssd1`... each one controlling the value of its corresponding display.

- A single device file `/dev/ssd` that, when written a value `m` at offset `n` sets the value of the n-th display to `m`. The offset can be changed with the `lseek` system call.

This guide will explain the first implementation, but the structure of the character driver is very similar in the three implementations. The driver consists of mainly six functions:

- **Initialization and cleanup:** All drivers must define an initialization function and can optionally define a cleanup function. The initialization function is called when the driver is loaded with `insmod` and the cleanup function when the driver is unloaded with `rmmod`. 
  
  - `ssd_init` is the initialization function, which is in charge of allocating a (major, minor) pair, registering the device, creating a device class, creating the device file, and performing the mapping of the physical address to a virtual address. An explanation of how the allocation and creation function work can be found in the Linux documentation. The `ioremap` function returns the virtual-address of the requested physical address, and is stored in a variable to be used in the memory operations 
  
  ```c
  static int __init ssd_init(void)
  {
      int ret;
  
      // Dynamically allocate 1 (major, minor) pair
      ret = alloc_chrdev_region(&dev, 0, 1, DRIVER_NAME);
      if (ret)
          return ret;
  
      // Register the device
      cdev_init(&ssd_cdev, &fops);
      ret = cdev_add(&ssd_cdev, dev, 1);
      if (ret)
          goto err_add;
  
      cls = class_create(DEVICE_NAME);
      if (IS_ERR(cls)) {
          pr_err("Failed to create class for device\n");
          ret = PTR_ERR(cls);
          goto err_add;
      }
      device_create(cls, NULL, dev, NULL, DEVICE_NAME);
  
      iomap = ioremap(base_addr, MAP_SIZE);
  
      pr_alert("%s driver installed. Major: %d, Minor: %d\n",
              DRIVER_NAME, MAJOR(dev), MINOR(dev));
  
  err_add:
      unregister_chrdev_region(dev, 1);
      return ret;
  }
  ```
  
  - `ssd_exit` is the cleanup function, which reverts the allocation of the resource from the init function. Additionally, it turns off all the displays and unmaps the virtual-address for the I/O map. Note that, if the configuration of the Linux kernel has the option `MODULE_UNLOAD` unset, this function will not be compiled and the `rmmod` command will fail. Our fork of the repository enables this option, the original repository doesn't.
  
  ```c
  static void __exit ssd_exit(void)
  {
      /* Turn off SSD */
      for (int i = 0; i < NUM_DIGITS; i++)
          iowrite32(0, (i * 4) + iomap);
  
      iounmap(iomap);
  
      device_destroy(cls, dev);
      class_destroy(cls);
  
      cdev_del(&ssd_cdev);
      unregister_chrdev_region(dev, 1);
      pr_alert("%s driver removed.\n", DRIVER_NAME);
  }
  ```

- **Open and close:** These functions define the behavior of the driver when the `open` or `close` system calls are used with the device. For our driver, there is nothing to do here so the functions are empty.

- **Read and write:** These functions define the behavior of read and write operations on the file. The read function is left empty as the interesting part is modifying the values of the displays. The write function takes the following arguments:
  
  - `struct file *filp`: File structure which hold additional information of the file accessed. Could be used to identify a file if the driver created multiple device files, which is not the case, so it is not used.
  
  - `const char __user *buff`: This buffer holds the data to be written. It is marked with the `__user` directive to indicate that the buffer comes from user-space and should be handled securely with the function `copy_from_user()`.
  
  - `size_t len`: Length of the data to be written.
  
  - `loff_t *off`: Offset value for the file
  
  The operation of the write function is:
  
  1. Securely copy the data from `buff` to a new buffer (dinamically allocated).
  
  2. If the value written is "OFF", turn off every display and exit.
  
  3. If not, parse the list of values updating the registers of the peripheral with the function `iowrite32`.
  
  4. Deallocate memory and return `len` to indicate that the write was successful.

```c
static ssize_t ssd_write(struct file *filp, const char __user *buff,
        size_t len, loff_t *off)
{
    int value, reg;
    char *kbuf = kmalloc(len, GFP_KERNEL);
    char *token;
    char *kbufptr = kbuf;
    if (kbuf == NULL) {
        pr_err("%s: Failed to allocated buffer\n", DRIVER_NAME);
        return -ENOMEM;
    }

    if (copy_from_user(kbuf, buff, len)) {
        kfree(kbuf);
        return -EFAULT;
    }

    if (strncmp(kbuf, "OFF", 3) == 0) {
        /* Turn off displays */
        pr_info("%s: Turn off displays.\n", DRIVER_NAME);
        for (int i = 0; i < NUM_DIGITS; i++) {
            iowrite32(0, (i * 4) + iomap);
        }

        return len;

    } else {
        token = strsep(&kbufptr, ":");
        while (token != NULL) {
            if (sscanf(token, "%d,%x", &reg, &value) == 2) {
                pr_info("%s: Update digit %d to: 0x%x\n", DRIVER_NAME, reg, value);
                iowrite32(value, (reg * 4) + iomap);
            } else {
                pr_err("%s: Invalid value: %s", DRIVER_NAME, token);
                kfree(kbuf);
                return -EINVAL;
            }
            token = strsep(&kbufptr, ":");
        }

    }
    kfree(kbuf);
    return len;
}
```

The remaining code in the driver is boilerplate code used in character driver that links the system calls with the functions that implements them. A much more in-depth tutorial on character drivers and drivers in general can be found in the book [Linux Device Drivers, Third Edition [LWN.net]](https://lwn.net/Kernel/LDD3/).

### Driver compilation

The compilation of Linux driver is done through a special Makefile that links the symbols with the symbols defined in the Linux headers. There are templates available for the Makefile, we added some modifications to compile it with the cross-compiler for Wally:

```makefile
export ARCH := riscv
export CROSS_COMPILE := ${RISCV}/buildroot/output/host/bin/riscv64-buildroot-linux-gnu-
obj-m += ssd_driver.o
KDIR := /opt/riscv/buildroot/output/build/linux-6.12.8
PWD := $(shell pwd)

default:
    $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
    $(MAKE) -C $(KDIR) M=$(PWD) clean
```

The driver can be compiled and stored in the fourth partition of the SD card, as we did with `devmem2` [here](https://github.com/mmiral04/cvw/edit/main/README.md#compilation-and-use-of-c-programs). In Wally, the driver is loaded with `insmod` and a character device file `/dev/ssd` should appear. Writing the value `2,0x77:1,0x7C:0,0x39` should show the characters 'A', 'b' and 'C' in the first three displays.
