-- Copyright (c) 2011-2019 Columbia University, System Level Design Group
-- SPDX-License-Identifier: Apache-2.0

-----------------------------------------------------------------------------
--  CPU tile
------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

use work.esp_global.all;
use work.amba.all;
use work.stdlib.all;
use work.sld_devices.all;
use work.devices.all;
use work.gencomp.all;
use work.leon3.all;
use work.ariane_esp_pkg.all;
use work.misc.all;
-- pragma translate_off
use work.sim.all;
library unisim;
use unisim.all;
-- pragma translate_on
use work.sldcommon.all;
use work.sldacc.all;
use work.nocpackage.all;
use work.tile.all;
use work.cachepackage.all;
use work.memoryctrl.all;
use work.coretypes.all;
use work.grlib_config.all;
use work.socmap.all;

entity tile_cpu is
  generic (
    SIMULATION : boolean := false;
    tile_id : integer range 0 to CFG_TILES_NUM - 1 := 0);
  port (
    rst                : in  std_ulogic;
    srst               : in  std_ulogic;
    refclk             : in  std_ulogic;
    pllbypass          : in  std_ulogic;
    pllclk             : out std_ulogic;
    cpuerr             : out std_ulogic;
    -- TODO: remove this; should use proxy
    irq                : in  std_logic_vector(1 downto 0);
    timer_irq           : in  std_ulogic;
    ipi                : in  std_ulogic;
    -- NOC
    noc1_input_port    : out noc_flit_type;
    noc1_data_void_in  : out std_ulogic;
    noc1_stop_in       : out std_ulogic;
    noc1_output_port   : in  noc_flit_type;
    noc1_data_void_out : in  std_ulogic;
    noc1_stop_out      : in  std_ulogic;
    noc2_input_port    : out noc_flit_type;
    noc2_data_void_in  : out std_ulogic;
    noc2_stop_in       : out std_ulogic;
    noc2_output_port   : in  noc_flit_type;
    noc2_data_void_out : in  std_ulogic;
    noc2_stop_out      : in  std_ulogic;
    noc3_input_port    : out noc_flit_type;
    noc3_data_void_in  : out std_ulogic;
    noc3_stop_in       : out std_ulogic;
    noc3_output_port   : in  noc_flit_type;
    noc3_data_void_out : in  std_ulogic;
    noc3_stop_out      : in  std_ulogic;
    noc4_input_port    : out noc_flit_type;
    noc4_data_void_in  : out std_ulogic;
    noc4_stop_in       : out std_ulogic;
    noc4_output_port   : in  noc_flit_type;
    noc4_data_void_out : in  std_ulogic;
    noc4_stop_out      : in  std_ulogic;
    noc5_input_port    : out misc_noc_flit_type;
    noc5_data_void_in  : out std_ulogic;
    noc5_stop_in       : out std_ulogic;
    noc5_output_port   : in  misc_noc_flit_type;
    noc5_data_void_out : in  std_ulogic;
    noc5_stop_out      : in  std_ulogic;
    noc6_input_port    : out noc_flit_type;
    noc6_data_void_in  : out std_ulogic;
    noc6_stop_in       : out std_ulogic;
    noc6_output_port   : in  noc_flit_type;
    noc6_data_void_out : in  std_ulogic;
    noc6_stop_out      : in  std_ulogic;
    mon_cache          : out monitor_cache_type;
    mon_dvfs_in        : in  monitor_dvfs_type;
    mon_dvfs           : out monitor_dvfs_type);
end;


architecture rtl of tile_cpu is

  signal clk_feedthru : std_ulogic;

  -- Monitor CPU idle
  signal irqo_int      : l3_irq_out_type;
  signal mon_dvfs_ctrl : monitor_dvfs_type;

  -- Leon3 debug signals
  signal dbgi : l3_debug_in_type;
  signal dbgo : l3_debug_out_type;

  -- CPU Reset
  signal cpurstn : std_ulogic;
  type cpu_rstn_state_type is (por, soft_reset_1_h, soft_reset_1_l,
                               soft_reset_2_h, soft_reset_2_l,
                               soft_reset_3_h, soft_reset_3_l,
                               soft_reset_4_h, run);
  signal cpu_rstn_state, cpu_rstn_next : cpu_rstn_state_type;

  -- L1 data-cache flush
  signal dflush : std_ulogic;

  -- L2 wrapper and cache debug reset
  signal l2_rstn : std_ulogic;

  -- Interrupt controller
  signal irqi : l3_irq_in_type;
  signal irqo : l3_irq_out_type;

  -- Queues
  signal coherence_req_wrreq        : std_ulogic;
  signal coherence_req_data_in      : noc_flit_type;
  signal coherence_req_full         : std_ulogic;
  signal coherence_fwd_rdreq        : std_ulogic;
  signal coherence_fwd_data_out     : noc_flit_type;
  signal coherence_fwd_empty        : std_ulogic;
  signal coherence_rsp_rcv_rdreq    : std_ulogic;
  signal coherence_rsp_rcv_data_out : noc_flit_type;
  signal coherence_rsp_rcv_empty    : std_ulogic;
  signal coherence_rsp_snd_wrreq    : std_ulogic;
  signal coherence_rsp_snd_data_in  : noc_flit_type;
  signal coherence_rsp_snd_full     : std_ulogic;
  signal dma_rcv_rdreq              : std_ulogic;
  signal dma_rcv_data_out           : noc_flit_type;
  signal dma_rcv_empty              : std_ulogic;
  signal dma_snd_wrreq              : std_ulogic;
  signal dma_snd_data_in_cpu        : noc_flit_type;
  signal dma_snd_data_in            : noc_flit_type;
  signal dma_snd_full               : std_ulogic;
  signal remote_ahbs_snd_wrreq      : std_ulogic;
  signal remote_ahbs_snd_data_in    : misc_noc_flit_type;
  signal remote_ahbs_snd_full       : std_ulogic;
  signal remote_ahbs_rcv_rdreq      : std_ulogic;
  signal remote_ahbs_rcv_data_out   : misc_noc_flit_type;
  signal remote_ahbs_rcv_empty      : std_ulogic;
  signal apb_rcv_rdreq              : std_ulogic;
  signal apb_rcv_data_out           : misc_noc_flit_type;
  signal apb_rcv_empty              : std_ulogic;
  signal apb_snd_wrreq              : std_ulogic;
  signal apb_snd_data_in            : misc_noc_flit_type;
  signal apb_snd_full               : std_ulogic;
  signal remote_apb_rcv_rdreq       : std_ulogic;
  signal remote_apb_rcv_data_out    : misc_noc_flit_type;
  signal remote_apb_rcv_empty       : std_ulogic;
  signal remote_apb_snd_wrreq       : std_ulogic;
  signal remote_apb_snd_data_in     : misc_noc_flit_type;
  signal remote_apb_snd_full        : std_ulogic;
  signal remote_irq_rdreq           : std_ulogic;
  signal remote_irq_data_out        : misc_noc_flit_type;
  signal remote_irq_empty           : std_ulogic;
  signal remote_irq_ack_wrreq       : std_ulogic;
  signal remote_irq_ack_data_in     : misc_noc_flit_type;
  signal remote_irq_ack_full        : std_ulogic;

  -- Bus (AHB-based processor core)
  signal ahbsi      : ahb_slv_in_type;
  signal ahbso      : ahb_slv_out_vector;
  signal noc_ahbso  : ahb_slv_out_vector;
  signal noc_ahbso_2  : ahb_slv_out_vector;
  signal ctrl_ahbso : ahb_slv_out_vector;
  signal ahbmi      : ahb_mst_in_type;
  signal ahbmo      : ahb_mst_out_vector;
  signal apbi       : apb_slv_in_type;
  signal apbo       : apb_slv_out_vector;
  signal noc_apbi   : apb_slv_in_type;
  signal noc_apbo   : apb_slv_out_vector;
  signal apb_req    : std_ulogic;
  signal apb_ack    : std_ulogic;
  signal mosi       : axi_mosi_vector(0 to 3);
  signal somi       : axi_somi_vector(0 to 3);

  signal ariane_drami : axi_mosi_type;
  signal ariane_dramo : axi_somi_type;

  signal cache_drami : axi_mosi_type;
  signal cache_dramo : axi_somi_type;

  signal cache_ahbsi : ahb_slv_in_type;
  signal cache_ahbso : ahb_slv_out_type;

  -- GRLIB parameters
  constant disas : integer := CFG_DISAS;
  constant pclow : integer := CFG_PCLOW;

  -- Tile parameters
  constant this_cpu_id            : integer                            := tile_cpu_id(tile_id);
  constant this_cpu_id_lv         : std_logic_vector(63 downto 0)      := conv_std_logic_vector(this_cpu_id, 64);
  constant this_dvfs_pindex       : integer                            := cpu_dvfs_pindex(tile_id);
  constant this_dvfs_paddr        : integer                            := cpu_dvfs_paddr(tile_id);
  constant this_dvfs_pmask        : integer                            := cpu_dvfs_pmask;
  constant this_dvfs_pconfig      : apb_config_type                    := cpu_dvfs_pconfig(tile_id);
  constant this_cache_id          : integer                            := tile_cache_id(tile_id);
  constant this_local_apb_en      : std_logic_vector(0 to NAPBSLV - 1) := local_apb_mask(tile_id);
  constant this_remote_apb_slv_en : std_logic_vector(0 to NAPBSLV - 1) := remote_apb_slv_mask(tile_id);
  constant this_local_ahb_en      : std_logic_vector(0 to NAHBSLV - 1) := local_ahb_mask(tile_id);
  constant this_remote_ahb_slv_en : std_logic_vector(0 to NAHBSLV - 1) := remote_ahb_mask(tile_id);
  constant this_l2_pindex         : integer                            := l2_cache_pindex(tile_id);
  constant this_l2_pconfig        : apb_config_type                    := fixed_apbo_pconfig(this_l2_pindex);
  constant this_has_dvfs          : integer                            := tile_has_dvfs(tile_id);
  constant this_has_pll           : integer                            := tile_has_pll(tile_id);
  constant this_extra_clk_buf     : integer                            := extra_clk_buf(tile_id);
  constant this_local_y           : local_yx                           := tile_y(tile_id);
  constant this_local_x           : local_yx                           := tile_x(tile_id);

  -- attribute mark_debug : string;

  -- attribute mark_debug of apbi : signal is "true";
  -- attribute mark_debug of apbo : signal is "true";
  -- attribute mark_debug of apb_req : signal is "true";
  -- attribute mark_debug of apb_ack : signal is "true";
  -- attribute mark_debug of remote_apb_snd_wrreq : signal is "true";
  -- attribute mark_debug of remote_apb_snd_data_in : signal is "true";
  -- attribute mark_debug of remote_apb_snd_full : signal is "true";
  -- attribute mark_debug of remote_apb_rcv_rdreq : signal is "true";
  -- attribute mark_debug of remote_apb_rcv_data_out : signal is "true";
  -- attribute mark_debug of remote_apb_rcv_empty : signal is "true";

begin

  pllclk <= clk_feedthru;

  -----------------------------------------------------------------------------
  -- Bus
  -----------------------------------------------------------------------------

  leon3_bus_gen: if GLOB_CPU_ARCH = leon3 generate

  hbus_pnp_gen : process (ahbso, noc_ahbso, noc_ahbso_2) is
  begin  -- process hbus_pnp_gen
    ctrl_ahbso <= noc_ahbso;

    for i in 0 to NAHBSLV - 1 loop
      if slm_ahb_mask(i) = '1' then
        ctrl_ahbso(i) <= noc_ahbso_2(i);
      end if;

      if this_local_ahb_en(i) = '1' then
        ctrl_ahbso(i) <= ahbso(i);
      end if;
    end loop;  -- i

  end process hbus_pnp_gen;

  ahb0 : ahbctrl                        -- AHB arbiter/multiplexer
    generic map (defmast => CFG_DEFMST, split => CFG_SPLIT,
                 rrobin  => CFG_RROBIN, ioaddr => CFG_AHBIO, fpnpen => CFG_FPNPEN,
                 nahbm   => maxahbm, nahbs => maxahbs)
    port map (rst, clk_feedthru, ahbmi, ahbmo, ahbsi, ctrl_ahbso);


  -- apb2noc proxy handles pindex and pconfig assignments
  -- All APB slaves in the CPU tile are seen as remote for the CPU, even those
  -- that are local to the tile. This allows any SoC master to access these slaves.
  -- Requests from the CPU are forwarded to an apb2noc proxy, then to the
  -- router. Requests for local slaves reenter immediately the tile and are
  -- served by a noc2apb proxy. All other requests will reach the destination
  -- tile. The AHB2APB bridge has been modified to be latency insensitive.
  apb0 : patient_apbctrl                -- AHB/APB bridge
    generic map (hindex     => ahb2apb_hindex, haddr => CFG_APBADDR, hmask => ahb2apb_hmask, nslaves => NAPBSLV,
                 remote_apb => this_remote_apb_slv_en)
    port map (rst, clk_feedthru, ahbsi, ahbso(ahb2apb_hindex), apbi, apbo, apb_req, apb_ack);

  end generate leon3_bus_gen;

  -----------------------------------------------------------------------------
  -- Drive unused bus ports
  -----------------------------------------------------------------------------

  leon3_bus_not_driven_gen: if GLOB_CPU_ARCH = leon3 generate

  -- Master hindex must match cpu_id. This restriction only applies to LEON3
  nam0 : for i in 0 to this_cpu_id - 1 generate
    ahbmo(i) <= ahbm_none;
  end generate;

  nam1 : for i in this_cpu_id + 1 to CFG_NCPU_TILE - 1 generate
    ahbmo(i) <= ahbm_none;
  end generate;

  nam2 : for i in CFG_NCPU_TILE + CFG_L2_ENABLE to NAHBMST-1 generate
    ahbmo(i) <= ahbm_none;
  end generate;

  no_hslv_gen : for i in 0 to NAHBSLV - 1 generate
    no_hslv_i_gen : if this_local_ahb_en(i) = '0' generate
      ahbso(i) <= ahbs_none;
    end generate no_hslv_i_gen;
  end generate no_hslv_gen;

  end generate leon3_bus_not_driven_gen;

  no_pslv_gen : for i in 0 to NAPBSLV - 1 generate
    -- NB: all local I/O-bus slaves are accessed through proxy as if they were
    -- remote. This allows any master in the system to access them
    no_pslv_i_gen : if this_local_apb_en(i) = '0' generate
      noc_apbo(i) <= apb_none;
    end generate no_pslv_i_gen;
  end generate no_pslv_gen;


  -----------------------------------------------------------------------------
  ---  Processor core
  -----------------------------------------------------------------------------

  --pragma translate_off
  process(clk_feedthru, rst)
  begin  -- process
    if rst = '1' then
      assert (GLOB_CPU_ARCH = leon3 or GLOB_CPU_ARCH = ariane) report "Processor core architecture not supported!" severity failure;
    end if;
  end process;
  --pragma translate_on

  -- Processor core reset
  cpu_rstn_gen_sim: if SIMULATION = true generate
    cpurstn <= rst;
  end generate cpu_rstn_gen_sim;

  cpu_rstn_gen: if SIMULATION = false generate

    -- SRST (soft reset) must be asserted twice:
    -- In between the two reset period, a program can be loaded to both the
    -- bootrom and the system main memory.
    -- The reset pulse is longer than one cycle, so we need to detect both edges
    cpu_rstn_state_update: process (clk_feedthru, rst) is
    begin  -- process cpu_rstn_gen
      if rst = '0' then                 -- asynchronous reset (active low)
        cpu_rstn_state <= por;
      elsif clk_feedthru'event and clk_feedthru = '1' then  -- rising clock edge
        cpu_rstn_state <= cpu_rstn_next;
      end if;
    end process cpu_rstn_state_update;

    cpu_rstn_fsm: process (cpu_rstn_state, srst) is
    begin  -- process cpu_rstn_fsm

      cpu_rstn_next <= cpu_rstn_state;
      cpurstn <= '0';

      case cpu_rstn_state is

        when por =>
          if srst = '1' then
            cpu_rstn_next <= soft_reset_1_h;
          end if;

        when soft_reset_1_h =>
          if srst = '0' then
            cpu_rstn_next <= soft_reset_1_l;
          end if;

        when soft_reset_1_l =>
          if srst = '1' then
            cpu_rstn_next <= soft_reset_2_h;
          end if;

        when soft_reset_2_h =>
          if srst = '0' then
            cpu_rstn_next <= soft_reset_2_l;
          end if;

        when soft_reset_2_l =>
          if srst = '1' then
            cpu_rstn_next <= soft_reset_3_h;
          end if;

        when soft_reset_3_h =>
          if srst = '0' then
            cpu_rstn_next <= soft_reset_3_l;
          end if;

        when soft_reset_3_l =>
          if srst = '1' then
            cpu_rstn_next <= soft_reset_4_h;
          end if;

        when soft_reset_4_h =>
          if srst = '0' then
            cpu_rstn_next <= run;
          end if;

        when run =>
          cpurstn <= '1';
          if srst = '1' then
            cpu_rstn_next <= soft_reset_1_h;
          end if;

        when others =>
          cpu_rstn_next <= por;

      end case;
    end process cpu_rstn_fsm;

  end generate cpu_rstn_gen;


  -- Leon3
  leon3_cpu_gen: if GLOB_CPU_ARCH = leon3 generate

  leon3_0 : leon3s
    generic map (this_cpu_id, CFG_FABTECH, CFG_MEMTECH, CFG_NWIN, CFG_DSU, CFG_FPU, CFG_V8,
                 0, CFG_MAC, pclow, CFG_NOTAG, CFG_NWP, CFG_ICEN, CFG_IREPL, CFG_ISETS, CFG_ILINE,
                 CFG_ISETSZ, CFG_ILOCK, CFG_DCEN, CFG_DREPL, CFG_DSETS, CFG_DLINE, CFG_DSETSZ,
                 CFG_DLOCK, CFG_DSNOOP, CFG_ILRAMEN, CFG_ILRAMSZ, CFG_ILRAMADDR, CFG_DLRAMEN,
                 CFG_DLRAMSZ, CFG_DLRAMADDR, CFG_MMUEN, CFG_ITLBNUM, CFG_DTLBNUM, CFG_TLB_TYPE, CFG_TLB_REP,
                 CFG_LDDEL, disas, CFG_ITBSZ, CFG_PWD, CFG_SVT, CFG_RSTADDR, CFG_NCPU_TILE-1,
                 CFG_DFIXED, CFG_SCAN, CFG_MMU_PAGE, CFG_BP)
    port map (clk_feedthru, cpurstn, ahbmi, ahbmo(this_cpu_id), ahbsi, ahbso, dflush,
              irqi, irqo_int, dbgi, dbgo);

  dbgi <= dbgi_none;
  cpuerr <= not dbgo.error;
  irqo <= irqo_int;

  end generate leon3_cpu_gen;

  -- Ariane
  ariane_cpu_gen: if GLOB_CPU_ARCH = ariane generate

    ariane_axi_wrap_1: ariane_axi_wrap
      generic map (
        HART_ID          => this_cpu_id_lv,
        NMST             => 2,
        NSLV             => 5,
        ROMBase          => X"0000_0000_0001_0000",
        ROMLength        => X"0000_0000_0001_0000",
        APBBase          => X"0000_0000" & conv_std_logic_vector(CFG_APBADDR, 12) & X"0_0000",
        APBLength        => X"0000_0000_1000_0000",
        CLINTBase        => X"0000_0000_0200_0000",
        CLINTLength      => X"0000_0000_000C_0000",
        SLMBase          => X"0000_0000_0400_0000",
        SLMLength        => X"0000_0000_0400_0000",  -- Reserving up to 64MB; devtree can set less
        DRAMBase         => X"0000_0000" & conv_std_logic_vector(ddr_haddr(0), 12) & X"0_0000",
        DRAMLength       => X"0000_0000_6000_0000",
        DRAMCachedLength => X"0000_0000_2000_0000")  -- TODO: length set automatically to match devtree
      port map (
        clk         => clk_feedthru,
        rstn        => cpurstn,
        irq         => irq,
        timer_irq   => timer_irq,
        ipi         => ipi,
        romi        => mosi(0),
        romo        => somi(0),
        drami       => ariane_drami,
        dramo       => ariane_dramo,
        clinti      => mosi(2),
        clinto      => somi(2),
        slmi        => mosi(3),
        slmo        => somi(3),
        apbi        => apbi,
        apbo        => apbo,
        apb_req     => apb_req,
        apb_ack     => apb_ack);

    -- exit() writes to this address right before completing the program
    -- Next instruction is a jump to current PC.
    cpuerr <= '1' when ariane_drami.aw.addr = X"80001000" and ariane_drami.aw.valid = '1' else '0';

    -- L1 can't be flushed on Ariane. So flush upon command.
    dflush <= '1';

  end generate ariane_cpu_gen;

  -----------------------------------------------------------------------------
  -- Services
  -----------------------------------------------------------------------------

  l2_rstn <= cpurstn and rst;

  with_cache_coherence : if CFG_L2_ENABLE /= 0 generate

    -- Memory access w/ cache coherence (write-back L2 cache)
    l2_wrapper_1 : l2_wrapper
      generic map (
        tech          => CFG_FABTECH,
        sets          => CFG_L2_SETS,
        ways          => CFG_L2_WAYS,
        hindex_mst    => CFG_NCPU_TILE,
        pindex        => this_l2_pindex,
        pirq          => CFG_SLD_L2_CACHE_IRQ,
        pconfig       => this_l2_pconfig,
        local_y       => this_local_y,
        local_x       => this_local_x,
        mem_hindex    => ddr_hindex(0),
        mem_hconfig   => cpu_tile_mig7_hconfig,
        mem_num       => CFG_NMEM_TILE,
        mem_info      => tile_mem_list(0 to CFG_NMEM_TILE - 1),
        cache_y       => cache_y,
        cache_x       => cache_x,
        cache_id      => this_cache_id,
        cache_tile_id => cache_tile_id)
      port map (
        rst                        => l2_rstn,
        clk                        => clk_feedthru,
        ahbsi                      => cache_ahbsi,
        ahbso                      => cache_ahbso,
        ahbmi                      => ahbmi,
        ahbmo                      => ahbmo(CFG_NCPU_TILE),
        mosi                       => cache_drami,
        somi                       => cache_dramo,
        apbi                       => noc_apbi,
        apbo                       => noc_apbo(this_l2_pindex),
        flush                      => dflush,
        coherence_req_wrreq        => coherence_req_wrreq,
        coherence_req_data_in      => coherence_req_data_in,
        coherence_req_full         => coherence_req_full,
        coherence_fwd_rdreq        => coherence_fwd_rdreq,
        coherence_fwd_data_out     => coherence_fwd_data_out,
        coherence_fwd_empty        => coherence_fwd_empty,
        coherence_rsp_rcv_rdreq    => coherence_rsp_rcv_rdreq,
        coherence_rsp_rcv_data_out => coherence_rsp_rcv_data_out,
        coherence_rsp_rcv_empty    => coherence_rsp_rcv_empty,
        coherence_rsp_snd_wrreq    => coherence_rsp_snd_wrreq,
        coherence_rsp_snd_data_in  => coherence_rsp_snd_data_in,
        coherence_rsp_snd_full     => coherence_rsp_snd_full,
        mon_cache                  => mon_cache
        );

  end generate with_cache_coherence;

  leon3_cpu_tile_services_gen: if GLOB_CPU_ARCH = leon3 generate

  leon3_no_cache_coherence : if CFG_L2_ENABLE = 0 generate
    coherence_rsp_snd_data_in <= (others => '0');
    coherence_rsp_snd_wrreq   <= '0';
    coherence_fwd_rdreq       <= '0';
    mon_cache                 <= monitor_cache_none;

    -- Remote uncached slaves, including memory
    -- Memory request/response sue planes 1 and 3; other slaves use plane 5
    cpu_ahbs2noc_1 : cpu_ahbs2noc
      generic map (
        tech             => CFG_FABTECH,
        hindex           => this_remote_ahb_slv_en,
        hconfig          => cpu_tile_fixed_ahbso_hconfig,
        local_y          => this_local_y,
        local_x          => this_local_x,
        mem_hindex       => ddr_hindex(0),
        mem_num          => CFG_NMEM_TILE,
        mem_info         => tile_mem_list,
        slv_y            => tile_y(io_tile_id),
        slv_x            => tile_x(io_tile_id),
        retarget_for_dma => 0,
        dma_length       => CFG_DLINE)
      port map (
        rst                        => rst,
        clk                        => clk_feedthru,
        ahbsi                      => ahbsi,
        ahbso                      => noc_ahbso,
        dma_selected               => '0',
        coherence_req_wrreq        => coherence_req_wrreq,
        coherence_req_data_in      => coherence_req_data_in,
        coherence_req_full         => coherence_req_full,
        coherence_rsp_rcv_rdreq    => coherence_rsp_rcv_rdreq,
        coherence_rsp_rcv_data_out => coherence_rsp_rcv_data_out,
        coherence_rsp_rcv_empty    => coherence_rsp_rcv_empty,
        remote_ahbs_snd_wrreq      => remote_ahbs_snd_wrreq,
        remote_ahbs_snd_data_in    => remote_ahbs_snd_data_in,
        remote_ahbs_snd_full       => remote_ahbs_snd_full,
        remote_ahbs_rcv_rdreq      => remote_ahbs_rcv_rdreq,
        remote_ahbs_rcv_data_out   => remote_ahbs_rcv_data_out,
        remote_ahbs_rcv_empty      => remote_ahbs_rcv_empty);

  end generate leon3_no_cache_coherence;

  leon3_with_cache_coherence : if CFG_L2_ENABLE /= 0 generate

    ahbso(ddr_hindex(0)) <= cache_ahbso;
    cache_ahbsi <= ahbsi;


    -- Remote uncached slaves
    cpu_ahbs2noc_1 : cpu_ahbs2noc
      generic map (
        tech             => CFG_FABTECH,
        hindex           => this_remote_ahb_slv_en,
        hconfig          => cpu_tile_fixed_ahbso_hconfig,
        local_y          => this_local_y,
        local_x          => this_local_x,
        mem_hindex       => ddr_hindex(0),
        mem_num          => CFG_NMEM_TILE,
        mem_info         => tile_mem_list,
        slv_y            => tile_y(io_tile_id),
        slv_x            => tile_x(io_tile_id),
        retarget_for_dma => 0,
        dma_length       => CFG_DLINE)
      port map (
        rst                        => rst,
        clk                        => clk_feedthru,
        ahbsi                      => ahbsi,
        ahbso                      => noc_ahbso,
        dma_selected               => '0',
        coherence_req_wrreq        => open,
        coherence_req_data_in      => open,
        coherence_req_full         => '0',
        coherence_rsp_rcv_rdreq    => open,
        coherence_rsp_rcv_data_out => (others => '0'),
        coherence_rsp_rcv_empty    => '1',
        remote_ahbs_snd_wrreq      => remote_ahbs_snd_wrreq,
        remote_ahbs_snd_data_in    => remote_ahbs_snd_data_in,
        remote_ahbs_snd_full       => remote_ahbs_snd_full,
        remote_ahbs_rcv_rdreq      => remote_ahbs_rcv_rdreq,
        remote_ahbs_rcv_data_out   => remote_ahbs_rcv_data_out,
        remote_ahbs_rcv_empty      => remote_ahbs_rcv_empty);

  end generate leon3_with_cache_coherence;

  -- Remote uncached slaves: shared-local memory; using DMA planes
  cpu_ahbs2noc_3 : cpu_ahbs2noc
    generic map (
      tech             => CFG_FABTECH,
      hindex           => slm_ahb_mask,
      hconfig          => cpu_tile_fixed_ahbso_hconfig,
      local_y          => this_local_y,
      local_x          => this_local_x,
      mem_hindex       => slm_hindex(0),
      mem_num          => CFG_NSLM_TILE,
      mem_info         => tile_slm_list,
      slv_y            => tile_y(io_tile_id),
      slv_x            => tile_x(io_tile_id),
      retarget_for_dma => 0,
      dma_length       => CFG_DLINE)
    port map (
      rst                        => rst,
      clk                        => clk_feedthru,
      ahbsi                      => ahbsi,
      ahbso                      => noc_ahbso_2,
      dma_selected               => '0',
      coherence_req_wrreq        => dma_snd_wrreq,
      coherence_req_data_in      => dma_snd_data_in_cpu,
      coherence_req_full         => dma_snd_full,
      coherence_rsp_rcv_rdreq    => dma_rcv_rdreq,
      coherence_rsp_rcv_data_out => dma_rcv_data_out,
      coherence_rsp_rcv_empty    => dma_rcv_empty,
      remote_ahbs_snd_wrreq      => open,
      remote_ahbs_snd_data_in    => open,
      remote_ahbs_snd_full       => '0',
      remote_ahbs_rcv_rdreq      => open,
      remote_ahbs_rcv_data_out   => (others => '0'),
      remote_ahbs_rcv_empty      => '1');

  end generate leon3_cpu_tile_services_gen;


  ariane_cpu_tile_services_gen: if GLOB_CPU_ARCH = ariane generate

    ariane_with_cache_coherence: if CFG_L2_ENABLE /= 0 generate
      cache_drami <= ariane_drami;
      ariane_dramo <= cache_dramo;

      mosi(1) <= axi_mosi_none;

      cache_ahbsi <= ahbs_in_none;

      cpu_axi2noc_1: cpu_axi2noc
        generic map (
          tech         => CFG_FABTECH,
          nmst         => 3,
          local_y      => this_local_y,
          local_x      => this_local_x,
          retarget_for_dma => 0,
          mem_axi_port => 1,
          mem_num      => CFG_NMEM_TILE,
          mem_info     => tile_mem_list,
          slv_y        => tile_y(io_tile_id),
          slv_x        => tile_x(io_tile_id))
        port map (
          rst                        => rst,
          clk                        => clk_feedthru,
          mosi                       => mosi(0 to 2),
          somi                       => somi(0 to 2),
          coherence_req_wrreq        => open,
          coherence_req_data_in      => open,
          coherence_req_full         => '0',
          coherence_rsp_rcv_rdreq    => open,
          coherence_rsp_rcv_data_out => (others => '0'),
          coherence_rsp_rcv_empty    => '1',
          remote_ahbs_snd_wrreq      => remote_ahbs_snd_wrreq,
          remote_ahbs_snd_data_in    => remote_ahbs_snd_data_in,
          remote_ahbs_snd_full       => remote_ahbs_snd_full,
          remote_ahbs_rcv_rdreq      => remote_ahbs_rcv_rdreq,
          remote_ahbs_rcv_data_out   => remote_ahbs_rcv_data_out,
          remote_ahbs_rcv_empty      => remote_ahbs_rcv_empty);

    end generate ariane_with_cache_coherence;

    ariane_no_cache_coherence : if CFG_L2_ENABLE = 0 generate
      cache_drami <= axi_mosi_none;

      mosi(1) <= ariane_drami;
      ariane_dramo <= somi(1);

      coherence_rsp_snd_data_in <= (others => '0');
      coherence_rsp_snd_wrreq   <= '0';
      coherence_fwd_rdreq       <= '0';
      mon_cache                 <= monitor_cache_none;

      cpu_axi2noc_1: cpu_axi2noc
        generic map (
          tech         => CFG_FABTECH,
          nmst         => 3,
          local_y      => this_local_y,
          local_x      => this_local_x,
          retarget_for_dma => 0,
          mem_axi_port => 1,
          mem_num      => CFG_NMEM_TILE,
          mem_info     => tile_mem_list,
          slv_y        => tile_y(io_tile_id),
          slv_x        => tile_x(io_tile_id))
        port map (
          rst                        => rst,
          clk                        => clk_feedthru,
          mosi                       => mosi(0 to 2),
          somi                       => somi(0 to 2),
          coherence_req_wrreq        => coherence_req_wrreq,
          coherence_req_data_in      => coherence_req_data_in,
          coherence_req_full         => coherence_req_full,
          coherence_rsp_rcv_rdreq    => coherence_rsp_rcv_rdreq,
          coherence_rsp_rcv_data_out => coherence_rsp_rcv_data_out,
          coherence_rsp_rcv_empty    => coherence_rsp_rcv_empty,
          remote_ahbs_snd_wrreq      => remote_ahbs_snd_wrreq,
          remote_ahbs_snd_data_in    => remote_ahbs_snd_data_in,
          remote_ahbs_snd_full       => remote_ahbs_snd_full,
          remote_ahbs_rcv_rdreq      => remote_ahbs_rcv_rdreq,
          remote_ahbs_rcv_data_out   => remote_ahbs_rcv_data_out,
          remote_ahbs_rcv_empty      => remote_ahbs_rcv_empty);

    end generate ariane_no_cache_coherence;

    -- Remote uncached slaves: shared-local memory; using DMA planes
    cpu_axi2noc_3: cpu_axi2noc
      generic map (
        tech         => CFG_FABTECH,
        nmst         => 1,
        local_y      => this_local_y,
        local_x      => this_local_x,
        retarget_for_dma => 0,
        mem_axi_port => 0,
        mem_num      => CFG_NSLM_TILE,
        mem_info     => tile_slm_list,
        slv_y        => tile_y(io_tile_id),
        slv_x        => tile_x(io_tile_id))
      port map (
        rst                        => rst,
        clk                        => clk_feedthru,
        mosi                       => mosi(3 to 3),
        somi                       => somi(3 to 3),
        coherence_req_wrreq        => dma_snd_wrreq,
        coherence_req_data_in      => dma_snd_data_in_cpu,
        coherence_req_full         => dma_snd_full,
        coherence_rsp_rcv_rdreq    => dma_rcv_rdreq,
        coherence_rsp_rcv_data_out => dma_rcv_data_out,
        coherence_rsp_rcv_empty    => dma_rcv_empty,
        remote_ahbs_snd_wrreq      => open,
        remote_ahbs_snd_data_in    => open,
        remote_ahbs_snd_full       => '0',
        remote_ahbs_rcv_rdreq      => open,
        remote_ahbs_rcv_data_out   => (others => '0'),
        remote_ahbs_rcv_empty      => '1');

  end generate ariane_cpu_tile_services_gen;

  -- DVFS
  dvfs_gen : if this_has_dvfs /= 0 and this_has_pll /= 0 generate
    dvfs_top_1 : dvfs_top
      generic map (
        tech          => CFG_FABTECH,
        extra_clk_buf => this_extra_clk_buf,
        pindex        => this_dvfs_pindex,
        paddr         => this_dvfs_paddr,
        pmask         => this_dvfs_pmask)
      port map (
        rst       => rst,
        clk       => clk_feedthru,
        refclk    => refclk,
        pllbypass => pllbypass,
        pllclk    => clk_feedthru,
        apbi      => noc_apbi,
        apbo      => noc_apbo(this_dvfs_pindex),
        acc_idle  => mon_dvfs_in.acc_idle,
        traffic   => mon_dvfs_in.traffic,
        burst     => mon_dvfs_in.burst,
        mon_dvfs  => mon_dvfs_ctrl
        );

    mon_dvfs.clk       <= mon_dvfs_ctrl.clk;
    mon_dvfs.vf        <= mon_dvfs_ctrl.vf;
    mon_dvfs.transient <= mon_dvfs_ctrl.transient;
  end generate dvfs_gen;

  dvfs_no_master_or_no_dvfs : if this_has_dvfs = 0 or this_has_pll = 0 generate
    clk_feedthru <= refclk;
    process (clk_feedthru, rst)
    begin  -- process
      if rst = '0' then                 -- asynchronous reset (active low)
        mon_dvfs.vf <= "1000";
      elsif clk_feedthru'event and clk_feedthru = '1' then  -- rising clock edge
        if this_has_dvfs /= 0 then
          mon_dvfs.vf <= mon_dvfs_in.vf;
        end if;
      end if;
    end process;
    process (mon_dvfs_in)
    begin  -- process
      if this_has_dvfs = 1 then
        mon_dvfs.transient <= mon_dvfs_in.transient;
      else
        mon_dvfs.transient <= '0';
      end if;
    end process;
    mon_dvfs.clk <= clk_feedthru;
  end generate dvfs_no_master_or_no_dvfs;

  mon_dvfs.acc_idle <= irqo_int.pwd;
  mon_dvfs.traffic  <= '0';
  mon_dvfs.burst    <= '0';


  -- I/O bus proxy - remote memory-mapped I/O accessed from local masters
  apb2noc_1 : apb2noc
    generic map (
      tech        => CFG_FABTECH,
      ncpu        => CFG_NCPU_TILE,
      local_y     => this_local_y,
      local_x     => this_local_x,
      apb_slv_en  => this_remote_apb_slv_en,
      apb_slv_cfg => fixed_apbo_pconfig,
      apb_slv_y   => apb_slv_y,
      apb_slv_x   => apb_slv_x)
    port map (
      rst                     => rst,
      clk                     => clk_feedthru,
      apbi                    => apbi,
      apbo                    => apbo,
      apb_req                 => apb_req,
      apb_ack                 => apb_ack,
      remote_apb_snd_wrreq    => remote_apb_snd_wrreq,
      remote_apb_snd_data_in  => remote_apb_snd_data_in,
      remote_apb_snd_full     => remote_apb_snd_full,
      remote_apb_rcv_rdreq    => remote_apb_rcv_rdreq,
      remote_apb_rcv_data_out => remote_apb_rcv_data_out,
      remote_apb_rcv_empty    => remote_apb_rcv_empty);

  -- I/O bus proxy - local memory-mapped I/O accessed from remote masters
  misc_noc2apb_1 : misc_noc2apb
    generic map (
      tech         => CFG_FABTECH,
      local_y      => this_local_y,
      local_x      => this_local_x,
      local_apb_en => this_local_apb_en)
    port map (
      rst              => rst,
      clk              => clk_feedthru,
      apbi             => noc_apbi,
      apbo             => noc_apbo,
      pready           => '1',
      dvfs_transient   => '0',
      apb_snd_wrreq    => apb_snd_wrreq,
      apb_snd_data_in  => apb_snd_data_in,
      apb_snd_full     => apb_snd_full,
      apb_rcv_rdreq    => apb_rcv_rdreq,
      apb_rcv_data_out => apb_rcv_data_out,
      apb_rcv_empty    => apb_rcv_empty);

  -- Interrupt level acknowledge - remote interrupt controller
  cpu_irq2noc_1 : cpu_irq2noc
    generic map (
      tech    => CFG_FABTECH,
      cpu_id  => this_cpu_id,
      local_y => this_local_y,
      local_x => this_local_x,
      irq_y   => tile_y(io_tile_id),
      irq_x   => tile_x(io_tile_id))
    port map (
      rst                    => rst,
      clk                    => clk_feedthru,
      irqi                   => irqi,
      irqo                   => irqo,
      irqo_fifo_overflow     => open,
      remote_irq_rdreq       => remote_irq_rdreq,
      remote_irq_data_out    => remote_irq_data_out,
      remote_irq_empty       => remote_irq_empty,
      remote_irq_ack_wrreq   => remote_irq_ack_wrreq,
      remote_irq_ack_data_in => remote_irq_ack_data_in,
      remote_irq_ack_full    => remote_irq_ack_full);

  -----------------------------------------------------------------------------
  -- Tile queues
  -----------------------------------------------------------------------------

  -- Mark CPU DMA
  set_cpu_dma: process (dma_snd_data_in_cpu) is
  begin
    dma_snd_data_in <= dma_snd_data_in_cpu;
    if get_preamble(NOC_FLIT_SIZE, dma_snd_data_in_cpu) = PREAMBLE_HEADER then
      dma_snd_data_in(NOC_FLIT_SIZE - PREAMBLE_WIDTH - 4*YX_WIDTH - MSG_TYPE_WIDTH - 2 downto
                      NOC_FLIT_SIZE - PREAMBLE_WIDTH - 4*YX_WIDTH - MSG_TYPE_WIDTH - RESERVED_WIDTH) <= CPU_DMA;
    end if;
  end process set_cpu_dma;

  cpu_tile_q_1 : cpu_tile_q
    generic map (
      tech => CFG_FABTECH)
    port map (
      rst                        => rst,
      clk                        => clk_feedthru,
      coherence_req_wrreq        => coherence_req_wrreq,
      coherence_req_data_in      => coherence_req_data_in,
      coherence_req_full         => coherence_req_full,
      coherence_fwd_rdreq        => coherence_fwd_rdreq,
      coherence_fwd_data_out     => coherence_fwd_data_out,
      coherence_fwd_empty        => coherence_fwd_empty,
      coherence_rsp_rcv_rdreq    => coherence_rsp_rcv_rdreq,
      coherence_rsp_rcv_data_out => coherence_rsp_rcv_data_out,
      coherence_rsp_rcv_empty    => coherence_rsp_rcv_empty,
      coherence_rsp_snd_wrreq    => coherence_rsp_snd_wrreq,
      coherence_rsp_snd_data_in  => coherence_rsp_snd_data_in,
      coherence_rsp_snd_full     => coherence_rsp_snd_full,
      dma_rcv_rdreq              => dma_rcv_rdreq,
      dma_rcv_data_out           => dma_rcv_data_out,
      dma_rcv_empty              => dma_rcv_empty,
      dma_snd_wrreq              => dma_snd_wrreq,
      dma_snd_data_in            => dma_snd_data_in,
      dma_snd_full               => dma_snd_full,
      remote_ahbs_snd_wrreq      => remote_ahbs_snd_wrreq,
      remote_ahbs_snd_data_in    => remote_ahbs_snd_data_in,
      remote_ahbs_snd_full       => remote_ahbs_snd_full,
      remote_ahbs_rcv_rdreq      => remote_ahbs_rcv_rdreq,
      remote_ahbs_rcv_data_out   => remote_ahbs_rcv_data_out,
      remote_ahbs_rcv_empty      => remote_ahbs_rcv_empty,
      apb_rcv_rdreq              => apb_rcv_rdreq,
      apb_rcv_data_out           => apb_rcv_data_out,
      apb_rcv_empty              => apb_rcv_empty,
      apb_snd_wrreq              => apb_snd_wrreq,
      apb_snd_data_in            => apb_snd_data_in,
      apb_snd_full               => apb_snd_full,
      remote_apb_rcv_rdreq       => remote_apb_rcv_rdreq,
      remote_apb_rcv_data_out    => remote_apb_rcv_data_out,
      remote_apb_rcv_empty       => remote_apb_rcv_empty,
      remote_apb_snd_wrreq       => remote_apb_snd_wrreq,
      remote_apb_snd_data_in     => remote_apb_snd_data_in,
      remote_apb_snd_full        => remote_apb_snd_full,
      remote_irq_rdreq           => remote_irq_rdreq,
      remote_irq_data_out        => remote_irq_data_out,
      remote_irq_empty           => remote_irq_empty,
      remote_irq_ack_wrreq       => remote_irq_ack_wrreq,
      remote_irq_ack_data_in     => remote_irq_ack_data_in,
      remote_irq_ack_full        => remote_irq_ack_full,
      noc1_out_data              => noc1_output_port,
      noc1_out_void              => noc1_data_void_out,
      noc1_out_stop              => noc1_stop_in,
      noc1_in_data               => noc1_input_port,
      noc1_in_void               => noc1_data_void_in,
      noc1_in_stop               => noc1_stop_out,
      noc2_out_data              => noc2_output_port,
      noc2_out_void              => noc2_data_void_out,
      noc2_out_stop              => noc2_stop_in,
      noc2_in_data               => noc2_input_port,
      noc2_in_void               => noc2_data_void_in,
      noc2_in_stop               => noc1_stop_out,
      noc3_out_data              => noc3_output_port,
      noc3_out_void              => noc3_data_void_out,
      noc3_out_stop              => noc3_stop_in,
      noc3_in_data               => noc3_input_port,
      noc3_in_void               => noc3_data_void_in,
      noc3_in_stop               => noc3_stop_out,
      noc4_out_data              => noc4_output_port,
      noc4_out_void              => noc4_data_void_out,
      noc4_out_stop              => noc4_stop_in,
      noc4_in_data               => noc4_input_port,
      noc4_in_void               => noc4_data_void_in,
      noc4_in_stop               => noc4_stop_out,
      noc5_out_data              => noc5_output_port,
      noc5_out_void              => noc5_data_void_out,
      noc5_out_stop              => noc5_stop_in,
      noc5_in_data               => noc5_input_port,
      noc5_in_void               => noc5_data_void_in,
      noc5_in_stop               => noc5_stop_out,
      noc6_out_data              => noc6_output_port,
      noc6_out_void              => noc6_data_void_out,
      noc6_out_stop              => noc6_stop_in,
      noc6_in_data               => noc6_input_port,
      noc6_in_void               => noc6_data_void_in,
      noc6_in_stop               => noc6_stop_out);

end;
