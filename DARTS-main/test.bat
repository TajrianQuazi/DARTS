@echo off
REM Simple test script to verify server freeze fix and logging
echo Starting DARTS Server...
start "DARTS Server" .\darts_server.exe

timeout /t 3

echo Connecting first client (alice)...
start "Client Alice" cmd /k "cd /d %cd% && echo alice > input.txt && echo /all Hello from alice >> input.txt && echo /quit >> input.txt && type input.txt | .\darts_client.exe 127.0.0.1 && pause"

timeout /t 2

echo Connecting second client (bob)...
start "Client Bob" cmd /k "cd /d %cd% && echo bob > input.txt && echo /all Hi everyone! >> input.txt && echo /users >> input.txt && echo /quit >> input.txt && type input.txt | .\darts_client.exe 127.0.0.1 && pause"

echo.
echo Test in progress. Check:
echo - Server console for connection logs
echo - darts_chat.log for broadcast messages
echo - Both clients should receive messages without freezing
echo.
pause
