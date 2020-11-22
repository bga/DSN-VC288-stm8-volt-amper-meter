set buildProfile=Release
if not _%1==_ set buildProfile=%~1
tail -n 6 "%~dp0.\%buildProfile%\List\stm8-volt-amper-meter-iar.map" | head -n 3
