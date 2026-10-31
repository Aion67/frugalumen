# sdcard/

Contents of the simulated microSD card when running under **Wokwi for VS Code** or
**wokwi-cli** (`[[sdcard]]` in `wokwi.toml`, `writeback = true`). After a simulation stops,
`events.csv` appears here and can be opened in a spreadsheet.

In the **browser** simulator this folder is not used: the card lives only in memory while
the simulation runs. Prove the log is on the card with console item 8 (file size) and
item 4 (rows read back), or press the board's reset button and watch item 7's counts keep
growing.
