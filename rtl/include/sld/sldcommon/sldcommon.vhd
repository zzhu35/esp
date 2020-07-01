-- Copyright (c) 2011-2019 Columbia University, System Level Design Group
-- SPDX-License-Identifier: Apache-2.0

library ieee;
use ieee.std_logic_1164.all;

use work.esp_global.all;

use work.amba.all;
use work.stdlib.all;
use work.sld_devices.all;
use work.devices.all;

use work.gencomp.all;
use work.genacc.all;

use work.coretypes.all;
use work.acctypes.all;

package sldcommon is

  type monitor_ddr_type is record
    clk              : std_ulogic;
    word_transfer    : std_ulogic;
  end record;

  type monitor_mem_type is record
    clk              : std_ulogic;
    coherent_req     : std_ulogic;
    coherent_fwd     : std_ulogic;
    coherent_rsp_rcv : std_ulogic;
    coherent_rsp_snd : std_ulogic;
    dma_req          : std_ulogic;
    dma_rsp          : std_ulogic;
    coherent_dma_req : std_ulogic;
    coherent_dma_rsp : std_ulogic;
  end record;

  type monitor_noc_type is record
    clk          : std_ulogic;
    tile_inject  : std_ulogic;
    queue_full   : std_logic_vector(4 downto 0);
  end record;

  type monitor_cache_type is record
    clk  : std_ulogic;
    hit  : std_ulogic;
    miss : std_ulogic;
  end record monitor_cache_type;

  type monitor_acc_type is record
    clk   : std_ulogic;
    go    : std_ulogic;
    run   : std_ulogic;
    done  : std_ulogic;
    burst : std_ulogic;
  end record;

  type monitor_dvfs_type is record
    clk     : std_ulogic;
    vf      : std_logic_vector(3 downto 0);
    acc_idle    : std_ulogic;
    traffic : std_ulogic;
    burst   : std_ulogic;
    transient : std_ulogic;
  end record;

  type monitor_ddr_vector is array (natural range <>) of monitor_ddr_type;

  type monitor_noc_vector is array (natural range <>) of monitor_noc_type;
  type monitor_noc_matrix is array (natural range <>, natural range <>) of monitor_noc_type;

  type monitor_mem_vector is array (natural range <>) of monitor_mem_type;

  type monitor_cache_vector is array (natural range <>) of monitor_cache_type;

  type monitor_acc_vector is array (natural range <>) of monitor_acc_type;

  type monitor_dvfs_vector is array (natural range <>) of monitor_dvfs_type;

  constant monitor_noc_none : monitor_noc_type := (
    clk => '0',
    tile_inject => '0',
    queue_full => (others => '0')
    );

  constant monitor_acc_none : monitor_acc_type := (
    clk   => '0',
    go    => '0',
    run   => '0',
    done  => '0',
    burst => '0'
    );

  constant monitor_cache_none : monitor_cache_type := (
    clk => '0',
    hit => '0',
    miss => '0'
    );

  constant monitor_dvfs_none : monitor_dvfs_type := (
    clk => '0',
    vf => (others => '0'),
    acc_idle => '0',
    traffic => '0',
    burst => '0',
    transient => '0'
    );

  component monitor
    generic (
      memtech                : integer;
      mmi64_width            : integer;
      ddrs_num               : integer;
      slms_num               : integer;
      nocs_num               : integer;
      tiles_num              : integer;
      accelerators_num       : integer;
      l2_num                 : integer;
      llc_num                : integer;
      mon_ddr_en             : integer;
      mon_noc_tile_inject_en : integer;
      mon_noc_queues_full_en : integer;
      mon_acc_en             : integer;
      mon_mem_en             : integer;
      mon_l2_en              : integer;
      mon_llc_en             : integer;
      mon_dvfs_en            : integer);
    port (
      profpga_clk0_p   : in  std_logic;
      profpga_clk0_n   : in  std_logic;
      profpga_sync0_p  : in  std_logic;
      profpga_sync0_n  : in  std_logic;
      dmbi_h2f         : in  std_logic_vector(19 downto 0);
      dmbi_f2h         : out std_logic_vector(19 downto 0);
      user_rstn        : in  std_logic;
      mon_ddr          : in  monitor_ddr_vector(0 to ddrs_num-1);
      mon_noc          : in  monitor_noc_matrix(0 to nocs_num-1, 0 to tiles_num-1);
      mon_acc          : in  monitor_acc_vector(0 to relu(accelerators_num-1));
      mon_mem          : in  monitor_mem_vector(0 to ddrs_num+slms_num-1);
      mon_l2           : in  monitor_cache_vector(0 to relu(l2_num-1));
      mon_llc          : in  monitor_cache_vector(0 to relu(llc_num-1));
      mon_dvfs         : in  monitor_dvfs_vector(0 to tiles_num-1)
      );

  end component;

  component acc_tlb
    generic (
      tech           : integer;
      scatter_gather : integer range 0 to 1;
      tlb_entries    : integer);
    port (
      clk                  : in  std_ulogic;
      rst                  : in  std_ulogic;
      bankreg              : in  bank_type(0 to MAXREGNUM - 1);
      rd_request           : in  std_ulogic;
      rd_index             : in  std_logic_vector(31 downto 0);
      rd_length            : in  std_logic_vector(31 downto 0);
      wr_request           : in  std_ulogic;
      wr_index             : in  std_logic_vector(31 downto 0);
      wr_length            : in  std_logic_vector(31 downto 0);
      dma_tran_start       : out std_ulogic;
      dma_tran_header_sent : in  std_ulogic;
      dma_tran_done        : in  std_ulogic;
      pending_dma_write    : out std_ulogic;
      pending_dma_read     : out std_ulogic;
      tlb_empty            : out std_ulogic;
      tlb_clear            : in  std_ulogic;
      tlb_valid            : in  std_ulogic;
      tlb_write            : in  std_ulogic;
      tlb_wr_address       : in  std_logic_vector((log2xx(tlb_entries) -1) downto 0);
      tlb_datain           : in  std_logic_vector(GLOB_PHYS_ADDR_BITS - 1 downto 0);
      dma_address          : out std_logic_vector(GLOB_PHYS_ADDR_BITS - 1 downto 0);
      dma_length           : out std_logic_vector(31 downto 0));
  end component;


  -- ram with two AHB interfaces (AHB1->r/w, AHB2->r)

  component ahbram_dp
    generic (
      hindex1 : integer := 0;
      haddr1  : integer := 0;
      hindex2 : integer := 0;
      haddr2  : integer := 0;
      hmask   : integer := 16#fff#;
      tech    : integer := DEFMEMTECH;
      kbytes  : integer := 1;
      wordsz  : integer := AHBDW);
    port (
      rst    : in  std_ulogic;
      clk    : in  std_ulogic;
      ahbsi1 : in  ahb_slv_in_type;
      ahbso1 : out ahb_slv_out_type;
      ahbsi2 : in  ahb_slv_in_type;
      ahbso2 : out ahb_slv_out_type);
  end component;

  -- ESP Link

  component esplink is
    generic (
      APB_DW     : integer;
      APB_AW     : integer;
      REV_ENDIAN : integer range 0 to 1);
    port (
      clk     : in  std_ulogic;
      rstn    : in  std_ulogic;
      srst    : out std_ulogic;
      psel    : in  std_ulogic;
      penable : in  std_ulogic;
      pwrite  : in  std_ulogic;
      paddr   : in  std_logic_vector(APB_AW - 1 downto 0);
      pwdata  : in  std_logic_vector(APB_DW - 1 downto 0);
      pready  : out std_ulogic;
      pslverr : out std_ulogic;
      prdata  : out std_logic_vector(APB_DW - 1 downto 0));
  end component esplink;


  -- Shared Local Memory

  component ahbslm is
    generic (
      hindex : integer;
      haddr  : integer;
      hmask  : integer;
      tech   : integer;
      mbytes : integer);
    port (
      rst    : in  std_ulogic;
      clk    : in  std_ulogic;
      ahbsi  : in  ahb_slv_in_type;
      ahbso  : out ahb_slv_out_type);
  end component ahbslm;

  component slm_bank_1mb_unisim is
    port (
      CLK  : in  std_ulogic;
      CE0  : in  std_ulogic;
      A0   : in  std_logic_vector(16 downto 0);
      D0   : in  std_logic_vector(63 downto 0);
      WE0  : in  std_ulogic;
      WEM0 : in  std_logic_vector(63 downto 0);
      CE1  : in  std_ulogic;
      A1   : in  std_logic_vector(16 downto 0);
      Q1   : out std_logic_vector(63 downto 0));
  end component slm_bank_1mb_unisim;

  component slm_bank_2mb_unisim is
    port (
      CLK  : in  std_ulogic;
      CE0  : in  std_ulogic;
      A0   : in  std_logic_vector(17 downto 0);
      D0   : in  std_logic_vector(63 downto 0);
      WE0  : in  std_ulogic;
      WEM0 : in  std_logic_vector(63 downto 0);
      CE1  : in  std_ulogic;
      A1   : in  std_logic_vector(17 downto 0);
      Q1   : out std_logic_vector(63 downto 0));
  end component slm_bank_2mb_unisim;

  component slm_bank_4mb_unisim is
    port (
      CLK  : in  std_ulogic;
      CE0  : in  std_ulogic;
      A0   : in  std_logic_vector(18 downto 0);
      D0   : in  std_logic_vector(63 downto 0);
      WE0  : in  std_ulogic;
      WEM0 : in  std_logic_vector(63 downto 0);
      CE1  : in  std_ulogic;
      A1   : in  std_logic_vector(18 downto 0);
      Q1   : out std_logic_vector(63 downto 0));
  end component slm_bank_4mb_unisim;


end sldcommon;
