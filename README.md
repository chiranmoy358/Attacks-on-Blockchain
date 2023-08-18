# Attacks-on-Blockchain

Compile as: "g++ main.cpp -o main"

Run as: "./main n alpha zeta mode"

Parameters:
	n: Number of Miners
	alpha: Adversary's hashing power in percentage (value ∈ [1, 100])
	zeta: Percentage of honest miners adversary is connected to (value ∈ [1, 100])
	mode: Selfish or Stubborn mode (value ∈ {self, stub})

Parameters fixed for ease of use:
	Percent of Fast CPU Miners (z0) = 50
	Percent of Fast Link Miners (z1) = 50
	Block Inter-arrival time (tblk) = 600 (in seconds)
	Mean time to generate a transaction (ttxn) = 1000000 (in seconds)
	Number of events to simulate (eventCount) = 750000

The mean time to generate a transaction (ttxn) is kept high to focus on block generation.


Ex: ./main 100 20 20 self
	Number of miners = 100
	alpha = 20 i.e., adversary has .2 fraction of hashing power
	zeta = 20 i.e., adversary is connected to 20% of honest miners
	mode = self i.e., simulate selfish mining attack
    

NOTE: graphviz must be installed and dot must be added to path for tree generation, if not 
installed press "n" during the tree generation prompt.

Graphviz Download Link: https://graphviz.org/download/

After the simulation is completed a prompts will be shown 
    1. "Generate per-miner simulation data and blockchain tree (Note: graphviz must be installed) (y/n): " 
		
	press "y" to generate simulation data for each miner like block arrival time, fork length, etc, and a blockchain tree for adversary and honest node. 
