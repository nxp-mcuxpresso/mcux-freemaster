@echo off

set ZEPHYR_DIR=..\..\..\..\zephyr

if EXIST %ZEPHYR_DIR% (
	for %%P in (*.patch) do git apply --directory %ZEPHYR_DIR% %%P 
) else (
	echo Error: need ZEPHYR_DIR to tell Zephyr root dir. 
)

