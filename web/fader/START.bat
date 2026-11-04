@echo off
title BBL Fader Link
python bridge.py
if errorlevel 1 py bridge.py
pause
