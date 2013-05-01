Each row contains some info about the single block at height blockNumber:

- Time when the block was created (UTC)
- Decimal target
- Difficulty
- The average number of hashes it takes to solve a block at this difficulty

Each row also contains stats that apply to the set of blocks between blockNumber and the previous blockNumber:

- Average interval between blocks.
- Average target over these blocks. This is only different from the block
  target when a retarget occurred in this section. (I'm not totally sure I'm
  doing this correctly.)
- The estimated number of network-wide hashes per second during this time,
  calculated from the average interval and average target.\n";

blockNumber, time, target, avgTargetSinceLast, difficulty, hashesToWin, avgIntervalSinceLast, netHashPerSecond

START DATA
{foreach $rows as $row}
{implode(", ", $row)}
{/foreach}
