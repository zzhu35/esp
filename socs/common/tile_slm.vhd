-- Copyright (c) 2011-2020 Columbia University, System Level Design Group
-- SPDX-License-Identifier: Apache-2.0

-----------------------------------------------------------------------------
--  Memory interface tile
------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use work.esp_global.all;
use work.amba.all;
use work.stdlib.all;
use work.sld_devices.all;
use work.devices.all;
use work.gencomp.all;
use work.leon3.all;
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

entity tile_slm is
  generic (
    tile_id : integer range 0 to CFG_TILES_NUM - 1 := 0);
  port (
    rst                : in  std_ulogic;
    clk                : in  std_ulogic;
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
    mon_mem            : out monitor_mem_type;
    mon_dvfs           : out monitor_dvfs_type);
end;


architecture rtl of tile_slm is

  -- Queues (despite their name, for this tile, all queues carry non-coherent requests)
  signal dma_rcv_rdreq              : std_ulogic;
  signal dma_rcv_data_out           : noc_flit_type;
  signal dma_rcv_empty              : std_ulogic;
  signal dma_snd_wrreq              : std_ulogic;
  signal dma_snd_data_in            : noc_flit_type;
  signal dma_snd_full               : std_ulogic;
  signal dma_snd_atleast_4slots     : std_ulogic;
  signal dma_snd_exactly_3slots     : std_ulogic;
  signal cpu_dma_rcv_rdreq          : std_ulogic;
  signal cpu_dma_rcv_data_out       : noc_flit_type;
  signal cpu_dma_rcv_empty          : std_ulogic;
  signal cpu_dma_snd_wrreq          : std_ulogic;
  signal cpu_dma_snd_data_in        : noc_flit_type;
  signal cpu_dma_snd_full           : std_ulogic;
  signal coherent_dma_rcv_rdreq     : std_ulogic;
  signal coherent_dma_rcv_data_out  : noc_flit_type;
  signal coherent_dma_rcv_empty     : std_ulogic;
  signal coherent_dma_snd_wrreq     : std_ulogic;
  signal coherent_dma_snd_data_in   : noc_flit_type;
  signal coherent_dma_snd_full      : std_ulogic;
  signal coherent_dma_snd_atleast_4slots : std_ulogic;
  signal coherent_dma_snd_exactly_3slots : std_ulogic;
  -- These requests are delivered through NoC5 (32 bits always)
  -- however, the proxy that handles expects a flit size in
  -- accordance with ARCH_BITS. Hence we need to pad and move
  -- header info and preamble to the right bit position
  signal remote_ahbs_rcv_rdreq      : std_ulogic;
  signal remote_ahbs_rcv_data_out   : misc_noc_flit_type;
  signal remote_ahbs_rcv_empty      : std_ulogic;
  signal remote_ahbs_snd_wrreq      : std_ulogic;
  signal remote_ahbs_snd_data_in    : misc_noc_flit_type;
  signal remote_ahbs_snd_full       : std_ulogic;
  -- Extended remote_ahbs_* signals that
  signal remote_ahbm_rcv_rdreq      : std_ulogic;
  signal remote_ahbm_rcv_data_out   : noc_flit_type;
  signal remote_ahbm_rcv_empty      : std_ulogic;
  signal remote_ahbm_snd_wrreq      : std_ulogic;
  signal remote_ahbm_snd_data_in    : noc_flit_type;
  signal remote_ahbm_snd_full       : std_ulogic;

  -- Bus
  signal ahbsi : ahb_slv_in_type;
  signal ahbso : ahb_slv_out_vector;
  signal ahbmi : ahb_mst_in_type;
  signal ahbmo : ahb_mst_out_vector;

  signal ctrl_ahbsi : ahb_slv_in_type;
  signal ctrl_ahbso : ahb_slv_out_vector;
  signal ctrl_ahbmi : ahb_mst_in_type;
  signal ctrl_ahbmo : ahb_mst_out_vector;

  -- Tile parameters
  constant this_slm_id       : integer                            := tile_slm_id(tile_id);
  constant this_slm_hindex   : integer                            := slm_hindex(this_slm_id);
  constant this_slm_haddr    : integer                            := slm_haddr(this_slm_id);
  constant this_slm_hmask    : integer                            := slm_hmask(this_slm_id);
  constant this_local_ahb_en : std_logic_vector(0 to NAHBSLV - 1) := local_ahb_mask(tile_id);
  constant this_local_y      : local_yx                           := tile_y(tile_id);
  constant this_local_x      : local_yx                           := tile_x(tile_id);

begin

  -----------------------------------------------------------------------------
  -- Bus
  -----------------------------------------------------------------------------

  hp_pandp_gen : process (ctrl_ahbmi, ctrl_ahbsi, ahbmo, ahbso)
  begin  -- process assign_bus_ctrl_sig
    ahbmi      <= ctrl_ahbmi;
    ahbsi      <= ctrl_ahbsi;
    ctrl_ahbmo <= ahbmo;
    ctrl_ahbso <= ahbso;

    for i in 0 to NAHBSLV - 1 loop
      if this_local_ahb_en(i) = '1' then
        ctrl_ahbso(i).hconfig <= fixed_ahbso_hconfig(i);
      else
        ctrl_ahbso(i).hconfig <= hconfig_none;
      end if;
    end loop;  -- i
  end process hp_pandp_gen;

  ahb2 : ahbctrl                        -- AHB arbiter/multiplexer
    generic map (defmast => 0, split => CFG_SPLIT,
                 rrobin  => CFG_RROBIN, ioaddr => CFG_AHBIO, fpnpen => CFG_FPNPEN,
                 nahbm   => maxahbm, nahbs => maxahbs)
    port map (rst, clk, ctrl_ahbmi, ctrl_ahbmo, ctrl_ahbsi, ctrl_ahbso);


  -----------------------------------------------------------------------
  ---  Drive unused bus ports
  -----------------------------------------------------------------------

  no_hmst_gen : for i in 2 to NAHBMST-1 generate
    ahbmo(i) <= ahbm_none;
  end generate;

  no_hslv_gen : for i in 0 to NAHBSLV - 1 generate
    no_hslv_i_gen : if this_local_ahb_en(i) = '0' generate
      ahbso(i) <= ahbs_none;
    end generate no_hslv_i_gen;
  end generate;

  -----------------------------------------------------------------------------
  -- Local devices
  -----------------------------------------------------------------------------

  -- Shared Local Memory (SLM)
  ahbslm_1: ahbslm
    generic map (
      hindex => this_slm_hindex,
      haddr  => this_slm_haddr,
      hmask  => this_slm_hmask,
      tech   => CFG_FABTECH,
      mbytes => CFG_SLM_MBYTES)
    port map (
      rst    => rst,
      clk    => clk,
      ahbsi  => ahbsi,
      ahbso  => ahbso(this_slm_hindex));


  -----------------------------------------------------------------------------
  -- Services
  -----------------------------------------------------------------------------

  -- DVFS monitor
  mon_dvfs.vf        <= "1000";         --run at highest frequency always
  mon_dvfs.transient <= '0';
  mon_dvfs.clk       <= clk;
  mon_dvfs.acc_idle  <= '0';
  mon_dvfs.traffic   <= '0';
  mon_dvfs.burst     <= '0';

  -- Memory access monitor
  mon_mem.clk              <= clk;
  mon_mem.coherent_req     <= '0';
  mon_mem.coherent_fwd     <= '0';
  mon_mem.coherent_rsp_rcv <= '0';
  mon_mem.coherent_rsp_snd <= '0';
  -- we can allow Ethernet to use the SLM and Ethernet operates on the
  -- coherent DMA queues when LLC is enabled. Nevertheless, when using SLM,
  -- Ethernet data will not be cached, hence any activity on the coherent DMA
  -- queues is still reported as non-coherent DMA for this tile.
  mon_mem.dma_req          <= dma_rcv_rdreq or cpu_dma_rcv_rdreq or coherent_dma_rcv_rdreq;
  mon_mem.dma_rsp          <= dma_snd_wrreq or cpu_dma_snd_wrreq or coherent_dma_snd_wrreq;
  mon_mem.coherent_dma_req <= '0';
  mon_mem.coherent_dma_rsp <= '0';

  -----------------------------------------------------------------------------
  -- Proxies
  -----------------------------------------------------------------------------

  -- FROM NoC

  -- Hendle CPU requests accelerator DMA and Ethernet (non coherent only)
  mem_noc2ahbm_1 : mem_noc2ahbm
    generic map (
      tech        => CFG_FABTECH,
      hindex      => 0,
      local_y     => this_local_y,
      local_x     => this_local_x,
      axitran     => GLOB_CPU_AXI,
      little_end  => 0,
      eth_dma     => 0,
      narrow_noc  => 0,
      cacheline   => 1,
      l2_cache_en => 0)
    port map (
      rst                       => rst,
      clk                       => clk,
      ahbmi                     => ahbmi,
      ahbmo                     => ahbmo(0),
      coherence_req_rdreq       => cpu_dma_rcv_rdreq,
      coherence_req_data_out    => cpu_dma_rcv_data_out,
      coherence_req_empty       => cpu_dma_rcv_empty,
      coherence_fwd_wrreq       => open,
      coherence_fwd_data_in     => open,
      coherence_fwd_full        => '0',
      coherence_rsp_snd_wrreq   => cpu_dma_snd_wrreq,
      coherence_rsp_snd_data_in => cpu_dma_snd_data_in,
      coherence_rsp_snd_full    => cpu_dma_snd_full,
      dma_rcv_rdreq             => dma_rcv_rdreq,
      dma_rcv_data_out          => dma_rcv_data_out,
      dma_rcv_empty             => dma_rcv_empty,
      dma_snd_wrreq             => dma_snd_wrreq,
      dma_snd_data_in           => dma_snd_data_in,
      dma_snd_full              => dma_snd_full,
      dma_snd_atleast_4slots    => dma_snd_atleast_4slots,
      dma_snd_exactly_3slots    => dma_snd_exactly_3slots);

  -- Handle JTAG or EDCL requests to memory as well as Ethernet coherent DMA
  mem_noc2ahbm_2 : mem_noc2ahbm
    generic map (
      tech        => CFG_FABTECH,
      hindex      => 1,
      local_y     => this_local_y,
      local_x     => this_local_x,
      axitran     => 0,
      little_end  => 0,
      eth_dma     => 1,               -- Exception for fixed 32-bits DMA
      narrow_noc  => 0,
      cacheline   => 1,
      l2_cache_en => 0)
    port map (
      rst                       => rst,
      clk                       => clk,
      ahbmi                     => ahbmi,
      ahbmo                     => ahbmo(1),
      coherence_req_rdreq       => remote_ahbm_rcv_rdreq,
      coherence_req_data_out    => remote_ahbm_rcv_data_out,
      coherence_req_empty       => remote_ahbm_rcv_empty,
      coherence_fwd_wrreq       => open,
      coherence_fwd_data_in     => open,
      coherence_fwd_full        => '0',
      coherence_rsp_snd_wrreq   => remote_ahbm_snd_wrreq,
      coherence_rsp_snd_data_in => remote_ahbm_snd_data_in,
      coherence_rsp_snd_full    => remote_ahbm_snd_full,
      -- These requests are treated as non-coherent when no LLC is present!
      dma_rcv_rdreq             => coherent_dma_rcv_rdreq,
      dma_rcv_data_out          => coherent_dma_rcv_data_out,
      dma_rcv_empty             => coherent_dma_rcv_empty,
      dma_snd_wrreq             => coherent_dma_snd_wrreq,
      dma_snd_data_in           => coherent_dma_snd_data_in,
      dma_snd_full              => coherent_dma_snd_full,
      dma_snd_atleast_4slots    => coherent_dma_snd_atleast_4slots,
      dma_snd_exactly_3slots    => coherent_dma_snd_exactly_3slots);

  remote_ahbs_rcv_rdreq <= remote_ahbm_rcv_rdreq;
  remote_ahbm_rcv_empty <= remote_ahbs_rcv_empty;
  remote_ahbs_snd_wrreq <= remote_ahbm_snd_wrreq;
  remote_ahbm_snd_full  <= remote_ahbs_snd_full;

  large_bus: if ARCH_BITS /= 32 generate
    remote_ahbm_rcv_data_out <= narrow_to_large_flit(remote_ahbs_rcv_data_out);
    remote_ahbs_snd_data_in <= large_to_narrow_flit(remote_ahbm_snd_data_in);
  end generate large_bus;

  std_bus: if ARCH_BITS = 32 generate
    remote_ahbm_rcv_data_out <= remote_ahbs_rcv_data_out;
    remote_ahbs_snd_data_in  <= remote_ahbm_snd_data_in;
  end generate std_bus;

  -----------------------------------------------------------------------------
  -- Tile queues
  -----------------------------------------------------------------------------

  slm_tile_q_1 : slm_tile_q
    generic map (
      tech => CFG_FABTECH)
    port map (
      rst                        => rst,
      clk                        => clk,
      dma_rcv_rdreq              => dma_rcv_rdreq,
      dma_rcv_data_out           => dma_rcv_data_out,
      dma_rcv_empty              => dma_rcv_empty,
      cpu_dma_rcv_rdreq          => cpu_dma_rcv_rdreq,
      cpu_dma_rcv_data_out       => cpu_dma_rcv_data_out,
      cpu_dma_rcv_empty          => cpu_dma_rcv_empty,
      coherent_dma_snd_wrreq     => coherent_dma_snd_wrreq,
      coherent_dma_snd_data_in   => coherent_dma_snd_data_in,
      coherent_dma_snd_full      => coherent_dma_snd_full,
      coherent_dma_snd_atleast_4slots => coherent_dma_snd_atleast_4slots,
      coherent_dma_snd_exactly_3slots => coherent_dma_snd_exactly_3slots,
      dma_snd_wrreq              => dma_snd_wrreq,
      dma_snd_data_in            => dma_snd_data_in,
      dma_snd_full               => dma_snd_full,
      dma_snd_atleast_4slots     => dma_snd_atleast_4slots,
      dma_snd_exactly_3slots     => dma_snd_exactly_3slots,
      cpu_dma_snd_wrreq          => cpu_dma_snd_wrreq,
      cpu_dma_snd_data_in        => cpu_dma_snd_data_in,
      cpu_dma_snd_full           => cpu_dma_snd_full,
      coherent_dma_rcv_rdreq     => coherent_dma_rcv_rdreq,
      coherent_dma_rcv_data_out  => coherent_dma_rcv_data_out,
      coherent_dma_rcv_empty     => coherent_dma_rcv_empty,
      remote_ahbs_rcv_rdreq      => remote_ahbs_rcv_rdreq,
      remote_ahbs_rcv_data_out   => remote_ahbs_rcv_data_out,
      remote_ahbs_rcv_empty      => remote_ahbs_rcv_empty,
      remote_ahbs_snd_wrreq      => remote_ahbs_snd_wrreq,
      remote_ahbs_snd_data_in    => remote_ahbs_snd_data_in,
      remote_ahbs_snd_full       => remote_ahbs_snd_full,
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
